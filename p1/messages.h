void
reset_client_data(void);

int
subscribe(int channel);

int
unsubscribe(int channel);

int
add_message(int channel, char *msg);

int
is_subscribed(int channel);

int
total_count(int channel);

int
read_count(int channel);

char*
next_message(int channel, int *err);

void
print_channel(int channel);

void
free_channels(void);

void
free_channel(int channel);

char*
traverse(int channel, int count);

int
unread_count(int channel);
