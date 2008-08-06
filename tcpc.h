/*
 * tcpc.h
 *
 * AUTHOR: Robert C. Curtis
 */

/****************************************************************************/

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <poll.h>

#ifndef I__TCPC_H__
	#define I__TCPC_H__

/****************************************************************************
 * struct tcpc_server 
 * 	DESCRIPTION: Main data structure for a TCP Server. The application 
 * 	must allocate one of these for each server it has running. Use the 
 * 	CREATE_TCPC_SERVER() macro for creating statically allocated data
 * 	structures. Use tcpc_init_server() to initialize a dynamically 
 * 	allocated data structure, or one you've statically allocated yourself.
 */
struct tcpc_server {
	/* server address information */
	struct sockaddr_in serv_addr;

	/* private pointer. to be used by application */
	void *priv;

	/* configuration parameters */
	int max_connections;
	int listen_backlog;

	/* private members - don't modify directly */
	int _sock;
	int _active;
	pthread_t _listen_thread;
};

#define CREATE_TCPC_SERVER(name,port) \
	struct tcpc_server name = { \
		.serv_addr = { \
			.sin_family = AF_INET, \
			.sin_addr.s_addr = htonl(INADDR_ANY), \
			.sin_port = htons(port), \
		}, \
		._sock = -1, \
		.max_connections = 100, \
		.listen_backlog = 10, \
	}

static inline void tcpc_init_server(struct tcpc_server *s, in_port_t port)
{
	s->serv_addr.sin_family = AF_INET;
	s->serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	s->serv_addr.sin_port = htons(port);
	s->_sock = -1;
	s->_active = 0;
	s->_listen_thread = 0;
	s->priv = NULL;
	s->max_connections = 100;
	s->listen_backlog = 10;
}

static inline int tcpc_server_socket(struct tcpc_server *s)
{
	return s->_sock;
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
 * 	and calls the new connection callback. The routine will fail if this
 * 	callback is NULL.
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

#endif /* I__TCPC_H__ */
