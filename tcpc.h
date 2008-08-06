/*
 * tcpc.h
 *
 * AUTHOR: Robert C. Curtis
 */

/****************************************************************************/

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifndef I__TCPC_H__
	#define I__TCPC_H__

/****************************************************************************
 * struct tcpc_server 
 * 	DESCRIPTION: Main data structure for a TCP Server. The application 
 * 	must allocate one of these for each server it has running. Use the 
 * 	CREATE_TCPC_SERVER() macro for creating statically allocated data
 * 	structures. Use INIT_TCPC_SERVER() to initialize a dynamically 
 * 	allocated data structure, or one you've statically allocated yourself.
 */
struct tcpc_server {
	struct sockaddr_in servAddr;
	int sock;
	void *priv;
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
	s->sock = 0;
	s->priv = NULL;
	s->servAddr.sin_family = AF_INET;
	s->servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	s->servAddr.sin_port = htons(port);
}
/****************************************************************************/

#endif /* I__TCPC_H__ */
