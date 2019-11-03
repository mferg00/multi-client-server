// linked list node
typedef struct node
{
    char *message;
    struct node *next;
} node_t;


int
add_client_msg_data(int port);

int
remove_client_msg_data(int port);

int
subscribe(int port, int channel);

int
unsubscribe(int port, int channel);

int
add_message(int channel, char *msg);

char*
next_message(int port, int channel, int *err);

char*
traverse(node_t* head, int channel, int count);

int
is_subscribed(int port, int channel);

int
total_count(int channel);

int
unread_count(int port, int channel);

int
read_count(int port, int channel);

void
free_channels(void);

void
free_channel(int channel);
