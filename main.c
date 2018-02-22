#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <linux/serial.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>

#define UDP_PORT 6001
#define MAX_CMD 512

int udpsocket_descriptor = 0;
int g_count = 0;
struct timeval stTv;

typedef struct activator_cmd
{
    int channel;
    char data[MAX_CMD];
} activator_cmd;

int parse_cmd(activator_cmd * cmd, char * buf, size_t buf_len)
{
    cmd->channel=0;
    memset(cmd->data, 0, MAX_CMD );
    int cmd_bytes_written = 0;
    for(int i=0;i<buf_len;i++)
    {
        if(buf[i]>='0' && buf[i]<='9')
        {
            cmd->channel*=10;
            cmd->channel+=buf[i];
            cmd->channel-=48;
        }
        else if(buf[i]==':')
        {
            for(int j=i+1;(j<buf_len)&&(cmd_bytes_written<(MAX_CMD-1));j++)
            {
                cmd->data[cmd_bytes_written] = buf[j];
                cmd_bytes_written++;
            }
            return 0;
        }
        else
        {
            printf("can't parse command:\n%s\n",buf);
            return 1;
        }
    }

    printf("can't parse command_:\n%s\n",buf);
    return 1;
}


int check_socket(int socket_fd)
{
    int error_code;
    unsigned int error_code_size = sizeof(error_code);
    getsockopt(socket_fd, SOL_SOCKET, SO_ERROR, &error_code, &error_code_size);
    return error_code;
}

//TIMER_FUNCTION
void timer_function(int x, short int y, void *pargs)
{
    printf("\n\n%d,%d,**********Count is %d\n\n",x,y,g_count);
    g_count = 0 ;
    event_add((struct event*)pargs,&stTv);
}

void command_recieved_handler(int x, short int y, void *pargs)
{
    unsigned int unFromAddrLen;
    int nByte = 0;
    char aReqBuffer[MAX_CMD];
    struct sockaddr pack_from_addr;

    unFromAddrLen = sizeof(pack_from_addr);

    if ((nByte = recvfrom(x, aReqBuffer, sizeof(aReqBuffer), 0,&pack_from_addr, &unFromAddrLen)) == -1)
    {
        printf("error occured while receiving\n");
    }
    aReqBuffer[nByte] = 0;

    activator_cmd curCmd;
    parse_cmd(&curCmd, aReqBuffer, MAX_CMD);
    printf("channel:%d; cmd:%s\n",curCmd.channel,curCmd.data);

    ///////////////////////
    char ipstr[INET6_ADDRSTRLEN];
    int port;

    // deal with both IPv4 and IPv6:
    if (pack_from_addr.sa_family == AF_INET)
    {
        struct sockaddr_in *s = (struct sockaddr_in *)&pack_from_addr;
        port = ntohs(s->sin_port);
        inet_ntop(AF_INET, &s->sin_addr, ipstr, sizeof ipstr);
    }
    else
    { // AF_INET6
        struct sockaddr_in6 *s = (struct sockaddr_in6 *)&pack_from_addr;
        port = ntohs(s->sin6_port);
        inet_ntop(AF_INET6, &s->sin6_addr, ipstr, sizeof ipstr);
    }

    char buf[512];
    buf[0]='O';
    buf[1]='K';
    buf[2]=0;

    int n = sendto(udpsocket_descriptor, buf, strlen(buf), 0, &pack_from_addr, unFromAddrLen);
    if (n < 0)
    {
        printf("ERROR in sendto");
    }

    printf("Peer IP address: %s:%d\n", ipstr,port);

    g_count++;
}

int main(int argc, char **argv)
{
	DIR *dir;
	struct dirent *entry;
	if((dir = opendir("/sys/class/tty"))!=NULL)
	{
		while((entry = readdir(dir))!=NULL)
		{
			printf("%s\n", entry->d_name);
		}
		closedir(dir);
	}
	else
	{
		perror("CAN NOT READ PORTS AVAILABLE");
		return 1;
	}

	struct event_base *base;

    struct event timer_event;
	struct event command_event;

    struct sockaddr_in local_address;

    base = event_init();
    if ((udpsocket_descriptor = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("ERROR - unable to create listening socket\n");
        exit(-1);
    }

    //Start : Set flags in non-blocking mode
    int nReqFlags = fcntl(udpsocket_descriptor, F_GETFL, 0);
    if (nReqFlags< 0)
    {
        printf("ERROR - cannot set socket options");
    }

    if (fcntl(udpsocket_descriptor, F_SETFL, nReqFlags | O_NONBLOCK) < 0)
    {
        printf("ERROR - cannot set socket options");
    }

    memset(&local_address, 0, sizeof(struct sockaddr_in));

    local_address.sin_addr.s_addr = INADDR_ANY; //listening on local ip
    local_address.sin_port = htons(UDP_PORT);
    local_address.sin_family = AF_INET;


    int nOptVal = 1;
    if (setsockopt(udpsocket_descriptor, SOL_SOCKET, SO_REUSEADDR, (const void *)&nOptVal, sizeof(nOptVal)))
    {
        printf("ERROR - socketOptions: Error at Setsockopt");
    }

    if (bind(udpsocket_descriptor, (struct sockaddr *)&local_address, sizeof(local_address)) != 0)
    {
        printf("Error: Unable to bind the default IP on port:%d\n", UDP_PORT);
        exit(-1);
    }

    event_set(&command_event, udpsocket_descriptor, EV_READ | EV_PERSIST, command_recieved_handler, &command_event);
    event_add(&command_event, NULL);

    /////////////TIMER START///////////////////////////////////
    stTv.tv_sec = 3;
    stTv.tv_usec = 0;
    event_set(&timer_event, -1, EV_TIMEOUT , timer_function, &timer_event);
    event_add(&timer_event, &stTv);
    ////////////TIMER END/////////////////////////////////////
    printf("dispatching...");
    event_base_dispatch(base);

	return 0;
}

