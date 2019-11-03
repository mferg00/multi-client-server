#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "messages.h"
#include "../utils.h"

// node struct
typedef struct node
{
    char *message;
    struct node *next;
} node_t;

// head of each channel
node_t *head[MAX_CHANNELS] = {NULL};

// head of each channel (as seen by newly subscribed user)
node_t *client_head[MAX_CHANNELS] = {NULL};

// client data
int subscribed[MAX_CHANNELS] = {0};
int msg_count[MAX_CHANNELS] = {0};
int cl_read_count[MAX_CHANNELS] = {0};
int any_unread = 0;


// clear client-specific variables
void
reset_client_data(void)
{
    bzero(cl_read_count, sizeof(cl_read_count));
    bzero(subscribed, sizeof(subscribed));
}

int
subscribe(int channel)
{
    if (subscribed[channel]) return -1;

    subscribed[channel] = 1;

    for(int i = 0; i < MAX_CHANNELS; i++)
    {
        client_head[channel] = NULL;
    }

    return 1;
}

int
unsubscribe(int channel)
{
    if (!subscribed[channel]) return -1;

    subscribed[channel] = 0;

    return 1;
}

/*
  * adds a new message node to the end of the channel
  * out: -1 if malloc err, -2 if message is NULL, 1 if success
  * in: channel - desired channel, msg - pointer to message to add
  */
int
add_message(int channel, char *msg)
{
    if(msg == NULL || *msg == 0) return -2;

    // node pointer
    node_t* new_node;

    // malloc space for each node variable
    new_node = (node_t*)malloc(sizeof(node_t));
    if(new_node == NULL)
    {
        perror("node allocation error");
        return -1;
    }

    // store head of channel in variable
    node_t* last = head[channel];

    // malloc space for the string plus '\0'
    size_t alloc = strlen(msg) + 1;
    new_node->message = (char*)malloc(alloc);
    if(new_node->message == NULL)
    {
        perror("message allocation error");
        return -1;
    }

    // copy the message in to the node
    snprintf(new_node->message, alloc, "%s", msg);
    msg_count[channel]++;

    // free the original message passed in
    free(msg);
    msg = NULL;

    // make this node the end of the list
    new_node->next = NULL;

    // if the list is empty, then make the new node as head
    if (head[channel] == NULL || head[channel]->message == NULL)
    {
        head[channel] = new_node;
        return 0;
    }

    // else traverse till the last node
    while (last->next != NULL)
    {
        last = last->next;
    }

    last->next = new_node;

    // check if a client is subscribed but has not had their channel head pointer set
    for(int i = 0; i < MAX_CHANNELS; i++)
    {
        if(subscribed[i] == 1 && client_head[i] == NULL)
        {
              client_head[channel] = new_node;
        }
    }

    return 0;
}

/*
  * traverse to the next message based on client data
  * out: pointer to the message (NULL if no message)
  * in: channel - desired channel, *err = pointer to int which the error value is written to
  */
char
*next_message(int channel, int *err)
{
    if(!subscribed[channel])
    {
        *err = 1;
        return NULL;
    }

    node_t *temp = client_head[channel];

    // if channel is reserved but has no messages
    if(temp == NULL)
    {
        *err = 2;
        return NULL;
    }

    char *retval;

    if ((retval = traverse(channel, cl_read_count[channel])) == NULL)
    {
        *err = 3;
        return NULL;
    }
    else
    {
        cl_read_count[channel]++;
        *err = 0;
        return retval;
    }
}


/*
  * traverse a channels mesage node's for a specified count
  * out: pointer to the message (NULL if no message)
  * in: channel - desired channel, count - how far to traverse
  */
char*
traverse(int channel, int count)
{
    if (count < 0 || count > msg_count[channel])
    {
        return NULL;
    }

    node_t *temp = client_head[channel];

    for(int i = 0; i < count; i++)
    {
        if(temp->next != NULL)
        {
            temp = temp->next;
        }
        else
        {
            return NULL;
        }
    }

    return temp->message;
}


int
is_subscribed(int channel)
{
    return subscribed[channel];
}

int
total_count(int channel)
{
    return msg_count[channel];
}

int
unread_count(int channel)
{
    return (msg_count[channel] - cl_read_count[channel]);
}

int
read_count(int channel)
{
    return cl_read_count[channel];
}

// free all nodes
void
free_channels(void)
{
    // clear all the channels, ignoring the heads
    for(int i = 0; i < 255; i++)
    {
        if(head[i] != NULL)
        {
            free_channel(i);
        }
    }

    // clear the heads
    for(int i = 0; i < 255; i++)
    {
        if(head[i] != NULL)
        {
            free(head[i]);
        }
    }
}

// free a channel of it's nodes
void
free_channel(int channel){

    node_t *temp;

    while (head[channel] != NULL)
    {
        temp = head[channel];
        head[channel] = head[channel]->next;
        free(temp);
    }
}
