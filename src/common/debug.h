/*!
 * \file common/debug.h
 *
 * \author Jan Kadlec <jan.kadlec.@nic.cz>
 * \author Lubos Slovak <lubos.slovak@nic.cz>
 * \author Marek Vavrusa <marek.vavrusa@nic.cz>
 *
 * \brief Functions for debug output of structures.
 *
 * \addtogroup libknot
 * @{
 */
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

#ifndef _KNOT_DEBUG_H_
#define _KNOT_DEBUG_H_

#include <stdint.h>
#include <stdio.h>

#include "common/log.h"
#include "common/print.h"

/*
 * Debug macros
 */
#ifdef KNOT_ZONES_DEBUG
  #define KNOT_ZONE_DEBUG
  #define KNOT_ZONEDB_DEBUG
  #define KNOT_NODE_DEBUG
  #define KNOT_ZONEDIFF_DEBUG
#endif

#ifdef KNOT_NS_DEBUG
  #define KNOT_EDNS_DEBUG
  #define KNOT_NSEC3_DEBUG
#endif

#ifdef KNOT_PACKET_DEBUG
  #define KNOT_RESPONSE_DEBUG
#endif

#ifdef KNOT_RR_DEBUG
  #define KNOT_RRSET_DEBUG
#endif

#ifdef KNOT_XFR_DEBUG
  #define KNOT_XFRIN_DEBUG
  #define KNOT_TSIG_DEBUG
  #define KNOT_DDNS_DEBUG
#endif

/******************************************************************************/

#ifdef KNOT_NS_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_ns(msg...) log_msg(LOG_SERVER, LOG_DEBUG, msg)
#define dbg_ns_hex(data, len)  hex_log(LOG_SERVER, (data), (len))
#define dbg_ns_exec(cmds) do { cmds } while (0)
#else
#define dbg_ns(msg...)
#define dbg_ns_hex(data, len)
#define dbg_ns_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_ns_verb(msg...) log_msg(LOG_SERVER, LOG_DEBUG, msg)
#define dbg_ns_hex_verb(data, len) hex_log(LOG_SERVER, (data), (len))
#define dbg_ns_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_ns_verb(msg...)
#define dbg_ns_hex_verb(data, len)
#define dbg_ns_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_ns_detail(msg...) log_msg(LOG_SERVER, LOG_DEBUG, msg)
#define dbg_ns_hex_detail(data, len)  hex_log(LOG_SERVER, (data), (len))
#define dbg_ns_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_ns_detail(msg...)
#define dbg_ns_hex_detail(data, len)
#define dbg_ns_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_ns(msg...)
#define dbg_ns_hex(data, len)
#define dbg_ns_exec(cmds)
#define dbg_ns_verb(msg...)
#define dbg_ns_hex_verb(data, len)
#define dbg_ns_exec_verb(cmds)
#define dbg_ns_detail(msg...)
#define dbg_ns_hex_detail(data, len)
#define dbg_ns_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_NODE_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_node(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_node_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_node(msg...)
#define dbg_node_hex(data, len)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_node_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_node_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_node_verb(msg...)
#define dbg_node_hex_verb(data, len)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_node_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_node_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_node_detail(msg...)
#define dbg_node_hex_detail(data, len)
#endif

/* No messages. */
#else
#define dbg_node(msg...)
#define dbg_node_hex(data, len)
#define dbg_node_verb(msg...)
#define dbg_node_hex_verb(data, len)
#define dbg_node_detail(msg...)
#define dbg_node_hex_detail(data, len)
#endif

/******************************************************************************/

#ifdef KNOT_ZONE_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_zone(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_zone_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_zone_exec(cmds) do { cmds } while (0)
#else
#define dbg_zone(msg...)
#define dbg_zone_hex(data, len)
#define dbg_zone_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_zone_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_zone_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_zone_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_zone_verb(msg...)
#define dbg_zone_hex_verb(data, len)
#define dbg_zone_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_zone_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_zone_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_zone_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_zone_detail(msg...)
#define dbg_zone_hex_detail(data, len)
#define dbg_zone_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_zone(msg...)
#define dbg_zone_hex(data, len)
#define dbg_zone_exec(cmds)
#define dbg_zone_verb(msg...)
#define dbg_zone_hex_verb(data, len)
#define dbg_zone_exec_verb(cmds)
#define dbg_zone_detail(msg...)
#define dbg_zone_hex_detail(data, len)
#define dbg_zone_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_ZONEDB_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_zonedb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_zonedb_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_zonedb_exec(cmds) do { cmds } while (0)
#else
#define dbg_zonedb(msg...)
#define dbg_zonedb_hex(data, len)
#define dbg_zonedb_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_zonedb_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_zonedb_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_zonedb_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_zonedb_verb(msg...)
#define dbg_zonedb_hex_verb(data, len)
#define dbg_zonedb_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_zonedb_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_zonedb_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_zonedb_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_zonedb_detail(msg...)
#define dbg_zonedb_hex_detail(data, len)
#define dbg_zonedb_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_zonedb(msg...)
#define dbg_zonedb_hex(data, len)
#define dbg_zonedb_exec(cmds)
#define dbg_zonedb_verb(msg...)
#define dbg_zonedb_hex_verb(data, len)
#define dbg_zonedb_exec_verb(cmds)
#define dbg_zonedb_detail(msg...)
#define dbg_zonedb_hex_detail(data, len)
#define dbg_zonedb_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_ZONEDIFF_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_zonediff(msg...) fprintf(stderr, msg)
#define dbg_zonediff_hex(data, len)  hex_print((data), (len))
#else
#define dbg_zonediff(msg...)
#define dbg_zonediff_hex(data, len)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_zonediff_verb(msg...) fprintf(stderr, msg)
#define dbg_zonediff_hex_verb(data, len)  hex_print((data), (len))
#else
#define dbg_zonediff_verb(msg...)
#define dbg_zonediff_hex_verb(data, len)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_zonediff_detail(msg...) fprintf(stderr, msg)
#define dbg_zonediff_hex_detail(data, len)  hex_print((data), (len))
#define dbg_zonediff_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_zonediff_detail(msg...)
#define dbg_zonediff_hex_detail(data, len)
#define dbg_zonediff_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_zonediff(msg...)
#define dbg_zonediff_hex(data, len)
#define dbg_zonediff_verb(msg...)
#define dbg_zonediff_hex_verb(data, len)
#define dbg_zonediff_detail(msg...)
#define dbg_zonediff_hex_detail(data, len)
#define dbg_zonediff_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_RESPONSE_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_response(msg...) log_msg(LOG_ANSWER, LOG_DEBUG, msg)
#define dbg_response_hex(data, len)  hex_log(LOG_ANSWER, (data), (len))
#define dbg_response_exec(cmds) do { cmds } while (0)
#else
#define dbg_response(msg...)
#define dbg_response_hex(data, len)
#define dbg_response_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_response_verb(msg...) log_msg(LOG_ANSWER, LOG_DEBUG, msg)
#define dbg_response_hex_verb(data, len)  hex_log(LOG_ANSWER, (data), (len))
#define dbg_response_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_response_verb(msg...)
#define dbg_response_hex_verb(data, len)
#define dbg_response_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_response_detail(msg...) log_msg(LOG_ANSWER, LOG_DEBUG, msg)
#define dbg_response_hex_detail(data, len)  hex_log(LOG_ANSWER, (data), (len))
#define dbg_response_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_response_detail(msg...)
#define dbg_response_hex_detail(data, len)
#define dbg_response_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_response(msg...)
#define dbg_response_hex(data, len)
#define dbg_response_exec(cmds)
#define dbg_response_verb(msg...)
#define dbg_response_hex_verb(data, len)
#define dbg_response_exec_verb(cmds)
#define dbg_response_detail(msg...)
#define dbg_response_hex_detail(data, len)
#define dbg_response_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_PACKET_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_packet(msg...) log_msg(LOG_ANSWER, LOG_DEBUG, msg)
#define dbg_packet_hex(data, len)  hex_log(LOG_ANSWER, (data), (len))
#define dbg_packet_exec(cmds) do { cmds } while (0)
#else
#define dbg_packet(msg...)
#define dbg_packet_hex(data, len)
#define dbg_packet_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_packet_verb(msg...) log_msg(LOG_ANSWER, LOG_DEBUG, msg)
#define dbg_packet_hex_verb(data, len)  hex_log(LOG_ANSWER, (data), (len))
#define dbg_packet_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_packet_verb(msg...)
#define dbg_packet_hex_verb(data, len)
#define dbg_packet_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_packet_detail(msg...) log_msg(LOG_ANSWER, LOG_DEBUG, msg)
#define dbg_packet_hex_detail(data, len)  hex_log(LOG_ANSWER, (data), (len))
#define dbg_packet_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_packet_detail(msg...)
#define dbg_packet_hex_detail(data, len)
#define dbg_packet_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_packet(msg...)
#define dbg_packet_hex(data, len)
#define dbg_packet_exec(cmds)
#define dbg_packet_verb(msg...)
#define dbg_packet_hex_verb(data, len)
#define dbg_packet_exec_verb(cmds)
#define dbg_packet_detail(msg...)
#define dbg_packet_hex_detail(data, len)
#define dbg_packet_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_EDNS_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_edns(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_edns_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_edns(msg...)
#define dbg_edns_hex(data, len)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_edns_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_edns_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_edns_verb(msg...)
#define dbg_edns_hex_verb(data, len)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_edns_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_edns_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_edns_detail(msg...)
#define dbg_edns_hex_detail(data, len)
#endif

/* No messages. */
#else
#define dbg_edns(msg...)
#define dbg_edns_hex(data, len)
#define dbg_edns_verb(msg...)
#define dbg_edns_hex_verb(data, len)
#define dbg_edns_detail(msg...)
#define dbg_edns_hex_detail(data, len)
#endif

/******************************************************************************/

#ifdef KNOT_NSEC3_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_nsec3(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_nsec3_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_nsec3(msg...)
#define dbg_nsec3_hex(data, len)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_nsec3_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_nsec3_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_nsec3_verb(msg...)
#define dbg_nsec3_hex_verb(data, len)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_nsec3_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_nsec3_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_nsec3_detail(msg...)
#define dbg_nsec3_hex_detail(data, len)
#endif

/* No messages. */
#else
#define dbg_nsec3(msg...)
#define dbg_nsec3_hex(data, len)
#define dbg_nsec3_verb(msg...)
#define dbg_nsec3_hex_verb(data, len)
#define dbg_nsec3_detail(msg...)
#define dbg_nsec3_hex_detail(data, len)
#endif

/******************************************************************************/

#ifdef KNOT_XFRIN_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_xfrin(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_xfrin_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_xfrin_exec(cmds) do { cmds } while (0)
#else
#define dbg_xfrin(msg...)
#define dbg_xfrin_hex(data, len)
#define dbg_xfrin_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_xfrin_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_xfrin_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_xfrin_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_xfrin_verb(msg...)
#define dbg_xfrin_hex_verb(data, len)
#define dbg_xfrin_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_xfrin_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_xfrin_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_xfrin_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_xfrin_detail(msg...)
#define dbg_xfrin_hex_detail(data, len)
#define dbg_xfrin_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_xfrin(msg...)
#define dbg_xfrin_hex(data, len)
#define dbg_xfrin_exec(cmds)
#define dbg_xfrin_verb(msg...)
#define dbg_xfrin_hex_verb(data, len)
#define dbg_xfrin_exec_verb(cmds)
#define dbg_xfrin_detail(msg...)
#define dbg_xfrin_hex_detail(data, len)
#define dbg_xfrin_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_DDNS_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_ddns(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_ddns_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_ddns_exec(cmds) do { cmds } while (0)
#else
#define dbg_ddns(msg...)
#define dbg_ddns_hex(data, len)
#define dbg_ddns_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_ddns_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_ddns_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_ddns_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_ddns_verb(msg...)
#define dbg_ddns_hex_verb(data, len)
#define dbg_ddns_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_ddns_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_ddns_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_ddns_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_ddns_detail(msg...)
#define dbg_ddns_hex_detail(data, len)
#define dbg_ddns_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_ddns(msg...)
#define dbg_ddns_hex(data, len)
#define dbg_ddns_verb(msg...)
#define dbg_ddns_exec(cmds)
#define dbg_ddns_hex_verb(data, len)
#define dbg_ddns_exec_verb(cmds)
#define dbg_ddns_detail(msg...)
#define dbg_ddns_hex_detail(data, len)
#define dbg_ddns_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_TSIG_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_tsig(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_tsig_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_tsig(msg...)
#define dbg_tsig_hex(data, len)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_tsig_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_tsig_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_tsig_verb(msg...)
#define dbg_tsig_hex_verb(data, len)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_tsig_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_tsig_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#else
#define dbg_tsig_detail(msg...)
#define dbg_tsig_hex_detail(data, len)
#endif

/* No messages. */
#else
#define dbg_tsig(msg...)
#define dbg_tsig_hex(data, len)
#define dbg_tsig_verb(msg...)
#define dbg_tsig_hex_verb(data, len)
#define dbg_tsig_detail(msg...)
#define dbg_tsig_hex_detail(data, len)
#endif

/******************************************************************************/

#ifdef KNOT_RRSET_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_rrset(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_rrset_hex(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_rrset_exec(cmds) do { cmds } while (0)
#else
#define dbg_rrset(msg...)
#define dbg_rrset_hex(data, len)
#define dbg_rrset_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_rrset_verb(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_rrset_hex_verb(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_rrset_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_rrset_verb(msg...)
#define dbg_rrset_hex_verb(data, len)
#define dbg_rrset_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_rrset_detail(msg...) log_msg(LOG_ZONE, LOG_DEBUG, msg)
#define dbg_rrset_hex_detail(data, len)  hex_log(LOG_ZONE, (data), (len))
#define dbg_rrset_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_rrset_detail(msg...)
#define dbg_rrset_hex_detail(data, len)
#define dbg_rrset_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_rrset(msg...)
#define dbg_rrset_hex(data, len)
#define dbg_rrset_exec(cmds)
#define dbg_rrset_verb(msg...)
#define dbg_rrset_hex_verb(data, len)
#define dbg_rrset_exec_verb(cmds)
#define dbg_rrset_detail(msg...)
#define dbg_rrset_hex_detail(data, len)
#define dbg_rrset_exec_detail(cmds)
#endif

/******************************************************************************/

#ifdef KNOT_DNSSEC_DEBUG

/* Brief messages. */
#ifdef DEBUG_ENABLE_BRIEF
#define dbg_dnssec(msg...) log_msg(LOG_SERVER, LOG_DEBUG, msg)
#define dbg_dnssec_hex(data, len)  hex_log(LOG_SERVER, (data), (len))
#define dbg_dnssec_exec(cmds) do { cmds } while (0)
#else
#define dbg_dnssec(msg...)
#define dbg_dnssec_hex(data, len)
#define dbg_dnssec_exec(cmds)
#endif

/* Verbose messages. */
#ifdef DEBUG_ENABLE_VERBOSE
#define dbg_dnssec_verb(msg...) log_msg(LOG_SERVER, LOG_DEBUG, msg)
#define dbg_dnssec_hex_verb(data, len) hex_log(LOG_SERVER, (data), (len))
#define dbg_dnssec_exec_verb(cmds) do { cmds } while (0)
#else
#define dbg_dnssec_verb(msg...)
#define dbg_dnssec_hex_verb(data, len)
#define dbg_dnssec_exec_verb(cmds)
#endif

/* Detail messages. */
#ifdef DEBUG_ENABLE_DETAILS
#define dbg_dnssec_detail(msg...) log_msg(LOG_SERVER, LOG_DEBUG, msg)
#define dbg_dnssec_hex_detail(data, len)  hex_log(LOG_SERVER, (data), (len))
#define dbg_dnssec_exec_detail(cmds) do { cmds } while (0)
#else
#define dbg_dnssec_detail(msg...)
#define dbg_dnssec_hex_detail(data, len)
#define dbg_dnssec_exec_detail(cmds)
#endif

/* No messages. */
#else
#define dbg_dnssec(msg...)
#define dbg_dnssec_hex(data, len)
#define dbg_dnssec_exec(cmds)
#define dbg_dnssec_verb(msg...)
#define dbg_dnssec_hex_verb(data, len)
#define dbg_dnssec_exec_verb(cmds)
#define dbg_dnssec_detail(msg...)
#define dbg_dnssec_hex_detail(data, len)
#define dbg_dnssec_exec_detail(cmds)
#endif

/******************************************************************************/

#endif /* _KNOT_DEBUG_H_ */

/*! @} */
