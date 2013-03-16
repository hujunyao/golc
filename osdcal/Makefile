CC=gcc
CALENDAR=osdcal

DEBUG_CFLAGS=-DOSDCAL_DEBUG

#GPROF_FLAG = -pg
# sdl
OBJS=osdcal-main.o

CFLAGS= -I/usr/include/freetype2 $(DEBUG_CFLAGS)
LDFLAGS= -L/usr/lib -lX11 -lXext -lXft

all:$(CALENDAR)

$(CALENDAR): $(OBJS)
	$(CC) $(LDFLAGS) -o $(CALENDAR) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm $(CALENDAR) *.o *.txt *.bak *.out -f
