/*
 * tcpc.h - Main header file for the TCPC framework.
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

#define _GNU_SOURCE
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>

#ifndef I__TCPC_H__
	#define I__TCPC_H__

#define TCPC_DEFAULT_BUF_SZ	1024

/****************************************************************************
 * struct tcpc_server_conn
 * 	DESCRIPTION: TCPC server connection data structure. Linked list data
 * 	structure that contains the information about a current connection to
 * 	the server. These are dynamically allocated by the TCPC server
 * 	listening thread, and freed when a connection is disconnected and 
 * 	removed from the list. rxbuf_sz can be set to a desired value during 
 * 	the new_conn_h callback.
 */
struct tcpc_server_conn {
	/* connection address information */
	struct sockaddr_in conn_addr;

	/* data buffers */
	size_t rxbuf_sz;
	uint8_t *rxbuf;
	pthread_mutex_t rxbuf_mutex;

	/* private pointer. to be used by application */
	void *priv;

	/* callbacks */
	void (*conn_close_h)(struct tcpc_server_conn *);
	void (*new_data_h)(struct tcpc_server_conn *, size_t len);

	/* private members - don't modify directly */
	int _sock;
	volatile int _active;
	pthread_t _server_conn_thread;
	struct pollfd _poll;
	struct tcpc_server *_parent;
	struct tcpc_server_conn *_next;
	struct tcpc_server_conn *_prev;
};

/* tcpc_conn_server
 * 	DESCRIPTION: returns the server handling the connection
 */
static inline struct tcpc_server *tcpc_conn_server(struct tcpc_server_conn *c)
{
	return c->_parent;
}
/****************************************************************************/

/****************************************************************************
 * struct tcpc_server 
 * 	DESCRIPTION: Main data structure for a TCP Server. The application 
 * 	must allocate one of these for each server it has running. Use 
 * 	tcpc_init_server() to initialize a data structure. 
 */
struct tcpc_server {
	/* server address information */
	struct sockaddr_in serv_addr;

	/* private pointer. to be used by application */
	void *priv;

	/* callbacks */
	void (*new_conn_h)(struct tcpc_server_conn *);

	/* configuration parameters */
	int max_connections;
	int listen_backlog;

	/* private members - don't modify directly */
	int _sock; /* server socket */

	pthread_mutex_t _conn_ll_mutex; /* connection linked list mutex */
	struct tcpc_server_conn *_conns_ll; /* connection linked list */

	pthread_mutex_t _conn_count_mutex; /* connection count mutex */
	int _conn_count; /* master connection count for server */

	volatile int _active; /* server will listen until false */
	pthread_t _listen_thread;

	struct pollfd _poll;
};

/* tcpc_open_server
 * 	DESCRIPTION: intializes a tcpc_server structure to default values
 */
static inline void tcpc_init_server(struct tcpc_server *s, in_port_t port)
{
	s->serv_addr.sin_family = AF_INET;
	s->serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	s->serv_addr.sin_port = htons(port);

	s->_sock = -1;

	pthread_mutex_init(&s->_conn_ll_mutex, NULL);
	s->_conns_ll = NULL;
	pthread_mutex_init(&s->_conn_count_mutex, NULL);
	s->_conn_count = 0;

	s->_active = 0;
	s->_listen_thread = 0;

	s->priv = NULL;

	s->max_connections = 100;
	s->listen_backlog = 10;

	s->new_conn_h = NULL;

	s->_poll.fd = -1;
	s->_poll.events = POLLIN;
	s->_poll.revents = 0;
}

/* tcpc_server_socket
 * 	DESCRIPTION: returns the socket descriptor for a server
 */
static inline int tcpc_server_socket(struct tcpc_server *s)
{
	return s->_sock;
}

/* tcpc_server_conn_count
 * 	DESCRIPTION: returns the connection count for a server
 */
static inline int tcpc_server_conn_count(struct tcpc_server *s)
{
	return s->_conn_count;
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
/****************************************************************************/

/* tcpc_open_server
 * 	DESCRIPTION: Initializes a tcp server by opening the socket. Since
 * 	nothing other than creation is done, the socket options can be set
 * 	before tcpc_start_server is called.
 *
 * 	RETURN VALUES:
 * 		0	- everything went as planned
 * 		errors: errno will be set with specific error information
 * 		-1	- error creating socket
 */
int tcpc_open_server(struct tcpc_server *s);

/* tcpc_start_server
 * 	DESCRIPTION: starts a tcp server as described by a struct tcpc_server.
 * 	The socket is initialized and set up to listen, and the main listen
 * 	thread is started. The main listen thread waits for connecting clients
 * 	and calls the new connection callback.
 *
 * 	RETURN VALUES:
 * 		0	- everything went as planned
 * 		-1	- no socket (no errno. you messed up)
 * 		errors: errno will be set with specific error information
 * 		-2	- error binding socket
 * 		-3	- error setting socket to listen
 * 		-4	- error creating listen thread
 */
int tcpc_start_server(struct tcpc_server *s);

/* tcpc_close_server
 * 	DESCRIPTION: stops and closes tcp server. this will end all threads
 * 	associated with the server as well.
 */
void tcpc_close_server(struct tcpc_server *s);

/* tcpc_server_send_to
 * 	DESCRIPTION: sends a buffer to a server connection. This function is
 * 	basically a direct interface to SEND(2). Return values are directly
 * 	from send(), and flags are sent directly to send(). The MSG_NOSIGNAL
 * 	flag is always passed to send(). You must check for the EPIPE return
 * 	value if the other end breaks the connection.
 */
static inline int tcpc_server_send_to(struct tcpc_server_conn *c,
		const void *buf, size_t len, int flags)
{
	return send(c->_sock, buf, len, flags | MSG_NOSIGNAL);
}

#endif /* I__TCPC_H__ */
