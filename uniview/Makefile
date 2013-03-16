all: parse-vid.c
	gcc -std=gnu99 -o pvcmd parse-vid.c `pkg-config libcurl json --cflags --libs`

clean:
	rm *.o -f
