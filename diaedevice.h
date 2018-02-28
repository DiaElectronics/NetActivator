#ifndef DIAEDEVICE_H
#define DIAEDEVICE_H

#define CONN_TYPE_NON 0
#define CONN_TYPE_USB 1
#define CONN_TYPE_ETH 2

#define DEV_TYPE_KEYBOARD 1
#define DEV_TYPE_MONEY_ACCEPTOR 2
#define DEV_TYPE_RELAY_ACTIVATOR 4
#define DEV_TYPE_SENSORS 8

#define MAX_CONNECTION_STRING 512

#include <stddef.h>
//.. many others expected in the future

typedef struct diaedevice
{
    int attempts;
    int handler;
    int isOpened;
    int connection_type;
    short int device_type;
    char buf[MAX_CONNECTION_STRING];
    int bytes_read;
    char port_name[MAX_CONNECTION_STRING];
    char connection_string[MAX_CONNECTION_STRING]; //like com port name or IP
} diaedevice;

diaedevice * create_device();
int close_device(diaedevice * currentDevice);
int read_port_byte(diaedevice * currentDevice);
int open_port(diaedevice * currentDevice);
int write_to_port(diaedevice * currentDevice, char * buf, size_t len);


#endif // DIAEDEVICE_H
