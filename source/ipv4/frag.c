/*
 * E/STACK - IP fragmentation
 *
 * Author: Michel Megens
 * Date: 19/12/2017
 * Email: dev@bietje.net
 */

#include <stdlib.h>
#include <string.h>

#include <estack/estack.h>
#include <estack/netdev.h>
#include <estack/netbuf.h>
#include <estack/ip.h>
#include <estack/list.h>

static struct list_head ip_frag_backlog = STATIC_INIT_LIST_HEAD(ip_frag_backlog);

struct fragment_bucket {
	struct list_head lh;
	struct list_head entry;
	bool last_recv;
	int size;
};

static inline bool ipfrag_is_in_seq(struct netbuf *nb1, struct netbuf *nb2)
{
	struct ipv4_header *hdr1, *hdr2;

	hdr1 = nb1->network.data;
	hdr2  = nb2->network.data;

	return hdr1->saddr == hdr2->saddr && hdr1->daddr == hdr2->daddr &&
		hdr1->id == hdr2->id && hdr1->protocol == hdr2->protocol;
}

static inline bool ipfrag_is_last(struct netbuf *nb)
{
	struct ipv4_header *hdr;
	uint8_t flags;

	hdr = nb->network.data;
	flags = ipv4_get_flags(hdr);

	return !flags;
}

static struct netbuf *ipfrag_defragment(struct fragment_bucket *fb)
{
	struct netbuf *nb, *enb;
	struct list_head *lh, *tmp;
	struct ipv4_header *hdr;
	uint16_t offset, length;
	const uint8_t *src;
	uint8_t *dst;

	nb = list_first_entry(&fb->lh, struct netbuf, entry);
	nb = netbuf_realloc(nb, NBAF_TRANSPORT, fb->size);
	offset = 0;
	assert(nb);

	list_for_each_safe(lh, tmp, &fb->lh) {
		enb = list_entry(lh, struct netbuf, entry);

		if(unlikely(enb == nb))
			continue;

		hdr = enb->network.data;
		offset += ipv4_get_offset(hdr);

		hdr->offset = 0;
		length = hdr->length - sizeof(*hdr);
		src = enb->transport.data;
		dst = (uint8_t*)nb->transport.data + offset;

		memcpy(dst, src, length);

		list_del(lh);
		if(enb != nb) {
			netbuf_free(enb);
		}
	}

	list_del(&fb->entry);
	free(fb);

	netbuf_set_flag(nb, NBUF_NOCSUM);

	return nb;
}

static int ipfrag_try_add_packet(struct fragment_bucket *fb, struct netbuf *nb)
{
	struct netbuf *enb, *pnb;
	struct list_head *entry;
	uint16_t start, end;
	uint16_t start_e, end_e;
	uint16_t length;
	struct ipv4_header *hdr, *ehdr;
	int size;

	hdr = nb->network.data;
	
	start = ipv4_get_offset(hdr);
	length = hdr->length - sizeof(*hdr);
	end = start + length;

	if(!fb->last_recv)
		fb->last_recv = ipfrag_is_last(nb);

	list_for_each(entry, &fb->lh) {
		enb = list_entry(entry, struct netbuf, entry);
		if(!ipfrag_is_in_seq(nb, enb))
			return 0;
		
		
		ehdr = enb->network.data;
		start_e = ipv4_get_offset(ehdr);
		length = ehdr->length - sizeof(*ehdr);
		end_e = start_e + length;

		if(start < start_e) {
			list_add_tail(&nb->entry, entry);
			break;
		}

		if(start == start_e || start < end_e) {
			/* we already have the packet or there is overlap,
			   drop the packet in both scenario's.
			*/
			netbuf_set_flag(nb, NBUF_DROPPED);
			return -1;
		}

		if(list_is_last(entry, &fb->lh)) {
			list_add_tail(&nb->entry, &fb->lh);
			break;
		}
	}

	/* validate the buffer chain */
	if(!fb->last_recv)
		return 0;

	pnb = NULL;
	size = 0;
	list_for_each(entry, &fb->lh) {
		enb = list_entry(entry, struct netbuf, entry);
		ehdr = enb->network.data;
		start_e = ipv4_get_offset(ehdr);
		length = ehdr->length - sizeof(*ehdr);
		end_e = start_e + length;		
		size += length;

		if(!pnb) {
			if(start_e)
				return 1;

			start = start_e;
			end = end_e;
			pnb = enb;
			continue;
		}

		if(end != start_e)
			return 1;
		
		start = start_e;
		end = end_e;
		pnb = enb;
	}

	fb->size = size;
	return 2;
}

void ipfrag4_add_packet(struct netbuf *nb)
{
	struct list_head *lh;
	struct fragment_bucket *fb;
	int rc;
	struct netbuf *copy, *old;

	copy = netbuf_clone(nb, (1 << NBAF_NETWORK) | (1 << NBAF_TRANSPORT));
	old = nb;
	nb = copy;

	list_for_each(lh, &ip_frag_backlog) {
		fb = list_entry(lh, struct fragment_bucket, entry);
		rc = ipfrag_try_add_packet(fb, nb);

		switch(rc) {
		case -1:
			netbuf_set_flag(old, NBUF_DROPPED);
			return;
		case 1:
			netbuf_set_flag(old, NBUF_ARRIVED);
			return;
		case 2:
			netbuf_set_flag(old, NBUF_ARRIVED);
			nb = ipfrag_defragment(fb);
			ipv4_input_postfrag(nb);

			if(!netbuf_test_flag(nb, NBUF_REUSE))
				netbuf_free(nb);
			/* ip_input() */
			return;

		default:
			continue;
		}
	}

	fb = z_alloc(sizeof(*fb));
	list_head_init(&fb->lh);
	list_head_init(&fb->entry);
	list_add(&nb->entry, &fb->lh);
	list_add(&fb->entry, &ip_frag_backlog);
	netbuf_set_flag(old, NBUF_ARRIVED);
}
