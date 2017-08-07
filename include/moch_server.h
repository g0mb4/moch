#ifndef __MOCH_SERVER_H__
#define __MOCH_SERVER_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#include <thread>
#include <atomic>
#include <map>
#include <vector>
#include <sstream>

#include "moch.h"
#include "log.h"

#define MSLEEP( x )		usleep(x * 1000)

#define MOCH_VER_MAJOR	1
#define MOCH_VER_MINOR	0

#define MOCH_SRV_ID	1

class MOCH_Server {

public:
	MOCH_Server(short port, Log *log);
	~MOCH_Server(void);

	void start(void);
	void stop(void);

	void _listen_handler(void);
	void _connection_handler(int client_sock, struct sockaddr_in client);
	void _send_handler(void);

	int get_err_num(void) const { return _err_num; };

private:
	short _port;
	int _err_num;

	std::thread _listen_thread, _sender_thread;
	std::atomic<bool> _running;

	int _socket_fd, _client_sock, _client_len;
	struct sockaddr_in _server, _client;

	std::vector<Message_Que_Data> _message_que;
	pthread_mutex_t _message_mutex;

	std::vector<Client_Data> _client_data;
	pthread_mutex_t _client_mutex;

	Log *_log;

	bool _add_client(Client_Data cd);
	void _delete_client(pthread_t id);

	bool _is_client_online(pthread_t id);

	bool _get_id_from_nick(char *nick, pthread_t *id);
	bool _get_sock_from_nick(char *nick, int *sock);
	bool _get_nick_from_id(pthread_t id, char *nick);

	bool _add_message(Message m);
};

#endif
