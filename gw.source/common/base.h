#ifndef __GW_BASE_H__
#define __GW_BASE_H__

#include <event.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define GWAPPIPC "gw.ipc.app"
#define PROTOIPC "gw.ipc.proto"
#define ZNETIPC "gw.ipc.znet"

evutil_socket_t gw_ipcsock_new();
evutil_socket_t gw_udp_new();
evutil_socket_t gw_tcp_new();

void gw_udp_connect(evutil_socket_t sock, const char *addr, int port);
void gw_udp_bind(evutil_socket_t sock, int port);

#endif
