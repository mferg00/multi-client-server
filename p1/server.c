#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "client_functions.h"
#include "messages.h"
#include "../utils.h"

// utils.h has MAX_CLIENTS defined as 10, so redefine it
#undef MAX_CLIENTS
#define MAX_CLIENTS 1

// socket variables
unsigned int sockfd = 0, clilen = 0;
struct sockaddr_in servaddr, cliaddr;

// interrupt flag
volatile sig_atomic_t flag = 0;

// message buffer
char buff[MAX_MSG_SIZE] = {0};

// function prototypes
void
catch_sigchld(int sig);
void
catch_sigint(int sig);
void
setup_server(int port);
int
accept_client(void);
void
server_loop(void);

// main, checks cmd arguments for validity before commencing
int main(int argc, char **argv)
{
    switch(argc)
    {
        case 1:
          setup_server(DEFAULT_PORT);
          break;

        case 2:
          setup_server(atoi(argv[1]));
          break;

        default:
          printf("usage: <port>\n");
          exit(1);
    }

    while(1)
    {
        server_loop();
    }

    return 0;
}

// catch sigchld
// originally, the server forked each new client, but now i just use it for the message
void
catch_sigchld(int sig)
{
    write(0, "client disconnected\n", 21);
    flag = sig;
}

// catch sigint, shutdown the server gracefully and clear messages
void
catch_sigint(int sig)
{
    write(0, "server exiting\n", 16);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    free_channels();
    exit(0);
}

// server setup based on port
void
setup_server(int port)
{
    // signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = catch_sigint;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    // server message
    puts("* using server V1:");
    puts("* multi-client not implemented until V2");
    puts("\nserver starting...");

    //connect to socket
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror ("socket creation failed");
        exit (1);
    }

    // build server address
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(port);

    // bind
    if (bind (sockfd, (struct sockaddr *) &servaddr, sizeof (servaddr)) < 0)
    {
        perror ("bind failed");
        exit (1);
    }

    int option = 1;
    clilen = sizeof (cliaddr);

    // set socket options
    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &option, clilen) < 0)
    {
        perror ("setsockopt failed");
        exit (1);
    }

    // listen for clients
    if (listen (sockfd, MAX_CLIENTS) < 0)
    {
        perror ("listen failed");
        exit (1);
    }

    printf ("waiting for incoming connections...\n");

}

// accept new clients and return the connection fd
int
accept_client(void)
{
    int connfd = 0;
    if((connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
    {
        perror(0);
        exit(1);
    }

    printf("connection accepted from %s:%d\n", inet_ntoa(cliaddr.sin_addr), \
          ntohs(cliaddr.sin_port));

    // build welcome message
    snprintf(buff, MAX_MSG_SIZE, "welcome! your client ID is %d\n", \
            ntohs(cliaddr.sin_port));

    // send welcome message
    if(write(connfd, buff, MAX_MSG_SIZE) < 0)
    {
        perror("server->client write error");
    }

    return connfd;
}

// server loop - accept a client and then cater for it
void
server_loop(void)
{
    // client is initially allowed to talk
    server_request client = TALK;

    int connfd = 0;
    connfd = accept_client();

    while(1)
    {
        bzero(buff, sizeof(buff));

        // if client can talk, read the socket
        if(client == TALK)
        {
            if(read(connfd, buff, sizeof(buff)) < 0)
            {
                break;
            }

            // if nothing sent, client has disconnected
            if(buff[0] == 0)
            {
                break;
            }

            // if parse_client returns -1, the buffer now contains an error message
            // skip straight to the write stage
            if (parse_client(buff, MAX_MSG_SIZE) == -1)
            {
                client = TALK;
                goto write;
            }
        }

        bzero(buff, sizeof(buff));

        // build a message for the client and save it's state
        client = execute_client(buff, sizeof(buff));

        // write to client
        write:

        if (write(connfd, buff, MAX_MSG_SIZE) < 0)
        {
            perror("server->client write error");
            return;
        }
    }

    // when client disconnects, reset the message data and close the conn socket
    catch_sigchld(0);
    reset_client_data();
    close(connfd);
}
