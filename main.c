#include <stdio.h>
#include <unistd.h>

#define HTTP_TCP_PORT 22225

int start_server(int port);


int main(int argc, char **argv){

	int port = HTTP_TCP_PORT;
	int opt;



	while ((opt = getopt(argc, argv, "p:d")) != -1) {
		switch (opt) {
			case 'p':
				port = atoi(optarg);
				break;
			case 'd':
				printf("daemonize... is not implement\n");
				//dopt = 1;
				break;
			default:
				printf("error! \'%c\' \'%c\'\n", opt, optopt);
				return 1;
		}
	}
	
	start_server(port);
}
