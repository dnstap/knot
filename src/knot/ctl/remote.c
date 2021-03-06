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

#include <sys/stat.h>
#include "knot/ctl/remote.h"
#include "common/log.h"
#include "common/fdset.h"
#include "knot/knot.h"
#include "knot/conf/conf.h"
#include "knot/server/net.h"
#include "knot/server/tcp-handler.h"
#include "knot/server/zones.h"
#include "libknot/packet/wire.h"
#include "common/descriptor.h"
#include "libknot/tsig-op.h"
#include "libknot/rdata/rdname.h"
#include "libknot/rdata/soa.h"
#include "libknot/dnssec/random.h"
#include "knot/dnssec/zone-sign.h"
#include "knot/dnssec/zone-nsec.h"

#define KNOT_CTL_REALM "knot."
#define KNOT_CTL_REALM_EXT ("." KNOT_CTL_REALM)
#define CMDARGS_BUFLEN (1024*1024) /* 1M */
#define CMDARGS_BUFLEN_LOG 256
#define KNOT_CTL_SOCKET_UMASK 0007

/*! \brief Remote command structure. */
typedef struct remote_cmdargs_t {
	const knot_rrset_t *arg;
	unsigned argc;
	knot_rcode_t rc;
	char resp[CMDARGS_BUFLEN];
	size_t rlen;
} remote_cmdargs_t;

/*! \brief Callback prototype for remote commands. */
typedef int (*remote_cmdf_t)(server_t*, remote_cmdargs_t*);

/*! \brief Callback prototype for per-zone operations. */
typedef int (remote_zonef_t)(server_t*, const zone_t *);

/*! \brief Remote command table item. */
typedef struct remote_cmd_t {
	const char *name;
	remote_cmdf_t f;
} remote_cmd_t;

/* Forward decls. */
static int remote_c_stop(server_t *s, remote_cmdargs_t* a);
static int remote_c_reload(server_t *s, remote_cmdargs_t* a);
static int remote_c_refresh(server_t *s, remote_cmdargs_t* a);
static int remote_c_status(server_t *s, remote_cmdargs_t* a);
static int remote_c_zonestatus(server_t *s, remote_cmdargs_t* a);
static int remote_c_flush(server_t *s, remote_cmdargs_t* a);
static int remote_c_signzone(server_t *s, remote_cmdargs_t* a);

/*! \brief Table of remote commands. */
struct remote_cmd_t remote_cmd_tbl[] = {
	{ "stop",      &remote_c_stop },
	{ "reload",    &remote_c_reload },
	{ "refresh",   &remote_c_refresh },
	{ "status",    &remote_c_status },
	{ "zonestatus",&remote_c_zonestatus },
	{ "flush",     &remote_c_flush },
	{ "signzone",  &remote_c_signzone },
	{ NULL,        NULL }
};

/* Private APIs. */

/*! \brief Apply callback to all zones specified by RDATA of CNAME RRs. */
static int remote_rdata_apply(server_t *s, remote_cmdargs_t* a, remote_zonef_t *cb)
{
	if (!s || !a || !cb) {
		return KNOT_EINVAL;
	}

	zone_t *zone = NULL;
	int ret = KNOT_EOK;

	for (unsigned i = 0; i < a->argc; ++i) {
		/* Process all zones in data section. */
		const knot_rrset_t *rr = &a->arg[i];
		if (rr->type != KNOT_RRTYPE_NS) {
			continue;
		}

		uint16_t rr_count = rr->rrs.rr_count;
		for (uint16_t i = 0; i < rr_count; i++) {
			const knot_dname_t *dn = knot_ns_name(&rr->rrs, i);
			rcu_read_lock();
			zone = knot_zonedb_find(s->zone_db, dn);
			if (cb(s, zone) != KNOT_EOK) {
				a->rc = KNOT_RCODE_SERVFAIL;
			}
			rcu_read_unlock();
		}
	}

	return ret;
}

/*! \brief Zone refresh callback. */
static int remote_zone_refresh(server_t *server, const zone_t *zone)
{
	if (!server || !zone) {
		return KNOT_EINVAL;
	}

	zones_schedule_refresh((zone_t *)zone, REFRESH_NOW);

	return KNOT_EOK;
}

/*! \brief Zone flush callback. */
static int remote_zone_flush(server_t *server, const zone_t *zone)
{
	if (!server || !zone) {
		return KNOT_EINVAL;
	}

	zones_schedule_zonefile_sync((zone_t *)zone, 0);

	return KNOT_EOK;
}

/*! \brief Sign zone callback. */
static int remote_zone_sign(server_t *server, const zone_t *zone)
{
	if (!server || !zone) {
		return KNOT_EINVAL;
	}

	char *zone_name = knot_dname_to_str(zone->name);
	log_server_info("Requested zone resign for '%s'.\n", zone_name);
	free(zone_name);

	uint32_t refresh_at = 0;
	zone_t *wzone = (zone_t *)zone;

	zones_cancel_dnssec(wzone);
	zones_dnssec_sign(wzone, true, &refresh_at);
	zones_schedule_dnssec(wzone, refresh_at);

	return KNOT_EOK;
}

/*!
 * \brief Remote command 'stop' handler.
 *
 * QNAME: stop
 * DATA: NULL
 */
static int remote_c_stop(server_t *s, remote_cmdargs_t* a)
{
	UNUSED(a);
	UNUSED(s);
	return KNOT_CTL_STOP;
}

/*!
 * \brief Remote command 'reload' handler.
 *
 * QNAME: reload
 * DATA: NULL
 */
static int remote_c_reload(server_t *s, remote_cmdargs_t* a)
{
	UNUSED(a);
	return server_reload(s, conf()->filename);
}

/*!
 * \brief Remote command 'status' handler.
 *
 * QNAME: status
 * DATA: NONE
 */
static int remote_c_status(server_t *s, remote_cmdargs_t* a)
{
	UNUSED(s);
	UNUSED(a);
	/*! \todo #2035 Add some TXT RRs with stats. */
	dbg_server("remote: %s\n", __func__);
	return KNOT_EOK;
}

static char *dnssec_info(const zone_t *zone, char *buf, size_t buf_size)
{
	assert(zone && zone->dnssec.timer);
	assert(buf);

	time_t diff_time = zone->dnssec.timer->tv.tv_sec;
	struct tm *t = localtime(&diff_time);

	size_t written = strftime(buf, buf_size, "%c", t);
	if (written == 0) {
		return NULL;
	}

	return buf;
}

/*!
 * \brief Remote command 'zonestatus' handler.
 *
 * QNAME: zonestatus
 * DATA: NONE
 */
static int remote_c_zonestatus(server_t *s, remote_cmdargs_t* a)
{
	dbg_server("remote: %s\n", __func__);
	char *dst = a->resp;
	size_t rb = sizeof(a->resp) - 1;

	int ret = KNOT_EOK;
	rcu_read_lock();

	knot_zonedb_iter_t it;
	knot_zonedb_iter_begin(s->zone_db, &it);
	while(!knot_zonedb_iter_finished(&it)) {
		const zone_t *zone = knot_zonedb_iter_val(&it);

		/* Fetch latest serial. */
		const knot_rdataset_t *soa_rrs = NULL;
		uint32_t serial = 0;
		if (zone->contents) {
			soa_rrs = node_rdataset(zone->contents->apex,
			                        KNOT_RRTYPE_SOA);
			assert(soa_rrs != NULL);
			serial = knot_soa_serial(soa_rrs);
		}

		/* Evalute zone type. */
		const char *state = NULL;
		if (serial == 0)  {
			state = "bootstrap";
		} else if (zone_master(zone) != NULL) {
			state = "xfer";
		}

		/* Evaluate zone state. */
		char *when = NULL;
		if (zone->xfr_in.state == XFR_PENDING) {
			when = strdup("pending");
		} else if (zone->xfr_in.timer && zone->xfr_in.timer->tv.tv_sec != 0) {
			struct timeval now, dif;
			gettimeofday(&now, 0);
			timersub(&zone->xfr_in.timer->tv, &now, &dif);
			when = malloc(64);
			if (when == NULL) {
				ret = KNOT_ENOMEM;
				break;
			}
			/*! Workaround until proper zone fetching API and locking
			 *  is implemented (ref #31)
			 */
			if (dif.tv_sec < 0) {
				memcpy(when, "busy", 5);
			} else if (snprintf(when, 64, "in %uh%um%us",
			                    (unsigned int)dif.tv_sec / 3600,
			                    (unsigned int)(dif.tv_sec % 3600) / 60,
			                    (unsigned int)dif.tv_sec % 60) < 0) {
				free(when);
				ret = KNOT_ESPACE;
				break;
			}
		} else {
			when = strdup("idle");
		}

		/* Workaround, some platforms ignore 'size' with snprintf() */
		char buf[512] = { '\0' };
		char dnssec_buf[128] = { '\0' };
		int n = snprintf(buf, sizeof(buf),
		                 "%s\ttype=%s | serial=%u | %s %s | %s %s\n",
		                 zone->conf->name,
		                 zone_master(zone) ? "slave" : "master",
		                 serial,
		                 state ? state : "",
		                 when ? when : "",
		                 zone->conf->dnssec_enable ? "automatic DNSSEC, resigning at:" : "",
		                 zone->conf->dnssec_enable ? dnssec_info(zone, dnssec_buf, sizeof(dnssec_buf)) : "");
		free(when);
		if (n < 0 || (size_t)n > rb) {
			*dst = '\0';
			ret = KNOT_ESPACE;
			break;
		}

		assert(n <= sizeof(buf));

		memcpy(dst, buf, n);
		rb -= n;
		dst += n;

		knot_zonedb_iter_next(&it);
	}
	rcu_read_unlock();

	a->rlen = sizeof(a->resp) - 1 - rb;
	return ret;
}

/*!
 * \brief Remote command 'refresh' handler.
 *
 * QNAME: refresh
 * DATA: NONE for all zones
 *       CNAME RRs with zones in RDATA
 */
static int remote_c_refresh(server_t *s, remote_cmdargs_t* a)
{
	/* Refresh all. */
	dbg_server("remote: %s\n", __func__);
	if (a->argc == 0) {
		dbg_server_verb("remote: refreshing all zones\n");
		knot_zonedb_foreach(s->zone_db, zones_schedule_refresh, REFRESH_NOW);
		return KNOT_EOK;
	}

	/* Refresh specific zones. */
	return remote_rdata_apply(s, a, &remote_zone_refresh);
}

/*!
 * \brief Remote command 'flush' handler.
 *
 * QNAME: flush
 * DATA: NONE for all zones
 *       CNAME RRs with zones in RDATA
 */
static int remote_c_flush(server_t *s, remote_cmdargs_t* a)
{
	/* Flush all. */
	dbg_server("remote: %s\n", __func__);
	if (a->argc == 0) {
		int ret = 0;
		dbg_server_verb("remote: flushing all zones\n");
		rcu_read_lock();
		knot_zonedb_iter_t it;
		knot_zonedb_iter_begin(s->zone_db, &it);
		while(!knot_zonedb_iter_finished(&it)) {
			ret = remote_zone_flush(s, knot_zonedb_iter_val(&it));
			knot_zonedb_iter_next(&it);
		}
		rcu_read_unlock();
		return ret;
	}

	/* Flush specific zones. */
	return remote_rdata_apply(s, a, &remote_zone_flush);
}

/*!
 * \brief Remote command 'signzone' handler.
 *
 */
static int remote_c_signzone(server_t *server, remote_cmdargs_t* arguments)
{
	dbg_server("remote: %s\n", __func__);

	if (arguments->argc == 0) {
		log_server_error("signzone for all zone was requested\n");
		// TODO
		return KNOT_ENOTSUP;
	}

	return remote_rdata_apply(server, arguments, remote_zone_sign);
}

/*!
 * \brief Prepare and send error response.
 * \param c Client fd.
 * \param buf Query buffer.
 * \param buflen Query size.
 * \return number of bytes sent
 */
static int remote_senderr(int c, uint8_t *qbuf, size_t buflen)
{
	knot_wire_set_qr(qbuf);
	knot_wire_set_rcode(qbuf, KNOT_RCODE_REFUSED);
	return tcp_send(c, qbuf, buflen);
}

/* Public APIs. */

int remote_bind(conf_iface_t *desc)
{
	if (desc == NULL) {
		return KNOT_EINVAL;
	}

	char addr_str[SOCKADDR_STRLEN] = {0};
	sockaddr_tostr(&desc->addr, addr_str, sizeof(addr_str));
	log_server_info("Binding remote control interface to '%s'.\n", addr_str);

	/* Create new socket. */
	mode_t old_umask = umask(KNOT_CTL_SOCKET_UMASK);
	int sock = net_bound_socket(SOCK_STREAM, &desc->addr);
	umask(old_umask);
	if (sock < 0) {
		return sock;
	}

	/* Start listening. */
	int ret = listen(sock, TCP_BACKLOG_SIZE);
	if (ret < 0) {
		log_server_error("Could not bind to '%s'.\n", addr_str);
		close(sock);
		return ret;
	}

	return sock;
}

int remote_unbind(conf_iface_t *desc, int sock)
{
	if (desc == NULL || sock < 0) {
		return KNOT_EINVAL;
	}

	/* Remove control socket file.  */
	if (desc->addr.ss_family == AF_UNIX) {
		char addr_str[SOCKADDR_STRLEN] = {0};
		sockaddr_tostr(&desc->addr, addr_str, sizeof(addr_str));
		unlink(addr_str);
	}

	return close(sock);
}

int remote_poll(int sock)
{
	/* Wait for events. */
	fd_set rfds;
	FD_ZERO(&rfds);
	if (sock > -1) {
		FD_SET(sock, &rfds);
	} else {
		sock = -1; /* Make sure n == r + 1 == 0 */
	}

	return fdset_pselect(sock + 1, &rfds, NULL, NULL, NULL, NULL);
}

int remote_recv(int sock, struct sockaddr *addr, uint8_t* buf, size_t *buflen)
{
	int c = tcp_accept(sock);
	if (c < 0) {
		dbg_server("remote: couldn't accept incoming connection\n");
		return c;
	}

	/* Receive data. */
	int n = tcp_recv(c, buf, *buflen, addr);
	*buflen = n;
	if (n <= 0) {
		dbg_server("remote: failed to receive data\n");
		close(c);
		return KNOT_ECONNREFUSED;
	}

	return c;
}

int remote_parse(knot_pkt_t* pkt)
{
	return knot_pkt_parse(pkt, 0);
}

static int remote_send_chunk(int c, knot_pkt_t *query, const char* d, uint16_t len)
{
	knot_pkt_t *resp = knot_pkt_new(NULL, KNOT_WIRE_MAX_PKTSIZE, &query->mm);
	if (!resp) {
		return KNOT_ENOMEM;
	}

	/* Initialize response. */
	int ret = knot_pkt_init_response(resp, query);
	if (ret != KNOT_EOK) {
		goto failed;
	}

	/* Write to NS section. */
	ret = knot_pkt_begin(resp, KNOT_AUTHORITY);
	assert(ret == KNOT_EOK);

	/* Create TXT RR with result. */
	knot_rrset_t rr;
	ret = remote_build_rr(&rr, "result.", KNOT_RRTYPE_TXT);
	if (ret != KNOT_EOK) {
		goto failed;
	}

	ret = remote_create_txt(&rr, d, len);
	assert(ret == KNOT_EOK);

	ret = knot_pkt_put(resp, 0, &rr, KNOT_PF_FREE);
	if (ret != KNOT_EOK) {
		knot_rrset_clear(&rr, NULL);
		goto failed;
	}

	ret = tcp_send(c, resp->wire, resp->size);

failed:

	/* Free packet. */
	knot_pkt_free(&resp);

	return ret;
}

static void log_command(const char *cmd, const remote_cmdargs_t* args)
{
	char params[CMDARGS_BUFLEN_LOG] = { 0 };
	size_t rest = CMDARGS_BUFLEN_LOG;

	for (unsigned i = 0; i < args->argc; i++) {
		const knot_rrset_t *rr = &args->arg[i];
		if (rr->type != KNOT_RRTYPE_NS) {
			continue;
		}

		uint16_t rr_count = rr->rrs.rr_count;
		for (uint16_t j = 0; j < rr_count; j++) {
			const knot_dname_t *dn = knot_ns_name(&rr->rrs, j);
			char *name = knot_dname_to_str(dn);

			int ret = snprintf(params, rest, " %s", name);
			free(name);
			if (ret <= 0 || ret >= rest) {
				break;
			}
			rest -= ret;
		}
	}

	log_server_info("Remote command: '%s%s'\n", cmd, params);
}

int remote_answer(int sock, server_t *s, knot_pkt_t *pkt)
{
	if (sock < 0 || s == NULL || pkt == NULL) {
		return KNOT_EINVAL;
	}

	/* Prerequisites:
	 * QCLASS: CH
	 * QNAME: <CMD>.KNOT_CTL_REALM.
	 */
	const knot_dname_t *qname = knot_pkt_qname(pkt);
	if (knot_pkt_qclass(pkt) != KNOT_CLASS_CH) {
		dbg_server("remote: qclass != CH\n");
		return KNOT_EMALF;
	}

	knot_dname_t *realm = knot_dname_from_str(KNOT_CTL_REALM);
	if (!knot_dname_is_sub(qname, realm) != 0) {
		dbg_server("remote: qname != *%s\n", KNOT_CTL_REALM_EXT);
		knot_dname_free(&realm, NULL);
		return KNOT_EMALF;
	}
	knot_dname_free(&realm, NULL);

	/* Command:
	 * QNAME: leftmost label of QNAME
	 */
	size_t cmd_len = *qname;
	char *cmd = strndup((char*)qname + 1, cmd_len);

	/* Data:
	 * NS: TSIG
	 * AR: data
	 */
	int ret = KNOT_EOK;
	remote_cmdargs_t* args = malloc(sizeof(remote_cmdargs_t));
	if (!args) {
		free(cmd);
		return KNOT_ENOMEM;
	}
	memset(args, 0, sizeof(remote_cmdargs_t));

	const knot_pktsection_t *authority = knot_pkt_section(pkt, KNOT_AUTHORITY);
	args->arg = authority->rr;
	args->argc = authority->count;
	args->rc = KNOT_RCODE_NOERROR;

	log_command(cmd, args);

	remote_cmd_t *c = remote_cmd_tbl;
	while (c->name != NULL) {
		if (strcmp(cmd, c->name) == 0) {
			ret = c->f(s, args);
			break;
		}
		++c;
	}

	/* Prepare response. */
	if (ret != KNOT_EOK || args->rlen == 0) {
		args->rlen = strlen(knot_strerror(ret));
		strncpy(args->resp, knot_strerror(ret), args->rlen);
	}

	unsigned p = 0;
	size_t chunk = 16384;
	for (; p + chunk < args->rlen; p += chunk) {
		remote_send_chunk(sock, pkt, args->resp + p, chunk);
	}

	unsigned r = args->rlen - p;
	if (r > 0) {
		remote_send_chunk(sock, pkt, args->resp + p, r);
	}

	free(args);
	free(cmd);
	return ret;
}

int remote_process(server_t *s, conf_iface_t *ctl_if, int sock,
                   uint8_t* buf, size_t buflen)
{
	knot_pkt_t *pkt =  knot_pkt_new(buf, buflen, NULL);
	if (pkt == NULL) {
		return KNOT_ENOMEM;
	}

	/* Initialize remote party address. */
	struct sockaddr_storage ss;
	memset(&ss, 0, sizeof(struct sockaddr_storage));

	/* Accept incoming connection and read packet. */
	int client = remote_recv(sock, (struct sockaddr *)&ss, pkt->wire, &buflen);
	if (client < 0) {
		dbg_server("remote: couldn't receive query = %d\n", client);
		knot_pkt_free(&pkt);
		return client;
	} else {
		pkt->size = buflen;
	}

	/* Parse packet and answer if OK. */
	int ret = remote_parse(pkt);
	if (ret == KNOT_EOK && ctl_if->addr.ss_family != AF_UNIX) {

		/* Check ACL list. */
		char addr_str[SOCKADDR_STRLEN] = {0};
		sockaddr_tostr(&ss, addr_str, sizeof(addr_str));
		knot_tsig_key_t *tsig_key = NULL;
		const knot_dname_t *tsig_name = NULL;
		if (pkt->tsig_rr) {
			tsig_name = pkt->tsig_rr->owner;
		}
		acl_match_t *match = acl_find(conf()->ctl.acl, &ss, tsig_name);
		uint16_t ts_rc = 0;
		uint16_t ts_trc = 0;
		uint64_t ts_tmsigned = 0;
		if (match == NULL) {
			log_server_warning("Denied remote control for '%s' "
			                   "(doesn't match ACL).\n",
			                   addr_str);
			remote_senderr(client, pkt->wire, pkt->size);
			ret = KNOT_EACCES;
			goto finish;
		} else {
			tsig_key = match->key;
		}

		/* Check TSIG. */
		if (tsig_key) {
			if (pkt->tsig_rr == NULL) {
				log_server_warning("Denied remote control for '%s' "
				                   "(key required).\n",
				                   addr_str);
				remote_senderr(client, pkt->wire, pkt->size);
				ret = KNOT_EACCES;
				goto finish;
			}
			ret = zones_verify_tsig_query(pkt, tsig_key, &ts_rc,
			                              &ts_trc, &ts_tmsigned);
			if (ret != KNOT_EOK) {
				log_server_warning("Denied remote control for '%s' "
				                   "(key verification failed).\n",
				                   addr_str);
				remote_senderr(client, pkt->wire, pkt->size);
				ret = KNOT_EACCES;
				goto finish;
			}
		}
	}

	/* Answer packet. */
	if (ret == KNOT_EOK) {
		ret = remote_answer(client, s, pkt);
	}

finish:
	knot_pkt_free(&pkt);
	close(client);
	return ret;
}

knot_pkt_t* remote_query(const char *query, const knot_tsig_key_t *key)
{
	if (!query) {
		return NULL;
	}

	knot_pkt_t *pkt = knot_pkt_new(NULL, KNOT_WIRE_MAX_PKTSIZE, NULL);
	if (!pkt) {
		return NULL;
	}

	knot_wire_set_id(pkt->wire, knot_random_uint16_t());
	knot_pkt_reserve(pkt, tsig_wire_maxsize(key));

	/* Question section. */
	char *qname = strcdup(query, KNOT_CTL_REALM_EXT);
	knot_dname_t *dname = knot_dname_from_str(qname);
	free(qname);
	if (!dname) {
		knot_pkt_free(&pkt);
		return NULL;
	}

	/* Cannot return != KNOT_EOK, but still. */
	if (knot_pkt_put_question(pkt, dname, KNOT_CLASS_CH, KNOT_RRTYPE_ANY) != KNOT_EOK) {
		knot_pkt_free(&pkt);
		knot_dname_free(&dname, NULL);
		return NULL;
	}

	knot_dname_free(&dname, NULL);
	return pkt;
}

int remote_query_sign(uint8_t *wire, size_t *size, size_t maxlen,
                      const knot_tsig_key_t *key)
{
	if (!wire || !size || !key) {
		return KNOT_EINVAL;
	}

	size_t dlen = knot_tsig_digest_length(key->algorithm);
	uint8_t *digest = malloc(dlen);
	if (!digest) {
		return KNOT_ENOMEM;
	}

	int ret = knot_tsig_sign(wire, size, maxlen, NULL, 0, digest, &dlen,
	                         key, 0, 0);
	free(digest);

	return ret;
}

int remote_build_rr(knot_rrset_t *rr, const char *k, uint16_t t)
{
	if (!k) {
		return KNOT_EINVAL;
	}

	/* Assert K is FQDN. */
	knot_dname_t *key = knot_dname_from_str(k);
	if (key == NULL) {
		return KNOT_ENOMEM;
	}

	/* Init RRSet. */
	knot_rrset_init(rr, key, t, KNOT_CLASS_CH);

	return KNOT_EOK;
}

int remote_create_txt(knot_rrset_t *rr, const char *v, size_t v_len)
{
	if (!rr || !v) {
		return KNOT_EINVAL;
	}

	/* Number of chunks. */
	const size_t K = 255;
	unsigned chunks = v_len / K + 1;
	uint8_t raw[v_len + chunks];
	memset(raw, 0, v_len + chunks);

	/* Write TXT item. */
	unsigned p = 0;
	size_t off = 0;
	if (v_len > K) {
		for (; p + K < v_len; p += K) {
			raw[off++] = (uint8_t)K;
			memcpy(raw + off, v + p, K);
			off += K;
		}
	}
	unsigned r = v_len - p;
	if (r > 0) {
		raw[off++] = (uint8_t)r;
		memcpy(raw + off, v + p, r);
	}

	return knot_rrset_add_rdata(rr, raw, v_len + chunks, 0, NULL);
}

int remote_create_ns(knot_rrset_t *rr, const char *d)
{
	if (!rr || !d) {
		return KNOT_EINVAL;
	}

	/* Create dname. */
	knot_dname_t *dn = knot_dname_from_str(d);
	if (!dn) {
		return KNOT_ERROR;
	}

	/* Build RDATA. */
	int dn_size = knot_dname_size(dn);
	int result = knot_rrset_add_rdata(rr, dn, dn_size, 0, NULL);
	knot_dname_free(&dn, NULL);

	return result;
}

int remote_print_txt(const knot_rrset_t *rr, uint16_t i)
{
	if (!rr || rr->rrs.rr_count < 1) {
		return -1;
	}

	/* Packet parser should have already checked the packet validity. */
	char buf[256];
	uint16_t parsed = 0;
	uint16_t rlen = knot_rrset_rr_size(rr, i);
	uint8_t *p = knot_rrset_rr_rdata(rr, i);
	while (parsed < rlen) {
		memcpy(buf, (const char*)(p+1), *p);
		buf[*p] = '\0';
		printf("%s", buf);
		parsed += *p + 1;
		p += *p + 1;
	}
	return KNOT_EOK;
}
