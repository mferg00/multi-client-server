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
#include <pthread.h>
#include "../utils.h"
#include "client_functions.h"
#include "messages.h"

// array for each client thread
pthread_t client_threads[MAX_CLIENTS];

// server variables
int sockfd = 0;
struct sockaddr_in servaddr;

// interrupt flag
volatile sig_atomic_t flag = 0;

// struct to pass into a thread upon client connection
typedef struct
{
    int port;
    int socket;
} conn_info;

//function protoypes
void
setup_server(int port);
void
accept_clients(void* data);
void
client_thread(void* socket);
void
catch_sigchld(int sig);
void
catch_sigint(int sig);

// main: check cmd args for validity
int
main(int argc, char** argv)
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

    return 0;
}

// note: this is useless for signal handling, i just use it's message
void
catch_sigchld(int sig)
{
    write(0, "client disconnected\n", 21);
    flag = sig;
}

// catch sigint, close server and free messages
void
catch_sigint(int sig)
{
    flag = sig;
    write(0, "server exiting\n", 16);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    free_channels();
    exit(0);
}


void
setup_server(int port)
{
    // server message
    puts("* using server V3:");
    puts("* threading implemented");
    puts("server starting...");

    // setup signal handlers
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

    // set sokcet options
    if (setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, (void *) &option, sizeof(servaddr)) < 0)
    {
        perror("setsockopt failed");
        exit (1);
    }

    // listen for clients on socket
    if (listen (sockfd, MAX_CLIENTS) < 0)
    {
        perror("listen failed");
        exit (1);
    }

    // make a seperate thread to handle connections
    pthread_t connection_thread;

    if((pthread_create(&connection_thread, NULL, (void *)&accept_clients, \
        (void *)NULL)) < 0)
    {
        perror("pthread create");
    }

    puts ("waiting for incoming connections...");

    // wait for the connection thhread to finish
    pthread_join(connection_thread, NULL);

}

// accept clients loop, takes no arguments
void
accept_clients(void* data)
{
    // client info
    struct sockaddr_in cliaddr;
    unsigned int clilen = sizeof(cliaddr);
    static int num_threads = 0;
    conn_info conn;

    while(1)
    {
        // accept a new client and store it's connection socket
        if((conn.socket = accept(sockfd, (struct sockaddr*)&cliaddr, &clilen)) < 0)
        {
            if(errno != EBADF) perror("client accept error");
            continue;
        }

        // store it's port
        conn.port = ntohs(cliaddr.sin_port);
        printf("connection accepted from %s:%d\n", inet_ntoa(cliaddr.sin_addr), \
              conn.port);

        // if there is space for more clients
        if(num_threads < MAX_CLIENTS)
        {
            // create client thread, and pass the port and connection socket in
            if((pthread_create(&client_threads[num_threads], NULL, (void *)&client_thread, \
                (void *)&conn)) < 0)
            {
                perror("pthread create");
                continue;
            }

            num_threads++;
        }

        else break;
    }

    printf("not accepting new connections\n");
    pthread_exit(NULL);

}

// client thread, takes port and connection socket as input
void
client_thread(void* data)
{
      // msg buffer
      static char buff[MAX_MSG_SIZE] = {0};

      // cast void* data to conn_info struct
      conn_info conn = *((conn_info*)data);

      server_request state = TALK;

      // malloc client's message data
      add_client(conn.port);

      // build welcome message
      snprintf(buff, MAX_MSG_SIZE, "welcome! your client ID is %d", \
              conn.port);

      // go into the loop, skipping the read section
      goto write;

      while(1)
      {
          bzero(buff, sizeof(buff));

          // if client can talk, read the socket
          if(state == TALK)
          {
              if (read(conn.socket, buff, sizeof(buff)) < 0)
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
              if (parse_client(conn.port, buff, MAX_MSG_SIZE) == -1)
              {
                  state = TALK;
                  goto write;
              }
          }

          bzero(buff, sizeof(buff));

          // build a message for the client and save it's state
          state = execute_client(conn.port, buff, sizeof(buff));

          if(state == CLOSE) break;

          write:

          if (write(conn.socket, buff, MAX_MSG_SIZE) < 0)
          {
              perror("client->server write error");
              break;
          }
      }

      // free client msg data and close the connection port
      remove_client(conn.port);
      close(conn.socket);
      catch_sigchld(0);
      pthread_exit(NULL);
}
