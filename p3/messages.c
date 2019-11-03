#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <pthread.h>
#include "messages.h"
#include "../utils.h"

// client msg data struct
typedef struct
{
    node_t* client_head[MAX_CHANNELS];
    int id;
    char subscribed[MAX_CHANNELS];
    int cl_read_count[MAX_CHANNELS];
} client_msg_data;

// mutex and semaphore variables
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int write_lock = 0, read_lock = 0;

// head of each channel
node_t* head[MAX_CHANNELS] = {NULL};

// global data
unsigned int msg_count[MAX_CHANNELS] = {0};

// array of pointers to future client data
client_msg_data* client_data[MAX_CLIENTS] = {NULL};

// find msg data pointer based on client port
client_msg_data*
find_client_msg_data(int port)
{
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_data[i]->id == port)
        {
            return client_data[i];
        }

    }
    return NULL;
}

// malloc some client msg data for a new client
int
add_client_msg_data(int port)
{
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if (client_data[i] == NULL)
        {
            client_data[i] = (client_msg_data*) malloc(sizeof(client_msg_data));
            if (client_data[i] == NULL)
            {
                perror("client msg data alloc error");
                return -1;
            }

            // initialise client data
            client_data[i]->id = port;

            return 1;
        }
    }
    return -1;
}

// free a client msg data based on port
int
remove_client_msg_data(int port)
{
    client_msg_data* client;

    if((client = find_client_msg_data(port)) == NULL)
    {
        printf("client msg data non existant\n");
        return -1;
    }

    free(client);
    client = NULL;

    return 1;
}

int
subscribe(int port, int channel)
{
    client_msg_data* client;

    if((client = find_client_msg_data(port)) == NULL)
    {
        return -1;
    }

    if (client->subscribed[channel]) return -1;

    client->subscribed[channel] = 1;

    for(int i = 0; i < MAX_CHANNELS; i++)
    {
        client->client_head[channel] = NULL;
    }

    return 1;
}

int
unsubscribe(int port, int channel)
{
    client_msg_data* client;

    if((client = find_client_msg_data(port)) == NULL)
    {
        return -1;
    }

    if (!client->subscribed[channel]) return -1;

    client->subscribed[channel] = 0;

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

    // wait for everyone to finish reading and writing
    while(write_lock && read_lock);

    // set mutex lock and semphore
    pthread_mutex_lock(&mutex);
    write_lock = 1;

    // store head of channel in variable
    node_t* last = head[channel];

    // malloc space for the string plus '\0'
    size_t alloc = strlen(msg) + 1;
    new_node->message = (char*)malloc(alloc);
    if(new_node->message == NULL)
    {
        perror("message allocation error");
        write_lock = 0;
        pthread_mutex_unlock(&mutex);
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
        write_lock = 0;
        pthread_mutex_unlock(&mutex);
        return 0;
    }

    // else traverse till the last node
    while (last->next != NULL)
    {
        last = last->next;
    }

    last->next = new_node;

    // check if a client is subscribed but has not had their channel head pointer set
    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(client_data[i] != NULL)
        {
            if(client_data[i]->subscribed[channel] == 1 && \
                client_data[i]->client_head[channel] == NULL)
            {
                  client_data[i]->client_head[channel] = new_node;
            }
        }
    }

    // unlock mutex and semphore
    write_lock = 0;
    pthread_mutex_unlock(&mutex);
    return 0;
}

/*
  * traverse to the next message based on client data
  * out: pointer to the message (NULL if no message)
  * in: client port, channel, pointer to int
  */
char*
next_message(int port, int channel, int *err)
{
    client_msg_data* client;

    if((client = find_client_msg_data(port)) == NULL)
    {
        *err = 5;
        return NULL;
    }

    if(!client->subscribed[channel])
    {
        *err = 1;
        return NULL;
    }

    node_t *temp = client->client_head[channel];

    // if channel is reserved but has no messages
    if(temp == NULL)
    {
        *err = 2;
        return NULL;
    }

    char *retval;

    //wait for everyone to finish writing
    while(write_lock);

    // set read lock
    read_lock = 1;

    if ((retval = traverse(temp, channel, client->cl_read_count[channel])) == NULL)
    {
        *err = 3;
        read_lock = 0;
        return NULL;
    }
    else
    {
        client->cl_read_count[channel]++;
        *err = 0;
        read_lock = 0;
        return retval;
    }
}

/*
  * traverse a channels mesage node's for a specified count
  * out: pointer to the message (NULL if no message)
  * in: channel - desired channel, count - how far to traverse
  */
char*
traverse(node_t* head, int channel, int count)
{
    if (head == NULL || count < 0 || count > msg_count[channel])
    {
        return NULL;
    }

    while(write_lock);

    node_t* temp = head;

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
is_subscribed(int port, int channel)
{
    client_msg_data* client;

    if((client = find_client_msg_data(port)) == NULL)
    {
        return -1;
    }

    return client->subscribed[channel];
}

int
total_count(int channel)
{
    return msg_count[channel];
}

int
unread_count(int port, int channel){

    client_msg_data* client;

    if((client = find_client_msg_data(port)) == NULL)
    {
        return -1;
    }

    return (msg_count[channel] - client->cl_read_count[channel]);
}

int
read_count(int port, int channel)
{
    client_msg_data* client;

    if((client = find_client_msg_data(port)) == NULL)
    {
        return -1;
    }

    return client->cl_read_count[channel];
}

// free all nodes
void
free_channels(void)
{
    // free channels messages
    for(int i = 0; i < 255; i++)
    {
        if(head[i] != NULL)
        {
            free_channel(i);
        }
    }

    // free channel heads
    for(int i = 0; i < 255; i++)
    {
        if(head[i] != NULL)
        {
            free(head[i]);
            head[i] = NULL;
        }
    }
}


// free a channel of it's nodes
void
free_channel(int channel)
{
    node_t *temp;

    while (head[channel] != NULL)
    {
       temp = head[channel];
       head[channel] = head[channel]->next;
       if(temp != NULL)
       {
           free(temp);
           temp = NULL;
       }
    }
}
