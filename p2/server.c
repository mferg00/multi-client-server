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

// socket variables
int sockfd = 0, connfd = 0;
unsigned int clilen = 0;
struct sockaddr_in servaddr, cliaddr;

// interrupt flag
volatile sig_atomic_t flag = 0;

// msg buffer
char buff[MAX_MSG_SIZE] = {0};

// function prototypes
void
catch_sigchld(int sig);
void
catch_sigint(int sig);
void
setup_server(int port);
void
accept_client(void);
void
server_loop(void);

// main, checks cmd arguments for validity before commencing
int
main(int argc, char **argv)
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

// catch child process death
void
catch_sigchld(int sig)
{
    write(0, "client disconnected\n", 21);
    flag = sig;
}

// catch sigint, close server, detach shmem and close process
void
catch_sigint(int sig)
{
    write(0, "server exiting\n", 16);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    detach_shmem();
    while(waitpid(-1, NULL, WNOHANG) > 0);
    exit(0);
}

//setup server at a port
void
setup_server(int port)
{

    // server message
    puts("* using server V2:");
    puts("* threading not implemented until V3");
    puts("\nserver starting...");

    // if shmem can't be setup, exit
    if (setup_messages() < 0)
    {
        exit(1);
    }

    // signal handlers
    signal(SIGCHLD, catch_sigchld);

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = catch_sigint;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    // connect to socket
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket creation failed");
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
        perror("bind failed");
        exit (1);
    }

    int option = 1;
    clilen = sizeof (cliaddr);

    // set socket options
    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &option, clilen) < 0)
    {
        perror("setsockopt failed");
        exit (1);
    }

    // listen for clients
    if (listen (sockfd, MAX_CLIENTS) < 0)
    {
        perror("listen failed");
        exit (1);
    }

    printf ("waiting for incoming connections...\n");

}

// accept new client sand store the connection fd
void
accept_client(void)
{
    if((connfd = accept(sockfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
    {
        // print error if the error is not bad file descriptor
        if (errno != EBADF) perror(0);
        exit(1);
    }

    printf("connection accepted from %s:%d\n", inet_ntoa(cliaddr.sin_addr), \
          ntohs(cliaddr.sin_port));

    // build and send welcome message
    snprintf(buff, MAX_MSG_SIZE, "welcome! your client ID is %d", ntohs(cliaddr.sin_port));

    if(write(connfd, buff, MAX_MSG_SIZE) < 0)
    {
        perror("server->client write error");
    }
}

void
server_loop(void)
{
    // client is allowed to talk initially
    server_request client = TALK;

    accept_client();

    // fork when a new client comes in
    switch(fork())
    {
        case -1:
            perror("fork error");
            return;

        // CHILD PROCESS:
        case 0:
            // close listening socket
            close(sockfd);


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
            break;

        // PARENT PROCESS
        default:
            // close conenction socket
            close(connfd);

            // wait for child to die :'(
            while(waitpid(-1, NULL, WNOHANG) > 0);
            break;

    }
}
