#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_functions.h"
#include "messages.h"
#include "../utils.h"

// variables to store the parsed client's message
char *message = NULL;
int channel = -1;
client_request mode = NONE;
int argc = 0;

// client's livefeed status
client_request livefeed = NONE;

// loop iteration variables to save state of channels and next commands
int channel_loop_count = -1;
int next_loop_count = 0;

/*
  * parses the client message and stores each component
  * out: 1 if success, -1 if not success
  * in: buff - pointer to msg buffer with client message
  * note: msg buffer overwritten with error message if not successful
  */
int
parse_client(char *buff, size_t buff_size)
{
    if(buff == NULL || *buff == 0)
    {
        snprintf(buff, buff_size, _TALK _RED "invalid command");
        return -1;
    }

    // reset iter variables so channel and next commands display fully
    channel_loop_count = -1;
    next_loop_count = 0;

    argc = 0;
    char *token;

    // counters to know where the message is in SEND <channel> <message>
    int linelen = strlen(buff);
    int len = 0;          // increases when anything other than ' ' appears
    int spaces = 0;       // increases when ' ' appears

    // if enter was pressed
    if(buff[0] == '\n')
    {
        buff[0] = 0;

        // run next, next_all or error based on livefeed status
        if(livefeed == LIVEFEED_ALL)
        {
            mode = NEXT_ALL;
            return 1;
        }
        else if(livefeed == LIVEFEED)
        {
            mode = NEXT;
            return 1;
        }
        else
        {
            return -1;
        }
    }

    // find argc for the string, ignoring spaces in the message
    for(int i = 0; i < strlen(buff); i++)
    {
        if(buff[i] != ' ')
        {
            if(buff[i+1] == ' ' || buff[i+1] == '\n')
            {
                argc++;
                if(argc >= 3) break;
            }
        }
        else { spaces++ ;}
    }

    if(argc > 3) argc = 3;

    // get the mode (first arg)
    token = strtok(buff, DELIM_CHARS);
    if((mode = get_mode(token)) == -1)
    {
        bzero(buff, buff_size);
        snprintf(buff, buff_size, _TALK _RED "invalid command");
        return -1;
    }

    len += strlen(token);

    // if there is 2 args, get the channel id (second arg)
    if (argc > 1)
    {
        token = strtok(NULL, DELIM_CHARS);
        if((channel = get_channel(token)) == -1)
        {
            bzero(buff, buff_size);
            snprintf(buff, buff_size, _TALK _RED \
                      "channel must be from 0->%d (inclusive)", MAX_CHANNELS - 1);
            return -1;
        }
        len += strlen(token);
    }

    // if there is 3 args, get the message (third arg)
    if (argc > 2)
    {
        // find the start pointer of the message
        char *start = buff + len + spaces;

        // allocate space for the message
        size_t alloc = sizeof(char) * (linelen - (spaces + len));
        if ((message = (char*)malloc(alloc)) == NULL)
        {
            perror("alloc");
            exit(1);
        }
        snprintf(message, alloc, "%s", start);
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

    if(line == end) return -1;

    if(ch_id < 0 || ch_id > MAX_CHANNELS - 1)
    {
        return -1;
    }

    return ch_id;
}


/*
  * converts the first arg to a client_request value
  * out: < 0 if success, -1 if not success
  * in: line - pointer to the first arg
  */
client_request
get_mode(char *line)
{
    if(strcmp(line, "/intr") == 0)
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
  * in: buff - pointer to msg buffer
  * note: _TALK = "/t " and _QUIET = "/q ". This is how the server tells the
  *       client to talk or be quiet
  * note: _GRN, _RED... are ANSI colour strings to make the messages look nice
  */
server_request
execute_client(char* buff, size_t buff_size)
{
    char* msg = NULL;

    if(mode == LIVEFEED)
    {
        if(is_subscribed(channel))
        {
            livefeed = LIVEFEED;
            snprintf(buff, buff_size, _TALK _GRN "livefeed on for channel %d\n" \
                      _CLR "*press enter to receive new messages", channel);
            return TALK;
        }

        snprintf(buff, buff_size, _TALK _MAG "not subscribed to channel %d", channel);
    }

    else if(mode == LIVEFEED_ALL)
    {
        for(int i = 0; i < MAX_CHANNELS; i++)
        {
            if(is_subscribed(i))
            {
                livefeed = LIVEFEED_ALL;
                snprintf(buff, buff_size, _TALK _GRN "livefeed on for all channels\n" \
                          _CLR "*press enter to receive new messages");
                return TALK;
            }
        }

        snprintf(buff, buff_size, _TALK _RED "not subscribed to any channels");

    }

    else if(mode == INTERRUPT)
    {
        if(livefeed == LIVEFEED_ALL || livefeed == LIVEFEED)
        {
            snprintf(buff, buff_size, _TALK _GRN "livefeed off");
            livefeed = NONE;
        }
        else
        {
            snprintf(buff, buff_size, "BYE");
        }
    }

    else if (mode == NEXT_ALL)
    {
        static int flag = -1;
        while(next_loop_count < MAX_CHANNELS)
        {
            int err = 0;
            if(is_subscribed(next_loop_count))
            {
                flag = 0;
                msg = next_message(next_loop_count, &err);
                if(err == 0)
                {
                    snprintf(buff, buff_size, _QUIET _CYN "%3d:" _CLR " %s", next_loop_count, msg);
                    next_loop_count++;
                    return QUIET;
                }
            }
            next_loop_count++;
        }

        if(flag == -1)
            snprintf(buff, buff_size, _TALK _RED "not subscribed to any channels");
        flag = -1;
    }

    else if(mode == NEXT)
    {
        int err = 0;
        char* msg = next_message(channel, &err);


        if(livefeed == LIVEFEED || livefeed == LIVEFEED_ALL)
        {
            switch(err)
            {
                case 0:
                    snprintf(buff, buff_size, _QUIET _CYN "%3d:" _CLR " %s", channel, msg);
                    return QUIET;
                default:
                    break;
            }
        }

        else
        {
            switch(err){
                case 0:
                  snprintf(buff, buff_size, _TALK _CYN "%3d:" _CLR " %s", channel, msg);
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

    else if(mode ==  SUB)
    {
        if(argc == 2)
        {
            if(subscribe(channel) == 1)
            {
                snprintf(buff, buff_size, _TALK _GRN "subscribed to channel %d", channel);
            }
            else
            {
                snprintf(buff, buff_size, _TALK _MAG "already subscribed to channel %d", channel);
            }
        }

        else
        {
            snprintf(buff, buff_size, _TALK _RED "usage: SUB <channel_id>");
        }
    }

    else if(mode ==  CHANNELS)
    {
        while(channel_loop_count < MAX_CHANNELS)
        {
            if (channel_loop_count == -1)
            {
                snprintf(buff, buff_size, _QUIET "[ch #] [all] [read] [unread]");
                channel_loop_count++;
                return QUIET;
            }

            else if(is_subscribed(channel_loop_count) == 1)
            {
              snprintf(buff, buff_size, _QUIET "  %3d   %3d   %3d    %3d",  \
                      channel_loop_count, \
                      total_count(channel_loop_count), \
                      read_count(channel_loop_count), \
                      unread_count(channel_loop_count));
                channel_loop_count++;
                return QUIET;
            }

            channel_loop_count++;
        }
    }

    else if(mode == UNSUB)
    {
        if(argc == 2)
        {
            if(unsubscribe(channel) == 1)
            {
                snprintf(buff, buff_size, _TALK _GRN "unsubscribed from channel %d", channel);
            }
            else
            {
                snprintf(buff, buff_size, _TALK _MAG "not subscribed to channel %d", channel);
            }
        }

        else
        {
            snprintf(buff, buff_size, _TALK _RED "usage: UNSUB <channel_id>");
        }
    }

    else if(mode == SEND)
    {
        switch(add_message(channel, message))
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
