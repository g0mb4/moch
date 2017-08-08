#ifndef __MOCH_CLIENT_H__
#define __MOCH_CLIENT_H__

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <thread>
#include <atomic>
#include <vector>

#include "moch.h"

class MOCH_Client {

public:
	MOCH_Client(char *addr, short port, char *nick_name);
	~MOCH_Client(void);

	int get_err_num(void) const { return _err_num; };

	void _connection_handler(void);

	bool send_message(Message msg);

	void get_messages(std::vector<Message> &messages);

	std::string get_nick_name(void) { return _nick_name; };
	bool is_running(void) { return _running.load(); };

private:
	int _err_num;

	int _socket_fd;
	struct sockaddr_in _server;

	std::atomic<bool> _running;

	std::thread _connection_thread;

	std::vector<Message> _message_que;
	pthread_mutex_t _message_mutex;

	std::string _nick_name;
};

#endif
