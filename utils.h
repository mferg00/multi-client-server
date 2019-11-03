#ifndef UTILS_H
#define UTILS_H

// client request
typedef enum
{
    NONE,
    SUB,
    CHANNELS,
    UNSUB,
    NEXT,
    NEXT_ALL,
    LIVEFEED,
    LIVEFEED_ALL,
    SEND,
    INTERRUPT,
    EXIT
} client_request;

//server request
typedef enum
{
    TALK,
    QUIET,
    CLOSE
} server_request;


// message data
#define MAX_MSG_SIZE 1024
#define MAX_MSGS 1000
#define MAX_CHANNELS 255

// server
#define DEFAULT_PORT 12345
#define MAX_CLIENTS 10

// server argc conversion chars
#define DELIM_CHARS " \t\r\n\a"

// server request strings
#define _TALK "/t "
#define _QUIET "/q "

// colour strings
#define _RED  "\x1B[31m"
#define _GRN  "\x1B[32m"
#define _YEL  "\x1B[33m"
#define _BLU  "\x1B[34m"
#define _MAG  "\x1B[35m"
#define _CYN  "\x1B[36m"
#define _WHT  "\x1B[37m"
#define _CLR  "\033[0m"

#endif
