/*
 * tcpc.c
 *
 * AUTHOR:      Robert C. Curtis
 *
 * DESCRIPTION: TCPC is a threaded TCP server/client framework.
 */

/****************************************************************************/

#include "tcpc.h"
#include <stdio.h>
#include <unistd.h>


/* thread functions */
static void *listen_thread_routine(void *arg)
{
	struct tcpc_server *s = (struct tcpc_server *)arg;
	while(s->_active) {
		sched_yield();
	}
	return NULL;
}


/* api functions */
int tcpc_open_server(struct tcpc_server *s)
{
	int sock = socket(s->serv_addr.sin_family, SOCK_STREAM, 0);
	if(sock < 0) {
		perror("tcpc_open_server");
		return -1;
	}
	s->_sock = sock;
	return 0;
}

int tcpc_start_server(struct tcpc_server *s)
{
	/* check for valid socket descriptor */
	if(s->_sock < 0) {
		return -1;
	}

	/* bind the socket to serv_addr */
	if(bind(s->_sock, (struct sockaddr *) &s->serv_addr,
			sizeof(s->serv_addr)) < 0) {
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
	 * take care of closing all the client connections and joining those
	 * threads
	 */
	s->_active=0;
	pthread_join(s->_listen_thread, NULL);
	close(s->_sock);
	s->_sock = -1;
}
