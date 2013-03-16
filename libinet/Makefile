all: inet-test.c inet-common.c inet-types.h
	gcc -g -o test inet-test.c -I/include -L/lib

bnep:
	gcc -o bnepup bnepc.c
	gcc -o bnepdown bnepc.c
	gcc -o bnepd bnepd.c -I /usr/include `pkg-config glib-2.0 --cflags --libs`
clean:
	rm *.o -f
	rm bnepup bnepdown bnepd
