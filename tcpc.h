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
#include <pthread.h>
#include <poll.h>
#include <stdlib.h>
#include "pt.h"

#ifndef I__TCPC_H__
	#define I__TCPC_H__

#define TCPC_DEFAULT_BUF_SZ	1024
#define TCPC_DEFAULT_POLL_TO	10

/* SERVER FRAMEWORK */
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
	struct sockaddr *conn_addr;

	/* data buffers */
	size_t rxbuf_sz;
	uint8_t *rxbuf;
	pthread_mutex_t rxbuf_mutex;

	/* private pointer. to be used by application */
	void *priv;

	/* configuration parameters */
	int poll_timeout_ms;
	ssize_t (*rx_h)(int sock, void *buf, size_t len);

	/* callbacks */
	/* conn_close_h is called whenever a client connection is closed.
	 */
	void (*conn_close_h)(struct tcpc_server_conn *);
	/* conn_h is called consistently. When len is non-zero, there are len
	 * new bytes in rxbuf.
	 */
	PT_THREAD((*conn_h)(struct tcpc_server_conn *, size_t len));
	pt_t conn_h_pt;

	/* private members - don't modify directly */
	int _sock;
	size_t _sockaddr_size;
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

/* tcpc_server_conn_socket
 * 	DESCRIPTION: returns the socket descriptor for a server connection
 */
static inline int tcpc_server_conn_socket(struct tcpc_server_conn *c)
{
	return c->_sock;
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
	struct sockaddr *serv_addr;

	/* private pointer. to be used by application */
	void *priv;

	/* callbacks */
	void (*new_conn_h)(struct tcpc_server_conn *);

	/* configuration parameters */
	int max_connections;
	int listen_backlog;

	/* private members - don't modify directly */
	int _sock; /* server socket */

	size_t _sockaddr_size; /* size of sockaddr structure */

	pthread_mutex_t _conn_ll_mutex; /* connection linked list mutex */
	struct tcpc_server_conn *_conns_ll; /* connection linked list */

	pthread_mutex_t _conn_count_mutex; /* connection count mutex */
	int _conn_count; /* master connection count for server */

	volatile int _active; /* server will listen until false */
	pthread_t _listen_thread;

	struct pollfd _poll;
};

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

/* free_tcpc_server_members
 * 	DESCRIPTION: free's up all the malloced members of the structure
 */
static inline void free_tcpc_server_members(struct tcpc_server *s)
{
	/* always free everything. free does nothing with NULLs */
	free(s->serv_addr);
	s->serv_addr = NULL;
}
/****************************************************************************/

/* tcpc_init_server
 * 	DESCRIPTION: intializes a tcpc_server structure to default values and 
 * 	allocates the sockaddr structure.
 *
 * 	RETURN VALUES:
 * 		0	- everything went as planned
 * 		errors: errno will be set with specific error information
 * 		-1	- error
 */
int tcpc_init_server(struct tcpc_server *s, size_t sockaddr_size,
		void (*new_conn_h)(struct tcpc_server_conn *));

/* tcpc_open_server
 * 	DESCRIPTION: Initializes a tcp server by opening the socket. Since
 * 	nothing other than creation is done, the socket options can be set
 * 	before tcpc_start_server is called. The serv_addr must be set before
 * 	this call.
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

/* CLIENT FRAMEWORK */
/****************************************************************************
 * struct tcpc_client
 * 	DESCRIPTION: Main data structure describing a client TCP connection
 */
struct tcpc_client {
	/* server address information */
	struct sockaddr *serv_addr;

	/* private pointer. to be used by application */
	void *priv;

	/* configuration parameters */
	int poll_timeout_ms;
	ssize_t (*rx_h)(int sock, void *buf, size_t len);

	/* callbacks */
	/* conn_close_h is called whenever a server connection is closed.
	 */
	void (*conn_close_h)(struct tcpc_client *);
	/* conn_h is called consistently. When len is non-zero, there are len
	 * new bytes in rxbuf.
	 */
	PT_THREAD((*conn_h)(struct tcpc_client *, size_t len));
	pt_t conn_h_pt;

	/* data buffers */
	uint8_t *rxbuf;
	pthread_mutex_t rxbuf_mutex;

	/* private members - don't modify directly */
	int _sock; /* client socket */

	volatile int _active; /* client will stay connected until false */
	pthread_t _client_thread;

	struct pollfd _poll;

	size_t _sockaddr_size;
	size_t _rxbuf_sz;
};

/* tcpc_init_client
 * 	DESCRIPTION: intializes a tcpc_client structure to default values and 
 * 	allocates the sockaddr structure.
 *
 * 	RETURN VALUES:
 * 		0	- everything went as planned
 * 		errors: errno will be set with specific error information
 * 		-1	- error
 */
int tcpc_init_client(struct tcpc_client *c, size_t sockaddr_size,
		size_t rxbuf_sz,
		PT_THREAD((*conn_h)(struct tcpc_client *, size_t len)),
		void (*conn_close_h)(struct tcpc_client *));

/* tcpc_client_socket
 * 	DESCRIPTION: returns the socket descriptor for a client
 */
static inline int tcpc_client_socket(struct tcpc_client *c)
{
	return c->_sock;
}

/* free_tcpc_client_members
 * 	DESCRIPTION: free's up all the malloced members of the structure
 */
static inline void free_tcpc_client_members(struct tcpc_client *c)
{
	/* always free everything. free does nothing with NULLs */
	free(c->rxbuf);
	free(c->serv_addr);
	c->rxbuf = NULL;
	c->serv_addr = NULL;
}
/****************************************************************************/

/* tcpc_open_client
 * 	DESCRIPTION: Initializes a tcp client by opening the socket. Since
 * 	nothing other than creation is done, the socket options can be set
 * 	before tcpc_start_client is called. The serv_addr must be set before
 * 	this call.
 *
 * 	RETURN VALUES:
 * 		0	- everything went as planned
 * 		errors: errno will be set with specific error information
 * 		-1	- error creating socket
 */
int tcpc_open_client(struct tcpc_client *c);

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
int tcpc_start_client(struct tcpc_client *c);

/* tcpc_close_client
 * 	DESCRIPTION: stops and closes tcp client. this will end the thread
 * 	associated with the client as well.
 */
void tcpc_close_client(struct tcpc_client *c);

/* tcpc_client_send_to
 * 	DESCRIPTION: sends a buffer to a connected server. This function is
 * 	basically a direct interface to SEND(2). Return values are directly
 * 	from send(), and flags are sent directly to send(). The MSG_NOSIGNAL
 * 	flag is always passed to send(). You must check for the EPIPE return
 * 	value if the other end breaks the connection.
 */
static inline int tcpc_client_send_to(struct tcpc_client *c,
		const void *buf, size_t len, int flags)
{
	return send(c->_sock, buf, len, flags | MSG_NOSIGNAL);
}

#endif /* I__TCPC_H__ */
