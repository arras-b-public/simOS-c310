CC=gcc
#CFLAGS=-g -O0 -Wall #-DNDEBUG
CFLAGS=-DNDEBUG -Wall -D FRAME_STORE_SIZE=$(framesize) -D VAR_MEM_SIZE=$(varmemsize)

mysh: shell.c interpreter.c shellmemory.c
	$(CC) $(CFLAGS) -c shell.c interpreter.c shellmemory.c pcb.c queue.c schedule_policy.c
	$(CC) $(CFLAGS) -o mysh shell.o interpreter.o shellmemory.o pcb.o queue.o schedule_policy.o -lm

clean: 
	rm mysh; rm *.o
