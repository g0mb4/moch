/* moch.h
 *
 * Data structures for the MOCH protocol
 *
 * 2017, gmb */

#ifndef __MOCH_H__
#define __MOCH_H__

#define MOCH_MSG_SIZE		512		// maximal size of the message
#define MOCH_NICK_LENGTH	8		// maximal size of the nick name

#define MOCH_TYPE_ITSME		1		// login
#define MOCH_TYPE_MSG		2		// regular message
#define MOCH_TYPE_CMD		3		// command
#define MOCH_TYPE_ERROR		99		// error

#define MOCH_SRV_NICK	"srv"			// nick name of the server

/*
 * Infos needed for a client
 */
typedef struct Client_Data {
	pthread_t id;							// thread id of the client
	int socket;								// soket
	char nick_name[MOCH_NICK_LENGTH + 1];	// nick name (size + '\0')
}Client_Data;

/*
 * MOCH message
 */
typedef struct Message {
	uint8_t type;					// type
	char to[MOCH_NICK_LENGTH + 1];	// to (other arg if needed)
	pthread_t from;					// thread id of the sender
	time_t time;					// time of the message
	char message[MOCH_MSG_SIZE];	// message
}Message;

/*
 * Infos for the server, to send the message
 */
typedef struct Message_Queue_Data {
	uint8_t type;
	int to;								// socket of the reciever
	char from[MOCH_NICK_LENGTH + 1];	// nick name of the sender
	time_t time;						// time of the message
	char message[MOCH_MSG_SIZE];		// message
}Message_Que_Data;


#endif
