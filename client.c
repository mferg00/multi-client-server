#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "utils.h"

// global vars
int sockfd = 0;
struct sockaddr_in servaddr;
volatile sig_atomic_t flag = 0;

// function prototypes
void
catch_signal(int sig);
void
client_setup(char* host, int port);
void
get_input(char* buff);
server_request
parse_response(char* buff);
void
client_loop(void);

// main: check arg validity
int main(int argc, char **argv)
{
    struct hostent *he;

    switch(argc)
    {
        case 3:
            if ((he = gethostbyname(argv[1])) == NULL)
            {
                herror("gethostbyname");
                exit(1);
            }
            client_setup(he->h_addr, atoi(argv[2]));
            break;

        default:
            printf("usage: <host> <port>\n");
            exit(1);
    }

    while(1)
    {
        client_loop();
    }

    return 0;
}

// set flag to 1 when signal is caught
void
catch_signal(int sig)
{
    flag = 1;
}

// setup client signal handler and connection to server
void
client_setup(char* host, int port)
{
    // setup signal handler
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = catch_signal;
    sa.sa_flags = 0;      // 0 will allow the signal to take priority over getchar()

    sigaction(SIGINT, &sa, NULL);

    // create and check socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd == -1)
    {
        printf("socket creation failed...\n");
        exit(1);
    }
    else
    {
        printf("Socket successfully created..\n");
    }

    bzero(&servaddr, sizeof(servaddr));

    // assign IP and port
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr = *((struct in_addr *)host);
    servaddr.sin_port = htons(port);

    // connect client socket to server socket
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("connection error");
        exit(1);
    }
}

// get stdin input
// overwrite and send "/intr" if ctrl_c is pressed
// close socket if "BYE" is entered
void
get_input(char* buff)
{
    bzero(buff, MAX_MSG_SIZE);

    int n = 0;

    // get stdin input
    while (((buff[n++] = getchar()) != '\n') && !flag);
    buff[n] = 0;

    // if client says bye, write BYE to server and exit
    if(strncmp(buff, "BYE", 3) == 0)
    {
        if (write(sockfd, buff, sizeof(buff)) < 0)
        {
            perror("client->server write error");
            exit(1);
        }
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        exit(0);
    }

    // if flag is set, send "/intr" to server
    if(flag)
    {
        bzero(buff, MAX_MSG_SIZE);
        sprintf(buff, "/intr");
        flag = 0;
    }

}

// parse and display server's message
server_request
parse_response(char* buff)
{
    // if server said bye, shutdown
    if(strncmp(buff, "BYE", 3) == 0)
    {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        exit(0);
    }

    // is server says "/q ", the server wants us to not send anything back
    if(strncmp(buff, "/q ", 3) == 0)
    {
        printf("%s\n" _CLR, buff + 3);
        return QUIET;
    }

    // else, the usual read/write loop happens
    else if(strncmp(buff, "/t ", 3) == 0)
    {
        printf("%s\n" _CLR, buff + 3);
        return TALK;
    }

    else if(buff[0] != 0)
    {
        printf("%s\n" _CLR, buff);
    }

    return TALK;
}

// read and write cycle
void
client_loop(void)
{
    // buffer to read/write at the socket
    static char buff[MAX_MSG_SIZE];

    // client is set to QUIET initially, as the server has a welcome message to send
    static server_request client = QUIET;

    if(client == TALK)
    {
        // get stdin input
        get_input(buff);

        // write to socket
        if (write(sockfd, buff, sizeof(buff)) < 0)
        {
            perror("client->server write error");
            exit(1);
        }
    }

    bzero(buff, MAX_MSG_SIZE);

    // read from socket
    if (read(sockfd, buff, sizeof(buff)) < 0)
    {
        perror("server->client read error");
        exit(1);
    }

    // parse response and get state for next iteration
    client = parse_response(buff);

}
