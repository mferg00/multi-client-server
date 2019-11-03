#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include "messages.h"
#include "../utils.h"

// client data
char subbed[MAX_CHANNELS] = {0};
unsigned int cl_read_count[MAX_CHANNELS] = {0};
unsigned int total_msg_count[MAX_CHANNELS] = {0};

// pointer to the end of the last read message of each channel
char* last_message[MAX_CHANNELS];

// shm key init
key_t key_msgs = -1;

// pointers to regions of shared memory, shown from start to end
char* read_lock_ptr;      // binary semaphore, set to 1 if shmem being read
char* write_lock_ptr;     // binary semaphore, set to 1 if shmem being written to
char* free_message_ptr;   // byte offset from start of message space to free space
char* messages_start;     // start of message space
char* messages_end;       // end of message space

// sizes for the data at each pointer
#define LOCK_SIZE 1
#define FREE_PTR_SIZE 11
#define MSG_SPACE_SIZE (MAX_MSG_SIZE * MAX_MSGS)
#define SHM_SIZE (LOCK_SIZE + LOCK_SIZE + FREE_PTR_SIZE + MSG_SPACE_SIZE)

int shmid = -1;

// lock byte functions
void
write_lock(void)
{
    *write_lock_ptr = 1;
}

void
write_unlock(void)
{
    *write_lock_ptr = 0;
}

char
check_write_lock(void)
{
    return *write_lock_ptr;
}

// lock byte functions
void
read_lock(void)
{
    *read_lock_ptr = 1;
}

void
read_unlock(void)
{
    *read_lock_ptr = 0;
}

char
check_read_lock(void)
{
    return *read_lock_ptr;
}

int
setup_messages(void)
{

    // get key based on current file
    key_msgs = ftok(".", 'b');

    // get shared mem id
    if ((shmid = shmget(key_msgs, SHM_SIZE, IPC_CREAT | 0666)) < 0)
    {
        perror("shmget messages");
        return -1;
    }

    // attach shared memory
    if ((read_lock_ptr = (char*)shmat(shmid, NULL, 0)) == (char*) -1) \
    {
        perror("shmat messages");
        return -1;
    }

    // set pointers
    write_lock_ptr = read_lock_ptr + LOCK_SIZE;
    free_message_ptr = write_lock_ptr + LOCK_SIZE;
    messages_start = free_message_ptr + FREE_PTR_SIZE;
    messages_end = messages_start + MSG_SPACE_SIZE;

    // set lock bytes to 0
    write_unlock();
    read_unlock();

    // initialise last messages read
    for(int i = 0; i < 255; i++)
    {
        last_message[i] = messages_start;
    }

    // nothing is in message space yet, so a byte offset of 0 is written in
    strncpy(free_message_ptr, "0000000000\0", 11);

    return 1;

}

// detach and delete shared memory
int
detach_shmem(void)
{
    if(shmdt(read_lock_ptr) < 0)
    {
        perror("shmdt error");
        return -1;
    }

    if (shmctl(shmid, IPC_RMID, NULL) < 0)
    {
        perror("shmctl error");
        return -1;
    }

    write(0, "shared memory detached\n", 24);
    return 1;
}

// get pointer to free message space
char*
get_free_message(void)
{
    return messages_start + strtol(free_message_ptr, NULL, 10);
}

// inrease the free message offset by a value
void
set_free_message(int offset)
{
    snprintf(free_message_ptr, FREE_PTR_SIZE, "%010ld", \
             strtol(free_message_ptr, NULL, FREE_PTR_SIZE - 1) + offset);
}

/*
  * adds a new message 1 byte in from the the last message
  * out: -1 if msg too long, -2 if buffer is full, 1 if success
  * in: channel - desired channel, msg - pointer to message to add
  */
int
add_message(int channel, char* msg)
{
    // check to see if message is valid
    if(msg == NULL || *msg == 0) return -1;

    // space to alloc: <###> + strlen of message + '\0'
    size_t msg_len = 5 + strlen(msg) + 1;

    //check if space is too large
    if(msg_len >= MAX_MSG_SIZE)
    {
        return -1;
    }

    // check if message doesn't fit in memory
    if(msg_len + get_free_message() >= messages_end)
    {
        printf("buffer full\n");
        return -2;
    }

    // wait for noone to be reading or writing to buffer
    while(check_write_lock() && check_read_lock());

    // lock the write lock
    write_lock();

    // copy message as <###>msg into free space
    int space_used = 1 + snprintf(get_free_message(), msg_len, \
                                  "<%03d>%s", channel, msg);

    // free original message
    free(msg);
    msg = NULL;

    // increase the free message pointer
    set_free_message(space_used);

    total_msg_count[channel]++;

    // unlock the write lock
    write_unlock();

    return 1;
}

/*
  * traverse to the next message from the last_message[channel] pointer
  * out: pointer to the message (NULL if no message)
  * in: channel - desired channel, *err = pointer to int which the error value is written to
  */
char*
next_message(int channel, int *err)
{
    // wait for write to be complete
    while(check_write_lock());

    // lock the read lock
    read_lock();

    // pointer to traverse from
    char* traverse = last_message[channel];

    // buffer to store the channel string in
    char ch_buf[4] = {0};

    if (subbed[channel])
    {
        if(total_msg_count[channel] == 0)
        {
            *err = 2;
            read_unlock();
            return NULL;
        }

        while(traverse <= messages_end)
        {
            // if two consecutive 0 bytes, end of all messages is reached
            if(traverse[0] == '\0' && traverse[1] == '\0')
            {
                *err = 3;
                read_unlock();
                return NULL;
            }

            // if channel id is found
            if(traverse[0] == '<' && traverse[4] == '>')
            {
                // copy the numbers into the channel buffer
                strncpy(ch_buf, traverse + 1, 3);
                ch_buf[4] = 0;

                // increase traverse so it points to the message
                traverse += 5;

                // check if its the right channel
                if (atoi(ch_buf) == channel)
                {
                    *err = 0;
                    cl_read_count[channel]++;
                    last_message[channel] = traverse;
                    read_unlock();
                    return traverse;
                }
            }

            else traverse++;
        }
    }

    *err = 1;
    // unlock the read
    read_unlock();
    return NULL;
}

int
subscribe(int channel)
{
    if (subbed[channel]) return -1;

    subbed[channel] = 1;

    // set the last read message for each channel to empty space
    // so they can't read into the past
    for(int i = 0; i < MAX_CHANNELS; i++)
    {
        last_message[i] = get_free_message();
    }

    return 1;
}

char
is_subscribed(int channel)
{
    return subbed[channel];
}

int
unsubscribe(int channel)
{
    if (!subbed[channel]) return -1;

    subbed[channel] = 0;
    return 1;
}

int
total_count(int channel)
{
    return total_msg_count[channel];
}

int
unread_count(int channel)
{
    return total_msg_count[channel] - cl_read_count[channel];
}

int
read_count(int channel)
{
    return cl_read_count[channel];
}
