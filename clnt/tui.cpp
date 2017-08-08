#include "tui.h"

MOCH_Client_TUI::MOCH_Client_TUI(MOCH_Client *clnt)
{
	_err_num = 0;
	_clnt = clnt;

	_main_win = NULL;
	_type_win = NULL;
	_msg_win = NULL;
	_clients_win = NULL;

	if((_main_win = initscr()) == NULL){
		_err_num = 1;
		return;
	}

	_cury = _maxy;
	_curx = 0;

	_redraw();

	noecho();
	keypad(_main_win, TRUE);

	Conversation c;
	memset(c.nick_name, MOCH_NICK_LENGTH + 1, 0);
	strcpy(c.nick_name, MOCH_SRV_NICK);
	_convs.push_back(c);

	_active_conv = 0;
}


MOCH_Client_TUI::~MOCH_Client_TUI(void)
{
	stop();

	_tui_thread.detach();

	delwin(_clients_win);
	delwin(_msg_win);
	delwin(_type_win);
	delwin(_main_win);
}

void MOCH_Client_TUI::start(void)
{
	_running = true;

	if(_err_num == 0){
		_tui_thread = std::thread(&MOCH_Client_TUI::_keyboard_handler, this);
		/* nem sikerült elindítani */
		if(!_tui_thread.joinable()){
			_err_num = 6;
		}
	} else {
		_err_num = 5;
	}

	for(;_running.load();){
		_refresh();
		MSLEEP(100);
	}
}

void MOCH_Client_TUI::stop(void)
{
	_running = false;
	endwin();
}

void MOCH_Client_TUI::_redraw(void)
{
	getmaxyx(_main_win, _maxy, _maxx);


	if(_type_win != NULL){
		delwin(_type_win);
	}

	if(_msg_win != NULL){
		delwin(_msg_win);
	}

	if(_clients_win != NULL){
		delwin(_clients_win);
	}

	_type_win_y = _maxy - 1;
	_type_win_w = _maxx;

	_msg_win_w = _maxx - 13;
	_msg_win_h = _maxy - 2;

	_clients_win_x = _msg_win_w + 2;
 	_clients_win_w = 12;
	_clients_win_h = _msg_win_h;

	if((_type_win = newwin(1, _type_win_w, _type_win_y, 0)) == NULL){
		_err_num = 2;
		return;
	}

	if((_msg_win = newwin(_msg_win_h, _msg_win_w, 0, 0)) == NULL){
		_err_num = 3;
		return;
	}

	if((_clients_win = newwin(_clients_win_h, _clients_win_w, 0, _clients_win_x)) == NULL){
		_err_num = 4;
		return;
	}

	for(int32_t i = 0; i < _maxx; i++){
		move(_msg_win_h, i);
		addch('-');
	}

	for(uint32_t i = 0; i < _msg_win_h; i++){
		move(i, _msg_win_w);
		addch('|');
	}

	keypad(_type_win, TRUE);

	move(_cury, _curx);

	refresh();
}

void MOCH_Client_TUI::_mvprints(uint32_t *y, uint32_t x, uint32_t width, std::string msg)
{
	uint32_t _x = x;

	std::vector<std::string> words = _split(msg, ' ');
	for(uint32_t i = 0; i < words.size(); i++){
		std::string word = words[i];

		if(_x + word.size() > x + width){
			*y = *y + 1;
			_x = 0;
		}

		for(uint32_t j = 0; j < word.size(); j++){
			if(word[j] == '\n'){
				*y = *y + 1;
				_x = 0;
			} else {
				move(*y, _x++);
				addch(word[j]);
			}

			if(j == word.size() - 1){
				move(*y, _x++);
				addch(' ');
			}
		}
	}
}

void MOCH_Client_TUI::_clear(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	for(uint32_t _x = x; _x < x + w; _x++){
		for(uint32_t _y = y; _y < y + h; _y++){
			move(_y, _x);
			addch(' ');
		}
	}
}

std::vector<std::string> MOCH_Client_TUI::_split(std::string s, char c)
{
	std::vector<std::string> vec;
	std::string elem = "";

	for(unsigned int i = 0; i < s.size(); i++){
		if(s[i] == c){
			vec.push_back(elem);
			elem = "";
		} else if(i == (s.size() - 1)) {
			elem += s[i];
			vec.push_back(elem);
			elem = "";
		} else {
			elem += s[i];
		}
	}

	return vec;
}

bool MOCH_Client_TUI::_is_ch_out(int32_t ch)
{
	if(ispunct(ch) || isalnum(ch) || ch == ' ')
		return true;
	else
		return false;
}

void MOCH_Client_TUI::_keyboard_handler(void)
{
	for(;_running.load();)
	{
		int ch = getch();

		if(ch == '\n' || ch == '\r'){
			_parse_message();

			_message.clear();
		} else if(ch == KEY_BACKSPACE){
			if(!_message.empty())
				_message.pop_back();

		} else if(ch == '\t'){
			_active_conv++; // _refresh() -ben limitálva
		} else if(_is_ch_out(ch)){
			_message.push_back((char)ch);
		}

		uint32_t i;
		for(i = 0; i < _message.size(); i++){
			move(_type_win_y, i);
			addch(_message[i]);
		}
		_curx = i;
		_cury = _type_win_y;

		for(; i < (uint32_t)_maxx; i++){
			move(_type_win_y, i);
                	addch(' ');
		}

		move(_cury, _curx);
	}
}

void MOCH_Client_TUI::_refresh(void)
{
	if(_clnt->is_running() == false){
		stop();
		_err_num = 10;
		return;
	}

	std::vector<Message> messages;
	_clnt->get_messages(messages);
	while(!messages.empty()){
		Message m = messages.back();
		messages.pop_back();
		_add_conv_message(m);
	}

	uint32_t i;
	//_msg_win
	if(_active_conv >= _convs.size())
		_active_conv = 0;

	_clear(0, 0, _msg_win_w, _msg_win_h);
	_clear_conv(_msg_win_h);
	uint32_t pos = 0;
	for(i = 0; i < _convs[_active_conv].messages.size(); i++){
		std::string msg = _convs[_active_conv].messages[i];
		_mvprints(&pos, 0, _msg_win_w, msg);
		pos++;
	}

	// _clients_win
	_clear(_clients_win_x, 0, _clients_win_w, _clients_win_h);

	for(i = 0; i < _convs.size(); i++){
		if(i == _active_conv){
			attron(A_REVERSE);
			_convs[i].is_new_msg = false;
		}

		std::string nick;
		if(_convs[i].is_new_msg){
			nick = std::string(_convs[i].nick_name, strlen(_convs[i].nick_name)) + "+";
		} else {
			nick = std::string(_convs[i].nick_name, strlen(_convs[i].nick_name));
		}

		_mvprints(&i, _clients_win_x, _clients_win_w, nick);

		if(i == _active_conv){
			attroff(A_REVERSE);
		}
	}

	move(_cury, _curx);
	refresh();
}

void MOCH_Client_TUI::_clear_conv(uint32_t win_h)
{
	uint32_t pos = 0;
	for(uint32_t i = 0; i < _convs[_active_conv].messages.size(); i++){
		std::string s = _convs[_active_conv].messages[i];
		pos += std::count(s.begin(), s.end(), '\n');
		pos++;
		if(pos > win_h){
			_convs[_active_conv].messages.erase(_convs[_active_conv].messages.begin());
		}
	}
}

void MOCH_Client_TUI::_add_conv_message(Message m)
{
	bool found = false;
	std::string msg;
	struct tm tm = *localtime(&m.time);
	char msg_time[32];
	sprintf(msg_time, "[%02d:%02d:%02d]", tm.tm_hour, tm.tm_min, tm.tm_sec);

	std::stringstream ss;

	if(m.type == MOCH_TYPE_ERROR){
		ss << "ERROR: ";
	} else {
		if(strcmp(m.to, MOCH_SRV_NICK) != 0){
			ss << std::string(msg_time, strlen(msg_time));
			ss << " ";
			ss << std::string(m.to, strlen(m.to));
			ss << ": ";
		}
	}

	ss << std::string(m.message, strlen(m.message));

	if(m.type == MOCH_TYPE_ERROR){
		_convs[_active_conv].messages.push_back(ss.str());
	} else {
		for(uint32_t i = 0; i < _convs.size(); i++){
			if(strcmp(_convs[i].nick_name, m.to) == 0){
				found = true;
				_convs[i].messages.push_back(ss.str());
				_convs[i].is_new_msg = true;
				break;
			}
		}

		if(found == false){
			Conversation c;
			memset(c.nick_name, MOCH_NICK_LENGTH + 1, 0);
			strcpy(c.nick_name, m.to);
			c.is_new_msg = true;
			c.messages.push_back(ss.str());
			_convs.push_back(c);
		}
	}
}

void MOCH_Client_TUI::_parse_message(void)
{
	if(_message.size() == 0)
		return;

	Message m;
	m.time = time(NULL);
	char cmd[128];
	char arg[MOCH_NICK_LENGTH + 1];
	// server command
	if(_message[0] == ':'){
		m.type = MOCH_TYPE_CMD;
		_parse_command(_message, cmd, arg);
		strcpy(m.to, arg);
		strcpy(m.message, cmd);
	// client command
	} else if(_message[0] == '/') {
		_parse_command(_message, cmd, arg);
		_client_command(cmd, arg);
		return;
	} else {
		if(_active_conv > 0){
			m.type = MOCH_TYPE_MSG;
			strcpy(m.to, _convs[_active_conv].nick_name);
			strcpy(m.message, _message.c_str());
		}
	}

	if(_clnt->send_message(m)){
		if(m.type == MOCH_TYPE_MSG && _active_conv > 0){
			struct tm tm = *localtime(&m.time);
			char msg_time[32];
			sprintf(msg_time, "[%02d:%02d:%02d]", tm.tm_hour, tm.tm_min, tm.tm_sec);
			std::stringstream ss;
			ss << std::string(msg_time);
			ss << " ";
			ss << _clnt->get_nick_name();
			ss << ": ";
			ss << std::string(m.message);
			_convs[_active_conv].messages.push_back(ss.str());
		}
	} else {
		_convs[_active_conv].messages.push_back(std::string("ERROR: Unable to send the message!"));
	}
}

void MOCH_Client_TUI::_parse_command(std::string command, char *cmd, char *arg)
{
	uint32_t i, j;
	// skip ':'
	for(i = 1, j = 0; command[i] != ' ' && command[i] != '\0' && i < command.size(); i++){
		cmd[j++] = command[i];
	}
	cmd[j] = '\0';
	i++;

	if(i < command.size())
	{
		for(j = 0; command[i] != ' ' && command[i] != '\0' && i < command.size() && j <= MOCH_NICK_LENGTH; i++){
			arg[j++] = command[i];
		}
		arg[j] = '\0';
	}
}


void MOCH_Client_TUI::_client_command(char *cmd, char *arg)
{
	if(strcmp(cmd, "exit") == 0){
		stop();
	} else if(strcmp(cmd, "redraw") == 0){
		_redraw();
	} else if(strcmp(cmd, "chat") == 0 && strlen(arg) > 0){

		bool found = false;

		uint32_t i;
		for(i = 0; i < _convs.size(); i++){
			if(strcmp(_convs[i].nick_name, arg) == 0){
				found = true;
				_active_conv = i;
				break;
			}
		}
		if(found == false){
			Conversation c;
			strcpy(c.nick_name, arg);
			_convs.push_back(c);
			_active_conv = i;
		}
	} else if(strcmp(cmd, "clear") == 0){
		_convs[_active_conv].messages.clear();
	} else if(strcmp(cmd, "leave") == 0){
		if(_active_conv > 0){
			_convs.erase(_convs.begin() + _active_conv);
		}
	} else if(strcmp(cmd, "help") == 0){
		std::string msg = "/exit - Exits the program\n/redraw - Redraws the interface\n/chat nick_name - Starts a conversation\n/clear - Clears the active conversation\n/leave - Close the active conversation\n<TAB> - Switch conversation";
		_convs[_active_conv].messages.push_back(msg);
	} else {
		std::string msg = "ERROR: Unknown command!";
		_convs[_active_conv].messages.push_back(msg);
	}
}
