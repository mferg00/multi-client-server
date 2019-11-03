#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_functions.h"
#include "messages.h"
#include "../utils.h"

// struct to store client state
typedef struct
{
    int id;
    char* message;
    int channel;
    client_request mode;
    int argc;
    client_request livefeed;
    int channel_loop_count;
    int next_loop_count;
} client_state_data;

// array of pointers to future client states
client_state_data* client_state[MAX_CLIENTS] = {NULL};

// find client pointer based on client port
client_state_data*
find_client(int port)
{
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(client_state[i] != NULL)
        {
            if (client_state[i]->id == port)
            {
                return client_state[i];
            }
        }
    }
    return NULL;
}

// initialise client info
void
init_client(client_state_data* client)
{
    client->message = NULL;
    client->channel = -1;
    client->mode = NONE;
    client->argc = 0;
    client->livefeed = NONE;
    client->channel_loop_count = -1;
    client->next_loop_count = 0;
}

// malloc some client data for a new client
int
add_client(int port)
{
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_state[i] == NULL)
        {
            client_state[i] = (client_state_data*) malloc(sizeof(client_state_data));
            if (client_state[i] == NULL)
            {
                perror("client data alloc error");
                return -1;
            }
            client_state[i]->id = port;
            init_client(client_state[i]);

            // also malloc client msg data
            return add_client_msg_data(port);
        }
    }
    return -1;
}

// free a clients data based on port
int
remove_client(int port)
{
    client_state_data* client;

    if((client = find_client(port)) == NULL)
    {
        printf("client doesn't exist\n");
        return -1;
    }

    free(client);
    client = NULL;

    // also free it's message data
    return remove_client_msg_data(port);
}

int
parse_client(int port, char *buff, size_t buff_size)
{
    if(buff == NULL || *buff == 0)
    {
        snprintf(buff, buff_size, _TALK _RED "invalid command");
        return -1;
    }

    // find client data
    client_state_data* client;

    if((client = find_client(port)) == NULL)
    {
        printf("client doesn't exist\n");
        return -1;
    }

    // reset variables
    client->channel_loop_count = -1;
    client->next_loop_count = 0;
    client->argc = 0;

    // argv argc variables
    char *token;
    int linelen = strlen(buff);
    int len = 0;
    int spaces = 0;

    // if enter was rpessed
    if(buff[0] == '\n')
    {
        buff[0] = 0;

        if(client->livefeed == LIVEFEED_ALL)
        {
            client->mode = NEXT_ALL;
            return 1;
        }
        else if(client->livefeed == LIVEFEED)
        {
            client->mode = NEXT;
            return 1;
        }
        else
        {
            return -1;
        }
    }

    // find argc for the string, ignoring spaces in the message
    for(int i = 0; i < linelen; i++)
    {
        if(buff[i] != ' ')
        {
            if(buff[i+1] == ' ' || buff[i+1] == '\n')
            {
                client->argc++;
                if(client->argc >= 3) break;
            }
        }
        else spaces++;
    }

    if(client->argc > 3) client->argc = 3;

    // get the mode (first arg)
    token = strtok(buff, DELIM_CHARS);
    if((client->mode = get_mode(client->argc, token)) == -1)
    {
        bzero(buff, buff_size);
        snprintf(buff, buff_size, _TALK _RED "invalid command");
        return -1;
    }

    len += strlen(token);

    // if there is 2 args, get the channel id (second arg)
    if (client->argc > 1)
    {
        token = strtok(NULL, DELIM_CHARS);

        if((client->channel = get_channel(token)) == -1)
        {
            bzero(buff, buff_size);
            snprintf(buff, buff_size, _TALK _RED \
                      "channel must be from 0->%d (inclusive)", MAX_CHANNELS - 1);
            return -1;
        }
        len += strlen(token);
    }

    // if there is 3 args, get the message (third arg)
    if (client->argc > 2)
    {
        // find the start pointer of the message
        char *start = buff + len + spaces;

        // allocate space for the message
        size_t alloc = sizeof(char) * (linelen - (spaces + len));
        if ((client->message = (char*)malloc(alloc)) == NULL)
        {
            perror("alloc");
            exit(1);
        }
        snprintf(client->message, alloc, "%s", start);
    }

    bzero(buff, buff_size);
    return 1;
}

/*
  * converts the second arg to an int, and checks if it is a valid channel
  * out: 1 if success, -1 if not success
  * in: line - pointer to the second arg
  */
int
get_channel(char *line)
{
    char* end = NULL;
    int ch_id = (int)strtol(line, &end, 10);

    if(line == end || end == NULL) return -1;

    if(ch_id < 0 || ch_id > MAX_CHANNELS - 1)
    {
        return -1;
    }

    return ch_id;
}

/*
  * converts the first arg to a client_request value
  * out: < 0 if success, -1 if not success
  * in: argc - the argc counted, line - pointer to the first arg
  */
client_request
get_mode(int argc, char *line)
{
    if(line == NULL)
    {
        return NONE;
    }

    if(strcmp(line, "BYE") == 0)
    {
        return EXIT;
    }

    else if(strcmp(line, "/intr") == 0)
    {
        return INTERRUPT;
    }

    else if (strcmp(line, "SUB") == 0)
    {
        return SUB;
    }

    // CHANNELS
    else if (strcmp(line, "CHANNELS") == 0)
    {
        return CHANNELS;
    }

    // UNSUB
    else if (strcmp(line, "UNSUB") == 0)
    {
        return UNSUB;
    }

    else if (strcmp(line, "NEXT") == 0)
    {
        if(argc == 2)
        {
            return NEXT;
        }
        else if(argc == 1)
        {
            return NEXT_ALL;
        }
    }

    else if (strcmp(line, "LIVEFEED") == 0)
    {
        if(argc == 2)
        {
            return LIVEFEED;
        }
        else if(argc == 1)
        {
            return LIVEFEED_ALL;
        }
    }

    else if (strcmp(line, "SEND") == 0)
    {
        return SEND;
    }

    return -1;

}


/*
  * composes a message to be written to the client based on many factors
  * out: TALK, QUIET
  * in: port - client port, buff - pointer to msg buffer
  * note: _TALK = "/t " and _QUIET = "/q ". This is how the server tells the
  *       client to talk or be quiet
  * note: _GRN, _RED... are ANSI colour strings to make the messages look nice
  */
server_request
execute_client(int port, char* buff, size_t buff_size)
{
    client_state_data* client;
    char* msg = NULL;

    if((client = find_client(port)) == NULL)
    {
        printf("client doesn't exist\n");
        return -1;
    }

    if(client->mode == EXIT)
    {
        return CLOSE;
    }

    if(client->mode == NONE)
    {
        return TALK;
    }

    if(client->mode == LIVEFEED)
    {
        if(is_subscribed(client->id, client->channel))
        {
            client->livefeed = LIVEFEED;
            snprintf(buff, buff_size, _TALK _GRN "livefeed on for channel %d\n" \
                      _CLR "*press enter to receive new messages", client->channel);
            return TALK;
        }
        snprintf(buff, buff_size, _TALK _MAG "not subscribed to channel %d", client->channel);
    }

    else if(client->mode == LIVEFEED_ALL)
    {
        for(int i = 0; i < MAX_CHANNELS; i++)
        {
            if(is_subscribed(client->id, client->channel))
            {
                client->livefeed = LIVEFEED_ALL;
                snprintf(buff, buff_size, _TALK _GRN "livefeed on for all channels\n" \
                          _CLR "*press enter to receive new messages");
                return TALK;
            }
        }

        snprintf(buff, buff_size, _TALK _RED "not subscribed to any channels");

    }

    else if(client->mode == INTERRUPT)
    {
        if(client->livefeed == LIVEFEED_ALL || client->livefeed == LIVEFEED)
        {
            snprintf(buff, buff_size, _TALK _GRN "livefeed off");
            client->livefeed = NONE;
        }
        else
        {
            snprintf(buff, buff_size, "BYE");
        }
    }

    else if (client->mode == NEXT_ALL)
    {
        static int flag = -1;
        while(client->next_loop_count < MAX_CHANNELS)
        {
            int err = 0;
            if(is_subscribed(client->id, client->next_loop_count))
            {
                flag = 0;
                msg = next_message(client->id, client->next_loop_count, &err);
                if(err == 0)
                {
                    snprintf(buff, buff_size, _QUIET _CYN "%3d:" _CLR " %s",  \
                            client->next_loop_count, msg);
                    client->next_loop_count++;
                    return QUIET;
                }
            }
            client->next_loop_count++;
        }

        if(flag == -1)
            snprintf(buff, buff_size, _TALK _RED "not subscribed to any channels");
        flag = -1;
    }

    else if(client->mode == NEXT)
    {
        int err = 0;
        msg = next_message(client->id, client->channel, &err);

        if(client->livefeed == LIVEFEED || client->livefeed == LIVEFEED_ALL)
        {
            switch(err)
            {
                case 0:
                    snprintf(buff, buff_size, _QUIET _CYN "%3d:" _CLR " %s", \
                            client->channel, msg);
                    return QUIET;
                default:
                    break;
            }
        }

        else
        {
            switch(err){
                case 0:
                  snprintf(buff, buff_size, _TALK _CYN "%3d:" _CLR " %s", \
                            client->channel, msg);
                  return TALK;
                case 1:
                  snprintf(buff, buff_size, _TALK _RED "not subscribed to this channel");
                  break;
                case 2:
                  snprintf(buff, buff_size, _TALK _RED "no messages in this channel");
                  break;
                case 3:
                  snprintf(buff, buff_size, _TALK _MAG "no more new messages in this channel");
                  break;
                default:
                  snprintf(buff, buff_size, _TALK _RED "unkown message error");
                  break;
            }
        }

    }

    else if(client->mode ==  SUB)
    {
        if(client->argc == 2)
        {
            if(subscribe(client->id, client->channel) == 1)
            {
                snprintf(buff, buff_size, _TALK _GRN "subscribed to channel %d", \
                        client->channel);
            }
            else
            {
                snprintf(buff, buff_size, _TALK _MAG "already subscribed to channel %d",\
                        client->channel);
            }
        }

        else
        {
            snprintf(buff, buff_size, _TALK _RED "usage: SUB <channel_id>");
        }
    }

    else if(client->mode ==  CHANNELS)
    {
        while(client->channel_loop_count < MAX_CHANNELS)
        {
            if (client->channel_loop_count == -1)
            {
                snprintf(buff, buff_size, _QUIET "[ch #] [all] [read] [unread]");
                client->channel_loop_count++;
                return QUIET;
            }

            else if(is_subscribed(client->id, client->channel_loop_count) == 1)
            {
              snprintf(buff, buff_size, _QUIET "  %3d   %3d   %3d   %3d", \
                      client->channel_loop_count, \
                      total_count(client->channel_loop_count), \
                      read_count(client->id, client->channel_loop_count), \
                      unread_count(client->id, client->channel_loop_count));
                client->channel_loop_count++;
                return QUIET;
            }

            client->channel_loop_count++;
        }
    }

    else if(client->mode ==  UNSUB)
    {
        if(client->argc == 2)
        {
            if(unsubscribe(client->id, client->channel) == 1)
            {
                snprintf(buff, buff_size, _TALK _GRN "unsubscribed from channel %d", \
                        client->channel);
            }
            else
            {
                snprintf(buff, buff_size, _TALK _MAG "not subscribed to channel %d", \
                        client->channel);
            }
        }

        else
        {
            snprintf(buff, buff_size, _TALK _RED "usage: UNSUB <channel_id>");
        }
    }

    else if(client->mode == SEND)
    {
        switch(add_message(client->channel, client->message))
        {
            case -1:
              snprintf(buff, buff_size, _TALK _RED "could not add to channel (malloc error)");
              break;
            case -2:
              snprintf(buff, buff_size, _TALK _RED "could not add to channel (invalid format)");
              break;

        }
    }

    return TALK;


}
