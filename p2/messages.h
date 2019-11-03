int
setup_messages(void);

char*
get_message(int channel, int count, int *err);

char*
next_message(int channel, int *err);

int
add_message(int channel, char* msg);

int
detach_shmem(void);

int
subscribe(int channel);

char
is_subscribed(int channel);

int
unsubscribe(int channel);

int
total_count(int channel);

int
unread_count(int channel);

int
read_count(int channel);
