#include "moch_server.h"

MOCH_Server::MOCH_Server(short port, Log *log)
{
	_err_num = 0;
	_port = port;
	_log = log;

	_running = false;

	_socket_fd = socket(AF_INET, SOCK_STREAM , 0);
	if (_socket_fd == -1){
		_err_num = 1;
		return;
	}

	/* ha újraindul a szerver, így nem lesz hibás a bind(),
	* sok idő mire a kernel e nélkül felszabadítja az erőforrásokat
	*/
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

	_server.sin_family = AF_INET;
	_server.sin_addr.s_addr = INADDR_ANY;
	_server.sin_port = htons(_port);

	if(bind(_socket_fd, (struct sockaddr *)&_server , sizeof(_server)) < 0){
		close(_socket_fd);
		_err_num = 4;
		return;
	}

	/* maximum kliensek: 30 */
	if(listen(_socket_fd , 30) < 0){
		close(_socket_fd);
		_err_num = 5;
		return;
	}

	pthread_mutex_init(&_message_mutex, NULL);
	pthread_mutex_init(&_client_mutex, NULL);
}

MOCH_Server::~MOCH_Server(void)
{
	_listen_thread.detach();
	_sender_thread.detach();

	stop();

	if(_socket_fd != -1){
		shutdown(_socket_fd, SHUT_RDWR);
		close(_socket_fd);
	}

	pthread_mutex_destroy(&_message_mutex);
	pthread_mutex_destroy(&_client_mutex);
}

void MOCH_Server::start(void)
{
	_running = true;

	if(_err_num == 0){
		_listen_thread = std::thread(&MOCH_Server::_listen_handler, this);
		/* nem sikerült elindítani */
		if(!_listen_thread.joinable()){
			_err_num = 6;
		}

		_sender_thread = std::thread(&MOCH_Server::_send_handler, this);

		if(!_sender_thread.joinable()){
			_err_num = 7;
		}
	} else {
		_err_num = 5;
	}
}

void MOCH_Server::stop(void)
{
	_running = false;
}

void MOCH_Server::_listen_handler(void)
{
	for(;_running.load();){
		_client_len = sizeof(struct sockaddr_in);

		while((_client_sock = accept(_socket_fd, (struct sockaddr *)&_client, (socklen_t*)&_client_len)) && _running.load()){
			std::thread t(&MOCH_Server::_connection_handler, this, _client_sock, _client);	// kliens kezelő szál elindítása

			t.detach();	// szál felszabadítása, így nem áll meg a ciklus és nem kell join() se
		}

		if (_client_sock < 0){
			_log->write(LOG_WARNING, "accept() failed");
		}
	}
}

bool MOCH_Server::_add_client(Client_Data cd)
{
	if(strcmp(cd.nick_name, MOCH_SRV_NICK) == 0)
		return false;

	bool found = false;

	pthread_mutex_lock(&_client_mutex);
	for(uint32_t i = 0; i < _client_data.size(); i++){
		if(strcmp(_client_data[i].nick_name, cd.nick_name) == 0){
			found = true;
			break;
		}
	}

	if(!found){
		_client_data.push_back(cd);
	}
	pthread_mutex_unlock(&_client_mutex);

	return !found;
}

void MOCH_Server::_delete_client(pthread_t id)
{
	pthread_mutex_lock(&_client_mutex);
	for(uint32_t i = 0; i < _client_data.size(); i++){
		if(_client_data[i].id == id){
			_client_data.erase(_client_data.begin() + i);
			break;
		}
	}
	pthread_mutex_unlock(&_client_mutex);
}

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
 * */
void MOCH_Server::_connection_handler(int client_sock, struct sockaddr_in client)
{
	int sock = client_sock;
	int read_size;
	pthread_t own_id = pthread_self();

	/* kliens hozzáadása a listához */
	Client_Data cd;
	cd.id = own_id;
	cd.socket = _client_sock;

	Message msg;

	/* beazonosításhoz szükséges szál-azonosító és IPv4 cím */
	_log->writef(LOG_INFO, "%08x (%s) connected", own_id, inet_ntoa(client.sin_addr));

	/* bufferek kiürítése (jobb biztosra menni) */
	memset(&msg, 0, sizeof(Message));
	bool end_loop = false;

	/* beérkező válaszok értelmezése */
	while(((read_size = recv(sock , &msg , sizeof(Message) , 0)) > 0) && _running.load()){

		switch(msg.type)
		{
			case MOCH_TYPE_ITSME:
				strcpy(cd.nick_name, msg.to);	// arg

				if(_add_client(cd)){
					_log->writef(LOG_INFO, "%08x is '%s'", own_id, msg.to);
					sprintf(msg.message, "Minimalist, Old school CHat by gmb\nMOCH_srv v%d.%d\n\nHello %s!\ntype ':help' for server commands\ntype '/help' for client commands", MOCH_VER_MAJOR, MOCH_VER_MINOR, cd.nick_name);
					strcpy(msg.to, cd.nick_name);
					msg.time = time(NULL);
					msg.from = MOCH_SRV_ID;

					_add_message(msg);
				} else {
					_log->writef(LOG_WARNING, "client '%s' tries to reconnect as %08x", msg.to, own_id);
					msg.type = MOCH_TYPE_ERROR;
					msg.time = time(NULL);
					strcpy(msg.message, "Client is already connected!");
					write(sock, &msg , sizeof(Message));
				       	/* kliens lekapcsolása */
				       	read_size = 0;
				       	end_loop = true;
				}
				break;

			case MOCH_TYPE_MSG:
				pthread_t id;
				if(_get_id_from_nick(msg.to, &id)){
					// to the partner
					msg.from = own_id;
					_add_message(msg);

					// back to client to verify
					strcpy(msg.to, cd.nick_name);
					_add_message(msg);
				} else {
					msg.type = MOCH_TYPE_ERROR;
					msg.time = time(NULL);
					strcpy(msg.message, "Client is not logged in!");
					write(sock, &msg , sizeof(Message));
				}

				break;

			case MOCH_TYPE_CMD:

				if(_is_client_online(own_id)){
					msg.type = MOCH_TYPE_MSG;
					msg.time = time(NULL);
					msg.from = MOCH_SRV_ID;

					if(strcmp(msg.message, "who") == 0){
						std::stringstream ss;

						pthread_mutex_lock(&_client_mutex);

						ss << "online(" << _client_data.size() << "):\n";

						for(uint32_t i = 0; i < _client_data.size(); i++){
							ss << std::string(_client_data[i].nick_name) << ", ";
						}
						pthread_mutex_unlock(&_client_mutex);

						strcpy(msg.to, cd.nick_name);
						strcpy(msg.message, ss.str().c_str());

						_add_message(msg);
					} else if(strcmp(msg.message, "online") == 0){
						pthread_t id;
						if(_get_id_from_nick(msg.to, &id)){
							sprintf(msg.message, "'%s' is online", msg.to);
						} else {
							sprintf(msg.message, "'%s' is offline", msg.to);
						}
						strcpy(msg.to, cd.nick_name);
						_add_message(msg);
					} else if(strcmp(msg.message, "help") == 0){
						sprintf(msg.message, ":who - lists online users\n:online nick_name - checks if nick_name is online");
						strcpy(msg.to, cd.nick_name);
						_add_message(msg);
					} else {
						msg.type = MOCH_TYPE_ERROR;
						msg.time = time(NULL);
						strcpy(msg.message, "Unknown command!");
						write(sock, &msg , sizeof(Message));
					}
				} else {
					msg.type = MOCH_TYPE_ERROR;
					msg.time = time(NULL);
					strcpy(msg.message, "Client is not logged in!");
					write(sock, &msg , sizeof(Message));
					/* kliens lekapcsolása */
					read_size = 0;
					end_loop = true;
				}

				break;

			default:
				_log->writef(LOG_WARNING, "unknown command '%d' from %08x", msg.type, own_id);
				msg.type = MOCH_TYPE_ERROR;
				msg.time = time(NULL);
				strcpy(msg.message, "Unknown message type!");
				write(sock, &msg , sizeof(Message));
				/* kliens lekapcsolása */
				read_size = 0;
				end_loop = true;
		}

		 /* bufferek kiürítése */
		memset(&msg, 0, sizeof(Message));

		if(end_loop)
			break;
	}

	/* kliens lekapcsolódott */
	if(read_size == 0){
		_log->writef(LOG_INFO, "%08x disconnected", own_id);
		close(sock);	// socket lezárása
	/* olvasási hiba */
	} else if(read_size == -1){
		_log->writef(LOG_WARNING, "recv() failed at %08x", own_id);
		close(sock);	// socket lezárása
	}

	_delete_client(own_id);	// kliens törlése a listából
}

bool MOCH_Server::_add_message(Message m)
{
	Message_Que_Data mqd;
	int sock;
	char nick[MOCH_NICK_LENGTH + 1];

	if(!_get_sock_from_nick(m.to, &sock))
		return false;

	if(!_get_nick_from_id(m.from, nick))
		return false;

	mqd.type = m.type;
	mqd.to = sock;
	strcpy(mqd.from, nick);
	mqd.time = m.time;
	strcpy(mqd.message, m.message);

	pthread_mutex_lock(&_message_mutex);	// lezárjuk a mutexet, hogy egyszerre csak 1 kliens tudjon bele írni

	_message_que.push_back(mqd);	// adatokat hozzáadjuk a vektorhoz

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

void MOCH_Server::_send_handler(void)
{
	for(;_running.load();){
		pthread_mutex_lock(&_message_mutex);

		while(!_message_que.empty()){
			 Message_Que_Data mqd = _message_que.back();
			 _message_que.pop_back();

			 Message msg;
			 msg.type = mqd.type;
			 strcpy(msg.to, mqd.from);
			 strcpy(msg.message, mqd.message);
			 msg.time = mqd.time;

			 write(mqd.to, &msg , sizeof(Message));
		}

		pthread_mutex_unlock(&_message_mutex);

		MSLEEP(100);
	}
}
