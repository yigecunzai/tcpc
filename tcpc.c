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
#include <stdlib.h>
#include <string.h>


/* thread functions */
static void *client_thread_routine(void *arg)
{
	struct tcpc_server_conn *c = (struct tcpc_server_conn *)arg;

	while(c->_active) {
		sched_yield();
	}

	/* clean up this client */
	if(c->conn_close_h) (c->conn_close_h)(c);
	close(c->_sock);

	return c;
}

static void *listen_thread_routine(void *arg)
{
	struct tcpc_server *s = (struct tcpc_server *)arg;
	struct tcpc_server_conn *nc;
	socklen_t client_addr_len;
	int e;

	while(s->_active) {
		sched_yield();
		e = poll(&s->_listen_poll, 1, 1);
		if(e == 0) {
			/* nothing to do */
			continue;
		} else if(e < 0) {
			/* error */
			perror("listen_thread");
			continue;
		} else {
			/* client is trying to connect */
			if(s->_connection_count >= s->max_connections)
				continue;
			/* get a connection structure */
			nc = (struct tcpc_server_conn *)
				malloc(sizeof(struct tcpc_server_conn));
			if(!nc) {
				perror("listen_thread");
				continue;
			}
			memset(nc, 0, sizeof(struct tcpc_server_conn));
			/* accept the connection */
			client_addr_len = sizeof(nc->client_addr);
			nc->_sock = accept(s->_sock,
				(struct sockaddr *)&nc->client_addr,
				&client_addr_len);
			if(nc->_sock < 0) {
				perror("listen_thread");
				free(nc);
				continue;
			}
			/* fill in the parent pointer */
			nc->_parent = s;
			/* call callback */
			if(s->new_conn_h)
				(s->new_conn_h)(nc);
			/* add connection to list */
			_tcpc_server_add_client(s, nc);
			s->_connection_count++;
			/* start the client thread */
			nc->_active = 1;
			if(pthread_create(&nc->_client_thread, NULL, 
					&client_thread_routine, nc) != 0) {
				perror("listen_thread");
				if(nc->conn_close_h) (nc->conn_close_h)(nc);
				close(nc->_sock);
				_tcpc_server_remove_client(nc->_parent, nc);
				free(nc);
				continue;
			}
		}
	}

	/* clean up clients */
	while(s->_clients) {
		struct tcpc_server_conn *t = s->_clients;
		t->_active = 0;
		pthread_join(t->_client_thread, NULL);
		_tcpc_server_remove_client(t->_parent, t);
		free(t);
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
	s->_listen_poll.fd = sock;

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
	s->_listen_poll.fd = -1;
}
