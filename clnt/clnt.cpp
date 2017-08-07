#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tui.h"
#include "moch_client.h"

bool running = true;

int main(int argc, char **argv){
	int a, return_number = 0;

	char nick_name[MOCH_NICK_LENGTH + 1];
	char addr[16];
	short port = 4554;

	strcpy(nick_name, "unknown");
	strcpy(addr, "127.0.0.1");

	for(a = 0; a < argc; a++){
		if(argv[a][0] == '-'){
			switch(argv[a][1]){
				case 'a':
					strcpy(addr, argv[++a]);
					break;
				case 'p':
					port = atoi(argv[++a]);
					break;
				case 'n':
					strcpy(nick_name, argv[++a]);
					break;
			}
		}
	}

	MOCH_Client clnt(addr, port, nick_name);

	if(clnt.get_err_num() != 0){
		fprintf(stderr, "unale to connect to the server (%s:%d)(%d)\n", addr, port, clnt.get_err_num());
		return_number = 1;
		exit(return_number);
	}

	MOCH_Client_TUI tui(&clnt);
	if(tui.get_err_num() != 0){
		fprintf(stderr, "unale to init TUI (%d)\n", tui.get_err_num());
		return_number = 2;
		exit(return_number);
	}

	tui.start();

	if(tui.get_err_num() != 0){
		fprintf(stderr, "unale to start TUI (%d)\n", tui.get_err_num());
		return_number = 3;
		exit(return_number);
	}

	if(return_number == 0){
		for(;tui.running();){
			MSLEEP(1000);
		}
	}

	tui.stop();

	exit(return_number);
}
