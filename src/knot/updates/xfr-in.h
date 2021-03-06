/*!
 * \file xfr-in.h
 *
 * \author Lubos Slovak <lubos.slovak@nic.cz>
 *
 * \brief XFR client API.
 *
 * \addtogroup xfr
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

#ifndef _KNOT_XFR_IN_H_
#define _KNOT_XFR_IN_H_

#include <stdint.h>
#include <string.h>

#include "libknot/dname.h"
#include "knot/zone/zone.h"
#include "libknot/packet/pkt.h"
#include "knot/server/xfr-handler.h"
#include "knot/updates/changesets.h"

/*----------------------------------------------------------------------------*/

typedef enum xfrin_transfer_result {
	XFRIN_RES_COMPLETE = 1,
	XFRIN_RES_SOA_ONLY = 2,
	XFRIN_RES_FALLBACK = 3
} xfrin_transfer_result_t;

/*----------------------------------------------------------------------------*/

/*!
 * \brief Checks if a zone transfer is required by comparing the zone's SOA with
 *        the one received from master server.
 *
 * \param zone Zone to check.
 * \param soa_response Response to SOA query received from master server.
 *
 * \retval < 0 if an error occured.
 * \retval 1 if the transfer is needed.
 * \retval 0 if the transfer is not needed.
 */
int xfrin_transfer_needed(const knot_zone_contents_t *zone,
                          knot_pkt_t *soa_response);

/*!
 * \brief Creates normal query for the given zone name and the SOA type.
 *
 * \param zone Zone for which a query should be created.
 * \param pkt Packet to be written.
 *
 * \retval KNOT_EOK
 * \retval KNOT_ESPACE
 * \retval KNOT_ERROR
 */
int xfrin_create_soa_query(const zone_t *zone, knot_pkt_t *pkt);

/*!
 * \brief Creates normal query for the given zone name and the AXFR type.
 *
 * \param zone Zone for which a query should be created.
 * \param pkt Packet to be written.
 *
 * \todo Parameter use_tsig probably not needed.
 *
 * \retval KNOT_EOK
 * \retval KNOT_ESPACE
 * \retval KNOT_ERROR
 */
int xfrin_create_axfr_query(const zone_t *zone, knot_pkt_t *pkt);

/*!
 * \brief Creates normal query for the given zone name and the IXFR type.
 *
 * \param zone Zone for which a query should be created.
 * \param pkt Packet to be written.
 *
 * \todo Parameter use_tsig probably not needed.
 *
 * \retval KNOT_EOK
 * \retval KNOT_ESPACE
 * \retval KNOT_ERROR
 */
int xfrin_create_ixfr_query(const zone_t *zone, knot_pkt_t *pkt);

/*!
 * \brief Processes one incoming packet of AXFR transfer by updating the given
 *        zone.
 *
 * \param pkt Incoming packet in wire format.
 * \param size Size of the packet in bytes.
 * \param zone Zone being built. If there is no such zone (i.e. this is the
 *             first packet, \a *zone may be set to NULL, in which case a new
 *             zone structure is created).
 *
 * \retval KNOT_EOK
 *
 * \todo Refactor!!!
 */
int xfrin_process_axfr_packet(knot_pkt_t *pkt, knot_ns_xfr_t *xfr, knot_zone_contents_t **zone);

/*!
 * \brief Destroys the whole changesets structure.
 *
 * Frees all RRSets present in the changesets and all their data. Also frees
 * the changesets structure and sets the parameter to NULL.
 *
 * \param changesets Changesets to destroy.
 */
void xfrin_free_changesets(knot_changesets_t **changesets);

/*!
 * \brief Parses IXFR reply packet and fills in the changesets structure.
 *
 * \param pkt Packet containing the IXFR reply in wire format.
 * \param size Size of the packet in bytes.
 * \param changesets Changesets to be filled in.
 *
 * \retval KNOT_EOK
 * \retval KNOT_EINVAL
 * \retval KNOT_EMALF
 * \retval KNOT_ENOMEM
 */
int xfrin_process_ixfr_packet(knot_pkt_t *pkt, knot_ns_xfr_t *xfr);

/*!
 * \brief Applies changesets *with* zone shallow copy.
 *
 * \param zone          Zone to be updated.
 * \param chsets        Changes to be made.
 * \param new_contents  New zone will be returned using this arg.
 * \return KNOT_E*
 */
int xfrin_apply_changesets(zone_t *zone,
                           knot_changesets_t *chsets,
                           knot_zone_contents_t **new_contents);

/*!
 * \brief Applies DNSSEC changesets after DDNS.
 *
 * \param z_new           Post DDNS/reload zone.
 * \param sec_chsets      Changes with RRSIGs/NSEC(3)s.
 * \param chsets          DDNS/reload changes, for rollback.
 * \return KNOT_E*
 *
 * This function does not do shallow copy of the zone, as it is already created
 * by the UPDATE-processing function. It uses new and old zones from this
 * operation.
 */
int xfrin_apply_changesets_dnssec_ddns(zone_t *zone,
                                       knot_zone_contents_t *z_new,
                                       knot_changesets_t *sec_chsets,
                                       knot_changesets_t *chsets);

/*!
 * \brief Applies changesets directly to the zone, without copying it.
 *
 * \param contents Zone contents to apply the changesets to. Will be modified.
 * \param chsets   Changesets to be applied to the zone.
 *
 * \retval KNOT_EOK if successful.
 * \retval KNOT_EINVAL if given one of the arguments is NULL.
 * \return Other error code if the application went wrong.
 */
int xfrin_apply_changesets_directly(knot_zone_contents_t *contents,
                                    knot_changesets_t *chsets);

int xfrin_prepare_zone_copy(knot_zone_contents_t *old_contents,
                            knot_zone_contents_t **new_contents);

/*!
 * \brief Sets pointers and NSEC3 nodes after signing/DDNS.
 * \param contents_copy    Contents to be updated.
 * \param set_nsec3_names  Set to true if NSEC3 hashes should be set.
 * \return KNOT_E*
 */
int xfrin_finalize_updated_zone(knot_zone_contents_t *contents_copy,
                                bool set_nsec3_names);

int xfrin_switch_zone(zone_t *zone,
                      knot_zone_contents_t *new_contents,
                      int transfer_type);

void xfrin_rollback_update(knot_changesets_t *chgs,
                           knot_zone_contents_t **new_contents);

int xfrin_copy_rrset(zone_node_t *node, uint16_t type,
                     knot_rrset_t **rrset);

int xfrin_copy_old_rrset(knot_rrset_t *old, knot_rrset_t **copy);

int xfrin_replace_rrset_in_node(zone_node_t *node,
                                knot_rrset_t *rrset_new,
                                knot_zone_contents_t *contents);

void xfrin_cleanup_successful_update(knot_changesets_t *chgs);

#endif /* _KNOTXFR_IN_H_ */

/*! @} */
