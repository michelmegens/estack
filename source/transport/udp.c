/*
 * E/STACK - UDP implementation
 *
 * Author: Michel Megens
 * Date:   22/12/2017
 * Email:  dev@bietje.net
 */

#include <stdlib.h>
#include <stdint.h>

#include <estack/estack.h>
#include <estack/log.h>
#include <estack/netbuf.h>
#include <estack/udp.h>
#include <estack/ip.h>
#include <estack/inet.h>
#include <estack/addr.h>
#include <estack/socket.h>

static void udp_port_unreachable(struct netbuf *nb)
{
	netbuf_set_flag(nb, NBUF_DROPPED);
}

void udp_input(struct netbuf *nb)
{
	struct udp_header *hdr;
	uint16_t length;
	uint16_t csum;
	ip_addr_t addr;
	struct socket *sock;

	hdr = nb->transport.data;
	if(nb->transport.size <= sizeof(*hdr)) {
		netbuf_set_flag(nb, NBUF_DROPPED);
		return;
	}

	length = ntohs(hdr->length);
	if(hdr->csum) {
		if(hdr->csum == 0xFFFF)
			hdr->csum = 0x0;
		
		if(ip_is_ipv4(nb)) {
			csum = ipv4_inet_csum(nb->transport.data, (uint16_t)nb->transport.size,
								ipv4_get_saddr(nb->network.data),
								ipv4_get_daddr(nb->network.data), IP_PROTO_UDP);
			
			if(csum) {
				print_dbg("Dropping UDP packet with bogus checksum: %x\n", csum);
				netbuf_set_flag(nb, NBUF_DROPPED);
				return;
			}
		}
	}

	hdr->length = length;
	hdr->sport = ntohs(hdr->sport);
	hdr->dport = ntohs(hdr->dport);

	nb->application.size = nb->transport.size - sizeof(*hdr);
	if(!nb->application.size) {
		netbuf_set_flag(nb, NBUF_ARRIVED);
		return;
	}

	nb->application.data = (void*) (hdr + 1);
	/* Find the right socket and dump data into the socket */
	if(ip_is_ipv4(nb)) {
		addr.type = 4;
		addr.addr.in4_addr.s_addr = htonl(ipv4_get_daddr(nb->network.data));
		sock = socket_find(&addr, hdr->dport);

		if(sock) {
			sock->rcv_event(sock, nb);
			netbuf_set_flag(nb, NBUF_ARRIVED);
		} else {
			udp_port_unreachable(nb);
		}
	}
}

void udp_output(struct netbuf *nb)
{
}
