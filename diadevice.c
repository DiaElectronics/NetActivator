#include "diaedevice.h"

#include<stdio.h>
#include<stdlib.h>
#include<termios.h>
#include<unistd.h>
#include<fcntl.h>
#include<string.h>

diaedevice * create_device(char * ttyPortName)
{
    diaedevice * result = (diaedevice*)malloc(sizeof(diaedevice));
    memset(result->buf,0,sizeof(result->buf));
    memset(result->port_name,0, sizeof(result->port_name));

    memset(result->connection_string, 0, sizeof(result->connection_string));
    strcpy(result->connection_string, "/dev/");
    result->bytes_read = 0;
    strncpy(result->port_name, ttyPortName, (size_t)(MAX_CONNECTION_STRING));
    strncpy(&result->connection_string[5],ttyPortName, (size_t)(MAX_CONNECTION_STRING-6));
    return result;
}

int close_device(diaedevice * currentDevice)
{
    if(currentDevice == NULL || currentDevice->connection_string == NULL) return 1;
    close(currentDevice->handler);
    free(currentDevice);
    return 0;
}

int read_port_byte(diaedevice * currentDevice)
{
    int n = read(currentDevice->handler, currentDevice->buf, sizeof(currentDevice->buf));
    if(n <= 0)
    {

        return -1;
    }
    else
    {
        currentDevice->buf[n] = 0;
        currentDevice->bytes_read = n;
        printf("%d bytes returned\n",n);
        return n;
    }
}


int open_port(diaedevice * currentDevice)
{
    currentDevice->attempts = 20;
    currentDevice->isOpened = 0;
    currentDevice->handler = -1;

    if(currentDevice == NULL || currentDevice->connection_string == NULL) return 1;

    int fd; /* File descriptor for the port */
    fd = open(currentDevice->connection_string, O_RDWR | O_NOCTTY | O_NDELAY);


    if (fd == -1)
    {
        perror("open_port: Unable to open port: ");
        return 1;
    }
    else
    {
        struct termios options;

        tcgetattr(fd, &options);
        cfsetospeed(&options, B9600);
        cfsetispeed(&options, B9600);
        /* set raw input, 1 second timeout */
        options.c_cflag     |= (CLOCAL | CREAD);
        options.c_lflag     &= ~(ICANON | ECHO | ECHOE | ISIG);
        options.c_oflag     &= ~OPOST;
        options.c_cc[VMIN]  = 0;
        options.c_cc[VTIME] = 10;

        /* set the options */
        tcsetattr(fd, TCSANOW, &options);

        fcntl(fd, F_SETFL, FNDELAY);
    }

    currentDevice->handler=fd;
    currentDevice->isOpened=1;


    return 0;
}

int write_to_port(diaedevice * currentDevice, char * buf, size_t len)
{

    if(currentDevice == NULL||currentDevice->connection_string==NULL)
    {
        perror("NULL device or port name passed\n");
        return -1;
    }
    //printf("wrt_to:%s\n", currentDevice->port_name);
    if(currentDevice->isOpened)
    {
        size_t res = write(currentDevice->handler, buf, len);
        if(res<0)
        {
            currentDevice->isOpened = 0;
            printf("can't write\n");
            return -1;
        }
        if(res<len)
        {
            printf("can't write all\n");
            return -1;
        }
        return 0;
    }
    else
    {
        printf("dev_err_write:%s\n",currentDevice->port_name);
        return -1;
    }
}

