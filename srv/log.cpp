#include "log.h"

/* Constructor
 *  arg1: path of the logfile
 *  arg2: level of the logging
 *  arg3: is stdout active?
 */
Log::Log(std::string fname, unsigned int level, bool terminal)
{
    _fp = NULL;      // init file pointer to 0, for debugging

    _err_num = LOG_SUCCESS;

    /* open file, write + append */
    if((_fp = fopen(fname.c_str(), "a")) == NULL){
    	/* failed */
    	_err_num = LOG_EOPEN;
        _is_open = false;
    } else
        _is_open = true;

    /* set up member variables */
    _log_level = level;
    _show_terminal = terminal;
}

/* Destructor
 */
Log::~Log(void)
{
    /* hif file is open */
    if(_is_open || _fp != NULL){
        fclose(_fp); // close it
    }
}

/* Write a log entry
 *  arg1: level
 *  arg2: text (char*)
 */
void Log::write(char level, char *msg)
{
    /* if level is avaliable */
    if(level & _log_level && _is_open){
    	 _err_num = LOG_SUCCESS;
        time_t t = time(NULL);          // get time
        struct tm tm = *localtime(&t);  // get time for humans

        char lvl[10];   // level as a string

        /* set the level */
        switch(level){
        	case LOG_ERROR :
        		strcpy(lvl, "ERROR");
        		break;
        	case LOG_WARNING :
        		strcpy(lvl, "WARNING");
        		break;
        	case LOG_INFO :
        		strcpy(lvl, "INFO");
        		break;
        	default:
        		strcpy(lvl, "DAFUQ");
        }

        char log_msg[MSG_SIZE]; 	// string for the text

        /* create entry */
        sprintf(log_msg, "%04d-%02d-%02dT%02d:%02d:%02d [%s] %s\n", tm.tm_year + 1900, tm.tm_mon + 1,
                 tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                 lvl, msg);

        /* write the entry to the file */
        if(fprintf(_fp, "%s", log_msg) != (int32_t)strlen(msg)){
        	_err_num = LOG_EWRITE;
        }

        fflush(_fp); 	// immediately write buffer to the file (no waiting for the application to close)

        /* if stdout is active */
        if(_show_terminal){
            printf("%s", log_msg);  // write the entry to te console
        }
    }
}

/* Write a log entry
 *  arg1: level
 *  arg2: text (std::string)
 */
void Log::write(char level, std::string msg)
{
	write(level, msg.c_str());	// már megírt fv meghívása
}

/* Write a log entry to the console
 *  arg1: level
 *  arg2: text (std::string)
 */
void Log::write_con(char level, std::string msg)
{
	 if((level & _log_level) && _show_terminal){
		time_t t = time(NULL);
		struct tm tm = *localtime(&t);

		char lvl[10];

		switch(level){
			case LOG_ERROR :
				strcpy(lvl, "ERROR");
				break;
			case LOG_WARNING :
				strcpy(lvl, "WARNING");
				break;
			case LOG_INFO :
				strcpy(lvl, "INFO");
				break;
			case LOG_DISPLAY :
				strcpy(lvl, "DISPLAY");
				break;
			default:
				strcpy(lvl, "DAFUQ");
		}

		char log_msg[MSG_SIZE];

		sprintf(log_msg, "%04d-%02d-%02dT%02d:%02d:%02d [%s] %s\n", tm.tm_year + 1900, tm.tm_mon + 1,
				tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
				lvl, msg.c_str());

		printf("%s", log_msg);
	 }
}

/* Write a log entry in a formatted way
 *  arg1: level
 *  arg2: foramt text (char*)
 *  argn: values
 */
void Log::writef(char level, char *fmt, ...)
{
    char log_msg[MSG_SIZE];         // string for the text

    va_list arglist;                // set up argumentum list

    va_start(arglist, fmt);         	// get the values from the arg list
    vsprintf(log_msg, fmt, arglist);    // substutite the values
    va_end(arglist);                	// destroy the list

    write(level, log_msg);          // write the text to the file
}

/* Is file open?
 */
bool Log::is_open(void) const
{
    return _is_open;     // yes/no
}

/* Get the error number
 */
int Log::get_err_num(void) const
{
	return _err_num;
}
