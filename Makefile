CC=gcc
CFLAGS=-lpthread -levent -I . 
release: 
	$(CC) -o actnet  main.c diaqueue.c diadevice.c activator_cmd.c  $(CFLAGS)
debug:
	$(CC) -o actnet.debug main.c diaqueue.c diadevice.c activator_cmd.c $(CFLAGS) -g
