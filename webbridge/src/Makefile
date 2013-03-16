all: wb-main.c
	gcc -g -c wb-main.c -o obj_wb_main.o -I./ `pkg-config libsoup-2.4 --cflags` -I/usr/include/python2.5
	gcc -Xlinker -export-dynamic obj_wb_main.o -o go `pkg-config libsoup-2.4 --libs` -lnsl -ldl -lreadline -ltermcap -lieee -lpthread -lutil -lm /usr/lib/python2.5/config/libpython2.5.a

clean:
	rm go *.o -f
	cd pyroot/fs/; make clean
