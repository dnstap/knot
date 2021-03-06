/*!
 * \file rrset.h
 *
 * \author Lubos Slovak <lubos.slovak@nic.cz>
 * \author Jan Kadlec <jan.kadlec@nic.cz>
 *
 * \brief RRSet structure and API for manipulating it.
 *
 * \addtogroup libknot
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

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "common/mempattern.h"

#include "libknot/dname.h"
#include "libknot/rdataset.h"

struct knot_compr;
struct knot_node;

/*!
 * \brief Structure for representing RRSet.
 *
 * For RRSet definition see RFC2181, Section 5.
 */
struct knot_rrset {
	knot_dname_t *owner;  /*!< Domain name being the owner of the RRSet. */
	uint16_t type;        /*!< TYPE of the RRset. */
	uint16_t rclass;      /*!< CLASS of the RRSet. */
	knot_rdataset_t rrs;  /*!< RRSet's RRs */
	/* Optional fields. */
	struct zone_node **additional; /*!< Additional records. */
};

typedef struct knot_rrset knot_rrset_t;

typedef enum {
	KNOT_RRSET_COMPARE_PTR,
	KNOT_RRSET_COMPARE_HEADER,
	KNOT_RRSET_COMPARE_WHOLE
} knot_rrset_compare_type_t;

/* -------------------- Creation / initialization --------------------------- */

/*!
 * \brief Creates a new RRSet with the given properties.
 *
 * The created RRSet contains no RDATAs (i.e. is actually empty).
 *
 * \param owner   OWNER of the RRSet.
 * \param type    TYPE of the RRSet.
 * \param rclass  CLASS of the RRSet.
 *
 * \return New RRSet structure or NULL if an error occured.
 */
knot_rrset_t *knot_rrset_new(const knot_dname_t *owner, uint16_t type,
                             uint16_t rclass, mm_ctx_t *mm);

/*!
 * \brief Initializes RRSet structure with given data.
 *
 * \param rrset   RRSet to init.
 * \param owner   RRSet owner to use.
 * \param type    RR type to use.
 * \param rclass  Class to use.
 */
void knot_rrset_init(knot_rrset_t *rrset, knot_dname_t *owner, uint16_t type,
                     uint16_t rclass);

/*!
 * \brief Initializes given RRSet structure.
 *
 * \param rrset  RRSet to init.
 */
void knot_rrset_init_empty(knot_rrset_t *rrset);

/*!
 * \brief Creates new RRSet from \a src RRSet.
 *
 * \param src  Source RRSet.
 * \param mm   Memory context.
 *
 * \retval Pointer to new RRSet if all went OK.
 * \retval NULL on error.
 */
knot_rrset_t *knot_rrset_copy(const knot_rrset_t *src, mm_ctx_t *mm);

/* ---------------------------- Cleanup ------------------------------------- */

/*!
 * \brief Destroys the RRSet structure and all its substructures.
 )
 * Also sets the given pointer to NULL.
 *
 * \param rrset  RRset to be destroyed.
 * \param mm     Memory context.
 */
void knot_rrset_free(knot_rrset_t **rrset, mm_ctx_t *mm);

/*!
 * \brief Frees structures inside RRSet, but not the RRSet itself.
 *
 * \param rrset  RRSet to be cleared.
 * \param mm     Memory context used for allocations.
 */
void knot_rrset_clear(knot_rrset_t *rrset, mm_ctx_t *mm);

/* ----------- Getters / Setters (legacy, functionality in rdata_t) ------------ */

/*!
 * \brief Returns RDATA of RR on given position.
 *
 * \param rrset  RRSet to get the RDATA from.
 * \param pos    Position of RR to get.
 *
 * \retval  NULL if no RDATA on rdata_pos exist.
 * \retval  Pointer to RDATA on given position if successfull.
 */
uint8_t *knot_rrset_rr_rdata(const knot_rrset_t *rrset, size_t pos);

/*!
 * \brief Returns size of an RR RDATA on a given position.
 *
 * \param rrset  RRSet holding RR RDATA.
 * \param pos    RR position.
 *
 * \return Item size.
 */
uint16_t knot_rrset_rr_size(const knot_rrset_t *rrset, size_t pos);

/*!
 * \brief Returns TTL of an RR on a given position.
 *
 * \param rrset  RRSet holding RR RDATA.
 * \param pos    RR position.
 *
 * \return TTL.
 */
uint32_t knot_rrset_rr_ttl(const knot_rrset_t *rrset, size_t pos);

/*!
 * \brief Sets TTL for RR on a given position.
 *
 * \param rrset  RRSet containing RR.
 * \param pos    RR position.
 * \param ttl    TTL to be set.
 */
void knot_rrset_rr_set_ttl(const knot_rrset_t *rrset, size_t pos, uint32_t ttl);

/* ---------- Wire conversions (legacy, to be done in knot_pkt_t) ----------- */

/*!
 * \brief Converts RRSet structure to wireformat, compression included.
 *
 * \param rrset     RRSet to be converted.
 * \param wire      Destination wire.
 * \param size      Output size.
 * \param max_size  Wire size.
 * \param rr_count  Output RR count.
 * \param compr     Compression data.
 *
 * \return KNOT_E*
 */
int knot_rrset_to_wire(const knot_rrset_t *rrset, uint8_t *wire, size_t *size,
                       size_t max_size, uint16_t *rr_count, struct knot_compr *compr);

 /*!
 * \brief Creates one RR from wire, stores it into 'rrset'
 *
 * \param rrset       Destination RRSet.
 * \param wire        Source wire.
 * \param pos         Position in wire.
 * \param total_size  Size of wire.
 * \param ttl         Use this TTL to create RR.
 * \param rdlength    RDLENGTH.
 * \param mm          Memory context.
 *
 * \return KNOT_E*
 */
int knot_rrset_rdata_from_wire_one(knot_rrset_t *rrset,
                                   const uint8_t *wire, size_t *pos,
                                   size_t total_size, uint32_t ttl, size_t rdlength,
                                   mm_ctx_t *mm);

/* ---------- RR addition. (legacy, functionality in knot_rdataset_t) ------- */

/*!
 * \brief Adds the given RDATA to the RRSet.
 *
 * \param rrset  RRSet to add the RDATA to.
 * \param rdata  RDATA to add to the RRSet.
 * \param size   Size of RDATA.
 * \param ttl   TTL for RR.
 * \param mm     Memory context.
 *
 * \return KNOT_E*
 */
int knot_rrset_add_rdata(knot_rrset_t *rrset, const uint8_t *rdata,
                         const uint16_t size, const uint32_t ttl,
                         mm_ctx_t *mm);

/* ------------------ Equality / emptines bool checks ----------------------- */

/*!
 * \brief Compares two RRSets for equality.
 *
 * \param r1   First RRSet.
 * \param r2   Second RRSet.
 * \param cmp  Type of comparison to perform.
 *
 * \retval True   if RRSets are equal.
 * \retval False  if RRSets are not equal.
 */
bool knot_rrset_equal(const knot_rrset_t *r1, const knot_rrset_t *r2,
                      knot_rrset_compare_type_t cmp);

/*!
 * \brief Checks whether RRSet is empty.
 *
 * \param rrset  RRSet to check.
 *
 * \retval True if RRSet is empty.
 * \retval False if RRSet is not empty.
 */
bool knot_rrset_empty(const knot_rrset_t *rrset);

/*! @} */
