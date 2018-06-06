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
#include "errors.h"
#include "activator_cmd.h"

#define DIAE_NO_WAIT 0
#define DIAE_WAIT 1
#define DIAE_WAIT_MS 100

#define UDP_PORT 6001


int udpsocket_descriptor = 0;
int g_count = 0;
long int cmd_served = 0;

pthread_mutex_t subscribers_lock;
struct timeval stTv;

diaedevice * devices[MAX_CHANNELS_SUPPORTED];
char device_checked [MAX_CHANNELS_SUPPORTED];

char last_statuses[MAX_CHANNELS_SUPPORTED*MAX_CMD];

activator_cmd * subscribers[MAX_CHANNELS_SUPPORTED];

pthread_t physical_ports_thread;
pthread_t check_pendingdevice_queue_thread;
pthread_t process_command_queue_loop_thread;

diae_queue *pending_devices;
diae_queue *pending_commands;

void UpdateSubscriberIfNull(activator_cmd * cmd);
void UpdateSubscriber(activator_cmd * cmd);

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
            printf("%s is on %d channel, \n", devices[i]->port_name, i);
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

#define MAX_HEADER 100
int process_command_queue()
{
    char answerbuf[MAX_CMD+MAX_HEADER];
    memset(answerbuf, 0, sizeof(answerbuf));
    if(is_diae_queue_empty(pending_commands))
    {
        //printf("cmd queue empty\n");
        return DIAE_QUEUE_WAIT;
    }
    activator_cmd * curCmd = (activator_cmd *)dequeue(pending_commands);

    cmd_served++;
    if(curCmd == NULL)
    {
        printf("empty command in line \n ");
        return DIAE_QUEUE_NOWAIT;
    }
    //printf("sending_cmd_to_box to %d, total bytes:%zu\n",curCmd->channel,strlen(curCmd->data));
    const char * buf = "-";

    UpdateSubscriberIfNull(curCmd);

    if(curCmd->cmd_type == CMD_TYPE_ACTIVATE)
    {
        printf("sending_cmd_to_box to %d, type=%d, data='%s' total bytes:%zu\n",curCmd->channel, curCmd->cmd_type,  curCmd->data, strlen(curCmd->data));
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
    }
    else if (curCmd->cmd_type == CMD_TYPE_PING || curCmd->cmd_type == CMD_TYPE_SUBSCRIBE)
    {
        if(curCmd->cmd_type == CMD_TYPE_SUBSCRIBE)
        {
            UpdateSubscriber(curCmd);
        }

        if(devices[curCmd->channel]!=NULL)
        {
            strcpy(answerbuf,"P+:");
        }
        else
        {
            strcpy(answerbuf,"P-:");
        }
        sprintf(&answerbuf[strlen(answerbuf)], "%ld;", cmd_served);

        snprintf(&last_statuses[MAX_CMD*curCmd->channel], MAX_CMD, "%s", curCmd->data);

        last_statuses[MAX_CMD*curCmd->channel+511]=0;

        int n = sendto(udpsocket_descriptor, answerbuf, strlen(answerbuf), 0, &curCmd->pack_from_addr, curCmd->addr_len);
        if (n < 0)
        {
            printf("ERROR in sendto\n");
        }
        printf("sending_cmd_to_box to %d, type=%d, answer='%s' total bytes:%zu\n",curCmd->channel, curCmd->cmd_type,  answerbuf, strlen(curCmd->data));
     }
     else if (curCmd->cmd_type == CMD_TYPE_GETSTATUS)
     {
        strcpy(answerbuf,"S:");

        snprintf(&answerbuf[strlen(answerbuf)], MAX_CMD - 1, "%s", &last_statuses[MAX_CMD * curCmd->channel] );
         int n = sendto(udpsocket_descriptor, answerbuf, strlen(answerbuf), 0, &curCmd->pack_from_addr, curCmd->addr_len);
         if (n < 0)
         {
            printf("ERROR in sendto orig token\n");
         }
        //answerbur[strlen(answerbuf)]
     }
     else if (curCmd->cmd_type == CMD_TYPE_TRANSFER)
     {
         activator_cmd * lastCmd = subscribers[curCmd->channel];
         if(lastCmd!=NULL)
         {
             strcpy(answerbuf, "T+");
         }
         else
         {
             strcpy(answerbuf, "T-");
         }
         int n = sendto(udpsocket_descriptor, answerbuf, strlen(answerbuf), 0, &curCmd->pack_from_addr, curCmd->addr_len);
         if (n < 0)
         {
            printf("ERROR in sendto orig token\n");
         }

         n = sendto(udpsocket_descriptor, &curCmd->data, strlen(curCmd->data), 0, &lastCmd->pack_from_addr, lastCmd->addr_len);
         if (n < 0)
         {
            printf("ERROR in sendto transfered device\n");
         }
     }
     else
     {
        int n = sendto(udpsocket_descriptor, buf, strlen(buf), 0, &curCmd->pack_from_addr, curCmd->addr_len);
        if (n < 0)
        {
            printf("ERROR in sendto\n");
        }
     }


    free(curCmd);
    return DIAE_QUEUE_NOWAIT;
}

void * process_command_queue_loop()
{
    while(1)
    {
        //printf("\\O/\n");
        //printf( " |\n");
        //printf( "/ \\\n");
        int K = process_command_queue();
        if(K=DIAE_WAIT)
        {
            usleep(100000);
        }
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
        printf("error occurred while receiving\n");
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

        int channel = curDev->buf[0]-'0';//'0' is the first channel allowed
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
		    if(strstr(entry->d_name,"ttyUSB")||strstr(entry->d_name,"ttyACM"))
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
void UpdateSubscriberIfNull(activator_cmd * cmd)
{
    if(cmd!=NULL)
    {
        pthread_mutex_lock(&subscribers_lock);
        if(cmd->channel>=0 && cmd->channel<=MAX_CHANNELS_SUPPORTED)
        {
            if(subscribers[cmd->channel]==NULL)
            {
                subscribers[cmd->channel] = malloc(sizeof(activator_cmd));
                memcpy(subscribers[cmd->channel], cmd, sizeof(activator_cmd));
            }
        }
        pthread_mutex_unlock(&subscribers_lock);
    }
}
void UpdateSubscriber(activator_cmd * cmd)
{
    if(cmd!=NULL)
    {
        pthread_mutex_lock(&subscribers_lock);
        if(cmd->channel>=0 && cmd->channel<=MAX_CHANNELS_SUPPORTED)
        {
            if(subscribers[cmd->channel]!=NULL)
            {
                free(subscribers[cmd->channel]);
                subscribers[cmd->channel] = NULL;
            }
            subscribers[cmd->channel] = malloc(sizeof(activator_cmd));
            memcpy(subscribers[cmd->channel], cmd, sizeof(activator_cmd));
        }
        pthread_mutex_unlock(&subscribers_lock);
    }
}

int main(int argc, char **argv)
{
    memset(last_statuses,0, sizeof(last_statuses));
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
        subscribers[i]=NULL;
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
