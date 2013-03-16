CC=gcc
APP=labar

#GPROF_FLAG = -pg
# sdl
OBJS=labar-main.o labar-common.o

EXTRA_CFLAGS = `pkg-config glib-2.0 gdk-pixbuf-2.0 gdk-pixbuf-xlib-2.0 --cflags`
CFLAGS= -I/usr/include -I/usr/include/freetype2  $(EXTRA_CFLAGS)

EXTRA_LDFLAGS = `pkg-config glib-2.0 gdk-pixbuf-2.0 gdk-pixbuf-xlib-2.0 --libs`
LDFLAGS= -L/usr/X11R6/lib -lX11 -lXext -lXft $(EXTRA_LDFLAGS)

all:$(APP)

$(APP): $(OBJS)
	$(CC) $(LDFLAGS) -g -o $(APP) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -g -c $< -o $@

clean:
	rm $(APP) *.o *.txt *.bak *.out -f
