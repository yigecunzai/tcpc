/*
 * tcpc.h
 *
 * AUTHOR: Robert C. Curtis
 */

/****************************************************************************/

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <poll.h>

#ifndef I__TCPC_H__
	#define I__TCPC_H__

/* function return values */
#define TCPC_SUCCESS		0

/****************************************************************************
 * struct tcpc_server 
 * 	DESCRIPTION: Main data structure for a TCP Server. The application 
 * 	must allocate one of these for each server it has running. Use the 
 * 	CREATE_TCPC_SERVER() macro for creating statically allocated data
 * 	structures. Use INIT_TCPC_SERVER() to initialize a dynamically 
 * 	allocated data structure, or one you've statically allocated yourself.
 */
struct tcpc_server {
	/* server address information */
	struct sockaddr_in servAddr;

	/* private pointer. to be used by application */
	void *priv;

	/* private members - don't modify directly */
	int _sock;
	int _active;
};

#define CREATE_TCPC_SERVER(name,port) \
	struct tcpc_server name = { \
		.servAddr = { \
			.sin_family = AF_INET, \
			.sin_addr.s_addr = htonl(INADDR_ANY), \
			.sin_port = htons(port), \
		}, \
	}

static inline void INIT_TCPC_SERVER(struct tcpc_server *s, in_port_t port) {
	s->_sock = 0;
	s->_active = 0;
	s->priv = NULL;
	s->servAddr.sin_family = AF_INET;
	s->servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	s->servAddr.sin_port = htons(port);
}
/****************************************************************************/

/* tcpc_start_server
 * 	DESCRIPTION: starts a tcp server as described by a struct tcpc_server.
 * 	The socket is initialized and set up to listen, and the main listen
 * 	thread is started. The main listen thread waits for connecting clients
 * 	and calls the new connection callback. The routine will fail if this
 * 	callback is NULL.
 *
 * 	RETURN VALUES:
 * 		TCPC_SUCCESS
 */

#endif /* I__TCPC_H__ */
