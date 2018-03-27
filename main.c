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
#include <pthread.h>
#include <termios.h>

#include "diaequeue.h"
#include "diaedevice.h"


#define UDP_PORT 6001
#define MAX_CMD 512
#define MAX_CHANNELS_SUPPORTED 12

int udpsocket_descriptor = 0;
int g_count = 0;
struct timeval stTv;

diaedevice * devices[MAX_CHANNELS_SUPPORTED];
char device_checked [MAX_CHANNELS_SUPPORTED];

pthread_t physical_ports_thread;
pthread_t check_pendingdevice_queue_thread;
pthread_t process_command_queue_loop_thread;

diae_queue *pending_devices;
diae_queue *pending_commands;

typedef struct activator_cmd
{
    int channel;
    unsigned int addr_len;
    struct sockaddr pack_from_addr;
    char data[MAX_CMD];
} activator_cmd;

int parse_cmd(activator_cmd * cmd, char * buf, size_t buf_len)
{
    cmd->channel=0;
    memset(cmd->data, 0, MAX_CMD );
    int cmd_bytes_written = 0;
    for(int i=0;i<buf_len;i++)
    {
        if(buf[i]>='1' && buf[i]<='9')
        {
            cmd->channel*=10;
            cmd->channel+=buf[i];
            cmd->channel-='1';//'1' is the first possible channel
        }
        else if(buf[i]==':')
        {
            for(int j=i+1;(j<(buf_len-1))&&(cmd_bytes_written<(MAX_CMD-1));j++)
            {
                cmd->data[cmd_bytes_written] = buf[j];
                if(buf[j]==';')
                {
                    cmd->data[cmd_bytes_written+1]=0;
                    return 0;
                }
                if(buf[j]==0)
                {
                     printf("zero terminated command (instead of ;)\n");
                    return 1;
                }
                cmd_bytes_written++;
            }
            printf("not terminated command\n");
            return 1;
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
    //printf("\n%d,%d,**********Count is %d\n",x,y,g_count);
    printf("-------\n");
    for(int i=0;i<MAX_CHANNELS_SUPPORTED;i++)
    {
        if(devices[i]!=NULL)
        {
            printf("%s is on %d channel, \n", devices[i]->port_name, i+1);
            write_to_port(devices[i], "$", 1);

            int N = read_port_byte(devices[i]);
            if(N>0)
            {
                devices[i]->attempts = 5;
                printf("recieved:%s\n",devices[i]->buf);
            }
            else
            {
                devices[i]->attempts--;
                if(devices[i]->attempts<=0)
                {
                    diaedevice * dev_to_del = devices[i];
                    devices[i]=NULL;
                    printf("dev %s on channel %d is not responding and will be destroyed\n", dev_to_del->port_name, i+1);
                    close_device(devices[i]);
                }
            }
        }
    }
    printf("\n");
    g_count = 0 ;
    event_add((struct event*)pargs,&stTv);
}


int process_command_queue()
{
    if(is_diae_queue_empty(pending_commands))
    {
        //printf("cmd queue empty\n");
        return 1;
    }
    activator_cmd * curCmd = (activator_cmd *)dequeue(pending_commands);
    if(curCmd == NULL)
    {
        printf("empty command in line \n ");
        return 0;
    }
    printf("sending_cmd_to_box to %d, total bytes:%zu\n",curCmd->channel,strlen(curCmd->data));
    const char * buf = "-";

    if(devices[curCmd->channel]!=NULL)
    {
        buf="+";
        write_to_port(devices[curCmd->channel], curCmd->data, strlen(curCmd->data));
    }
    else
    {
        printf("device is not found :(\n");
    }

    int n = sendto(udpsocket_descriptor, buf, strlen(buf), 0, &curCmd->pack_from_addr, curCmd->addr_len);
    if (n < 0)
    {
        printf("ERROR in sendto\n");
    }
    free(curCmd);
    return 0;
}

void * process_command_queue_loop()
{
    while(1)
    {
        //printf("\\O/\n");
        //printf( " |\n");
        //printf( "/ \\\n");
        int K = process_command_queue();
        sleep(K);
    }
    return NULL;
}

void command_recieved_handler(int x, short int y, void *pargs)
{
    printf("---->\n");
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

    activator_cmd * curCmd = (activator_cmd *)malloc(sizeof(activator_cmd));
    printf("size_of act_cmd:%zu\n",sizeof(activator_cmd));
    memset(curCmd, 0, sizeof(activator_cmd));

    int err = parse_cmd(curCmd, aReqBuffer, MAX_CMD);
    if(!err)
    {
        printf("channel:%d; cmd:%s\n",curCmd->channel,curCmd->data);
        curCmd->addr_len = unFromAddrLen;
        memcpy(&curCmd->pack_from_addr, &pack_from_addr, unFromAddrLen );

        enqueue(pending_commands, curCmd);
    }
    else
    {
        printf("ERR: bad command not added to the queue\n");
        char buf[3];
        buf[0]='?';
        buf[1]=0;
        int n = sendto(udpsocket_descriptor, buf, strlen(buf), 0, &pack_from_addr, unFromAddrLen);
        if (n < 0)
        {
            printf("ERROR in sendto\n");
        }
    }
}


int process_pending_queue()
{
    //printf("cur cnt:%d\n", get_elements_count(pending_devices));
    //check_queue(pending_devices, 0);
    lock_queue(pending_devices);
    diae_queue_block * curblock = pending_devices->start_block;
    if(pending_devices->start_block !=NULL && pending_devices->end_block == NULL)
    {
        printf("queue damages in pending devices...\n");
    }
    while(curblock!=NULL)
    {
        diaedevice * curDev = (diaedevice *)curblock->data;
        if(curDev == NULL)
        {
            unlock_queue(pending_devices);
            delete_from_queue(pending_devices, curblock);
            return 1;
        }
        if(curDev->attempts<=0)
        {
            unlock_queue(pending_devices);
            printf("too many attempts... device %s is going to be ignored for a while\n", curDev->port_name);
            delete_from_queue(pending_devices, curblock);
            close_device(curDev);


            return 1;
        }

        write_to_port(curDev, "g", (size_t)1);
        //we must get answer like: c1\n
        int res = read_port_byte(curDev);

        int channel = curDev->buf[0]-'1';//'1' is the first channel allowed
        if(res<=0)
        {
            printf("err: device %s is not answering\n", curDev->port_name);
            curDev->attempts--;
            curblock = curblock->next;
            continue;
        }
        if(channel<MAX_CHANNELS_SUPPORTED && channel>=0)
        {
            printf("device %s channel is %i\n", curDev->port_name, channel+1);
            if(devices[channel]!=NULL)
            {
                diaedevice *aux = devices[channel];
                devices[channel] = curDev;
                close_device(aux);
            }
            else
            {
                devices[channel] = curDev;
            }
            curDev->attempts = 5;
            unlock_queue(pending_devices);
            delete_from_queue(pending_devices, curblock);
            return 1;
        }


        //if(channel<MAX_CHANNELS_SUPPORTED && channel>=0)
        //{
        //    if(devices[channel]!=NULL)
        //    {
        //        close_device(devices[channel]);
        //        devices[channel]=NULL;
        //    }
        //    devices[channel] = dev;
        //}
        curblock = curblock->next;
    }
    unlock_queue(pending_devices);
    return 0;
}

void * process_pending_queue_loop()
{
    while(1)
    {

        int res = process_pending_queue();
        if(res)
        {
            sleep(5);
        }
        sleep(1);
    }
}
//check connected devices
int check_connected_devices()
{
    printf("\n\nch_dev_con;\n\n\n");
    memset(device_checked, 0, sizeof(device_checked));//not checked at the start
    //let's read devices available
	struct dirent *entry;
	DIR * dir;
	if((dir = opendir("/dev"))!=NULL)
	{
		while((entry = readdir(dir))!=NULL)
		{
		    if(strstr(entry->d_name,"ttyUSB"))
            {
                printf("dev:%s",entry->d_name);
                char already_in_the_list = 0;
                for(int i=0;i<MAX_CHANNELS_SUPPORTED;i++)
                {
                    if(devices[i]!=NULL)
                    {
                        if(!strcmp(entry->d_name,devices[i]->port_name))
                        {
                            printf(" already in the list: %d\n", i);
                            device_checked[i] = 1;
                            already_in_the_list = 1;
                        }
                    }
                }
                printf("\n");

                if(!is_diae_queue_empty(pending_devices))
                {
                    lock_queue(pending_devices);
                    printf("\nloop through queue...\n");
                    diae_queue_block * curblock = pending_devices->start_block;
                    while(curblock!=NULL)
                    {
                        printf("block:");
                        diaedevice * curDev = (diaedevice *)curblock->data;
                        printf("port %s\n",curDev->port_name);
                        if(strcmp(curDev->port_name, entry->d_name)==0) //means equal
                        {
                            printf("%s is already in the queue \n", entry->d_name);
                            already_in_the_list = 1;
                            break;
                        }
                        curblock = curblock->next;
                    }
                    unlock_queue(pending_devices);
                }

                if(!already_in_the_list)
                {
                    diaedevice * dev = create_device(entry->d_name);
                    if(!open_port(dev))//no errors
                    {
                        enqueue(pending_devices, dev);

                        printf("dev %s is properly added to the queue\n", dev->connection_string);
                    }
                }
            }
		}
///////////////////
        for(int i=0;i<MAX_CHANNELS_SUPPORTED;i++)
        {
            if(devices[i]!=NULL&&!device_checked[i])
            {
                diaedevice * dev_to_del = devices[i];
                devices[i] = NULL;
                printf("dev %s on channel %d is not in the list of available devices and will be destroyed\n", dev_to_del->port_name, i+1);
                close_device(dev_to_del);
            }
        }
////////////////

		closedir(dir);
	}
	else
	{
	    printf("ERR!!: CANT read available ports\n");
		perror("CAN NOT READ PORTS AVAILABLE");
		return 1;
	}
	return 0;
}


void * check_connected_devices_loop()
{
    while(1)
    {
        check_connected_devices();
        sleep(5);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    pending_devices = create_queue("devices");
    pending_commands = create_queue("commands");

    //CHECK CONNECTED DEVICES
    int err = pthread_create(&physical_ports_thread, NULL, &check_connected_devices_loop, NULL);

    if (err != 0)
    {
        printf("\ncan't create thread :[%s]", strerror(err));
    }

    //CHECK PENDING DEVICES QUEUE
    err = pthread_create(&check_pendingdevice_queue_thread, NULL, &process_pending_queue_loop, NULL);
    if (err != 0)
    {
        printf("\ncan't create queue processing thread :[%s]", strerror(err));
    }

    //SEND COMANDS QUEUE
    err = pthread_create(&process_command_queue_loop_thread, NULL, &process_command_queue_loop, NULL);
    if (err != 0)
    {
        printf("\ncan't create command processing thread :[%s]", strerror(err));
    }

    for(int i=0;i<MAX_CHANNELS_SUPPORTED;i++)
    {
        devices[i]=NULL;
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
    printf("dispatching...\n\n");
    event_base_dispatch(base);

	return 0;
}
