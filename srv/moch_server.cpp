#include "moch_server.h"

/* Constructor
 *  arg1 - port of the server
 *  arg2 - log class
 */
MOCH_Server::MOCH_Server(short port, Log *log)
{
	_err_num = 0;
	_port = port;
	_log = log;

	_running = false;

	/* create the socket file descriptor */
	_socket_fd = socket(AF_INET, SOCK_STREAM , 0);
	if (_socket_fd == -1){
		_err_num = 1;
		return;
	}

	/* if the server restarts, the kernel takes too much time to free the resources (port),
	 * this way it is solved, so bind() won't fail */
	int enabled = 1;
	if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(int)) < 0){
		close(_socket_fd);
		_err_num = 2;
		return;
	}

	if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEPORT, &enabled, sizeof(int)) < 0){
		close(_socket_fd);
		_err_num = 3;
		return;
	}

	/* server addresses */
	_server.sin_family = AF_INET;			// IPv4
	_server.sin_addr.s_addr = INADDR_ANY;	// all interface can connect
	_server.sin_port = htons(_port);		// host byte order -> network byte order

	/* binds a name to a socket */
	if(bind(_socket_fd, (struct sockaddr *)&_server , sizeof(_server)) < 0){
		/* failed */
		close(_socket_fd);	// close to socket to prevent leakage
		_err_num = 4;
		return;
	}

	/* listen for connections on a socket, maximum clients: 30 */
	if(listen(_socket_fd , 30) < 0){
		/* failed */
		close(_socket_fd);	// close to socket to prevent leakage
		_err_num = 5;
		return;
	}

	/* initialization of the mutexes */
	pthread_mutex_init(&_message_mutex, NULL);
	pthread_mutex_init(&_client_mutex, NULL);
}

/*
 * Destructor
 */
MOCH_Server::~MOCH_Server(void)
{
	/* detach threads, so no need to wait them to finish */
	_listen_thread.detach();
	_sender_thread.detach();

	stop();	// stop the server

	/* if socket is open */
	if(_socket_fd != -1){
		shutdown(_socket_fd, SHUT_RDWR);	// shot down the full-duplex connection
		close(_socket_fd);	// close it
	}

	/* destroy mutexes */
	pthread_mutex_destroy(&_message_mutex);
	pthread_mutex_destroy(&_client_mutex);
}

/*
 * Start the server
 */
void MOCH_Server::start(void)
{
	_running = true;	// turn on flag

	/* if initialization was successful */
	if(_err_num == 0){
		_listen_thread = std::thread(&MOCH_Server::_listen_handler, this); // start the listen thread
		/* if thread is not running  */
		if(!_listen_thread.joinable()){
			_err_num = 6;
		}

		_sender_thread = std::thread(&MOCH_Server::_send_handler, this); // start the sender thread

		/* if thread is not running  */
		if(!_sender_thread.joinable()){
			_err_num = 7;
		}
	} else {
		_err_num = 5;
	}
}

/*
 * Stop the server
 */
void MOCH_Server::stop(void)
{
	_running = false;	// turn of flag
}

/*
 * Listen thread
 */
void MOCH_Server::_listen_handler(void)
{
	/* if flag is on (atomic bool) */
	for(;_running.load();){
		_client_len = sizeof(struct sockaddr_in);	// size of the struct

		/* continuously accept a connection on the socket */
		while((_client_sock = accept(_socket_fd, (struct sockaddr *)&_client, (socklen_t*)&_client_len)) && _running.load()){
			std::thread t(&MOCH_Server::_connection_handler, this, _client_sock, _client);	// start client thread

			t.detach();	// detach threads, so no need to wait it to finish (no join() needed)
		}

		if (_client_sock < 0){
			_log->write(LOG_WARNING, "accept() failed");
		}
	}
}

/*
 * Add client to the list
 *  arg1 - Client_Data struct
 *  ret - adding was successful
 */
bool MOCH_Server::_add_client(Client_Data cd)
{
	// if this is the server, it is forbidden
	if(strcmp(cd.nick_name, MOCH_SRV_NICK) == 0)
		return false;

	bool found = false;
	pthread_mutex_lock(&_client_mutex);		// close mutex, so other threads can't reach the list
	/* go through the list item-by-item */
	for(uint32_t i = 0; i < _client_data.size(); i++){
		/* if client is already on the list */
		if(strcmp(_client_data[i].nick_name, cd.nick_name) == 0){
			found = true;
			break;
		}
	}

	/* if client is not on the list*/
	if(!found){
		_client_data.push_back(cd);	// add the client to the list
	}
	pthread_mutex_unlock(&_client_mutex);	// open mutex, so other threads can reach the list

	return !found;	// return if adding was successful
}

/*
 * Delete client from the list
 *  arg1 - thread id of the client
 */
void MOCH_Server::_delete_client(pthread_t id)
{
	pthread_mutex_lock(&_client_mutex);		// close mutex, so other threads can't reach the list
	for(uint32_t i = 0; i < _client_data.size(); i++){
		if(_client_data[i].id == id){
			_client_data.erase(_client_data.begin() + i);	// delete item from the list
			break;
		}
	}
	pthread_mutex_unlock(&_client_mutex);	// open mutex, so other threads can reach the list
}

/*
 * Check if client is on the list
 *  arg1 - thread id of the client
 *  ret - is client on the list?
 */

bool MOCH_Server::_is_client_online(pthread_t id)
{
	bool found = false;
	pthread_mutex_lock(&_client_mutex);
	for(uint32_t i = 0; i < _client_data.size(); i++){
		if(_client_data[i].id == id){
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&_client_mutex);

	return found;
}

/*
 * This will handle connection for each client
 *  arg1 - socket of the client
 *  arg2 - address structure
 */
void MOCH_Server::_connection_handler(int client_sock, struct sockaddr_in client)
{
	int sock = client_sock;
	int read_size;
	pthread_t own_id = pthread_self();	// id of the client

	/* create client data */
	Client_Data cd;
	cd.id = own_id;
	cd.socket = _client_sock;

	Message msg;

	/* log the id and the IPv4 addr  */
	_log->writef(LOG_INFO, "%08x (%s) connected", own_id, inet_ntoa(client.sin_addr));

	/* empty buffers */
	memset(&msg, 0, sizeof(Message));
	bool end_loop = false;

	/* get the messages from client while there is any AND the server is running  */
	while(((read_size = recv(sock , &msg , sizeof(Message) , 0)) > 0) && _running.load()){
		/* identify the message*/
		switch(msg.type)
		{
			/* login */
			case MOCH_TYPE_ITSME:
				strcpy(cd.nick_name, msg.to);	// the nickname of the client is the "to" field of the message

				/* if client is added */
				if(_add_client(cd)){
					_log->writef(LOG_INFO, "%08x is '%s'", own_id, msg.to);	// log the info
					// welcome message
					sprintf(msg.message, "Minimalist, Old school CHat by gmb\nMOCH_srv v%d.%d\n\nHello %s!\ntype ':help' for server commands\ntype '/help' for client commands", MOCH_VER_MAJOR, MOCH_VER_MINOR, cd.nick_name);
					// send beck the message to the client
					strcpy(msg.to, cd.nick_name);
					msg.time = time(NULL);	// time of the message is NOW
					msg.from = MOCH_SRV_ID;	// the sender is the server

					_add_message(msg);	// add the message to the que

				/* client is not added */
				} else {
					_log->writef(LOG_WARNING, "client '%s' tries to reconnect as %08x", msg.to, own_id); // log the info
					msg.type = MOCH_TYPE_ERROR;
					msg.time = time(NULL);
					strcpy(msg.message, "Client is already connected!");
					write(sock, &msg , sizeof(Message));	// send back the bad news immediately
				    /* disconnect the client */
				    read_size = 0;
				    end_loop = true;
				}
				break;

			/* regular message */
			case MOCH_TYPE_MSG:
				/* if client is logged in */
				if(_is_client_online(own_id))
					pthread_t id;
					/* if client is online */
					if(_get_id_from_nick(msg.to, &id)){
						msg.from = own_id;	// register the sender id
						/* if message has not been added*/
						if(!_add_message(msg)){
							msg.type = MOCH_TYPE_ERROR;
							msg.time = time(NULL);
							strcpy(msg.message, "Unable to send the message!");
							write(sock, &msg , sizeof(Message));	// send back the bad news immediately
						}
					/* client is offline */
					} else {
						msg.type = MOCH_TYPE_ERROR;
						msg.time = time(NULL);
						strcpy(msg.message, "Client is not logged in!");
						write(sock, &msg , sizeof(Message));	// send back the bad news immediately
					}
				/* if not */
				} else{
					msg.type = MOCH_TYPE_ERROR;
					msg.time = time(NULL);
					strcpy(msg.message, "Client is not logged in!");
					write(sock, &msg , sizeof(Message));
					read_size = 0;
					end_loop = true;
				}

				break;

			/* regular message */
			case MOCH_TYPE_CMD:
				/* if client is logged in */
				if(_is_client_online(own_id)){
					msg.type = MOCH_TYPE_MSG;	// response is a regular message
					msg.time = time(NULL);		// now
					msg.from = MOCH_SRV_ID;		// from the server

					/* WHO command
					 * who's online?
					 */
					if(strcmp(msg.message, "who") == 0){
						std::stringstream ss;

						/* check the list of the clients */
						pthread_mutex_lock(&_client_mutex);

						ss << "online(" << _client_data.size() << "):\n";

						for(uint32_t i = 0; i < _client_data.size(); i++){
							ss << std::string(_client_data[i].nick_name) << ", ";
						}
						pthread_mutex_unlock(&_client_mutex);

						strcpy(msg.to, cd.nick_name);			// send back the massage
						strcpy(msg.message, ss.str().c_str());	// with the info

						_add_message(msg);						//  add the message to the queue

					/* ONLINE <NICK> command
					 * is NICK online?
					 */
					} else if(strcmp(msg.message, "online") == 0 && strlen(msg.to) > 0){
						pthread_t id;
						/* if nick is on the list, it's online */
						if(_get_id_from_nick(msg.to, &id)){
							sprintf(msg.message, "'%s' is online", msg.to);
						} else {
							sprintf(msg.message, "'%s' is offline", msg.to);
						}
						strcpy(msg.to, cd.nick_name);
						_add_message(msg);

					/* HELP command
					 * sends the available commands to the client
					 */
					} else if(strcmp(msg.message, "help") == 0){
						sprintf(msg.message, ":who - lists online users\n:online nick_name - checks if nick_name is online");
						strcpy(msg.to, cd.nick_name);
						_add_message(msg);

					/* unknown command :(
					 */
					} else {
						msg.type = MOCH_TYPE_ERROR;
						msg.time = time(NULL);
						strcpy(msg.message, "Unknown command!");
						write(sock, &msg , sizeof(Message));
					}

				/* client is not logged in */
				} else {
					msg.type = MOCH_TYPE_ERROR;
					msg.time = time(NULL);
					strcpy(msg.message, "Client is not logged in!");
					write(sock, &msg , sizeof(Message));
					read_size = 0;
					end_loop = true;
				}

				break;

			/* unkonwn message type :( */
			default:
				_log->writef(LOG_WARNING, "unknown command '%d' from %08x", msg.type, own_id);
				msg.type = MOCH_TYPE_ERROR;
				msg.time = time(NULL);
				strcpy(msg.message, "Unknown message type!");
				write(sock, &msg , sizeof(Message));
		}

		 /* clear the buffer */
		memset(&msg, 0, sizeof(Message));

		/* if end loop flag is on (force disconnect) */
		if(end_loop)
			break;
	}

	/* if client is disconnected */
	if(read_size == 0){
		_log->writef(LOG_INFO, "%08x disconnected", own_id);	// log info
		close(sock);	// close socket
	/* read error */
	} else if(read_size == -1){
		_log->writef(LOG_WARNING, "recv() failed at %08x", own_id);
		close(sock);
	}

	_delete_client(own_id);	// delete client from the list
}

/*
 * Add a message to the queue for the sender
 *  ret - was message added?
 */
bool MOCH_Server::_add_message(Message m)
{
	Message_Queue_Data mqd;
	int sock;
	char nick[MOCH_NICK_LENGTH + 1];

	/* get the socket of the reciever */
	if(!_get_sock_from_nick(m.to, &sock))
		return false;

	/* get nick of the sender */
	if(!_get_nick_from_id(m.from, nick))
		return false;

	/* fill the data */
	mqd.type = m.type;
	mqd.to = sock;
	strcpy(mqd.from, nick);
	mqd.time = m.time;
	strcpy(mqd.message, m.message);

	pthread_mutex_lock(&_message_mutex);

	_message_queue.push_back(mqd); // add the message

	pthread_mutex_unlock(&_message_mutex);

	return true;
}

bool MOCH_Server::_get_id_from_nick(char *nick, pthread_t *id)
{
	bool found = false;
	pthread_mutex_lock(&_client_mutex);
	for(uint32_t i = 0; i < _client_data.size(); i++){
		if(strcmp(_client_data[i].nick_name, nick) == 0){
			*id = _client_data[i].id;
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&_client_mutex);

	return found;
}

bool MOCH_Server::_get_sock_from_nick(char *nick, int *sock)
{
	bool found = false;
	pthread_mutex_lock(&_client_mutex);
	for(uint32_t i = 0; i < _client_data.size(); i++){
		if(strcmp(_client_data[i].nick_name, nick) == 0){
			*sock = _client_data[i].socket;
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&_client_mutex);

	return found;
}

bool MOCH_Server::_get_nick_from_id(pthread_t id, char *nick)
{
	if(id == MOCH_SRV_ID){
		strcpy(nick, MOCH_SRV_NICK);
		return true;
	}

	bool found = false;
	pthread_mutex_lock(&_client_mutex);
	for(uint32_t i = 0; i < _client_data.size(); i++){
		if(_client_data[i].id == id){
			strcpy(nick, _client_data[i].nick_name);
			found = true;
			break;
		}
	}
	pthread_mutex_unlock(&_client_mutex);
	return found;
}

/*
 * Sender thread
 */
void MOCH_Server::_send_handler(void)
{
	/* if flag is on (atomic bool) */
	for(;_running.load();){
		pthread_mutex_lock(&_message_mutex);	// close mutex, so other threads can't reach the list

		/* while there is a message in the list */
		while(!_message_queue.empty()){
			 Message_Que_Data mqd = _message_queue.back();	// get the next message
			 _message_queue.pop_back();		// delete it from the list

			 /* create te message */
			 Message msg;
			 msg.type = mqd.type;
			 strcpy(msg.to, mqd.from);
			 strcpy(msg.message, mqd.message);
			 msg.time = mqd.time;

			 write(mqd.to, &msg , sizeof(Message));	// send the message
		}

		pthread_mutex_unlock(&_message_mutex);	// open mutex, so other threads can reach the list

		MSLEEP(100);	// wait 100 ms, so the list can be updated
	}
}
