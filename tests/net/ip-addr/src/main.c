/* main.c - Application main entry point */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(net_test, CONFIG_NET_IPV6_LOG_LEVEL);

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/linker/sections.h>
#include <zephyr/random/random.h>

#include <zephyr/ztest.h>

#include <zephyr/net/net_core.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/dummy.h>

#define NET_LOG_ENABLED 1
#include "net_private.h"

#if defined(CONFIG_NET_IPV6_LOG_LEVEL) || defined(CONFIG_NET_IPV4_LOG_LEVEL)
#define DBG(fmt, ...) printk(fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...)
#endif

extern struct net_if_addr *net_if_ipv6_get_ifaddr(struct net_if *iface,
						  const void *addr);

static struct net_if *default_iface;
static struct net_if *second_iface;

#define TEST_BYTE_1(value, expected)				 \
	do {							 \
		char out[3];					 \
		net_byte_to_hex(out, value, 'A', true);		 \
		zassert_false(strcmp(out, expected),		 \
			      "Test 0x%s failed.\n", expected);	 \
	} while (0)

#define TEST_BYTE_2(value, expected)				 \
	do {							 \
		char out[3];					 \
		net_byte_to_hex(out, value, 'a', true);		 \
		zassert_false(strcmp(out, expected),		 \
			      "Test 0x%s failed.\n", expected);	 \
	} while (0)

#define TEST_LL_6(a, b, c, d, e, f, expected)				\
	do {								\
		uint8_t ll[] = { a, b, c, d, e, f };			\
		zassert_false(strcmp(net_sprint_ll_addr(ll, sizeof(ll)),\
				     expected),				\
			      "Test %s failed.\n", expected);		\
	} while (0)

#define TEST_LL_8(a, b, c, d, e, f, g, h, expected)			\
	do {								\
		uint8_t ll[] = { a, b, c, d, e, f, g, h };			\
		zassert_false(strcmp(net_sprint_ll_addr(ll, sizeof(ll)), \
				     expected),				\
			      "Test %s failed.\n", expected);		\
	} while (0)

#define LL_ADDR_STR_SIZE sizeof("xx:xx:xx:xx:xx:xx")

#define TEST_LL_6_TWO(a, b, c, d, e, f, expected)			\
	do {								\
		uint8_t ll1[] = { a, b, c, d, e, f };			\
		uint8_t ll2[] = { f, e, d, c, b, a };			\
		char out[2 * LL_ADDR_STR_SIZE + 1 + 1];	\
		snprintk(out, sizeof(out), "%s ",			\
			 net_sprint_ll_addr(ll1, sizeof(ll1)));		\
		snprintk(out + LL_ADDR_STR_SIZE,			\
			 sizeof(out) - LL_ADDR_STR_SIZE, "%s",		\
			 net_sprint_ll_addr(ll2, sizeof(ll2)));		\
		zassert_false(strcmp(out, expected),			\
			      "Test %s failed, got %s\n", expected,	\
			      out);					\
	} while (0)

#define TEST_IPV6(a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, expected)  \
	do {								     \
		struct in6_addr addr = { { { a, b, c, d, e, f, g, h,	     \
					       i, j, k, l, m, n, o, p } } }; \
		char *ptr = net_sprint_ipv6_addr(&addr);		     \
		zassert_false(strcmp(ptr, expected),			     \
			      "Test %s failed, got %s\n", expected,	     \
			      ptr);					     \
	} while (0)

#define TEST_IPV4(a, b, c, d, expected)					\
	do {								\
		struct in_addr addr = { { { a, b, c, d } } };		\
		char *ptr = net_sprint_ipv4_addr(&addr);		\
		zassert_false(strcmp(ptr, expected),			\
			      "Test %s failed, got %s\n", expected,	\
			      ptr);					\
	} while (0)

struct net_test_context {
	uint8_t mac_addr[6];
	struct net_linkaddr ll_addr;
};

int net_test_init(const struct device *dev)
{
	struct net_test_context *net_test_context = dev->data;

	net_test_context = net_test_context;

	return 0;
}

static uint8_t *net_test_get_mac(const struct device *dev)
{
	struct net_test_context *context = dev->data;

	if (context->mac_addr[2] == 0x00) {
		/* 00-00-5E-00-53-xx Documentation RFC 7042 */
		context->mac_addr[0] = 0x00;
		context->mac_addr[1] = 0x00;
		context->mac_addr[2] = 0x5E;
		context->mac_addr[3] = 0x00;
		context->mac_addr[4] = 0x53;
		context->mac_addr[5] = sys_rand8_get();
	}

	return context->mac_addr;
}

static void net_test_iface_init(struct net_if *iface)
{
	uint8_t *mac = net_test_get_mac(net_if_get_device(iface));

	net_if_set_link_addr(iface, mac, 6, NET_LINK_ETHERNET);
}

static int tester_send(const struct device *dev, struct net_pkt *pkt)
{
	return 0;
}

struct net_test_context net_test_context_data;

static struct dummy_api net_test_if_api = {
	.iface_api.init = net_test_iface_init,
	.send = tester_send,
};

#define _ETH_L2_LAYER DUMMY_L2
#define _ETH_L2_CTX_TYPE NET_L2_GET_CTX_TYPE(DUMMY_L2)

NET_DEVICE_INIT_INSTANCE(net_addr_test1, "net_addr_test1", iface1,
			 net_test_init, NULL,
			 &net_test_context_data, NULL,
			 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			 &net_test_if_api, _ETH_L2_LAYER, _ETH_L2_CTX_TYPE,
			 127);

NET_DEVICE_INIT_INSTANCE(net_addr_test2, "net_addr_test2", iface2,
			 net_test_init, NULL,
			 &net_test_context_data, NULL,
			 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
			 &net_test_if_api, _ETH_L2_LAYER, _ETH_L2_CTX_TYPE,
			 127);

ZTEST(ip_addr_fn, test_ip_addresses)
{
	TEST_BYTE_1(0xde, "DE");
	TEST_BYTE_1(0x09, "09");
	TEST_BYTE_2(0xa9, "a9");
	TEST_BYTE_2(0x80, "80");

	TEST_LL_6(0x12, 0x9f, 0xe3, 0x01, 0x7f, 0x00, "12:9F:E3:01:7F:00");
	TEST_LL_8(0x12, 0x9f, 0xe3, 0x01, 0x7f, 0x00, 0xff, 0x0f, \
		  "12:9F:E3:01:7F:00:FF:0F");

	TEST_LL_6_TWO(0x12, 0x9f, 0xe3, 0x01, 0x7f, 0x00,	\
		      "12:9F:E3:01:7F:00 00:7F:01:E3:9F:12");

	TEST_IPV6(0x20, 1, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, \
		  "2001:db8::1");

	TEST_IPV6(0x20, 0x01, 0x0d, 0xb8, 0x12, 0x34, 0x56, 0x78,	\
		  0x9a, 0xbc, 0xde, 0xf0, 0x01, 0x02, 0x03, 0x04,	\
		  "2001:db8:1234:5678:9abc:def0:102:304");

	TEST_IPV6(0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
		  0x0c, 0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,	\
		  "fe80::cb8:0:0:2");

	TEST_IPV6(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,	\
		  "::1");

	TEST_IPV6(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
		  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	\
		  "::");

	TEST_IPV4(192, 168, 0, 1, "192.168.0.1");
	TEST_IPV4(0, 0, 0, 0, "0.0.0.0");
	TEST_IPV4(127, 0, 0, 1, "127.0.0.1");
}

#if defined(CONFIG_NET_IPV6_LOG_LEVEL) || defined(CONFIG_NET_IPV4_LOG_LEVEL)
static const char *addr_state_to_str(enum net_addr_state state)
{
	switch (state) {
	case NET_ADDR_PREFERRED:
		return "preferred";
	case NET_ADDR_DEPRECATED:
		return "deprecated";
	case NET_ADDR_TENTATIVE:
		return "tentative";
	case NET_ADDR_ANY_STATE:
		return "invalid";
	default:
		break;
	}

	return "unknown";
}

static const char *get_addr_state(struct net_if *iface,
				  const struct in6_addr *addr)
{
	struct net_if_addr *ifaddr;

	if (iface == NULL) {
		return "<iface not set>";
	}

	ifaddr = net_if_ipv6_get_ifaddr(iface, addr);
	if (ifaddr) {
		return addr_state_to_str(ifaddr->addr_state);
	}

	return "<addr not found>";
}
#endif

ZTEST(ip_addr_fn, test_ipv6_addresses)
{
	struct in6_addr loopback = IN6ADDR_LOOPBACK_INIT;
	struct in6_addr any = IN6ADDR_ANY_INIT;
	struct in6_addr mcast = { { { 0xff, 0x84, 0, 0, 0, 0, 0, 0,
				      0, 0, 0, 0, 0, 0, 0, 0x2 } } };
	struct in6_addr addr6 = { { { 0xfe, 0x80, 0, 0, 0, 0, 0, 0,
				      0, 0, 0, 0, 0, 0, 0, 0x1 } } };
	struct in6_addr addr6_pref1 = { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
					    0, 0, 0, 0, 0, 0, 0, 0x1 } } };
	struct in6_addr addr6_pref2 = { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
					    0, 0, 0, 0, 0, 0, 0, 0x2 } } };
	struct in6_addr addr6_pref3 = { { { 0x20, 0x01, 0x0d, 0xb8, 0x64, 0, 0,
					    0, 0, 0, 0, 0, 0, 0, 0, 0x2 } } };
	struct in6_addr ula = { { { 0xfc, 0x00, 0xaa, 0xaa, 0, 0, 0, 0,
				    0, 0, 0, 0, 0xd1, 0xd2, 0xd3, 0xd4 } } };
	struct in6_addr ula2 = { { { 0xfc, 0x00, 0xaa, 0xaa, 0, 0, 0, 0,
				0, 0, 0, 0, 0xd1, 0xd2, 0xd3, 2 } } };
	struct in6_addr ula3 = { { { 0xfc, 0x00, 0xaa, 0xaa, 0, 0, 0, 0,
				0, 0, 0, 0, 0xd1, 0xd2, 0xf3, 3 } } };
	struct in6_addr ula4 = { { { 0xfc, 0x00, 0xaa, 0xaa, 0, 0, 0, 0,
				0, 0, 0, 0, 0xd1, 0xd2, 0xf3, 4 } } };
	struct in6_addr ula5 = { { { 0xfc, 0x00, 0xaa, 0xaa, 0, 0, 0, 0,
				0, 0, 0, 0, 0xd1, 0xd2, 0xd3, 0xd5 } } };
	struct in6_addr *tmp;
	const struct in6_addr *out;
	struct net_if_addr *ifaddr1, *ifaddr2, *ifaddr_ula, *ifaddr_ula3, *ifaddr_ula4;
	struct net_if_mcast_addr *ifmaddr1;
	struct net_if_ipv6_prefix *prefix;
	struct net_if *iface;
	int i;

	/**TESTPOINT: Check if the IPv6 address is a loopback address*/
	zassert_true(net_ipv6_is_addr_loopback(&loopback),
		     "IPv6 loopback address check failed.");

	/**TESTPOINT: Check if the IPv6 address is a multicast address*/
	zassert_true(net_ipv6_is_addr_mcast(&mcast),
		     "IPv6 multicast address check failed.");

	ifaddr1 = net_if_ipv6_addr_add(default_iface,
				      &addr6,
				      NET_ADDR_MANUAL,
				      0);
	/**TESTPOINT: Check if IPv6 interface address is added*/
	zassert_not_null(ifaddr1, "IPv6 interface address add failed");

	ifaddr2 = net_if_ipv6_addr_lookup(&addr6, NULL);

	/**TESTPOINT: Check if addresses match*/
	zassert_equal_ptr(ifaddr1, ifaddr2,
			  "IPv6 interface address mismatch");

	/**TESTPOINT: Check if the IPv6 address is a loopback address*/
	zassert_false(net_ipv6_is_my_addr(&loopback),
		      "My IPv6 loopback address check failed");

	/**TESTPOINT: Check IPv6 address*/
	zassert_true(net_ipv6_is_my_addr(&addr6),
		     "My IPv6 address check failed");

	/**TESTPOINTS: Check IPv6 prefix*/
	zassert_true(net_ipv6_is_prefix((uint8_t *)&addr6_pref1,
					(uint8_t *)&addr6_pref2, 64),
		     "Same IPv6 prefix test failed");

	zassert_false(net_ipv6_is_prefix((uint8_t *)&addr6_pref1,
					 (uint8_t *)&addr6_pref3, 64),
		      "Different IPv6 prefix test failed");

	zassert_false(net_ipv6_is_prefix((uint8_t *)&addr6_pref1,
					 (uint8_t *)&addr6_pref2, 128),
		      "Different full IPv6 prefix test failed");

	zassert_false(net_ipv6_is_prefix((uint8_t *)&addr6_pref1,
					 (uint8_t *)&addr6_pref3, 255),
		      "Too long prefix test failed");

	ifmaddr1 = net_if_ipv6_maddr_add(default_iface, &mcast);

	/**TESTPOINTS: Check IPv6 addresses*/
	zassert_not_null(ifmaddr1, "IPv6 multicast address add failed");

	ifmaddr1 = net_if_ipv6_maddr_add(default_iface, &addr6);

	zassert_is_null(ifmaddr1,
			"IPv6 multicast address could be added failed");

	zassert_false(memcmp(net_ipv6_unspecified_address(), &any, sizeof(any)),
		      "My IPv6 unspecified address check failed");

	ifaddr2 = net_if_ipv6_addr_add(default_iface,
				       &addr6,
				       NET_ADDR_AUTOCONF,
				       0);
	zassert_not_null(ifaddr2, "IPv6 ll address autoconf add failed");

	ifaddr2->addr_state = NET_ADDR_PREFERRED;

	tmp = net_if_ipv6_get_ll(default_iface, NET_ADDR_PREFERRED);
	zassert_false(tmp && memcmp(tmp, &addr6.s6_addr,
				    sizeof(struct in6_addr)),
		      "IPv6 ll address fetch failed");

	ifaddr2->addr_state = NET_ADDR_DEPRECATED;

	tmp = net_if_ipv6_get_ll(default_iface, NET_ADDR_PREFERRED);
	zassert_false(tmp && !memcmp(tmp, &any, sizeof(struct in6_addr)),
		      "IPv6 preferred ll address fetch failed");

	ifaddr1 = net_if_ipv6_addr_add(default_iface,
				       &addr6_pref2,
				       NET_ADDR_AUTOCONF,
				       0);
	zassert_not_null(ifaddr1, "IPv6 global address autoconf add failed");

	ifaddr1->addr_state = NET_ADDR_PREFERRED;

	/* Two tests for IPv6, first with interface given, then when
	 * iface is NULL
	 */
	for (i = 0, iface = default_iface; i < 2; i++, iface = NULL) {
		ifaddr2->addr_state = NET_ADDR_DEPRECATED;

		out = net_if_ipv6_select_src_addr(iface, &addr6_pref1);
		zassert_not_null(out,
				 "IPv6 src addr selection failed, iface %p\n",
				 iface);

		DBG("%d: Selected IPv6 address %s state %s, iface %p\n", __LINE__,
		    net_sprint_ipv6_addr(out),
		    get_addr_state(iface, out),
		    iface);

		zassert_false(memcmp(out->s6_addr, &addr6_pref2.s6_addr,
				     sizeof(struct in6_addr)),
			      "IPv6 wrong src address selected, iface %p\n",
			      iface);

		/* Now we should get :: address */
		out = net_if_ipv6_select_src_addr(iface, &addr6);
		zassert_not_null(out, "IPv6 src any addr selection failed, "
				 "iface %p\n", iface);

		DBG("%d: Selected IPv6 address %s, iface %p\n", __LINE__,
		    net_sprint_ipv6_addr(out),
		    iface);

		zassert_false(memcmp(out->s6_addr, &any.s6_addr,
				     sizeof(struct in6_addr)),
			      "IPv6 wrong src any address selected, iface %p\n",
			      iface);

		ifaddr2->addr_state = NET_ADDR_PREFERRED;

		/* Now we should get ll address */
		out = net_if_ipv6_select_src_addr(iface, &addr6);
		zassert_not_null(out,  "IPv6 src ll addr selection failed, "
				 "iface %p\n", iface);

		DBG("%d: Selected IPv6 address %s state %s, iface %p\n", __LINE__,
		    net_sprint_ipv6_addr(out),
		    iface != NULL ? get_addr_state(iface, out) : "unknown",
		    iface);

		zassert_false(memcmp(out->s6_addr, &addr6.s6_addr,
				     sizeof(struct in6_addr)),
			      "IPv6 wrong src ll address selected, iface %p\n",
			      iface);
	}

	zassert_true(net_if_ipv6_addr_rm(default_iface, &addr6),
		     "IPv6 removing address failed\n");
	zassert_true(net_if_ipv6_addr_rm(default_iface, &addr6_pref2),
		     "IPv6 removing address failed\n");

	/**TESTPOINTS: Check what IPv6 address is selected when some
	 * addresses are in preferred state and some in deprecated state.
	 */
	prefix = net_if_ipv6_prefix_add(default_iface, &ula, 96, 3600);
	zassert_not_null(prefix, "IPv6 ula prefix add failed");

	prefix = net_if_ipv6_prefix_add(default_iface, &ula2, 64, 3600);
	zassert_not_null(prefix, "IPv6 ula prefix add failed");

	ifaddr_ula = net_if_ipv6_addr_add(default_iface, &ula,
					  NET_ADDR_AUTOCONF, 0);
	zassert_not_null(ifaddr_ula, "IPv6 ula address add failed");

	ifaddr_ula->addr_state = NET_ADDR_PREFERRED;

	out = net_if_ipv6_select_src_addr(default_iface, &ula2);
	zassert_not_null(out, "IPv6 src ula addr selection failed, "
			 "iface %p\n", default_iface);

	DBG("%d: Selected IPv6 address %s state %s, iface %p\n", __LINE__,
	    net_sprint_ipv6_addr(out), get_addr_state(default_iface, out),
	    iface);

	zassert_false(memcmp(out->s6_addr, &ula.s6_addr, sizeof(struct in6_addr)),
		      "IPv6 wrong src ula address selected, iface %p\n", iface);

	/* Allow selection of deprecated address if no other address
	 * is available.
	 */
	ifaddr_ula->addr_state = NET_ADDR_DEPRECATED;

	out = net_if_ipv6_select_src_addr(default_iface, &ula3);
	zassert_not_null(out, "IPv6 src ula addr selection failed, "
			 "iface %p\n", default_iface);

	/* Back to preferred state so that later checks work correctly */
	ifaddr_ula->addr_state = NET_ADDR_PREFERRED;

	/* Then add another address with preferred state and check that we
	 * still do not select the deprecated address even if it is a better match.
	 */
	ifaddr_ula3 = net_if_ipv6_addr_add(default_iface, &ula3,
					   NET_ADDR_AUTOCONF, 0);
	zassert_not_null(ifaddr_ula3, "IPv6 ula address add failed");

	ifaddr_ula3->addr_state = NET_ADDR_PREFERRED;

	out = net_if_ipv6_select_src_addr(default_iface, &ula2);
	zassert_not_null(out, "IPv6 src ula addr selection failed, "
			 "iface %p\n", default_iface);

	DBG("%d: Selected IPv6 address %s state %s, iface %p\n", __LINE__,
	    net_sprint_ipv6_addr(out), get_addr_state(default_iface, out),
	    iface);

	zassert_false(memcmp(out->s6_addr, &ula3.s6_addr, sizeof(struct in6_addr)),
		      "IPv6 wrong src ula address selected, iface %p\n", iface);

	/* Then change the address to deprecated state and check that we
	 * do select the deprecated address.
	 */
	ifaddr_ula3->addr_state = NET_ADDR_DEPRECATED;

	out = net_if_ipv6_select_src_addr(default_iface, &ula2);
	zassert_not_null(out, "IPv6 src ula addr selection failed, "
			 "iface %p\n", default_iface);

	DBG("%d: Selected IPv6 address %s state %s, iface %p\n", __LINE__,
	    net_sprint_ipv6_addr(out), get_addr_state(default_iface, out),
	    iface);

	zassert_false(memcmp(out->s6_addr, &ula.s6_addr, sizeof(struct in6_addr)),
		      "IPv6 wrong src ula address selected, iface %p\n", iface);

	/* Then have two deprecated addresses */
	ifaddr_ula->addr_state = NET_ADDR_DEPRECATED;

	out = net_if_ipv6_select_src_addr(default_iface, &ula2);
	zassert_not_null(out, "IPv6 src ula addr selection failed, "
			 "iface %p\n", default_iface);

	DBG("%d: Selected IPv6 address %s state %s, iface %p\n", __LINE__,
	    net_sprint_ipv6_addr(out), get_addr_state(default_iface, out),
	    iface);

	zassert_false(memcmp(out->s6_addr, &ula3.s6_addr, sizeof(struct in6_addr)),
		      "IPv6 wrong src ula address selected, iface %p\n", iface);

	ifaddr_ula4 = net_if_ipv6_addr_add(default_iface, &ula4,
					   NET_ADDR_AUTOCONF, 0);
	zassert_not_null(ifaddr_ula4, "IPv6 ula address add failed");

	ifaddr_ula4->addr_state = NET_ADDR_DEPRECATED;
	ifaddr_ula3->addr_state = NET_ADDR_PREFERRED;

	/* There is now one preferred and two deprecated addresses.
	 * The preferred address should be selected.
	 */
	out = net_if_ipv6_select_src_addr(default_iface, &ula5);
	zassert_not_null(out, "IPv6 src ula addr selection failed, "
			 "iface %p\n", default_iface);

	DBG("%d: Selected IPv6 address %s state %s, iface %p\n", __LINE__,
	    net_sprint_ipv6_addr(out), get_addr_state(default_iface, out),
	    iface);

	zassert_false(memcmp(out->s6_addr, &ula3.s6_addr, sizeof(struct in6_addr)),
		      "IPv6 wrong src ula address selected, iface %p\n", iface);

	zassert_true(net_if_ipv6_addr_rm(default_iface, &ula),
		     "IPv6 removing address failed\n");

	zassert_true(net_if_ipv6_addr_rm(default_iface, &ula3),
		     "IPv6 removing address failed\n");

	zassert_true(net_if_ipv6_addr_rm(default_iface, &ula4),
		     "IPv6 removing address failed\n");
}

ZTEST(ip_addr_fn, test_ipv4_ll_address_select_default_first)
{
	struct net_if *iface;
	const struct in_addr *out;
	struct net_if_addr *ifaddr;
	struct in_addr lladdr4_1 = { { { 169, 254, 0, 1 } } };
	struct in_addr lladdr4_2 = { { { 169, 254, 0, 3 } } };
	struct in_addr netmask = { { { 255, 255, 0, 0 } } };
	struct in_addr dst4 = { { { 169, 254, 0, 2 } } };

	ifaddr = net_if_ipv4_addr_add(default_iface, &lladdr4_1, NET_ADDR_MANUAL, 0);
	zassert_not_null(ifaddr, "IPv4 interface address add failed");
	zassert_true(net_ipv4_is_my_addr(&lladdr4_1),
		     "My IPv4 address check failed");

	net_if_ipv4_set_netmask_by_addr(default_iface, &lladdr4_1, &netmask);

	ifaddr = net_if_ipv4_addr_add(second_iface, &lladdr4_2, NET_ADDR_MANUAL, 0);
	zassert_not_null(ifaddr, "IPv4 interface address add failed");
	zassert_true(net_ipv4_is_my_addr(&lladdr4_2),
		     "My IPv4 address check failed");

	net_if_ipv4_set_netmask_by_addr(second_iface, &lladdr4_2, &netmask);

	/* In case two network interfaces have two equally good addresses
	 * (same net mask), default interface should be selected.
	 */
	out = net_if_ipv4_select_src_addr(NULL, &dst4);
	iface = net_if_ipv4_select_src_iface(&dst4);
	zassert_not_null(out, "IPv4 src addr selection failed, iface %p\n",
			 iface);

	DBG("%d: Selected IPv4 address %s, iface %p\n", __LINE__,
	    net_sprint_ipv4_addr(out), iface);

	zassert_equal_ptr(iface, default_iface, "Wrong iface selected");
	zassert_equal(out->s_addr, lladdr4_1.s_addr,
		      "IPv4 wrong src address selected, iface %p\n", iface);
}

ZTEST(ip_addr_fn, test_ipv4_ll_address_select)
{
	struct net_if *iface;
	const struct in_addr *out;
	struct net_if_addr *ifaddr;
	struct in_addr lladdr4_1 = { { { 169, 254, 250, 1 } } };
	struct in_addr lladdr4_2 = { { { 169, 254, 253, 1 } } };
	struct in_addr netmask_1 = { { { 255, 255, 255, 0 } } };
	struct in_addr netmask_2 = { { { 255, 255, 255, 252 } } };
	struct in_addr dst4_1 = { { { 169, 254, 250, 2 } } };
	struct in_addr dst4_2 = { { { 169, 254, 253, 2 } } };

	ifaddr = net_if_ipv4_addr_add(default_iface, &lladdr4_1, NET_ADDR_MANUAL, 0);
	zassert_not_null(ifaddr, "IPv4 interface address add failed");
	zassert_true(net_ipv4_is_my_addr(&lladdr4_1),
		     "My IPv4 address check failed");

	net_if_ipv4_set_netmask_by_addr(default_iface, &lladdr4_1, &netmask_1);

	ifaddr = net_if_ipv4_addr_add(second_iface, &lladdr4_2, NET_ADDR_MANUAL, 0);
	zassert_not_null(ifaddr, "IPv4 interface address add failed");
	zassert_true(net_ipv4_is_my_addr(&lladdr4_2),
		     "My IPv4 address check failed");

	net_if_ipv4_set_netmask_by_addr(second_iface, &lladdr4_2, &netmask_2);

	out = net_if_ipv4_select_src_addr(NULL, &dst4_1);
	iface = net_if_ipv4_select_src_iface(&dst4_1);
	zassert_not_null(out, "IPv4 src addr selection failed, iface %p\n",
			 iface);

	DBG("%d: Selected IPv4 address %s, iface %p\n", __LINE__,
	    net_sprint_ipv4_addr(out), iface);

	zassert_equal(out->s_addr, lladdr4_1.s_addr,
		      "IPv4 wrong src address selected, iface %p\n", iface);
	zassert_equal_ptr(iface, default_iface, "Wrong iface selected");

	out = net_if_ipv4_select_src_addr(NULL, &dst4_2);
	iface = net_if_ipv4_select_src_iface(&dst4_2);
	zassert_not_null(out, "IPv4 src addr selection failed, iface %p\n",
			 iface);

	DBG("%d: Selected IPv4 address %s, iface %p\n", __LINE__,
	    net_sprint_ipv4_addr(out), iface);

	zassert_equal(out->s_addr, lladdr4_2.s_addr,
		      "IPv4 wrong src address selected, iface %p\n", iface);
	zassert_equal_ptr(iface, second_iface, "Wrong iface selected");
}

ZTEST(ip_addr_fn, test_ipv4_addresses)
{
	const struct in_addr *out;
	struct net_if_addr *ifaddr1;
	struct net_if_mcast_addr *ifmaddr1;
	struct in_addr addr4 = { { { 192, 168, 0, 1 } } };
	struct in_addr addr4b = { { { 192, 168, 1, 2 } } };
	struct in_addr addr4_not_found = { { { 10, 20, 30, 40 } } };
	struct in_addr lladdr4 = { { { 169, 254, 98, 203 } } };
	struct in_addr maddr4a = { { { 224, 0, 0, 1 } } };
	struct in_addr maddr4b = { { { 224, 0, 0, 2 } } };
	struct in_addr match_addr = { { { 192, 168, 0, 2 } } };
	struct in_addr fail_addr = { { { 10, 1, 0, 2 } } };
	struct in_addr netmask = { { { 255, 255, 255, 0 } } };
	struct in_addr netmask2 = { { { 255, 255, 0, 0 } } };
	struct in_addr gw = { { { 192, 168, 0, 42 } } };
	struct in_addr loopback4 = { { { 127, 0, 0, 1 } } };
	struct in_addr bcast_addr1 = { { { 255, 255, 255, 255 } } };
	struct in_addr bcast_addr2 = { { { 192, 168, 1, 255 } } };
	struct in_addr bcast_addr3 = { { { 192, 168, 255, 255 } } };
	struct in_addr bcast_addr4 = { { { 192, 0, 2, 255 } } };
	struct in_addr bcast_addr5 = { { { 192, 168, 0, 255 } } };
	struct net_if *iface, *iface1, *iface2;
	int i, ret;

	ifaddr1 = net_if_ipv4_addr_add(default_iface,
				       &addr4,
				       NET_ADDR_MANUAL,
				       0);
	zassert_not_null(ifaddr1, "IPv4 interface address add failed");

	zassert_true(net_ipv4_is_my_addr(&addr4),
		     "My IPv4 address check failed");

	net_if_ipv4_set_netmask_by_addr(default_iface, &addr4, &netmask);

	ifaddr1 = net_if_ipv4_addr_add(default_iface,
				       &lladdr4,
				       NET_ADDR_MANUAL,
				       0);
	zassert_not_null(ifaddr1, "IPv4 interface address add failed");

	net_if_ipv4_set_netmask_by_addr(default_iface, &lladdr4, &netmask2);

	zassert_true(net_ipv4_is_my_addr(&lladdr4),
		     "My IPv4 address check failed");

	zassert_false(net_ipv4_is_my_addr(&loopback4),
		      "My IPv4 loopback address check failed");

	/* Two tests for IPv4, first with interface given, then when
	 * iface is NULL
	 */
	for (i = 0, iface = default_iface; i < 2; i++, iface = NULL) {
		out = net_if_ipv4_select_src_addr(iface, &addr4);
		zassert_not_null(out,  "IPv4 src addr selection failed, "
				 "iface %p\n", iface);

		DBG("%d: Selected IPv4 address %s, iface %p\n", __LINE__,
		       net_sprint_ipv4_addr(out), iface);

		zassert_equal(out->s_addr, addr4.s_addr,
			      "IPv4 wrong src address selected, iface %p\n",
			      iface);

		/* Now we should get ll address */
		out = net_if_ipv4_select_src_addr(iface, &lladdr4);
		zassert_not_null(out, "IPv4 src ll addr selection failed, "
				 "iface %p\n", iface);

		DBG("%d: Selected IPv4 address %s, iface %p\n", __LINE__,
		       net_sprint_ipv4_addr(out), iface);

		zassert_equal(out->s_addr, lladdr4.s_addr,
			      "IPv4 wrong src ll address selected, iface %p\n",
			      iface);

		/* Now we should get 192.168.0.1 address */
		out = net_if_ipv4_select_src_addr(iface, &addr4b);
		zassert_not_null(out, "IPv4 src any addr selection failed, "
				 "iface %p\n", iface);

		DBG("%d: Selected IPv4 address %s, iface %p\n", __LINE__,
		       net_sprint_ipv4_addr(out), iface);

		zassert_equal(out->s_addr, addr4.s_addr,
			      "IPv4 wrong src address selected, iface %p\n",
			      iface);

		/* Now we should get 192.168.0.1 address */
		out = net_if_ipv4_select_src_addr(iface, &addr4_not_found);
		zassert_not_null(out, "IPv4 src any addr selection failed, "
				 "iface %p\n", iface);

		DBG("%d: Selected IPv4 address %s, iface %p\n", __LINE__,
		       net_sprint_ipv4_addr(out), iface);

		zassert_equal(out->s_addr, addr4.s_addr,
			      "IPv4 wrong src address selected, iface %p\n",
			      iface);
	}

	iface = default_iface;

	net_if_ipv4_set_gw(iface, &gw);

	zassert_false(net_ipv4_addr_mask_cmp(iface, &fail_addr),
		      "IPv4 wrong match failed");

	zassert_true(net_ipv4_addr_mask_cmp(iface, &match_addr),
		     "IPv4 match failed");

	zassert_true(net_ipv4_is_addr_mcast(&maddr4a),
		     "IPv4 multicast address");

	zassert_true(net_ipv4_is_addr_mcast(&maddr4b),
		     "IPv4 multicast address");

	zassert_false(net_ipv4_is_addr_mcast(&addr4), "IPv4 address");

	zassert_false(net_ipv4_is_addr_mcast(&bcast_addr1), "IPv4 broadcast address");

	ifmaddr1 = net_if_ipv4_maddr_add(default_iface, &maddr4a);
	zassert_not_null(ifmaddr1, "IPv4 multicast address add failed");

	ifmaddr1 = net_if_ipv4_maddr_add(default_iface, &maddr4b);
	zassert_not_null(ifmaddr1, "IPv4 multicast address add failed");

	iface = NULL;

	iface1 = net_if_get_by_index(1);
	iface2 = net_if_get_by_index(2);

	ifmaddr1 = net_if_ipv4_maddr_lookup(&maddr4a, &iface);
	zassert_not_null(ifmaddr1, "IPv4 multicast address lookup failed");
	zassert_equal(iface, iface1, "Interface not found");

	ifmaddr1 = net_if_ipv4_maddr_lookup(&maddr4b, &iface);
	zassert_not_null(ifmaddr1, "IPv4 multicast address lookup failed");
	zassert_equal(iface, iface1, "Interface not found");

	ifmaddr1 = net_if_ipv4_maddr_lookup(&maddr4a, &iface2);
	zassert_is_null(ifmaddr1, "IPv4 multicast address lookup succeed");

	ret = net_if_ipv4_maddr_rm(iface2, &maddr4a);
	zassert_false(ret, "IPv4 rm succeed");

	ret = net_if_ipv4_maddr_rm(iface1, &maddr4a);
	zassert_true(ret, "IPv4 rm failed");

	ifmaddr1 = net_if_ipv4_maddr_lookup(&maddr4a, &iface1);
	zassert_is_null(ifmaddr1, "IPv4 multicast address lookup succeed");

	ret = net_if_ipv4_maddr_rm(iface1, &maddr4b);
	zassert_true(ret, "IPv4 rm failed");

	ifmaddr1 = net_if_ipv4_maddr_lookup(&maddr4b, &iface1);
	zassert_is_null(ifmaddr1, "IPv4 multicast address lookup succeed");

	ret = net_ipv4_is_addr_bcast(iface, &bcast_addr1);
	zassert_true(ret, "IPv4 address 1 is not broadcast address");

	ret = net_ipv4_is_addr_bcast(iface, &bcast_addr2);
	zassert_false(ret, "IPv4 address 2 is broadcast address");

	ret = net_ipv4_is_addr_bcast(iface, &bcast_addr4);
	zassert_false(ret, "IPv4 address 4 is broadcast address");

	ret = net_ipv4_is_addr_bcast(iface, &maddr4b);
	zassert_false(ret, "IPv4 address is broadcast address");

	ret = net_ipv4_is_addr_bcast(iface, &bcast_addr5);
	zassert_true(ret, "IPv4 address 5 is not broadcast address");

	ret = net_ipv4_is_addr_bcast(iface, &bcast_addr2);
	zassert_false(ret, "IPv4 address 2 is broadcast address");

	net_if_ipv4_set_netmask_by_addr(iface, &addr4, &netmask2);

	ret = net_ipv4_is_addr_bcast(iface, &bcast_addr3);
	zassert_true(ret, "IPv4 address 3 is not broadcast address");
}

ZTEST(ip_addr_fn, test_ipv6_mesh_addresses)
{
	struct net_if_addr *ifaddr;
	const struct in6_addr *out;
	struct in6_addr lla = { { { 0xfe, 0x80, 0, 0, 0, 0, 0, 0, 0x54, 0xdb,
				    0x88, 0x1c, 0x38, 0x45, 0x57, 0xf4 } } };
	struct in6_addr ml_eid = { { { 0xfd, 0xe5, 0x8d, 0xba, 0x82, 0xe1, 0,
				       0x01, 0x40, 0x16, 0x99, 0x3c, 0x83, 0x99,
				       0x35, 0xab } } };
	struct in6_addr ll_mcast = { { { 0xff, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					 0, 0, 0, 0, 0x1 } } };
	struct in6_addr ml_mcast = { { { 0xff, 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					 0, 0, 0, 0, 0x1 } } };
	struct net_if *iface = default_iface;

	ifaddr = net_if_ipv6_addr_add(iface, &lla, NET_ADDR_AUTOCONF, 0);
	zassert_not_null(ifaddr, "IPv6 ll address autoconf add failed");

	ifaddr = net_if_ipv6_addr_add(iface, &ml_eid, NET_ADDR_AUTOCONF, 0);
	zassert_not_null(ifaddr, "IPv6 ll address autoconf add failed");

	ifaddr->is_mesh_local = true;

	zassert_true(net_ipv6_is_addr_mcast_mesh(&ml_mcast),
		     "IPv6 multicast mesh address check failed");

	out = net_if_ipv6_select_src_addr(iface, &ll_mcast);
	zassert_not_null(out, "IPv6 src addr selection failed\n");

	DBG("IPv6: destination: %s - selected %s\n",
	    net_sprint_ipv6_addr(&ll_mcast), net_sprint_ipv6_addr(out));

	zassert_false(memcmp(out->s6_addr, &lla.s6_addr,
			     sizeof(struct in6_addr)),
		      "IPv6 wrong src address selected\n");

	out = net_if_ipv6_select_src_addr(iface, &ml_mcast);
	zassert_not_null(out, "IPv6 src addr selection failed\n");

	DBG("IPv6: destination: %s - selected %s\n",
	    net_sprint_ipv6_addr(&ml_mcast), net_sprint_ipv6_addr(out));

	zassert_false(memcmp(out->s6_addr, &ml_eid.s6_addr,
			     sizeof(struct in6_addr)),
		      "IPv6 wrong src address selected\n");

	zassert_true(net_if_ipv6_addr_rm(iface, &lla),
		     "IPv6 removing address failed\n");
	zassert_true(net_if_ipv6_addr_rm(iface, &ml_eid),
		     "IPv6 removing address failed\n");
}

ZTEST(ip_addr_fn, test_private_ipv6_addresses)
{
	bool ret;
	struct {
		struct in6_addr addr;
		bool is_private;
	} addrs[] = {
		{
			.addr = { { { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0,
				      0, 0, 0, 0, 0, 0, 0x99, 0x1 } } },
			.is_private = true,
		},
		{
			.addr = { { { 0xfc, 0x01, 0, 0, 0, 0, 0, 0,
				      0, 0, 0, 0, 0, 0, 0, 1 } } },
			.is_private = true,
		},
		{
			.addr = { { { 0xfc, 0, 0, 0, 0, 0, 0, 0,
				      0, 0, 0, 0, 0, 0, 0, 2 } } },
			.is_private = true,
		},
		{
			.addr = { { { 0x20, 0x01, 0x1d, 0xb8, 0, 0, 0, 0,
				      0, 0, 0, 0, 0, 0, 0x99, 0x1 } } },
			.is_private = false,
		},
	};

	for (int i = 0; i < ARRAY_SIZE(addrs); i++) {
		ret = net_ipv6_is_private_addr(&addrs[i].addr);
		zassert_equal(ret, addrs[i].is_private, "Address %s check failed",
			      net_sprint_ipv6_addr(&addrs[i].addr));
	}

}

ZTEST(ip_addr_fn, test_private_ipv4_addresses)
{
	bool ret;
	struct {
		struct in_addr addr;
		bool is_private;
	} addrs[] = {
		{
			.addr = { { { 192, 0, 2, 1 } } },
			.is_private = true,
		},
		{
			.addr = { { { 10, 1, 2, 1 } } },
			.is_private = true,
		},
		{
			.addr = { { { 100, 124, 2, 1 } } },
			.is_private = true,
		},
		{
			.addr = { { { 172, 24, 100, 12 } } },
			.is_private = true,
		},
		{
			.addr = { { { 172, 15, 254, 255 } } },
			.is_private = false,
		},
		{
			.addr = { { { 172, 16, 0, 0 } } },
			.is_private = true,
		},
		{
			.addr = { { { 192, 168, 10, 122 } } },
			.is_private = true,
		},
		{
			.addr = { { { 192, 51, 100, 255 } } },
			.is_private = true,
		},
		{
			.addr = { { { 203, 0, 113, 122 } } },
			.is_private = true,
		},
		{
			.addr = { { { 1, 2, 3, 4 } } },
			.is_private = false,
		},
		{
			.addr = { { { 192, 1, 32, 4 } } },
			.is_private = false,
		},
	};

	for (int i = 0; i < ARRAY_SIZE(addrs); i++) {
		ret = net_ipv4_is_private_addr(&addrs[i].addr);
		zassert_equal(ret, addrs[i].is_private, "Address %s check failed",
			      net_sprint_ipv4_addr(&addrs[i].addr));
	}

}

void clear_addr4(struct net_if *iface, struct net_if_addr *addr, void *user_data)
{
	addr->is_used = false;
}

static void test_before(void *f)
{
	ARG_UNUSED(f);

	net_if_ipv4_addr_foreach(default_iface, clear_addr4, NULL);
	net_if_ipv4_addr_foreach(second_iface, clear_addr4, NULL);
}

void *test_setup(void)
{
	default_iface = net_if_get_by_index(1);
	second_iface = net_if_get_by_index(2);

	return NULL;
}

ZTEST_SUITE(ip_addr_fn, NULL, test_setup, test_before, NULL, NULL);
