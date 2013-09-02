#!/usr/bin/env python3

'''Test for AXFR from Bind to Knot'''

import dnstest

t = dnstest.DnsTest()

master = t.server("knot")
slave = t.server("bind")
zones = t.zone_rnd(2)

t.link(zones, master, slave)

t.start()

master.zones_wait(zones)
slave.zones_wait(zones)
t.xfr_diff(master, slave, zones)

t.stop()