/* log.h
 *
 * General logging class
 *
 * 2017, gmb */

#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <inttypes.h>
#include <string>

/* log levels */
#define LOG_NONE    	0
#define LOG_ERROR   	1
#define LOG_WARNING 	2
#define LOG_INFO    	4

/* errors */
#define LOG_SUCCESS		0
#define LOG_EOPEN		1
#define LOG_EWRITE		2

#define MSG_SIZE    	1024	// maximal size of a log message

class Log {

private:
        FILE *_fp;
        bool _is_open;
        unsigned int _log_level;
        bool _show_terminal;
        int _err_num;

public:
        Log(std::string fname, unsigned int level, bool terminal = false);
        ~Log(void);

        void write(char level, char *msg);
        void write(char level, std::string msg);

        void write_con(char level, std::string msg);

        void writef(char level, char *fmt, ...);

        bool is_open(void) const;
        int get_err_num(void) const;
};

#endif

