MOTIFI = /usr/X11R6/include
MOTIFL = /usr/X11R6/lib

CC = gcc
CFLAGS = $(LIBINC) $(CLDFLAGS) -O2 -Wall -I$(MOTIFI) 
LDFLAGS = $(LIBLNK) $(CLDFLAGS) -L$(MOTIFL) 
XLIBS = -lXm -lXt -lXext -lSM -lICE -lXmu -lX11
LIBS = $(XLIBS) -lm -lxpa


INCS =	protocol.h

OBJS =			\
	pointing.o	\
	protocol.o	\
	algorithms.o	\
	xmtel1.o

all:	xmtel1 

xmtel1: $(INCS) $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm -fr *.o xmtel1
