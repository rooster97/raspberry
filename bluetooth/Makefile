# makefile

SRCS	= $(wildcard *.c)
OBJECTS = $(SRCS:.c=.o)

CFLAGS = -g
LIBS = -lbluetooth -lpthread -lwiringPi

all: bluetooth

bluetooth : $(OBJECTS)
	$(CC) $(CFLAGS) $(LIBS) -o $@ $^

clean :
	rm -f *.o bluetooth 
