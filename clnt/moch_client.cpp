#include "moch_client.h"

MOCH_Client::MOCH_Client(char *addr, short port, char *nick_name)
{
	_err_num = 0;

	_socket_fd = socket(AF_INET, SOCK_STREAM , 0);
	if(_socket_fd == -1){
		_err_num = 1;
		return;
	}

	_server.sin_addr.s_addr = inet_addr(addr);
    	_server.sin_family = AF_INET;
    	_server.sin_port = htons(port);

	if(connect(_socket_fd ,(struct sockaddr *)&_server, sizeof(_server)) < 0){
		_err_num = 2;
		close(_socket_fd);
		return;
    	}

	_running = true;
	_connection_thread = std::thread(&MOCH_Client::_connection_handler, this);
	/* nem sikerült elindítani */
	if(!_connection_thread.joinable()){
		_err_num = 3;
		return;
	}

	Message m;
	m.type = MOCH_TYPE_ITSME;
	strcpy(m.to, nick_name);

	if(!send_message(m)){
		_err_num = 4;
		return;
	}

	pthread_mutex_init(&_message_mutex, NULL);
}

MOCH_Client::~MOCH_Client(void)
{
	_running = false;
	_connection_thread.detach();

	if(_socket_fd != -1){
		close(_socket_fd);
	}

	pthread_mutex_destroy(&_message_mutex);
}

void MOCH_Client::_connection_handler(void)
{
	Message msg;

	for(;_running.load();)
	{
		memset(&msg, 0, sizeof(Message));
		if(recv(_socket_fd, &msg, sizeof(Message), 0) < 0){
			// failed
       		} else {
			pthread_mutex_lock(&_message_mutex);
			_message_que.push_back(msg);
			pthread_mutex_unlock(&_message_mutex);
		}
	}
}

bool MOCH_Client::send_message(Message msg)
{
	if(send(_socket_fd, &msg, sizeof(Message), 0) < 0)
		return false;
	else
		return true;
}

void MOCH_Client::get_messages(std::vector<Message> &messages)
{
	messages.clear();

	pthread_mutex_lock(&_message_mutex);

	while(!_message_que.empty()){
		Message m = _message_que.back();
		_message_que.pop_back();
		messages.push_back(m);
	}

	pthread_mutex_unlock(&_message_mutex);
}
