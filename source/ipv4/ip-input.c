/*
 * E/STACK - IP input handler
 *
 * Author: Michel Megens
 * Date: 12/12/2017
 * Email: dev@bietje.net
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include <estack/estack.h>
#include <estack/netbuf.h>
#include <estack/netdev.h>
#include <estack/ip.h>
#include <estack/log.h>
#include <estack/inet.h>

static inline struct ipv4_header *ipv4_nbuf_to_iphdr(struct netbuf *nb)
{
	assert(nb->network.data);
	assert(nb->network.size);

	return nb->network.data;
}

void ipv4_input(struct netbuf *nb)
{
	struct ipv4_header *hdr;
	uint8_t hdrlen, version;
	struct netif *nif;
	uint32_t localmask;
	uint32_t localip;

	hdr = ipv4_nbuf_to_iphdr(nb);
#ifdef HAVE_BIG_ENDIAN
	hdrlen = (hdr->ihl_version >> 4) & 0xF;
	version = hdr->version & 0xF;
#else
	hdrlen = hdr->ihl_version & 0xF;
	version = (hdr->ihl_version >> 4) & 0xF;
#endif

	if (version != 4) {
		print_dbg("Dropping IPv4 packet with bogus version field (%u)!\n", version);
		netbuf_set_flag(nb, NBUF_DROPPED);
		return;
	}

	/* TODO: fragmentation */
	hdrlen = hdrlen * sizeof(uint32_t);
	if (hdrlen < sizeof(*hdr) || hdrlen > nb->network.size) {
		print_dbg("Dropping IPv4 packet with bogus header length (%u)!\n", hdrlen);
		print_dbg("\tHeader size: %u", hdrlen);
		print_dbg("\tsizeof(ipv4_header): %u :: Buffer size: %u\n", sizeof(*hdr), nb->network.size);
		netbuf_set_flag(nb, NBUF_DROPPED);
		return;
	}

	hdr->saddr = ntohl(hdr->saddr);
	hdr->daddr = ntohl(hdr->daddr);
	nif = &nb->dev->nif;

	localip = ipv4_ptoi(nif->local_ip);
	localmask = ipv4_ptoi(nif->ip_mask);

	if (unlikely(hdr->daddr == INADDR_BCAST ||
		(localip && localmask != INADDR_BCAST && (hdr->daddr | localmask) == INADDR_BCAST))) {
		/* Datagram is a broadcast */
		netbuf_set_flag(nb, NBUF_BCAST);
	} else if (unlikely(IS_MULTICAST(hdr->daddr))) {
		/* TODO: implement multicast */
		print_dbg("Multicast not supported, dropping IP datagram.\n");
		netbuf_set_flag(nb, NBUF_MULTICAST);
		netbuf_set_flag(nb, NBUF_DROPPED);
		return;
	} else {
		netbuf_set_flag(nb, NBUF_UNICAST);

		if (localip && (hdr->daddr == 0 || hdr->daddr != localip)) {
			print_dbg("Dropping IP packet that isn't ment for us..\n");
			netbuf_set_flag(nb, NBUF_DROPPED);
		}
	}

	nb->transport.size = htons(hdr->length);
	if (nb->transport.size < hdrlen || nb->transport.size > nb->network.size) {
		netbuf_set_flag(nb, NBUF_DROPPED);
		return;
	}

	nb->network.size = hdrlen;
	nb->transport.size -= hdrlen;

	if (nb->transport.size)
		nb->transport.data = ((uint8_t*)hdr) + hdrlen;

	switch (hdr->protocol) {
	case IP_PROTO_ICMP:
		print_dbg("Received an IPv4 packet!\n");
		print_dbg("\tIP version: %u :: Header length: %u\n", version, hdrlen);
		netbuf_set_flag(nb, NBUF_ARRIVED);
		break;

	case IP_PROTO_IGMP:
	default:
		netbuf_set_flag(nb, NBUF_DROPPED);
		break;
	}
}