#include "log.h"

/* Konstruktor
 *  arg1: naplófájl elérési útja és neve
 *  arg2: naplózási szint
 *  arg3: naplózás látszódjon az alapértlemezett kimeneten?
 */
Log::Log(std::string fname, unsigned int level, bool terminal)
{
    _fp = NULL;      // fájlmutató beállítása NULL értékre ,hibakeresés miatt

    _err_num = LOG_SUCCESS;

    /* naplófájl megnyitása, csak írásra */
    if((_fp = fopen(fname.c_str(), "a")) == NULL){
    	_err_num = LOG_EOPEN;
        _is_open = false;    // nem sikerült, ezt eltároljuk a változóban
    } else
        _is_open = true;     // sikerült, így ezt tároljuk el

    /* tagváltozók beállítása */
    _log_level = level;
    _show_terminal = terminal;
}

/* Üres kosntruktor, háthakell
 */
Log::Log(void){
	 _fp = NULL;
	 _err_num = LOG_SUCCESS;
	 _log_level = LOG_NONE;
	 _show_terminal = false;
}

/* Destruktor
 */
Log::~Log(void)
{
    /* ha a naplófájl meg van nyitva */
    if(_is_open || _fp != NULL){
        fclose(_fp); // bezárjuk
    }
}

/* Egy naplóbejegyzés beírása
 *  arg1: naplóbejegyzés szintje
 *  arg2: naplóbejegyzés szövege
 */
void Log::write(char level, char *msg)
{
    /* ha a beállított szint megegyezik az üzenet szintjével és a fájl nyitva van, akkor naplózunk */
    if(level & _log_level && _is_open){
    	 _err_num = LOG_SUCCESS;
        time_t t = time(NULL);          // aktuális idõ lekérdezése
        struct tm tm = *localtime(&t);  // idõ struktúrába helyezése

        char lvl[10];   // üzenet szintjét jelzõ karakterlánc léterhozása

        /* üzenet szintjét jelzõ karakterlánc értékének beállítása */
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

        char log_msg[MSG_SIZE]; // naplóbejegyzés lefoglalása

        /* naplóbejegyzés feltöltése adatokkal */
        sprintf(log_msg, "%04d-%02d-%02dT%02d:%02d:%02d [%s] %s\n", tm.tm_year + 1900, tm.tm_mon + 1,
                 tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
                 lvl, msg);

        /* naplóbejegyzés fájlba írása*/
        if(fprintf(_fp, "%s", log_msg) != (int32_t)strlen(msg)){
        	_err_num = LOG_EWRITE;
        }

        fflush(_fp); // tartalom azonnali fájlba írása (így nem csak a program végén kerülnek be az adatok)

        /* ha be van állítva hogy a kimeneten is megjelenjen a bejegyzés */
        if(_show_terminal){
            printf("%s", log_msg);  // akkor megjelenik
        }
    }
}

/* Egy naplóbejegyzés beírása
 *  arg1: naplóbejegyzés szintje
 *  arg2: naplóbejegyzés szövege (std::string)
 */
void Log::write(char level, std::string msg)
{
	write(level, msg.c_str());	// már megírt fv meghívása
}

/* Egy naplóbejegyzés kiírása a konzolra (fájlba NEM kerül)
 *  arg1: naplóbejegyzés szintje
 *  arg2: naplóbejegyzés szövege (std::string)
 */
void Log::write_con(char level, std::string msg)
{
	 if((level & _log_level) && _show_terminal){
		time_t t = time(NULL);          // aktuális idõ lekérdezése
		struct tm tm = *localtime(&t);  // idõ struktúrába helyezése

		char lvl[10];   // üzenet szintjét jelzõ karakterlánc léterhozása

		/* üzenet szintjét jelzõ karakterlánc értékének beállítása */
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

		char log_msg[MSG_SIZE]; // naplóbejegyzés lefoglalása

		/* naplóbejegyzés feltöltése adatokkal */
		sprintf(log_msg, "%04d-%02d-%02dT%02d:%02d:%02d [%s] %s\n", tm.tm_year + 1900, tm.tm_mon + 1,
				tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
				lvl, msg.c_str());

		printf("%s", log_msg);
	 }
}

/* Egy naplóbejegyzés formázott beírása
 *  arg1: naplóbejegyzés szintje
 *  arg2: naplóbejegyzés szövege
 *  argn: formátum paraméterek
 */
void Log::writef(char level, char *fmt, ...)
{
    char log_msg[MSG_SIZE];         // naplóbejegyzés szövege

    va_list arglist;                // argumentumlista létrehozása

    va_start(arglist, fmt);         // argumentumlista értékeinek beolvasása a szöveg alapján
    vsprintf(log_msg, fmt, arglist);    // argumentumlista behelyettesítése és a bufferbe helyezése
    va_end(arglist);                // lista lezárása

    write(level, log_msg);          // a már szépen mûködõ fv. meghívása, így már tudja kezelni
}

/* Naplófájl meg van e nyitva?
 */
bool Log::is_open(void) const
{
    return _is_open;     // ebbõl kiderül
}

/* Mi volt a hiba?
 */
int Log::get_err_num(void) const
{
	return _err_num;	// ez
}
