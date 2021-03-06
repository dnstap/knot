/*  Copyright (C) 2011 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <tap/basic.h>

#include "common/errcode.h"
#include "common/sockaddr.h"
#include "knot/updates/acl.h"

int main(int argc, char *argv[])
{
	plan(14);

	// 1. Create an ACL
	acl_match_t *match = NULL;
	acl_t *acl = acl_new();
	ok(acl != 0, "acl: new");

	// 2. Create IPv4 address
	struct sockaddr_storage test_v4;
	int ret = sockaddr_set(&test_v4, AF_INET, "127.0.0.1", 12345);
	ok(ret == KNOT_EOK, "acl: new IPv4 address");

	// 3. Create IPv6 address
	struct sockaddr_storage test_v6;
	ret = sockaddr_set(&test_v6, AF_INET6, "::1", 54321);
	ok(ret == KNOT_EOK, "acl: new IPv6 address");

	// 4. Create simple IPv4 rule
	ret = acl_insert(acl, &test_v4, IPV4_PREFIXLEN, NULL);
	ok(ret == KNOT_EOK, "acl: inserted IPv4 rule");

	// 5. Create simple IPv6 rule
	ret = acl_insert(acl, &test_v6, IPV6_PREFIXLEN, NULL);
	ok(ret == KNOT_EOK, "acl: inserted IPv6 rule");

	// 7. Attempt to match unmatching address
	struct sockaddr_storage unmatch_v4;
	sockaddr_set(&unmatch_v4, AF_INET, "10.10.10.10", 24424);
	match = acl_find(acl, &unmatch_v4, NULL);
	ok(match == NULL, "acl: matching non-existing address");

	// 8. Attempt to match unmatching IPv6 address
	struct sockaddr_storage unmatch_v6;
	sockaddr_set(&unmatch_v6, AF_INET6, "2001:db8::1428:57ab", 24424);
	match = acl_find(acl, &unmatch_v6, NULL);
	ok(match == NULL, "acl: matching non-existing IPv6 address");

	// 9. Attempt to match matching address
	match = acl_find(acl, &test_v4, NULL);
	ok(match != NULL, "acl: matching existing address");

	// 10. Attempt to match matching address
	match = acl_find(acl, &test_v6, NULL);
	ok(match != NULL, "acl: matching existing IPv6 address");

	// 14. Attempt to match subnet
	struct sockaddr_storage match_pf4, test_pf4;
	sockaddr_set(&match_pf4, AF_INET, "192.168.1.0", 0);
	acl_insert(acl, &match_pf4, 24, NULL);
	sockaddr_set(&test_pf4, AF_INET, "192.168.1.20", 0);
	match = acl_find(acl, &test_pf4, NULL);
	ok(match != NULL, "acl: searching address in matching prefix /24");

	// 15. Attempt to search non-matching subnet
	sockaddr_set(&test_pf4, AF_INET, "192.168.2.20", 0);
	match = acl_find(acl, &test_pf4, NULL);
	ok(match == NULL, "acl: searching address in non-matching prefix /24");

	// 16. Attempt to match v6 subnet
	struct sockaddr_storage match_pf6, test_pf6;
	sockaddr_set(&match_pf6, AF_INET6, "2001:0DB8:0400:000e:0:0:0:AB00", 0);
	acl_insert(acl, &match_pf6, 120, NULL);
	sockaddr_set(&test_pf6, AF_INET6, "2001:0DB8:0400:000e:0:0:0:AB03", 0);
	match = acl_find(acl, &test_pf6, NULL);
	ok(match != NULL, "acl: searching v6 address in matching prefix /120");

	// 17. Attempt to search non-matching subnet
	sockaddr_set(&test_pf6, AF_INET6, "2001:0DB8:0400:000e:0:0:0:CCCC", 0);
	match = acl_find(acl, &test_pf6, NULL);
	ok(match == NULL, "acl: searching v6 address in non-matching prefix /120");

	// 18. Scenario after truncating
	acl_truncate(acl);
	sockaddr_set(&test_pf6, AF_INET6, "2001:a1b0:e11e:50d1::3:300", 0);
	acl_insert(acl, &test_pf6, IPV6_PREFIXLEN, NULL);
	sockaddr_set(&test_pf4, AF_INET, "231.17.67.223", 0);
	acl_insert(acl, &test_pf4, IPV4_PREFIXLEN, NULL);
	sockaddr_set(&test_pf4, AF_INET, "82.87.48.136", 0);
	acl_insert(acl, &test_pf4, IPV4_PREFIXLEN, NULL);
	sockaddr_set(&match_pf4, AF_INET, "82.87.48.136", 12345);
	match = acl_find(acl, &match_pf4, NULL);
	ok(match != NULL, "acl: scenario after truncating");
	acl_delete(&acl);

	// Return
	return 0;
}
