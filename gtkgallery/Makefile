CC = gcc

CFLAGS = -Wall -Wunused		 	\
	-DG_DISABLE_DEPRECATED 	 	\
	-DGDK_DISABLE_DEPRECATED 	\
	-DGDK_PIXBUF_DISABLE_DEPRECATED \
	-DGTK_DISABLE_DEPRECATED

go: test.c gtkgallery.c
	$(CC) test.c gtkgallery.c -g -o go $(CFLAGS) `pkg-config gtk+-2.0 --cflags --libs`
fixed: test-fixed.c gtkgallery.c
	$(CC) test-fixed.c gtkgallery.c -g -o go $(CFLAGS) `pkg-config gtk+-2.0 --cflags --libs`
clean: 
	rm -f *.o go
