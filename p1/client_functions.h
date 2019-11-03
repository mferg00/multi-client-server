#include "../utils.h"

int
parse_client(char *line, size_t buff_size);

int
get_channel(char *line);

client_request
get_mode(char *line);

server_request
execute_client(char* buff, size_t buff_size);
