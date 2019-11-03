#include "../utils.h"

int
add_client(int port);

int
remove_client(int port);

int
parse_client(int port, char *buff, size_t buff_size);

int
get_channel(char *line);

client_request
get_mode(int argc, char *line);

server_request
execute_client(int port, char* buff, size_t buff_size);
