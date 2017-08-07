/* Text User Interface
*/

#ifndef __TUI_H__
#define __TUI_H__

#include <ncurses.h>
#include <stdint.h>
#include <unistd.h>

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

#include "moch.h"
#include "moch_client.h"

#define MSLEEP( x )	usleep(x * 1000)

typedef struct Conversation {
	char nick_name[MOCH_NICK_LENGTH + 1];
	std::vector<std::string> messages;
} Conversation;

class MOCH_Client_TUI {

public:
	MOCH_Client_TUI(MOCH_Client *clnt);
	~MOCH_Client_TUI(void);

	int get_err_num(void) const { return _err_num; };

	void _keyboard_handler(void);

	void start(void);
	void stop(void);

	bool running(void) { return _running.load(); };

private:

	int _err_num;

	int _maxx;
	int _maxy;

	void _redraw(void);
	void _refresh(void);
	void _clear(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

	bool _is_ch_out(int32_t ch);

	void _mvprints(uint32_t *y, uint32_t x, uint32_t width, std::string msg);

	std::vector<std::string> _split(std::string s, char c);

	uint32_t _curx, _cury;
	uint32_t _type_win_y, _type_win_w;
	uint32_t _msg_win_w, _msg_win_h;
	uint32_t _clients_win_x, _clients_win_w, _clients_win_h;

	WINDOW *_main_win;
	WINDOW *_type_win;
	WINDOW *_msg_win;
	WINDOW *_clients_win;

	std::thread _tui_thread;
	std::atomic<bool> _running;

	std::string _message;

	void _add_conv_message(Message m);
	void _parse_message(void);
	void _parse_command(std::string command, char *cmd, char *arg);
	void _client_command(char *cmd, char *arg);

	std::vector<Conversation> _convs;
	uint32_t _active_conv;

	MOCH_Client *_clnt;
};

#endif
