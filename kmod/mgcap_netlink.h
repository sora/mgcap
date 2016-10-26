#ifndef _LINUX_MGCAP_NETLINK_H_
#define _LINUX_MGCAP_NETLINK_H_

/*
 *	NETLINK_GENERIC mgcap family parameters
 */


#define MGCAP_GENL_NAME		"mgcap"
#define MGCAP_GENL_VERSION	0x01


/*
 * mgcap netlink commands
 *   START (DEVICE): start mgcap for a specified device
 *   STOP (DEVICE):  stop mgcap for a specified evice
 */

enum {
	MGCAP_CMD_START,
	MGCAP_CMD_STOP,

	__MGCAP_CMD_MAX,
};
#define MGCAP_CMD_MAX	(__MGCAP_CMD_MAX - 1)


/* ATTR types*/
enum {
	MGCAP_ATTR_UNSPEC,
	MGCAP_ATTR_DEVICE,	/* 32bit ifindex */

	__MGCAP_ATTR_MAX,
};
#define MGCAP_ATTR_MAX	(__MGCAP_ATTR_MAX - 1)



#endif /* _LINUX_MGCAP_NETLINK_H_ */
