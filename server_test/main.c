#include "tcpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>


/* main tcpc server structure */
static struct tcpc_server test_server;

/* signal handling */
static pthread_cond_t end_process = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t end_process_mutex = PTHREAD_MUTEX_INITIALIZER;

void signal_handler(int sig)
{
	pthread_cond_broadcast(&end_process);
}

static struct sigaction act = {
	.sa_handler = &signal_handler,
};
/*******************/

/* callbacks */
/* called when a server connection is closed */
void conn_close(struct tcpc_server_conn *c)
{
	printf("Closing Connection: %08x\n",
		ntohl(((struct sockaddr_in *)c->conn_addr)->sin_addr.s_addr));
	printf("Connection_Count: %d\n",
			tcpc_server_conn_count(tcpc_conn_server(c)));
}

/* called when a server connection has new data */
void new_data(struct tcpc_server_conn *c, size_t len)
{
	printf("New Data: %d\n",len);
	if(tcpc_server_send_to(c, c->rxbuf, len, 0) < 0)
		perror("Could not send data");
}

/* called when a new client has connected */
void new_conn(struct tcpc_server_conn *c)
{
	const char *greeting = "Hello from TCPC\r\n";

	printf("New Connection: %08x\n",
		ntohl(((struct sockaddr_in *)c->conn_addr)->sin_addr.s_addr));
	/* a new client has connected, so fill in the callbacks */
	c->conn_close_h = &conn_close;
	c->new_data_h = &new_data;
	printf("Connection_Count: %d\n",
			tcpc_server_conn_count(tcpc_conn_server(c)));
	if(tcpc_server_send_to(c, greeting, strlen(greeting), 0) < 0)
		perror("Could not send data");
}

/* Main Routine */
int main(int argc,char *argv[])
{
	int port;
	int one = 1;

	/* stupid argument checking to grab the port off the command line */
	if(argc != 2)
		return 1;
	/* grab the port */
	port = atoi(argv[1]);

	/* set up the signal handlers so we can shut down cleanly if given the
	 * chance
	 */
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	printf("Starting server on port: %d\n",port);

	/* initialize our server structure */
	if((tcpc_init_server(&test_server, sizeof(struct sockaddr_in),
			&new_conn)) < 0) {
		printf("Failed to start server\n");
		return 1;
	}

	/* setup the server address structure for our protocol family */
	((struct sockaddr_in *)test_server.serv_addr)->sin_family = AF_INET;
	((struct sockaddr_in *)test_server.serv_addr)->sin_port = htons(port);
	((struct sockaddr_in *)test_server.serv_addr)->sin_addr.s_addr = 
			htonl(INADDR_ANY);

	/* open the server. this basically just opens the socket */
	if(tcpc_open_server(&test_server) < 0)
		return 1;

	/* set some socket options. see SETSOCKOPT(2) */
	setsockopt(tcpc_server_socket(&test_server), SOL_SOCKET, SO_REUSEADDR, 
			&(one), sizeof(one));

	/* start the server. after this point the server is live and the
	 * listening thread has been started
	 */
	if(tcpc_start_server(&test_server) < 0)
		return 1;

	/* just sit and wait for someone to shut us down */
	pthread_mutex_lock(&end_process_mutex);
	pthread_cond_wait(&end_process, &end_process_mutex);
	pthread_mutex_unlock(&end_process_mutex);

	printf("Stopping server\n");

	/* closes all server connections and closes the socket */
	tcpc_close_server(&test_server);

	return 0;
}
