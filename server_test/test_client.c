#include "tcpc.h"
#include "packits/packits.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#define SERVER_CONNECTIONS 10
struct tcpc_client conns[SERVER_CONNECTIONS];

/* signal handling */
static volatile unsigned int stop_test = 0;

void signal_handler(int sig)
{
	stop_test = 1;
}

static struct sigaction act = {
	.sa_handler = &signal_handler,
};
/*******************/

/* callbacks */
PT_THREAD(conn_h(struct tcpc_client *c, size_t len))
{
	int i;

	PT_BEGIN(c->conn_h_pt);

	for(i = 0; i < len; i++) {
		if(c->rxbuf[i] == '!') {
			if(tcpc_client_send_to(c, "Q", 1, 0) < 0)
				perror("Could not send data");
		}
	}

	PT_RESTART(c->conn_h_pt);

	PT_END(c->conn_h_pt);
}

void conn_close_h(struct tcpc_client *c)
{
	return;
}

/* Main Routine */
int main(int argc,char *argv[])
{
	int i;
	struct hostent *h;
	int port;
	int one = 1;
	unsigned int test_count = 0;

	/* stupid argument checking to grab the address and port off the 
	 * command line
	 */
	if(argc != 3)
		return 1;
	/* grab the port */
	port = atoi(argv[2]);
	/* get the server address */
	if((h = gethostbyname(argv[1])) == NULL) {
		printf("Could not get host by name: %s\n", argv[1]);
		return 1;
	}

	/* set up the signal handlers so we can shut down cleanly if given the
	 * chance
	 */
	sigaction(SIGQUIT, &act, NULL);
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);

	printf("Connecting to server on port: %d\n",port);

	/* initialize our client structures */
	for(i = 0; i < SERVER_CONNECTIONS; i++) {
		if(tcpc_init_client(&conns[i], sizeof(struct sockaddr_in),
				1024,
				&conn_h,
				&conn_close_h) < 0) {
			printf("Failed to initialize tcpc client %d\n",i);
			return 1;
		}
		/* setup the server address structure for our protocol family */
		((struct sockaddr_in *)conns[i].serv_addr)->sin_family =
				AF_INET;
		((struct sockaddr_in *)conns[i].serv_addr)->sin_port =
				htons(port);
		memcpy(&(((struct sockaddr_in *)
					conns[i].serv_addr)->sin_addr.s_addr),
				h->h_addr_list[0],
				h->h_length);
	}

	printf("Beginning Test\n");

	while(!stop_test) {
		for(i = 0; i < SERVER_CONNECTIONS; i++) {
			if(conns[i]._active == 1)
				continue;

			if(tcpc_open_client(&conns[i]) < 0) {
				printf("Failed to open tcpc client %d\n",i);
				stop_test = 1;
				break;
			}

			/* set some socket options. see SETSOCKOPT(2) */
			setsockopt(tcpc_client_socket(&conns[i]), SOL_SOCKET,
					SO_REUSEADDR, &(one), sizeof(one));

			if(tcpc_start_client(&conns[i]) < 0) {
				printf("Failed to start tcpc client %d\n",i);
				stop_test = 1;
				break;
			}
			test_count++;
		}
		//sched_yield();
		sleep(1);
		if(test_count%100 == 0) {
			printf("Test Count: %d\n", test_count);
		}
	}

	printf("\nStopping Test\n");

	/* cleanup */
	for(i = 0; i < SERVER_CONNECTIONS; i++) {
		tcpc_close_client(&conns[i]);
		free_tcpc_client_members(&conns[i]);
	}

	return 0;
}
