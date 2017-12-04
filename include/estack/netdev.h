/*
 * E/STACK netdev header
 *
 * Author: Michel Megens
 * Date: 03/12/2017
 * Email: dev@bietje.net
 */

#ifndef __NETDEV_H__
#define __NETDEV_H__

#include <stdlib.h>
#include <stdint.h>

#include <estack/estack.h>
#include <estack/list.h>

struct DLL_EXPORT netdev_stats {
	uint32_t rx_bytes, rx_packets;
	uint32_t tx_bytes, tx_packets;
	uint32_t dropped;
};

struct DLL_EXPORT netdev_backlog {
	struct list_head head;
	int size;
};

#define MAX_ADDR_LEN 8

struct netif {
	uint8_t dummy;
};

struct netbuf;
typedef void(*xmit_handle)(struct netbuf *nb);

struct DLL_EXPORT netdev {	
	const char *name;
	struct list_head entry;

	uint16_t mtu;
	struct netdev_backlog backlog;
	struct netdev_stats stats;

	struct netif netif;
	uint8_t hwaddr[MAX_ADDR_LEN];
	uint8_t addrlen;

	xmit_handle rx;
	int(*write)(struct netdev *dev, struct netbuf *nb);
	int (*read)(struct netdev *dev, int num);
	int(*available)(struct netdev *dev);
};

CDECL
extern DLL_EXPORT void netdev_add_backlog(struct netdev *dev, struct netbuf *nb);
extern DLL_EXPORT void netdev_init(struct netdev *dev);
extern DLL_EXPORT int netdev_poll(struct netdev *dev);
CDECL_END

#define backlog_for_each_safe(bl, e, p) \
			list_for_each_safe(e, p, &((bl)->head))
#endif // !__NETDEV_H__
