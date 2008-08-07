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
 * struct tcpc_server_conn
 * 	DESCRIPTION: TCPC server connection data structure. Linked list data
 * 	structure that contains the information about a current connection to
 * 	the server. These are dynamically allocated by the TCPC server
 * 	listening thread, and freed when a client is disconnected and removed
 * 	from the list.
 */
struct tcpc_server_conn {
	struct tcpc_server_conn *_next;
	struct tcpc_server_conn *_prev;

	/* client address information */
	struct sockaddr_in client_addr;

	/* private pointer. to be used by application */
	void *priv;
};
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

	/* configuration parameters */
	int max_connections;
	int listen_backlog;

	/* private members - don't modify directly */
	int _sock;
	pthread_mutex_t _clients_mutex;
	struct tcpc_server_conn *_clients;
	int _active;
	pthread_t _listen_thread;
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
	pthread_mutex_init(&s->_clients_mutex, NULL);
	s->_clients = NULL;
	s->_active = 0;
	s->_listen_thread = 0;
	s->priv = NULL;
	s->max_connections = 100;
	s->listen_backlog = 10;
}

/* tcpc_server_socket
 * 	DESCRIPTION: returns the socket descriptor for a server
 */
static inline int tcpc_server_socket(struct tcpc_server *s)
{
	return s->_sock;
}

static inline void _tcpc_server_add_client(struct tcpc_server *s,
		struct tcpc_server_conn *c)
{
	pthread_mutex_lock(&s->_clients_mutex);
	/* insert at beginning */
	c->_prev = NULL;
	c->_next = s->_clients;
	if(s->_clients)
		s->_clients->_prev = c;
	s->_clients = c;
	pthread_mutex_unlock(&s->_clients_mutex);
}

static inline void _tcpc_server_remove_client(struct tcpc_server *s,
		struct tcpc_server_conn *c)
{
	pthread_mutex_lock(&s->_clients_mutex);
	if(c->_prev==NULL) {
		/* if I'm the beginning of the list, update the main list
		 * pointer to my next pointer.
		 */
		s->_clients = c->_next;
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
	pthread_mutex_unlock(&s->_clients_mutex);
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
