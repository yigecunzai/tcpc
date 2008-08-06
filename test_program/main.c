#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "tcpc.h"

static struct tcpc_server test_server;

static volatile end_process = 0;

void signal_handler(int sig)
{
	end_process = 1;
}

static const struct sigaction act = {
	.sa_handler = &signal_handler,
};

int main(int argc,char *argv[])
{
	int port;

	if(argc != 2)
		return 1;

	port = atoi(argv[1]);

	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGKILL, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	printf("Starting server on port: %d\n",port);

	tcpc_init_server(&test_server, (in_port_t)port);
	if(tcpc_open_server(&test_server) < 0)
		return 1;
	if(tcpc_start_server(&test_server) < 0)
		return 1;

	while(!end_process);

	printf("Stopping server\n");

	tcpc_close_server(&test_server);

	return 0;
}
