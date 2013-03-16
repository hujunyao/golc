#include "base.h"

#define GTAG "GW.BASE"
#include "log.h"

evutil_socket_t gw_ipcsock_new() {
  return socket(AF_UNIX, SOCK_DGRAM, 0);
}

evutil_socket_t gw_udp_new() {
	return socket(AF_INET, SOCK_DGRAM, 0);
}

evutil_socket_t gw_tcp_new() {
	return socket(AF_INET, SOCK_STREAM, 0);
}

void gw_udp_connect(evutil_socket_t sock, const char *addr, int port) {
	struct sockaddr_in sin;
	int r;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	evutil_inet_pton(AF_INET, addr, &sin.sin_addr);
	r = connect(sock, (struct sockaddr*)&sin, sizeof(sin));
	if(r < 0) {
		EE("udp connect %s:%d failed.\n", addr, port);
	}
}

void gw_udp_bind(evutil_socket_t sock, int port) {
	struct sockaddr_in sin;
	int r;

	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = INADDR_ANY;

	r = bind(sock, (struct sockaddr *)&sin, sizeof(sin));
	if(r < 0) {
		EE("udp bind port %d failed.\n", port);
	}
}
