/*
  A trivial static http webserver using Libevent's evhttp.

  This is not the best code in the world, and it does some fairly stupid stuff
  that you would never want to do in a production webserver. Caveat hackor!

 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/util.h>
#include <event2/dns.h>
#include <event2/keyvalq_struct.h>

#ifdef _EVENT_HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif

/* Compatibility for possible missing IPv6 declarations */
#include "util-internal.h"
#include "ne_request.h"
#include "ne_session.h"

#define DD(...) fprintf(stdout, __VA_ARGS__)

static void get_qiyi_req(const char *uri, char *membuf) {
  unsigned int key;
  char qiyiurl[1024] = {0};
  char dest[64] = {0}, *ptr = NULL;
  int ret, tm = 0;
  unsigned int mkey = 2391461978;
  ne_request *req = NULL;

  tm = time(NULL);
  key = mkey ^ tm;

  ptr = strstr(uri, ".com");
  strncpy(dest, uri, ptr-uri+4);
  //DD("dest = %s\n", dest);
  ne_session *s = ne_session_create("http", dest, 80);

  snprintf(qiyiurl, 1024, "/%u/%s", key, ptr+5);
  //DD("qiyiurl = %s\n", qiyiurl);
  req = ne_request_create(s, "GET", qiyiurl);
  ret = ne_begin_request(req);

  if(ret == NE_OK) {
    while((ret = ne_read_response_block(req, membuf, 1024)) > 0)
      DD("req.body = %s, size = %d\n", membuf, ret);

    if(ret == NE_OK)
      ret = ne_end_request(req);
  }

  ne_request_destroy(req);
  ne_session_destroy(s);
}

static void send_document_cb(struct evhttp_request *req, void *arg) {
	const char *uri = evhttp_request_get_uri(req);
	struct evhttp_uri *decoded = NULL;
	const char *path;
	char *qiyivideo = NULL;
  char membuf[1024] = {0};

	if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
    DD("evhttp_request_get_command != EVHTTP_REQ_GET\n");
		return;
	}

	/* Decode the URI */
	decoded = evhttp_uri_parse(uri);
	if (!decoded) {
		printf("It's not a good URI. Sending BADREQUEST\n");
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
    goto err;
	}

	path = evhttp_uri_get_path(decoded);
	if (!path) path = "/";
  DD("path = %s, query = %s\n", path, evhttp_uri_get_query(decoded));

  get_qiyi_req(evhttp_uri_get_query(decoded), membuf);
  qiyivideo = strstr(membuf, "http");
  if(qiyivideo != NULL) {
    char *end = strchr(qiyivideo, '"');
    *end = '\0';
  }
  DD("real video uri: %s\n", qiyivideo);

	evhttp_add_header(evhttp_request_get_output_headers(req), "Location", qiyivideo);
	evhttp_add_header(evhttp_request_get_output_headers(req), "Content-Type", "text/html");
	evhttp_send_reply(req, 301, "Moved Permanently", NULL);

err:
	if (decoded)
		evhttp_uri_free(decoded);
}

int main(int argc, char **argv) {
	struct event_base *base;
	struct evhttp *http;
	struct evhttp_bound_socket *handle;

	unsigned short port = 8181;

	base = event_base_new();
	if (!base) {
		fprintf(stderr, "Couldn't create an event_base: exiting\n");
		return 1;
	}

	/* Create a new evhttp object to handle requests. */
	http = evhttp_new(base);
	if (!http) {
		fprintf(stderr, "couldn't create evhttp. Exiting.\n");
		return 1;
	}

	/* We want to accept arbitrary requests, so we need to set a "generic"
	 * cb.  We can also add callbacks for specific paths. */
	evhttp_set_gencb(http, send_document_cb, base);

	/* Now we tell the evhttp what port to listen on */
	handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", port);
	if (!handle) {
		fprintf(stderr, "couldn't bind to port %d. Exiting.\n",
		    (int)port);
		return 1;
	}
	{
		/* Extract and display the address we're listening on. */
		struct sockaddr_storage ss;
		evutil_socket_t fd;
		ev_socklen_t socklen = sizeof(ss);
		char addrbuf[128];
		void *inaddr;
		const char *addr;
		int got_port = -1;
		fd = evhttp_bound_socket_get_fd(handle);
		memset(&ss, 0, sizeof(ss));
		if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
			perror("getsockname() failed");
			return 1;
		}
		if (ss.ss_family == AF_INET) {
			got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
			inaddr = &((struct sockaddr_in*)&ss)->sin_addr;
		} else if (ss.ss_family == AF_INET6) {
			got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
			inaddr = &((struct sockaddr_in6*)&ss)->sin6_addr;
		} else {
			fprintf(stderr, "Weird address family %d\n",
			    ss.ss_family);
			return 1;
		}
		addr = evutil_inet_ntop(ss.ss_family, inaddr, addrbuf,
		    sizeof(addrbuf));
		if (addr) {
			printf("Listening on %s:%d\n", addr, got_port);
			//evutil_snprintf(uri_root, sizeof(uri_root), "http://%s:%d",addr,got_port);
		} else {
			fprintf(stderr, "evutil_inet_ntop failed\n");
			return 1;
		}
	}

	event_base_dispatch(base);

	return 0;
}
