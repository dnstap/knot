/*!
 * \file ns_proc_query.h
 *
 * \author Marek Vavrusa <marek.vavrusa@nic.cz>
 *
 * \brief Query processor.
 *
 * \addtogroup query_processing
 * @{
 */
/*  Copyright (C) 2013 CZ.NIC, z.s.p.o. <knot-dns@labs.nic.cz>

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

#ifndef _KNOT_NS_PROC_QUERY_H_
#define _KNOT_NS_PROC_QUERY_H_

#include "libknot/nameserver/name-server.h"
#include "common/acl.h"

/* Query processing module implementation. */
extern const ns_proc_module_t _ns_proc_query;
#define NS_PROC_QUERY (&_ns_proc_query)
#define NS_PROC_QUERY_ID 1

/*! \brief Query processing logging common base. */
#define NS_PROC_LOG(severity, qdata, what, msg, ...) do { \
	sockaddr_t *addr = &(qdata)->param->query_source; \
	char addr_str[SOCKADDR_STRLEN] = {0}; \
	sockaddr_tostr(addr, addr_str, sizeof(addr_str)); \
	char *zone_str = knot_dname_to_str(knot_pkt_qname((qdata)->query)); \
	log_msg(LOG_SERVER, severity, what msg "\n", \
	                zone_str, \
	                addr_str, sockaddr_portnum(addr), ##__VA_ARGS__); \
	free(zone_str); \
	} while (0)

/*! \brief Query logging common base. */
#define QUERY_LOG(severity, qdata, what, msg...) \
	NS_PROC_LOG(severity, qdata, what " of '%s' from '%s:%d': ", msg)

/*! \brief Answer logging common base. */
#define ANSWER_LOG(severity, qdata, what, msg...)  \
	NS_PROC_LOG(severity, qdata, what " of '%s' to '%s:%d': ", msg)

/* Query processing specific flags. */
enum ns_proc_query_flag {
	NS_QUERY_NO_AXFR    = 1 << 0, /* Don't process AXFR */
	NS_QUERY_NO_IXFR    = 1 << 1, /* Don't process IXFR */
	NS_QUERY_LIMIT_ANY  = 1 << 2, /* Limit ANY QTYPE (respond with TC=1) */
	NS_QUERY_LIMIT_RATE = 1 << 3, /* Apply rate limits. */
	NS_QUERY_LIMIT_SIZE = 1 << 4  /* Apply UDP size limit. */
};

/* Module load parameters. */
struct ns_proc_query_param {
	uint16_t   proc_flags;
	sockaddr_t query_source;
};

/*! \brief Query processing intermediate data. */
struct query_data {
	uint16_t rcode;          /*!< Resulting RCODE. */
	uint16_t rcode_tsig;     /*!< Resulting TSIG RCODE. */
	uint16_t packet_type;    /*!< Resolved packet type. */
	knot_pkt_t *query;       /*!< Query to be solved. */
	const knot_zone_t *zone; /*!< Zone from which is answered. */
	list_t wildcards;        /*!< Visited wildcards. */

	/* Current processed name and nodes. */
	const knot_node_t *node, *encloser, *previous;
	const knot_dname_t *name;

	/* Original QNAME case. */
	uint8_t orig_qname[KNOT_DNAME_MAXLEN];

	/* Extensions. */
	void *ext;
	void (*ext_cleanup)(struct query_data*); /*!< Extensions cleanup callback. */
	ns_sign_context_t sign;            /*!< Signing context. */

	/* Everything below should be kept on reset. */
	struct ns_proc_query_param *param; /*!< Module parameters. */
	mm_ctx_t *mm;                      /*!< Memory context. */
};

/*! \brief Visited wildcard node list. */
struct wildcard_hit {
	node_t n;
	const knot_node_t *node;   /* Visited node. */
	const knot_dname_t *sname; /* Name leading to this node. */
};

/*!
 * \brief Initialize query processing context.
 *
 * \param ctx
 * \param module_param
 * \return MORE (awaits query)
 */
int ns_proc_query_begin(ns_proc_context_t *ctx, void *module_param);

/*!
 * \brief Reset query processing context.
 *
 * \param ctx
 * \return MORE (awaits next query)
 */
int ns_proc_query_reset(ns_proc_context_t *ctx);

/*!
 * \brief Finish and close current query processing.
 *
 * \param ctx
 * \return NOOP (context will be inoperable further on)
 */
int ns_proc_query_finish(ns_proc_context_t *ctx);

/*!
 * \brief Put query into query processing context.
 *
 * \param pkt
 * \param ctx
 * \retval NOOP (unsupported query)
 * \retval FULL (ready to write answer)
 */
int ns_proc_query_in(knot_pkt_t *pkt, ns_proc_context_t *ctx);

/*!
 * \brief Make query response.
 *
 * \param pkt
 * \param ctx
 * \retval DONE (finished response)
 * \retval FULL (partial response, send it and call again)
 * \retval FAIL (failure)
 */
int ns_proc_query_out(knot_pkt_t *pkt, ns_proc_context_t *ctx);

/*!
 * \brief Make an error response.
 *
 * \param pkt
 * \param ctx
 * \retval DONE (finished response)
 * \retval FAIL (failure)
 */
int ns_proc_query_err(knot_pkt_t *pkt, ns_proc_context_t *ctx);

/*!
 * \brief Check current query against ACL.
 *
 * \param acl
 * \param qdata
 * \return true if accepted, false if denied.
 */
bool ns_proc_query_acl_check(acl_t *acl, struct query_data *qdata);

/*!
 * \brief Verify current query transaction security and update query data.
 *
 * \param qdata
 * \retval KNOT_EOK
 * \retval KNOT_TSIG_EBADKEY
 * \retval KNOT_TSIG_EBADSIG
 * \retval KNOT_TSIG_EBADTIME
 * \retval (other generic errors)
 */
int ns_proc_query_verify(struct query_data *qdata);

/*!
 * \brief Sign query response if applicable.
 *
 * \param pkt
 * \param qdata
 * \retval KNOT_EOK
 * \retval (other generic errors)
 */
int ns_proc_query_sign_response(knot_pkt_t *pkt, struct query_data *qdata);

#endif /* _KNOT_NS_PROC_QUERY_H_ */

/*! @} */