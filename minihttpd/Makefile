CC = gcc

LDFLAGS = -lpthread
CFLAGS = -Wall -Wunused  -I ./ 

SRC = bufferevent.c bufferevent_pair.c epoll_sub.c  evthread_pthread.c poll.c strlcpy.c buffer.c bufferevent_filter.c bufferevent_ratelim.c event_tagging.c  evrpc.c  http.c listener.c  select.c  bufferevent_sock.c  epoll.c event.c evmap.c  evthread.c  evutil.c minihttpd.c  log.c  signal.c ne_alloc.c  ne_request.c  ne_session.c  ne_socket.c  ne_string.c  ne_utils.c ne_uri.c ne_dates.c

go: $(SRC)
	$(CC) -g -o go $(CFLAGS) $(LDFLAGS) $(SRC) 
clean: 
	rm -f *.o go
