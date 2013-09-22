#include "oc_sys_log.h"
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <linux/socket.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/if_ether.h>
#include <fcntl.h>
#include <netpacket/packet.h>
#include <linux/if_ether.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>
#include "reset_socket.h"
#include "oc_conntrac_parser.h"

#define TCPHDR	 sizeof(struct tcphdr)
#define IPHDR	 sizeof(struct iphdr)
#define PACKETSIZE  TCPHDR + IPHDR

#define LOG_HEADER  "ResetSocket: "

#define TRUE 1
#define FALSE 0


struct rpack{
	struct iphdr ip;
	struct tcphdr tcp;
	char space[8192];
};

struct tpack{
	struct iphdr ip;
	struct tcphdr tcp;
};

// pseudo header 4 the checksum
struct pseudo_header{		
	unsigned source_address;
	unsigned dest_address;
	unsigned char placeholder;
	unsigned char protocol;
	unsigned short tcp_length;
	struct tcphdr tcp;
};

struct ifreq ifreq[32];
//struct ifconf ifconf;

/****************************************************************************
 * Get the IP address of the next hop device for the specified IP (or
 * 0.0.0.0 if there isnt one - ie we are directly connected). And the local
 * interface that we should use to get there.
 */

int getnexthop(in_addr_t ip, char *intf, in_addr_t *gateway)
{
	FILE *id = fopen( "/proc/net/route", "r" );
	char buff[500];
	char iface[32];
	in_addr_t dest_ip, gateway_ip;
	unsigned long mask;
	int flags, refcnt, use, metric, mtu, window, irtt;

	if ( id == NULL ) return FALSE;

	while (fgets(buff, sizeof(buff), id) != NULL) {

		memset(iface, 0, sizeof(iface));
		dest_ip = gateway_ip = flags = refcnt = use
			= metric = mask = mtu = window = irtt = -1;

		if (sscanf(buff, "%s %8lx %8lx %4x %d %d %d %8lx %d %d %d",
			iface, (unsigned long *)&dest_ip, (unsigned long *)&gateway_ip, &flags, &refcnt,
			&use, &metric, &mask, &mtu, &window, &irtt
		) == 11) {
			dest_ip = dest_ip;
			gateway_ip = gateway_ip;
			mask = mask;
			if (
				iface[0] != '*' &&		// not a rejected interface
				(flags & 0x0001) &&		// route is UP
				(flags & 0x0200) == 0 &&	// not a "reject"
				(ip & mask) == dest_ip		// IP match
			) {
				strcpy(intf, iface);
				*gateway = gateway_ip;
				fclose(id);
				return TRUE;
			}
		}
	}
	fclose(id);
	return FALSE;
}

/****************************************************************************
 * Get the MAC address (in binary format) for a neighbouring IP
 */

int getmac(in_addr_t ip, unsigned char *mac)
{
	FILE *id = fopen( "/proc/net/arp", "r" );
	// union { unsigned char c[4]; in_addr_t n; } ipu;
	in_addr_t ipn;
	int mac0, mac1, mac2, mac3, mac4, mac5;
	int hwtype, flags;
	char dev[32], mask[32], arpip[32];
	char buff[200];

	if (id == NULL) return FALSE;
	while (fgets(buff, sizeof(buff), id) != NULL) {
		if (sscanf(buff, "%s 0x%x 0x%x %x:%x:%x:%x:%x:%x %s %s",
			arpip,
			&hwtype, &flags,
			&mac0,&mac1,&mac2,&mac3,&mac4,&mac5,
			mask,dev
		) == 11 ) {
			ipn = inet_addr(arpip);
			if (ipn == ip) {
				mac[0]=mac0; mac[1]=mac1; mac[2]=mac2;
				mac[3]=mac3; mac[4]=mac4; mac[5]=mac5;
				fclose(id);
				return TRUE;
			}
		}
	}
	fclose(id);
	return FALSE;
}

unsigned short in_cksum(unsigned short *ptr,int nbytes){

	register long sum;	// assumes long == 32 bits
	u_short oddbyte;
	register u_short answer;	// assumes u_short == 16 bits

	sum = 0;
	while (nbytes > 1)  {
		sum += *ptr++;
		nbytes -= 2;
	}

	if (nbytes == 1) {
		oddbyte = 0;				 // make sure top half is zero
		*((u_char *) &oddbyte) = *(u_char *)ptr; // one byte only
		sum += oddbyte;
	}

	sum  = (sum >> 16) + (sum & 0xffff);	// add high-16 to low-16
	sum += (sum >> 16);			// add carry
	answer = ~sum;				// ones-complement, then truncate to 16 bits
	return(answer);
}


int open_raw_ip_socket()
{
    int one = 1;
    int *oneptr = &one;
	int fd = socket(PF_INET, SOCK_RAW, IPPROTO_RAW);
    if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, oneptr, sizeof(one)) == -1) {
    	OC_SYS_LOG_E(OC_ERR_FAIL, "set IP_HDRINCL failed: %s", strerror(errno));
    	return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, oneptr, sizeof(one)) == -1)
    {
    	OC_SYS_LOG_E(OC_ERR_FAIL, "set SO_BROADCAST failed: %s", strerror(errno));
        return (-1);
    }
    return fd;
}

int open_raw_packet_socket(
	in_addr_t to_ip,
	int *ret_ifindex,
	unsigned char* ret_mac)
{
	int raw_sock;
	in_addr_t gateway_ip;
	struct sockaddr_ll myaddr;
	struct ifreq ifr;
	int ifindex;
	unsigned char mac[6];
	char interf[80];

	/*
	 * Open a PACKET (layer 2) socket.
	 * PF_PACKET: This family allows us to send/receive packets directly to/from the network interface.
	 * SOCK_DGRAM: Lets to the kernel add/remove Ethernet level headers. 
	 * ETH_P_IP: Allows capture of the IP-suite protocols (e.g., TCP, UDP, ICMP, raw IP and so on).
	 */
	if ((raw_sock = socket(PF_PACKET, SOCK_DGRAM, htons(ETH_P_IP))) == -1) {
		OC_SYS_LOG_E(OC_ERR_FAIL, "Unable to open raw socket");
		perror("open packet socket");
		return FALSE;
	}

	if (!getnexthop(to_ip, interf, &gateway_ip)) {
		OC_SYS_LOG_E(OC_ERR_FAIL, "Cant find next hop gateway in routing table");
		close(raw_sock);
		return FALSE;
	}

	/*
	 * Determine the ifindex of the interface we are going to use with
	 * our layer 2 comms.
	 */

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, interf, sizeof(ifr.ifr_name));
	if (ioctl(raw_sock, SIOCGIFINDEX, &ifr) == -1) {
		OC_SYS_LOG_E(OC_ERR_FAIL, "Unable to determine interface");
		perror("ioctl - get ifindex");
		close(raw_sock);
		return FALSE;
	}
	ifindex = ifr.ifr_ifindex;

	if (!getmac( gateway_ip == 0 ? to_ip : gateway_ip, mac) ) {
		memset(&ifr, 0, sizeof(ifr));
		strncpy(ifr.ifr_name, interf, sizeof(ifr.ifr_name));
		if (ioctl(raw_sock, SIOCGIFHWADDR, &ifr) == -1) {
			OC_SYS_LOG_E(OC_ERR_FAIL, "Gateway interface error");
			perror("ioctl - get hwaddr");
			close(raw_sock);
			return FALSE;
		}
		memcpy(mac, ifr.ifr_hwaddr.sa_data, 6);
	}

	/*
	 * Bind to the interface. We can use a null MAC address because the
	 * system will fill it in for us.
	 */

	memset(&myaddr, 0, sizeof(myaddr));
	myaddr.sll_family = AF_PACKET;
	myaddr.sll_ifindex = ifindex;
	if(bind(raw_sock, (struct sockaddr*)(&myaddr), sizeof(myaddr)) != 0) {
		OC_SYS_LOG_E(OC_ERR_FAIL, "Unable to bind raw socket");
		perror("bind");
		close(raw_sock);
		return FALSE;
	}

	*ret_ifindex = ifindex;
	memcpy(ret_mac, mac, 6);

	return raw_sock;
}

void getPacketStr(int packet_type, char* type_ret) {
	if (packet_type == ACK_PACKET) {
		strncpy( type_ret, "ACK", 3 );
	}else if (packet_type == FIN_PACKET) {
		strncpy( type_ret, "FIN", 3 );
	}else if (packet_type == RST_PACKET) {
		strncpy( type_ret, "RST", 3 );
	}	
	type_ret[3] = '\0';
}

int send_packet(
	in_addr_t app_ip,
	uint16_t app_port,
	in_addr_t server_ip,
	uint16_t server_port,
	unsigned long seq,
	unsigned long ack_seq,
	int packet_type)
{
	int i_result, raw_sock;
	struct tpack tpack;
	struct rpack rpack;
	struct pseudo_header pheader;

	raw_sock = open_raw_ip_socket();
	if (raw_sock < 0) {
		return raw_sock;
	}

	memset( &tpack, 0, sizeof(tpack) );

	// TCP header
	tpack.tcp.source=app_port;		// 16-bit Source port number
	tpack.tcp.dest=server_port;		// 16-bit Destination port
	tpack.tcp.seq=seq;				// 32-bit Sequence Number */
	tpack.tcp.ack_seq=ack_seq;			// 32-bit Acknowledgement Number */
	tpack.tcp.doff=5;				// Data offset */
	tpack.tcp.res1=0;				// reserved */
	tpack.tcp.urg=0;				// Urgent offset valid flag */
	tpack.tcp.ack=0;				// Acknowledgement field valid flag */
	tpack.tcp.psh=0;				// Push flag */
	tpack.tcp.rst=0;				// Reset flag */
	tpack.tcp.syn=0;				// Synchronize sequence numbers flag */
	tpack.tcp.fin=0;				// Finish sending flag */
	tpack.tcp.window=0;				// 16-bit Window size */
	tpack.tcp.check=0;				// space for 16-bit checksum
	tpack.tcp.urg_ptr=0;				// 16-bit urgent offset */

	//  IP header
	tpack.ip.version=4;				// 4-bit Version */
	tpack.ip.ihl=5; 				// 4-bit Header Length */
	tpack.ip.tos=0;/*0x10;				// 8-bit Type of service */
	tpack.ip.tot_len=htons(IPHDR+TCPHDR);		// 16-bit Total length */
	tpack.ip.id=(unsigned short) rand();					// 16-bit ID field */
	tpack.ip.frag_off=0;/*htons(0x4000);		// 13-bit Fragment offset */
	tpack.ip.ttl=0xff;				// 8-bit Time To Live */
	tpack.ip.protocol=IPPROTO_TCP;			// 8-bit Protocol */
	tpack.ip.check=0;				// space for 16-bit Header checksum
	tpack.ip.saddr = app_ip;
	tpack.ip.daddr = server_ip;

	memset(&rpack, 0, sizeof(rpack));
	
	if (packet_type == ACK_PACKET) {
		tpack.tcp.ack = 1;
	}else if (packet_type == FIN_PACKET) {
		tpack.tcp.fin = 1;
		tpack.tcp.ack = 1; // Both FIN and ACK are expected to be set for a 'FIN' packet
		tpack.tcp.window=htons(6567);	// A somewhat random but large enough 16-bit window size */
	}else if (packet_type == RST_PACKET) {
		tpack.tcp.rst = 1;
		tpack.tcp.ack_seq = 0;
	}

	// IP header checksum
	tpack.ip.check=in_cksum((unsigned short *)&tpack.ip,IPHDR);

	// TCP header checksum
	pheader.source_address=(unsigned)tpack.ip.saddr;
	pheader.dest_address=(unsigned)tpack.ip.daddr;
	pheader.placeholder=0;
	pheader.protocol=IPPROTO_TCP;
	pheader.tcp_length=htons(TCPHDR);

	bcopy((char *)&tpack.tcp,(char *)&pheader.tcp,TCPHDR);

	tpack.tcp.check=in_cksum((unsigned short *)&pheader,TCPHDR+12);

	char packet_type_str[4];
	getPacketStr(packet_type, packet_type_str);

	char app_ip_str[INET_ADDRSTRLEN], server_ip_str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &app_ip, app_ip_str, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &server_ip, server_ip_str, INET_ADDRSTRLEN);

	OC_SYS_LOG_I(OC_ERR_NONE, "Sending %s %s:%d (seq:%u ackseq:%u)-> %s:%d", packet_type_str,
			app_ip_str, ntohs(app_port),
			ntohl(seq), ntohl(ack_seq),
			server_ip_str, ntohs(server_port));

	struct sockaddr_in in_addr;
	in_addr.sin_family = AF_INET;
	in_addr.sin_addr.s_addr = pheader.dest_address;
	i_result = sendto(raw_sock,&tpack,PACKETSIZE,0,(void*)&in_addr,sizeof(in_addr));

	if (i_result != PACKETSIZE) {
		OC_SYS_LOG_W(OC_ERR_FAIL, "Unexpected packet size sent: %d != %d", i_result, PACKETSIZE);
		perror("sendto - reset packet");
		close(raw_sock);
		return FALSE;
	}

	close(raw_sock);
	return TRUE;
}

int send_close_packets(
	in_addr_t app_ip,
	uint16_t app_port,
	in_addr_t server_ip,
	int16_t server_port,
	unsigned long packet_seq_app, 
	unsigned long packet_seq_server,
	int	packet_type)
{
	int rst_result = TRUE;
	char packet_type_str[4];
	getPacketStr(packet_type, packet_type_str);

	
	// Update ack_seq to send
	unsigned long ack_seq_app = packet_seq_app; 
	unsigned long ack_seq_server = packet_seq_server; 
	if (packet_type == RST_PACKET) {
		ack_seq_app = 0; // No seq required for RST
		ack_seq_server = 0; 
	}

	char app_ip_str[INET_ADDRSTRLEN], server_ip_str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &app_ip, app_ip_str, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &server_ip, server_ip_str, INET_ADDRSTRLEN);

	// Send packet to app
	int result = send_packet(server_ip, server_port, app_ip, app_port, packet_seq_server, ack_seq_app, packet_type);
	if (!result) {
		OC_SYS_LOG_E(OC_ERR_FAIL, "Failed to send %s to %s:%d", packet_type_str, server_ip_str, ntohs(server_port));
		rst_result = FALSE;
	}


	// Send RST packet to server
	result = send_packet(app_ip, app_port, server_ip, server_port, packet_seq_app, ack_seq_server, packet_type);
	if (!result) {
		OC_SYS_LOG_E(OC_ERR_FAIL, "%s failed: %s:%d -> %s:%d", packet_type_str,
				app_ip_str, ntohs(app_port), server_ip_str, ntohs(server_port));
		rst_result = FALSE;
	}
	if (rst_result) {
		OC_SYS_LOG_I(OC_ERR_NONE, "%s sent successfully: %s:%d <-> %s:%d", packet_type_str,
				app_ip_str, ntohs(app_port), server_ip_str, ntohs(server_port));
	}
	return rst_result;
}


oc_error_t check_socket_still_exists(char * app_ip_str, uint16_t app_port)
{
	uint32_t uid = 0;
	oc_error_t conntrack_result = oc_conntrack_get_uid("/proc/net/tcp",
			app_ip_str, app_port,
			1, &uid);
	if (conntrack_result != OC_ERR_NONE) {
		conntrack_result = oc_conntrack_get_uid("/proc/net/tcp",
				app_ip_str, app_port,
				1, &uid);
	}
	return OC_ERR_NONE;
}

int close_sockets(in_addr_t app_ip, uint16_t app_port, in_addr_t server_ip, uint16_t server_port)
{
	int i_result, raw_sock_app, raw_sock_server;
	int ifindex;
	unsigned char mac[6];

	raw_sock_app = open_raw_packet_socket(app_ip, &ifindex, mac);
	raw_sock_server = open_raw_packet_socket(server_ip, &ifindex, mac);

	int fdmax = raw_sock_app;
	if (raw_sock_server > fdmax) {
		fdmax = raw_sock_server;
	}

	fd_set master_fds;
	FD_ZERO(&master_fds);
	FD_SET(raw_sock_app, &master_fds);
	FD_SET(raw_sock_server, &master_fds);

	char app_ip_str[INET_ADDRSTRLEN], server_ip_str[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &app_ip, app_ip_str, INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &server_ip, server_ip_str, INET_ADDRSTRLEN);

	OC_SYS_LOG_I(OC_ERR_NONE, "Listening: %s:%d <-> %s:%d",
			app_ip_str, ntohs(app_port), server_ip_str, ntohs(server_port));

	struct rpack rpack;
	unsigned long rst_seq_app = 0;
	unsigned long rst_seq_server = 0;
	int time_since_last_traffic = 0;
	time_t tstart = time(0);


	// We'll send FIN to both sides with 0 SEQs to coax an ACK with correct SEQs.
	// If ACK is not sent back to FIN with wrong SEQ, we'll just have to wait for
	// some real traffic.
	send_packet(app_ip, app_port, server_ip, server_port, 0, 0, FIN_PACKET);
	send_packet(server_ip, server_port, app_ip, app_port, 0, 0, FIN_PACKET);

	for ( ; getppid() > 1 && check_socket_still_exists(app_ip_str, app_port) == OC_ERR_NONE ; ) {
		struct timeval tv;

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		fd_set read_fds = master_fds; // copy master as readfds will be updated by select();
		int select_val = select(fdmax + 1, &read_fds, NULL, NULL, &tv);
		if (rst_seq_app != 0 && rst_seq_server != 0 && (time(0) - time_since_last_traffic) >= 2) {
			break;
		} else if ((time(0) - tstart)%(60*5) == 0) {
			OC_SYS_LOG_I(OC_ERR_NONE, "wait time = %d secs: %s:%d <-> %s:%d", (time(0) - tstart),
					app_ip_str, ntohs(app_port), server_ip_str, ntohs(server_port));
		}
		
		if (select_val == 0) {
			// No traffic.  select() timed-out so simple wait again.
			continue;
		}

		// Data on one of our raw sockets so figure out if it's server or app traffic.
		struct sockaddr_ll gotaddr;
		int addrlen = sizeof(gotaddr);
		memset(&gotaddr, 0, addrlen);
		memset(&rpack, 0, sizeof(rpack));

		if (FD_ISSET(raw_sock_app, &read_fds)) { // app > server traffic
			i_result = recvfrom(raw_sock_app, &rpack, sizeof(rpack), 0, (void*)&gotaddr, &addrlen);
		} else if (FD_ISSET(raw_sock_server, &read_fds)) { // app < server traffic
			i_result = recvfrom(raw_sock_server, &rpack, sizeof(rpack), 0, (void*)&gotaddr, &addrlen);
		}

		int is_to_server = 0;
		int is_to_app = 0;
		if (rpack.ip.version == 4 &&
				/*rpack.ip.ihl == 5 &&*/
				rpack.ip.protocol == IPPROTO_TCP) {
			char dst_ip_str[INET_ADDRSTRLEN], src_ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &rpack.ip.saddr, src_ip_str, INET_ADDRSTRLEN);
			inet_ntop(AF_INET, &rpack.ip.daddr, dst_ip_str, INET_ADDRSTRLEN);

			int rcvd_tcp_size = (ntohs(rpack.ip.tot_len) - (rpack.ip.ihl * 4) - (rpack.tcp.doff * 4));

			OC_SYS_LOG_D(OC_ERR_NONE, "packet: %s:%d -> %s:%d (%d bytes)",
					src_ip_str, ntohs(rpack.tcp.source),
					dst_ip_str, ntohs(rpack.tcp.dest),
					rcvd_tcp_size);

			is_to_app = (rpack.ip.saddr == server_ip && rpack.tcp.source == server_port &&
					rpack.ip.daddr == app_ip && rpack.tcp.dest == app_port);

			is_to_server = (rpack.ip.saddr == app_ip && rpack.tcp.source == app_port &&
					rpack.ip.daddr == server_ip && rpack.tcp.dest == server_port);

			if (is_to_app || is_to_server) {
				char* packet_type_str;
				if (rpack.tcp.ack == 1 && rpack.tcp.fin == 1) {
					packet_type_str = "FINACK";
				}else if (rpack.tcp.rst == 1) {
					packet_type_str = "RST";
				}else if (rpack.tcp.syn == 1) {
					packet_type_str = "SYN";
				}else if (rpack.tcp.ack == 1) {
					packet_type_str = "ACK";
				} else {
					packet_type_str = "UNK";
				}

				char* traffic_direction;
				if (is_to_server) {
					rst_seq_app = htonl(ntohl(rpack.tcp.seq) + rcvd_tcp_size);
					rst_seq_server = rpack.tcp.ack_seq;
					traffic_direction = "->";
				} else {
					rst_seq_app = rpack.tcp.ack_seq;
					rst_seq_server = htonl(ntohl(rpack.tcp.seq) + rcvd_tcp_size);
					traffic_direction = "<-";
				}

				OC_SYS_LOG_I(OC_ERR_NONE, "Traffic observed %s: %s:%d %s %s:%d (seq_app: %u, seq_srv:%u) (%u bytes)", packet_type_str,
						app_ip_str, ntohs(app_port), traffic_direction, server_ip_str, ntohs(server_port),
						ntohl(rst_seq_app), ntohl(rst_seq_server), rcvd_tcp_size);
				time_since_last_traffic = time(0);
			}
		}
	}
	int ret_val = FALSE;
	if (rst_seq_app == 0) {
		OC_SYS_LOG_W(OC_ERR_NONE, "No traffic detected so RSTs not sent: %s:%d <-> %s:%d",
				app_ip_str, ntohs(app_port), server_ip_str, ntohs(server_port));
	} else {
		ret_val = send_close_packets(app_ip, app_port, server_ip, server_port, rst_seq_app, rst_seq_server, RST_PACKET);
	}

	close(raw_sock_server);
	close(raw_sock_app);
	return ret_val;
}

