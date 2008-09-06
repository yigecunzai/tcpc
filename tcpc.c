/*
 * tcpc.c - Main source file for the TCPC framework.
 *
 * This file is part of TCPC.
 *
 * Copyright (C) 2008 Robert C. Curtis
 *
 * TCPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * TCPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with TCPC.  If not, see <http://www.gnu.org/licenses/>.
 */

/****************************************************************************/

#include "tcpc.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>


/* local helper functions */
static ssize_t _tcpc_rx_handler(int sock, void *buf, size_t len)
{
	return recv(sock, buf, len, 0);
}

static ssize_t _tcpc_tx_handler(int sock, const void *buf, size_t len, 
		int flags)
{
	return send(sock, buf, len, flags | MSG_NOSIGNAL);
}

static inline void _tcpc_server_add_conn(struct tcpc_server *s,
		struct tcpc_server_conn *c)
{
	pthread_mutex_lock(&s->_conn_ll_mutex);
	/* insert at beginning */
	c->_prev = NULL;
	c->_next = s->_conns_ll;
	if(s->_conns_ll)
		s->_conns_ll->_prev = c;
	s->_conns_ll = c;
	pthread_mutex_unlock(&s->_conn_ll_mutex);
}

static inline void _tcpc_server_remove_conn(struct tcpc_server *s,
		struct tcpc_server_conn *c)
{
	pthread_mutex_lock(&s->_conn_ll_mutex);
	if(c->_prev==NULL) {
		/* if I'm the beginning of the list, update the main list
		 * pointer to my next pointer.
		 */
		s->_conns_ll = c->_next;
	} else {
		/* otherwise, update the previous node's next pointer to my
		 * next pointer
		 */
		c->_prev->_next = c->_next;
	}
	/* if I have a next pointer, update its previous pointer to my
	 * previous pointer. this will be another node unless I'm the first
	 * element, and then it will be a NULL
	 */
	if(c->_next)
		c->_next->_prev = c->_prev;
	pthread_mutex_unlock(&s->_conn_ll_mutex);
}

static inline void _free_tcpc_server_conn(struct tcpc_server_conn *c)
{
	/* always free everything. free does nothing with NULLs */
	free(c->rxbuf);
	free(c->conn_addr);
	free(c);
}

static inline struct tcpc_server_conn *_setup_server_conn(struct tcpc_server *s)
{
	struct tcpc_server_conn *nc;

	/* get a connection structure */
	nc = (struct tcpc_server_conn *)malloc(sizeof(struct tcpc_server_conn));
	if(!nc) {
		perror("_setup_server_conn");
		return NULL;
	}
	/* clear the memory */
	memset(nc, 0, sizeof(struct tcpc_server_conn));
	/* allocate the sockaddr */
	nc->_sockaddr_size = s->_sockaddr_size;
	nc->conn_addr = (struct sockaddr *)malloc(nc->_sockaddr_size);
	if(!nc->conn_addr) {
		_free_tcpc_server_conn(nc);
		perror("_setup_server_conn");
		return NULL;
	}
	/* fill in the parent pointer */
	nc->_parent = s;
	/* set the default buffer size */
	nc->rxbuf_sz = TCPC_DEFAULT_BUF_SZ;
	/* set the default poll timeout */
	nc->poll_timeout_ms = TCPC_DEFAULT_POLL_TO;
	/* set the default rx handler */
	nc->rx_h = &_tcpc_rx_handler;
	/* set the default tx handler */
	nc->tx_h = &_tcpc_tx_handler;
	/* accept the connection */
	nc->_sock = accept(s->_sock, nc->conn_addr, &nc->_sockaddr_size);
	if(nc->_sock < 0) {
		perror("_setup_server_conn");
		_free_tcpc_server_conn(nc);
		return NULL;
	}
	/* add connection to list */
	_tcpc_server_add_conn(s, nc);
	pthread_mutex_lock(&s->_conn_count_mutex);
	s->_conn_count++;
	pthread_mutex_unlock(&s->_conn_count_mutex);
	/* initialize the rx buffer mutex */
	pthread_mutex_init(&nc->rxbuf_mutex, NULL);
	/* call callback */
	if(s->new_conn_h)
		(s->new_conn_h)(nc);
	/* allocate connection buffers - done after callback so callback can
	 * change default size
	 */
	nc->rxbuf = (uint8_t *)malloc(nc->rxbuf_sz);
	if(!nc->rxbuf) {
		perror("_setup_server_conn");
		if(nc->conn_close_h)
			(nc->conn_close_h)(nc);
		close(nc->_sock);
		_tcpc_server_remove_conn(nc->_parent, nc);
		_free_tcpc_server_conn(nc);
		return NULL;
	}

	return nc;
}

/* thread functions */
static void *server_conn_thread_routine(void *arg)
{
	struct tcpc_server_conn *c = (struct tcpc_server_conn *)arg;
	ssize_t l;

	c->_poll.fd = c->_sock;
	c->_poll.events = POLLRDHUP | POLLIN;

	while(c->_active > 0) {
		l = 0; /* initialize length to 0 on each loop */
		/* check for data in the socket */
		c->_poll.revents = 0;
		if(poll(&c->_poll, 1, c->poll_timeout_ms) < 0) {
			/* error */
			perror("server_conn_thread");
			continue;
		}
		/* handle the revents */
		if(c->_poll.revents & POLLRDHUP) {
			/* connection has closed */
			c->_active = 0;
			continue;
		}
		if(c->_poll.revents & POLLIN) {
			/* data available */
			if(pthread_mutex_trylock(&c->rxbuf_mutex) == 0) {
				l=(c->rx_h)(c->_sock, c->rxbuf, c->rxbuf_sz);
				pthread_mutex_unlock(&c->rxbuf_mutex);
				if(l == 0) {
					/* connection closed */
					c->_active = 0;
					continue;
				} else if(l < 0) {
					/* error */
					perror("server_conn_thread");
					continue;
				}
			}
		}
		/* call the connection protothread */
		if(c->conn_h) {
			if((c->conn_h)(c, (size_t)l) == PT_ENDED) {
				/* connection thread has ended */
				c->_active = 0;
			}
		}
	}

	/* clean up this connection */
	/* call the close callback */
	if(c->conn_close_h)
		(c->conn_close_h)(c);
	/* close the socket */
	close(c->_sock);
	/* remove from the linked list of connections */
	_tcpc_server_remove_conn(c->_parent, c);
	/* decrement the connection count in the parent */
	pthread_mutex_lock(&c->_parent->_conn_count_mutex);
	c->_parent->_conn_count--;
	pthread_mutex_unlock(&c->_parent->_conn_count_mutex);
	/* free the memory */
	_free_tcpc_server_conn(c);

	return NULL;
}

static void *listen_thread_routine(void *arg)
{
	struct tcpc_server *s = (struct tcpc_server *)arg;
	struct tcpc_server_conn *nc;
	int e;

	while(s->_active > 0) {
		e = poll(&s->_poll, 1, 100);
		if(e == 0) {
			/* nothing to do */
			continue;
		} else if(e < 0) {
			/* error */
			perror("listen_thread");
			continue;
		} else {
			/* client is trying to connect */
			if(s->_conn_count >= s->max_connections)
				continue;
			if((nc = _setup_server_conn(s)) == NULL)
				continue;
			/* start the connection thread */
			nc->_active = 1;
			if(pthread_create(&nc->_server_conn_thread, NULL, 
					&server_conn_thread_routine, nc) != 0) {
				perror("listen_thread");
				if(nc->conn_close_h)
					(nc->conn_close_h)(nc);
				close(nc->_sock);
				_tcpc_server_remove_conn(nc->_parent, nc);
				_free_tcpc_server_conn(nc);
				continue;
			}
			pthread_detach(nc->_server_conn_thread);
		}
	}

	/* clean up connections */
	pthread_mutex_lock(&s->_conn_ll_mutex);
	while(s->_conns_ll) {
		s->_conns_ll->_active = 0;
		pthread_mutex_unlock(&s->_conn_ll_mutex);
		/* give the thread a chance to exit */
		sched_yield();
		pthread_mutex_lock(&s->_conn_ll_mutex);
	}
	pthread_mutex_unlock(&s->_conn_ll_mutex);

	s->_active = 0;

	return NULL;
}

static void *client_thread_routine(void *arg)
{
	struct tcpc_client *c = (struct tcpc_client *)arg;
	ssize_t l;

	c->_poll.events = POLLRDHUP | POLLIN;

	while(c->_active > 0) {
		l = 0; /* initialize length to 0 on each loop */
		/* check for data in the socket */
		c->_poll.revents = 0;
		if(poll(&c->_poll, 1, c->poll_timeout_ms) < 0) {
			/* error */
			perror("client_thread");
			continue;
		}
		/* handle the revents */
		if(c->_poll.revents & POLLRDHUP) {
			/* connection has closed */
			c->_active = 0;
			continue;
		}
		if(c->_poll.revents & POLLIN) {
			/* data available */
			if(pthread_mutex_trylock(&c->rxbuf_mutex) == 0) {
				l=(c->rx_h)(c->_sock, c->rxbuf, c->_rxbuf_sz);
				pthread_mutex_unlock(&c->rxbuf_mutex);
				if(l == 0) {
					/* connection closed */
					c->_active = 0;
					continue;
				} else if(l < 0) {
					/* error */
					perror("client_thread");
					continue;
				}
			}
		}
		/* call the connection protothread */
		if(c->conn_h) {
			if((c->conn_h)(c, (size_t)l) == PT_ENDED) {
				/* connection thread has ended */
				c->_active = 0;
			}
		}
	}

	/* clean up this connection */
	/* close the socket */
	close(c->_sock);
	c->_sock = -1;
	c->_poll.fd = -1;

	/* call the close callback */
	if(c->conn_close_h)
		(c->conn_close_h)(c);

	/* since everything is cleaned up, we can set our state to inactive */
	c->_active = 0;

	return NULL;
}


/* API FUNCTIONS */
/* SERVER FRAMEWORK */
int tcpc_init_server(struct tcpc_server *s, size_t sockaddr_size,
		void (*new_conn_h)(struct tcpc_server_conn *))
{
	/* clear the structure */
	memset(s, 0, sizeof(struct tcpc_server));

	/* allocate the sockaddr */
	if((s->serv_addr = (struct sockaddr *)malloc(sockaddr_size))==NULL) {
		perror("tcpc_init_server");
		return -1;
	}
	/* set the sockaddr size */
	s->_sockaddr_size = sockaddr_size;

	/* init the socket descriptor to an invalid state */
	s->_sock = -1;

	/* init the mutexes */
	pthread_mutex_init(&s->_conn_ll_mutex, NULL);
	pthread_mutex_init(&s->_conn_count_mutex, NULL);

	/* set the default configurations */
	s->max_connections = 100;
	s->listen_backlog = 10;

	/* setup the poll */
	s->_poll.fd = -1;
	s->_poll.events = POLLIN;
	s->_poll.revents = 0;

	/* set the callback */
	s->new_conn_h = new_conn_h;

	return 0;
}

int tcpc_open_server(struct tcpc_server *s)
{
	int sock = socket(s->serv_addr->sa_family, SOCK_STREAM, 0);
	if(sock < 0) {
		perror("tcpc_open_server");
		return -1;
	}
	s->_sock = sock;
	s->_poll.fd = sock;

	return 0;
}

int tcpc_start_server(struct tcpc_server *s)
{
	/* check for valid socket descriptor */
	if(s->_sock < 0) {
		return -1;
	}

	/* bind the socket to serv_addr */
	if(bind(s->_sock, s->serv_addr, s->_sockaddr_size) < 0) {
		perror("tcpc_start_server");
		return -2;
	}

	/* set socket to listen for connections */
	if(listen(s->_sock,s->listen_backlog) < 0) {
		perror("tcpc_start_server");
		return -3;
	}

	/* start the main listen thread */
	s->_active = 1;
	if(pthread_create(&s->_listen_thread, NULL, &listen_thread_routine, s)
			!= 0) {
		perror("tcpc_start_server");
		return -4;
	}

	return 0;
}

void tcpc_close_server(struct tcpc_server *s)
{
	/* tell the main listen thread to end and wait for it to join. it will
	 * take care of closing all the connections and joining those threads
	 */
	s->_active = -1;
	pthread_join(s->_listen_thread, NULL);
	close(s->_sock);
	s->_sock = -1;
	s->_poll.fd = -1;
}

/* CLIENT FRAMEWORK */
int tcpc_init_client(struct tcpc_client *c, size_t sockaddr_size,
		size_t rxbuf_sz,
		PT_THREAD((*conn_h)(struct tcpc_client *, size_t len)),
		void (*conn_close_h)(struct tcpc_client *))
{
	/* clear the structure */
	memset(c, 0, sizeof(struct tcpc_client));

	/* allocate the sockaddr */
	if((c->serv_addr = (struct sockaddr *)malloc(sockaddr_size))==NULL) {
		perror("tcpc_init_client");
		return -1;
	}
	/* set the sockaddr size */
	c->_sockaddr_size = sockaddr_size;

	/* init the socket descriptor to an invalid state */
	c->_sock = -1;

	/* setup the poll */
	c->_poll.fd = -1;
	c->_poll.events = POLLIN;
	c->_poll.revents = 0;

	/* set the default poll timeout */
	c->poll_timeout_ms = TCPC_DEFAULT_POLL_TO;

	/* init the rxbuf mutex */
	pthread_mutex_init(&c->rxbuf_mutex, NULL);

	/* allocate the receive buffer */
	if((c->rxbuf = (uint8_t *)malloc(rxbuf_sz)) == NULL) {
		free_tcpc_client_members(c);
		perror("tcpc_init_client");
		return -1;
	}
	c->_rxbuf_sz = rxbuf_sz;

	/* set the default rx handler */
	c->rx_h = &_tcpc_rx_handler;
	/* set the default tx handler */
	c->tx_h = &_tcpc_tx_handler;

	/* set the callbacks */
	c->conn_h = conn_h;
	c->conn_close_h = conn_close_h;

	return 0;
}

int tcpc_open_client(struct tcpc_client *c)
{
	int sock = socket(c->serv_addr->sa_family, SOCK_STREAM, 0);
	if(sock < 0) {
		perror("tcpc_open_client");
		return -1;
	}
	c->_sock = sock;
	c->_poll.fd = sock;

	return 0;
}

/* tcpc_start_client
 * 	DESCRIPTION: starts a tcp client and connects to the server. If the
 * 	connection succeeds, the clients listen thread is started. The listen
 * 	thread calls the callbacks as necessary.
 *
 * 	RETURN VALUES:
 * 		0	- everything went as planned
 * 		-1	- no socket (no errno. you messed up)
 * 		errors: errno will be set with specific error information
 * 		-2	- error connecting socket
 * 		-3	- error creating client thread
 */
int tcpc_start_client(struct tcpc_client *c)
{
	/* check for valid socket descriptor */
	if(c->_sock < 0) {
		return -1;
	}

	/* bind the socket to serv_addr */
	if(connect(c->_sock, c->serv_addr, c->_sockaddr_size) < 0) {
		perror("tcpc_start_client");
		return -2;
	}

	/* start the client thread */
	c->_active = 1;
	if(pthread_create(&c->_client_thread, NULL, &client_thread_routine, c)
			!= 0) {
		perror("tcpc_start_client");
		return -3;
	}
	pthread_detach(c->_client_thread);

	return 0;
}

/* tcpc_close_client
 * 	DESCRIPTION: stops and closes tcp client. this will end the thread
 * 	associated with the client as well.
 */
void tcpc_close_client(struct tcpc_client *c)
{
	if(c->_active > 0) {
		c->_active = -1;
		while(c->_active != 0);
	}
}
