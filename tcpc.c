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


/* thread functions */
static void *client_thread_routine(void *arg)
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
			/* client has closed */
			c->_active = 0;
		}
	}

	/* clean up this client */
	/* close the socket */
	close(c->_sock);
	/* remove from the linked list of clients */
	_tcpc_server_remove_client(c->_parent, c);
	/* decrement the connection count in the parent */
	pthread_mutex_lock(&c->_parent->_conn_count_mutex);
	c->_parent->_conn_count--;
	pthread_mutex_unlock(&c->_parent->_conn_count_mutex);
	/* call the callback */
	if(c->conn_close_h)
		(c->conn_close_h)(c);
	/* free the memory */
	free(c);

	return NULL;
}

static void *listen_thread_routine(void *arg)
{
	struct tcpc_server *s = (struct tcpc_server *)arg;
	struct tcpc_server_conn *nc;
	socklen_t client_addr_len;
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
			/* add connection to list */
			_tcpc_server_add_client(s, nc);
			pthread_mutex_lock(&s->_conn_count_mutex);
			s->_conn_count++;
			pthread_mutex_unlock(&s->_conn_count_mutex);
			/* set the default buffer size */
			nc->rxbuf_sz = TCPC_DEFAULT_BUF_SZ;
			/* initialize the rx buffer mutex */
			pthread_mutex_init(&nc->rxbuf_mutex, NULL);
			/* call callback */
			if(s->new_conn_h)
				(s->new_conn_h)(nc);
			/* allocate client buffers */
			nc->rxbuf = (uint8_t *)malloc(nc->rxbuf_sz);
			if(!nc->rxbuf) {
				perror("listen_thread");
				if(nc->conn_close_h)
					(nc->conn_close_h)(nc);
				close(nc->_sock);
				_tcpc_server_remove_client(nc->_parent, nc);
				free(nc);
				continue;
			}
			/* start the client thread */
			nc->_active = 1;
			if(pthread_create(&nc->_client_thread, NULL, 
					&client_thread_routine, nc) != 0) {
				perror("listen_thread");
				if(nc->conn_close_h)
					(nc->conn_close_h)(nc);
				close(nc->_sock);
				_tcpc_server_remove_client(nc->_parent, nc);
				free(nc);
				continue;
			}
		}
	}

	/* clean up clients */
	pthread_mutex_lock(&s->_clients_mutex);
	while(s->_clients) {
		pthread_t ct = s->_clients->_client_thread;
		s->_clients->_active = 0;
		pthread_mutex_unlock(&s->_clients_mutex);
		pthread_join(ct, NULL);
		pthread_mutex_lock(&s->_clients_mutex);
	}
	pthread_mutex_unlock(&s->_clients_mutex);

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
	s->_poll.fd = -1;
}
