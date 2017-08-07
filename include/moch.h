#ifndef __MOCH_H__
#define __MOCH_H__

#define MOCH_MSG_SIZE		512
#define MOCH_NICK_LENGTH	8

#define MOCH_TYPE_ITSME		1
#define MOCH_TYPE_MSG		2
#define MOCH_TYPE_CMD		3
#define MOCH_TYPE_ERROR		99

#define MOCH_SRV_NICK		"srv"

typedef struct Client_Data {
	pthread_t id;				// thread id
	int socket;				// soket
	char nick_name[MOCH_NICK_LENGTH + 1];	// nick name
}Client_Data;

typedef struct Message_Que_Data {
	uint8_t type;
	int to;					// socket of the reciever
	char from[MOCH_NICK_LENGTH + 1];	// nick name of the sender
	time_t time;				// time of the message
	char message[MOCH_MSG_SIZE];		// message
}Message_Que_Data;

typedef struct Message {
	uint8_t type;			// type
	char to[MOCH_NICK_LENGTH + 1];	// to (other arg if needed)
	pthread_t from;
	time_t time;			// time of the message
	char message[MOCH_MSG_SIZE];	// message
}Message;

#endif
