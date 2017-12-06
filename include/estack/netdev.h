/*
 * E/STACK netdev header
 *
 * Author: Michel Megens
 * Date: 03/12/2017
 * Email: dev@bietje.net
 */

 /**
 * @addtogroup netdev
 * @{
 */

#ifndef __NETDEV_H__
#define __NETDEV_H__

#include <stdlib.h>
#include <stdint.h>

#include <estack/estack.h>
#include <estack/list.h>

/**
 * @brief Network device statistics.
 */
struct DLL_EXPORT netdev_stats {
	uint32_t rx_bytes, //!< Number of received bytes.
		     rx_packets; //!< Number of received packets.
	uint32_t tx_bytes,  //!< Number of transmitted bytes.
		     tx_packets; //!< Number of received packets.
	uint32_t dropped; //!< Number of dropped packets.
};

/**
 * @brief Network device backlog.
 */
struct DLL_EXPORT netdev_backlog {
	struct list_head head; //!< Backlog head.
	int size; //!< Backlog size.
};

#define MAX_ADDR_LEN 8 //!< Maximum device hardware address length.
#define MAX_LOCAL_ADDRESS_LENGTH 16 //!< Maximum network layer address length.

/**
 * @brief Network interface datastructure.
 */
struct netif {
	uint8_t dummy;
};

struct netbuf;
typedef void(*xmit_handle)(struct netbuf *nb);

/**
 * @brief Wrapper datastructure for (external) protocol handlers.
 */
struct protocol {
	struct list_head entry; //!< List entry.
	uint16_t protocol; //!< Protocol identifier.
	xmit_handle rx; //!< Receive handle.
};

/**
 * @brief Network device datastructure.
 */
struct DLL_EXPORT netdev {	
	const char *name; //!< Device name.
	struct list_head entry; //!< Entry into the global device list.
	struct list_head protocols; //!< Protocol handler list head.
	struct list_head destinations; //!< Destination cache head.

	uint16_t mtu; //!< MTU.
	struct netdev_backlog backlog; //!< Device backlog head.
	struct netdev_stats stats; //!< Device statistics.

	struct netif netif; //!< Network interface reprenting this device on the transport layer and up.
	uint8_t hwaddr[MAX_ADDR_LEN]; //!< Datalink layer address.
	uint8_t addrlen; //!< Length of \p hwaddr.

	xmit_handle rx; //!< Receive handler.

	int processing_weight;
	int rx_max;

	/**
	 * @brief PHY write handle.
	 * @param dev Device pointer.
	 * @param nb Packet buffer to write.
	 * @return Error code.
	 */
	int(*write)(struct netdev *dev, struct netbuf *nb);
	/**
	* @brief PHY read handle.
	* @param dev Device pointer.
	* @param num Maximum number of packet buffers to read.
	* @return Error code.
	*
	* Instead of returning the packet buffers, the packet buffers are enqued on the backlog
	* of \p dev by the PHY-layer drivers.
	*/
	int (*read)(struct netdev *dev, int num);
	/**
	 * @brief Get the number of bytes available in the network cards internal buffers.
	 * @param dev Network device strcuture pointer.
	 * @return Number of bytes available.
	 */
	int(*available)(struct netdev *dev);
};

/**
 * @brief Destination cache data structure.
 */
struct DLL_EXPORT dst_cache_entry {
	struct list_head entry; //!< List entry.

	uint8_t *saddr; //!< Source / network layer address.
	uint8_t saddr_length; //!< Length of \p saddr.
	uint8_t *hwaddr; //!< Hardware address that \p saddr is mapped to.
	uint8_t hwaddr_length; //!< Length of \p hwaddr.
};

CDECL
extern DLL_EXPORT void netdev_add_backlog(struct netdev *dev, struct netbuf *nb);
extern DLL_EXPORT void netdev_init(struct netdev *dev);
extern DLL_EXPORT int netdev_poll(struct netdev *dev);
extern DLL_EXPORT void netdev_demux_handle(struct netbuf *nb);
extern DLL_EXPORT bool netdev_remove_protocol(struct netdev *dev, struct protocol *proto);
extern DLL_EXPORT void netdev_add_destination(struct netdev *dev, const uint8_t *dst,
	uint8_t daddrlen, const uint8_t *src, uint8_t saddrlen);
extern DLL_EXPORT struct dst_cache_entry *netdev_find_destination(struct netdev *dev,
	const uint8_t *src, uint8_t length);
extern DLL_EXPORT bool netdev_remove_destination(struct netdev *dev, const uint8_t *src,
	uint8_t length);
extern DLL_EXPORT bool netdev_update_destination(struct netdev *dev, const uint8_t *dst,
	uint8_t dlength, const uint8_t *src, uint8_t slength);

static inline void netdev_config_params(struct netdev *dev, int maxrx, int maxweight)
{
	dev->rx_max = maxrx;
	dev->processing_weight = maxweight;
}
CDECL_END

/**
 * @brief Iterate over the backlog of a device.
 * @param bl Backlog pointer.
 * @param e Entry pointer.
 * @param p Temporary variable.
 */
#define backlog_for_each_safe(bl, e, p) \
			list_for_each_safe(e, p, &((bl)->head))
#endif // !__NETDEV_H__

/** @} */
