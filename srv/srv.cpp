/* srv.cpp
 *
 * SMOCH server appcliation
 *
 * 2017, gmb */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "log.h"
#include "moch_server.h"

bool running = true;	// global running flag

/*
 * Linux signal handler
 */
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
	bool silent;		// for the logging
	short port = 4554;	// default port

	signal(SIGINT, sig_handler);

	for(a = 0; a < argc; a++){
		if(argv[a][0] == '-'){
			switch(argv[a][1]){
				// -s : lisnt mode is on
				case 's' :
					silent = true;
					break;

				// -p set the port
				case 'p' :
					port = atoi(argv[++a]);
					break;
			}
		}
	}

	/* check if it's in root mode */
	if(getuid() != 0){
		fprintf(stderr, "root privilege is needed\n");
		return_number = 1;
	}

	Log log("moch.log", LOG_ERROR | LOG_WARNING | LOG_INFO, !silent);

	/* check if log was created */
	if(!log.is_open() || return_number != 0){
		fprintf(stderr, "unable to create log (%d)\n", log.get_err_num());
		return_number = 2;
	} else {
		log.write(LOG_INFO, "log initialized");
	}

	MOCH_Server server(port, &log);

	/* check if server was created */
	if(server.get_err_num() != 0 || return_number != 0){
		log.writef(LOG_ERROR, "unable to initialize the TCP server (%d)", server.get_err_num());
		return_number = 3;
	} else {
		log.write(LOG_INFO, "TCP server initialized");
	}

	server.start();

	/* check if server was started */
	if(server.get_err_num() != 0 || return_number != 0){
		log.writef(LOG_ERROR, "unable to start the TCP server (%d)", server.get_err_num());
		return_number = 4;
	} else {
		log.write(LOG_INFO, "TCP server started");
	}

	/* if there was no error */
	if(return_number == 0){
		/* while running flag is on */
		for(;running;){
			MSLEEP(1000);	// minimal CPU usage
		}
	}

	// stop the server
	server.stop();

	log.write(LOG_INFO, "program stopped");

	exit(return_number);
}
