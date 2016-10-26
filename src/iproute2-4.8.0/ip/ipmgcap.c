#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/ethernet.h>

#include <linux/genetlink.h>
#include "utils.h"
#include "ip_common.h"
#include "rt_names.h"
#include "libgenl.h"

#include "../../../kmod/mgcap_netlink.h"

/* netlink socket */
static struct rtnl_handle genl_rth;
static int genl_family = -1;

struct mgcap_param {
	__u32 ifindex;
};

static void usage(void) __attribute ((noreturn));

static int
parse_args(int argc, char **argv, struct mgcap_param *p)
{
	if (argc < -1)
		usage();

	memset(p, 0, sizeof(*p));

	while (argc > 1) {
		if (strcmp(*argv, "dev") == 0) {
                        NEXT_ARG();
			p->ifindex = if_nametoindex(*argv);
			if (!p->ifindex) {
				invarg ("invalid device", *argv);
				exit (-1);
			}
		}
		argc--;
		argv++;
	}

	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage:  ip mgcap { start | stop } [ dev DEVICE ]\n");
	exit (-1);
}

static int
do_start(int argc, char **argv)
{
	struct mgcap_param p;

	parse_args(argc, argv, &p);

	if (!p.ifindex) {
		fprintf(stderr, "dev must be specified\n");
		exit(-1);
	}

	GENL_REQUEST(req, 1024, genl_family, 0, MGCAP_GENL_VERSION,
		     MGCAP_CMD_START, NLM_F_REQUEST | NLM_F_ACK);

	addattr32(&req.n, 1024, MGCAP_ATTR_DEVICE, p.ifindex);

	if (rtnl_talk(&genl_rth, &req.n, NULL, 0) < 0)
		return -2;

	return 0;
}

static int
do_stop(int argc, char **argv)
{
	struct mgcap_param p;

	parse_args(argc, argv, &p);

	if (!p.ifindex) {
		fprintf(stderr, "dev must be specified\n");
		exit(-1);
	}

	GENL_REQUEST(req, 1024, genl_family, 0, MGCAP_GENL_VERSION,
		     MGCAP_CMD_STOP, NLM_F_REQUEST | NLM_F_ACK);

	addattr32(&req.n, 1024, MGCAP_ATTR_DEVICE, p.ifindex);

	if (rtnl_talk(&genl_rth, &req.n, NULL, 0) < 0)
		return -2;

	return 0;
}

int
do_ipmgcap(int argc, char **argv)
{
        if (genl_family < 0) {
		if (rtnl_open_byproto (&genl_rth, 0, NETLINK_GENERIC)< 0) {
			fprintf (stderr, "Can't open genetlink socket\n");
			exit (1);
		}
		genl_family = genl_resolve_family (&genl_rth,
						   MGCAP_GENL_NAME);
		if (genl_family < 0)
			exit (1);
	}
	
	if (argc < 1)
		usage ();

	if (matches(*argv, "start") == 0)
		return do_start(argc - 1, argv + 1);
	if (matches(*argv, "stop") == 0)
		return do_stop(argc - 1, argv + 1);
	if (matches(*argv, "help") == 0) {
		usage();
		return -1;
	}

        fprintf (stderr, "Command \"%s\" is unknown, try \"ip nsh help\".\n",
		 *argv);

	return -1;
}
	
