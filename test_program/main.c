#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "tcpc.h"


/* main tcpc server structure */
static struct tcpc_server test_server;

/* signal handling */
static volatile int end_process = 0;

void signal_handler(int sig)
{
	end_process = 1;
}

static struct sigaction act = {
	.sa_handler = &signal_handler,
};
/*******************/

/* callbacks */
void conn_close(struct tcpc_server_conn *c)
{
	printf("Closing Connection: %08x\n",
			ntohl(c->client_addr.sin_addr.s_addr));
}

void new_conn(struct tcpc_server_conn *c)
{
	printf("New Connection: %08x\n",ntohl(c->client_addr.sin_addr.s_addr));
	c->conn_close_h = &conn_close;
}

/* Main Routine */
int main(int argc,char *argv[])
{
	int port;
	int one = 1;

	if(argc != 2)
		return 1;

	port = atoi(argv[1]);

	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGKILL, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	printf("Starting server on port: %d\n",port);

	tcpc_init_server(&test_server, (in_port_t)port);
	test_server.new_conn_h = &new_conn;
	if(tcpc_open_server(&test_server) < 0)
		return 1;
	setsockopt(tcpc_server_socket(&test_server), SOL_SOCKET, SO_REUSEADDR, 
			&(one), sizeof(one));
	if(tcpc_start_server(&test_server) < 0)
		return 1;

	while(!end_process);

	printf("Stopping server\n");

	tcpc_close_server(&test_server);

	return 0;
}
