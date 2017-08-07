#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "log.h"
#include "moch_server.h"

bool running = true;

void sig_handler(int signo)
{
	// Ctrl + C
	if (signo == SIGINT) {
		running = false;
	}
}

int main(int argc, char **argv)
{
	int a, return_number = 0;
	bool silent;
	short port = 4554;

	signal(SIGINT, sig_handler);

	for(a = 0; a < argc; a++){
		if(argv[a][0] == '-'){
			switch(argv[a][1]){
				case 's' :
					silent = true;
					break;

				case 'p' :
					port = atoi(argv[++a]);
					break;
			}
		}
	}

	if(getuid() != 0){
		fprintf(stderr, "root privilege is needed\n");
		return_number = 1;
	}

	Log log("moch.log", LOG_ERROR | LOG_WARNING | LOG_INFO, !silent);

	if(!log.is_open() || return_number != 0){
		fprintf(stderr, "unable to create log (%d)\n", log.get_err_num());
		return_number = 2;
	} else {
		log.write(LOG_INFO, "log initialized");
	}

	MOCH_Server server(port, &log);

	if(server.get_err_num() != 0 || return_number != 0){
		log.writef(LOG_ERROR, "unable to initialize the TCP server (%d)", server.get_err_num());
		return_number = 3;
	} else {
		log.write(LOG_INFO, "TCP server initialized");
	}

	server.start();

	if(server.get_err_num() != 0 || return_number != 0){
		log.writef(LOG_ERROR, "unable to start the TCP server (%d)", server.get_err_num());
		return_number = 4;
	} else {
		log.write(LOG_INFO, "TCP server started");
	}

	if(return_number == 0){
		for(;running;){
			MSLEEP(1000);
		}
	}

	server.stop();

	log.write(LOG_INFO, "program stopped");

	exit(return_number);
}
