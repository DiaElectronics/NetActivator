#ifndef activator_cmd_h
#include <arpa/inet.h>

#define activator_cmd_h

#define MAX_CMD 512
#define MAX_CHANNELS_SUPPORTED 12

#define CMD_TYPE_UNKNOWN -1
#define CMD_TYPE_ACTIVATE 1
#define CMD_TYPE_PING 2
#define CMD_TYPE_TRANSFER 3
#define CMD_TYPE_SUBSCRIBE 4
#define CMD_TYPE_GETSTATUS 5


typedef struct activator_cmd
{
    int cmd_type;
    int channel;
    unsigned int addr_len;
    struct sockaddr pack_from_addr;
    char data[MAX_CMD];
} activator_cmd;

int parse_cmd(activator_cmd * cmd, char * buf, size_t buf_len);

#endif // activator_cmd_h
