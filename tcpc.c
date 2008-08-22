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
#include <stdlib.h>
#include <string.h>


/* local helper functions */
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

	c->_poll.fd = c->_sock;
	c->_poll.events = POLLRDHUP | POLLIN;

	while(c->_active) {
		sched_yield();
		c->_poll.revents = 0;
		if(poll(&c->_poll, 1, 1) < 0) {
			/* error */
			perror("listen_thread");
			continue;
		}
		/* handle the revents */
		if(c->_poll.revents & POLLIN) {
			/* data available */
			ssize_t l;
			pthread_mutex_lock(&c->rxbuf_mutex);
			l=recv(c->_sock, c->rxbuf, c->rxbuf_sz, 0);
			pthread_mutex_unlock(&c->rxbuf_mutex);
			if(l < 0) {
				/* error */
				perror("listen_thread");
				continue;
			} else if(l > 0 && c->new_data_h) {
				(c->new_data_h)(c, (size_t)l);
			}
		}
		if(c->_poll.revents & POLLRDHUP) {
			/* connection has closed */
			c->_active = 0;
		}
	}

	/* clean up this connection */
	/* close the socket */
	close(c->_sock);
	/* remove from the linked list of connections */
	_tcpc_server_remove_conn(c->_parent, c);
	/* decrement the connection count in the parent */
	pthread_mutex_lock(&c->_parent->_conn_count_mutex);
	c->_parent->_conn_count--;
	pthread_mutex_unlock(&c->_parent->_conn_count_mutex);
	/* call the callback */
	if(c->conn_close_h)
		(c->conn_close_h)(c);
	/* free the memory */
	_free_tcpc_server_conn(c);

	return NULL;
}

static void *listen_thread_routine(void *arg)
{
	struct tcpc_server *s = (struct tcpc_server *)arg;
	struct tcpc_server_conn *nc;
	int e;

	while(s->_active) {
		sched_yield();
		e = poll(&s->_poll, 1, 1);
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
		}
	}

	/* clean up connections */
	pthread_mutex_lock(&s->_conn_ll_mutex);
	while(s->_conns_ll) {
		pthread_t ct = s->_conns_ll->_server_conn_thread;
		s->_conns_ll->_active = 0;
		pthread_mutex_unlock(&s->_conn_ll_mutex);
		pthread_join(ct, NULL);
		pthread_mutex_lock(&s->_conn_ll_mutex);
	}
	pthread_mutex_unlock(&s->_conn_ll_mutex);

	return NULL;
}


/* api functions */
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
	s->_active=0;
	pthread_join(s->_listen_thread, NULL);
	close(s->_sock);
	s->_sock = -1;
	s->_poll.fd = -1;
	free(s->serv_addr);
	s->serv_addr = NULL;
}
