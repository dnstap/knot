/*  Copyright (C) 2011 CZ.NIC Labs

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

#include <assert.h>

#include "packet/packet.h"
#include "util/error.h"
#include "util/debug.h"
#include "common.h"
#include "util/descriptor.h"
#include "util/wire.h"

/*----------------------------------------------------------------------------*/

#define DEFAULT_RRCOUNT_QUERY(type) DEFAULT_##type##COUNT_QUERY
#define DEFAULT_RRCOUNT(type) DEFAULT_##type##COUNT

#define DEFAULT_RRSET_COUNT(type, packet) \
	((packet->prealloc_type == KNOT_PACKET_PREALLOC_NONE)  \
		? 0  \
		: (packet->prealloc_type == KNOT_PACKET_PREALLOC_QUERY)  \
			? DEFAULT_##type##_QUERY  \
			: DEFAULT_##type)



typedef enum {
	KNOT_PACKET_DUPL_IGNORE,
	KNOT_PACKET_DUPL_SKIP,
	KNOT_PACKET_DUPL_MERGE
} knot_packet_duplicate_handling_t;
/*----------------------------------------------------------------------------*/
/* Non-API functions                                                          */
/*----------------------------------------------------------------------------*/
/*!
 * \brief Sets all the pointers in the packet structure to the respective
 *        parts of the pre-allocated space.
 */
static void knot_packet_init_pointers_response(knot_packet_t *pkt)
{
	debug_knot_packet("Packet pointer: %p\n", pkt);

	// put QNAME directly after the structure
	pkt->question.qname =
		(knot_dname_t *)((char *)pkt + PREALLOC_PACKET);

	debug_knot_packet("QNAME: %p (%zu after start of packet)\n",
		pkt->question.qname,
		(void *)pkt->question.qname - (void *)pkt);

	pkt->question.qname->name = (uint8_t *)((char *)pkt->question.qname
	                                         + PREALLOC_QNAME_DNAME);
	pkt->question.qname->labels = (uint8_t *)((char *)
	                                           pkt->question.qname->name
	                                           + PREALLOC_QNAME_NAME);

	pkt->owner_tmp = (uint8_t *)((char *)pkt->question.qname->labels
	                              + PREALLOC_QNAME_LABELS);

	debug_knot_packet("Tmp owner: %p (%zu after QNAME)\n",
		pkt->owner_tmp,
		(void *)pkt->owner_tmp - (void *)pkt->question.qname);

	// then answer, authority and additional sections
	pkt->answer = (const knot_rrset_t **)
	                   ((char *)pkt->owner_tmp + PREALLOC_RR_OWNER);
	pkt->authority = pkt->answer + DEFAULT_ANCOUNT;
	pkt->additional = pkt->authority + DEFAULT_NSCOUNT;

	debug_knot_packet("Answer section: %p (%zu after QNAME)\n",
		pkt->answer,
		(void *)pkt->answer - (void *)pkt->question.qname);
	debug_knot_packet("Authority section: %p (%zu after Answer)\n",
		pkt->authority,
		(void *)pkt->authority - (void *)pkt->answer);
	debug_knot_packet("Additional section: %p (%zu after Authority)\n",
		pkt->additional,
		(void *)pkt->additional - (void *)pkt->authority);

	pkt->max_an_rrsets = DEFAULT_ANCOUNT;
	pkt->max_ns_rrsets = DEFAULT_NSCOUNT;
	pkt->max_ar_rrsets = DEFAULT_ARCOUNT;

	// then domain names for compression and offsets
	pkt->compression.dnames = (const knot_dname_t **)
	                               (pkt->additional + DEFAULT_ARCOUNT);
	pkt->compression.offsets = (size_t *)
		(pkt->compression.dnames + DEFAULT_DOMAINS_IN_RESPONSE);

	debug_knot_packet("Compression dnames: %p (%zu after Additional)\n",
		pkt->compression.dnames,
		(void *)pkt->compression.dnames - (void *)pkt->additional);
	debug_knot_packet("Compression offsets: %p (%zu after c. dnames)\n",
		pkt->compression.offsets,
		(void *)pkt->compression.offsets
		  - (void *)pkt->compression.dnames);

	pkt->compression.max = DEFAULT_DOMAINS_IN_RESPONSE;

	pkt->tmp_rrsets = (const knot_rrset_t **)
		(pkt->compression.offsets + DEFAULT_DOMAINS_IN_RESPONSE);

	debug_knot_packet("Tmp rrsets: %p (%zu after compression offsets)"
		"\n", pkt->tmp_rrsets,
		(void *)pkt->tmp_rrsets - (void *)pkt->compression.offsets);

	pkt->tmp_rrsets_max = DEFAULT_TMP_RRSETS;

	debug_knot_packet("End of data: %p (%zu after start of packet)\n",
		pkt->tmp_rrsets + DEFAULT_TMP_RRSETS,
		(void *)(pkt->tmp_rrsets + DEFAULT_TMP_RRSETS) - (void *)pkt);
	debug_knot_packet("Allocated total: %u\n", PREALLOC_RESPONSE);

	assert((char *)(pkt->tmp_rrsets + DEFAULT_TMP_RRSETS)
	       == (char *)pkt + PREALLOC_RESPONSE);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Sets all the pointers in the packet structure to the respective
 *        parts of the pre-allocated space.
 */
static void knot_packet_init_pointers_query(knot_packet_t *pkt)
{
	debug_knot_packet("Packet pointer: %p\n", pkt);

	// put QNAME directly after the structure
	pkt->question.qname =
		(knot_dname_t *)((char *)pkt + PREALLOC_PACKET);

	debug_knot_packet("QNAME: %p (%zu after start of packet)\n",
		pkt->question.qname,
		(void *)pkt->question.qname - (void *)pkt);

	pkt->question.qname->name = (uint8_t *)((char *)pkt->question.qname
	                                         + PREALLOC_QNAME_DNAME);
	pkt->question.qname->labels = (uint8_t *)((char *)
	                                           pkt->question.qname->name
	                                           + PREALLOC_QNAME_NAME);

//	pkt->owner_tmp = (uint8_t *)((char *)pkt->question.qname->labels
//	                              + PREALLOC_QNAME_LABELS);

	// then answer, authority and additional sections
	pkt->answer = (const knot_rrset_t **)
	          ((char *)pkt->question.qname->labels + PREALLOC_QNAME_LABELS);
	pkt->authority = pkt->answer + DEFAULT_ANCOUNT_QUERY;
	pkt->additional = pkt->authority + DEFAULT_NSCOUNT_QUERY;

	debug_knot_packet("Answer section: %p (%zu after QNAME)\n",
		pkt->answer,
		(void *)pkt->answer - (void *)pkt->question.qname);
	debug_knot_packet("Authority section: %p (%zu after Answer)\n",
		pkt->authority,
		(void *)pkt->authority - (void *)pkt->answer);
	debug_knot_packet("Additional section: %p (%zu after Authority)\n",
		pkt->additional,
		(void *)pkt->additional - (void *)pkt->authority);

	pkt->max_an_rrsets = DEFAULT_ANCOUNT_QUERY;
	pkt->max_ns_rrsets = DEFAULT_NSCOUNT_QUERY;
	pkt->max_ar_rrsets = DEFAULT_ARCOUNT_QUERY;

	pkt->tmp_rrsets = (const knot_rrset_t **)
	                      (pkt->additional + DEFAULT_ARCOUNT_QUERY);

	debug_knot_packet("Tmp rrsets: %p (%zu after Additional)\n",
		pkt->tmp_rrsets,
		(void *)pkt->tmp_rrsets - (void *)pkt->additional);

	pkt->tmp_rrsets_max = DEFAULT_TMP_RRSETS_QUERY;

	debug_knot_packet("End of data: %p (%zu after start of packet)\n",
		pkt->tmp_rrsets + DEFAULT_TMP_RRSETS_QUERY,
		(void *)(pkt->tmp_rrsets + DEFAULT_TMP_RRSETS_QUERY)
		- (void *)pkt);
	debug_knot_packet("Allocated total: %u\n", PREALLOC_QUERY);

	assert((char *)(pkt->tmp_rrsets + DEFAULT_TMP_RRSETS_QUERY)
	       == (char *)pkt + PREALLOC_QUERY);
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Parses DNS header from the wire format.
 *
 * \note This function also adjusts the position (\a pos) and size of remaining
 *       bytes in the wire format (\a remaining) according to what was parsed
 *       (though it actually always parses the 12 bytes of the header).
 *
 * \param[in,out] pos Wire format to parse the header from.
 * \param[in,out] remaining Remaining size of the wire format.
 * \param[out] header Header structure to fill in.
 *
 * \retval KNOT_EOK
 * \retval KNOT_EFEWDATA
 */
static int knot_packet_parse_header(const uint8_t *wire, size_t *pos,
                                      size_t size, knot_header_t *header)
{
	assert(wire != NULL);
	assert(pos != NULL);
	assert(header != NULL);

	if (size - *pos < KNOT_WIRE_HEADER_SIZE) {
		debug_knot_response("Not enough data to parse header.\n");
		return KNOT_EFEWDATA;
	}

	header->id = knot_wire_get_id(wire);
	// copy some of the flags: OPCODE and RD
	// do this by copying flags1 and setting QR to 1, AA to 0 and TC to 0
	header->flags1 = knot_wire_get_flags1(wire);
//	knot_wire_flags_set_qr(&header->flags1);
//	knot_wire_flags_clear_aa(&header->flags1);
//	knot_wire_flags_clear_tc(&header->flags1);
	// do not copy flags2 (all set by server)
	header->qdcount = knot_wire_get_qdcount(wire);
	header->ancount = knot_wire_get_ancount(wire);
	header->nscount = knot_wire_get_nscount(wire);
	header->arcount = knot_wire_get_arcount(wire);

	*pos += KNOT_WIRE_HEADER_SIZE;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Parses DNS Question entry from the wire format.
 *
 * \note This function also adjusts the position (\a pos) and size of remaining
 *       bytes in the wire format (\a remaining) according to what was parsed.
 *
 * \param[in,out] pos Wire format to parse the Question from.
 * \param[in,out] remaining Remaining size of the wire format.
 * \param[out] question DNS Question structure to be filled.
 *
 * \retval KNOT_EOK
 * \retval KNOT_EFEWDATA
 * \retval KNOT_ENOMEM
 */
static int knot_packet_parse_question(const uint8_t *wire, size_t *pos,
                                        size_t size,
                                        knot_question_t *question, int alloc)
{
	assert(pos != NULL);
	assert(wire != NULL);
	assert(question != NULL);

	if (size - *pos < KNOT_WIRE_QUESTION_MIN_SIZE) {
		debug_knot_response("Not enough data to parse question.\n");
		return KNOT_EFEWDATA;  // malformed
	}

	debug_knot_response("Parsing Question starting on position %zu.\n",
	                      *pos);

	// domain name must end with 0, so just search for 0
	int i = *pos;
	while (i < size && wire[i] != 0) {
		++i;
	}

	if (size - i - 1 < 4) {
		debug_knot_response("Not enough data to parse question.\n");
		return KNOT_EFEWDATA;  // no 0 found or not enough data left
	}

	debug_knot_response("Parsing dname starting on position %zu and "
	                      "%zu bytes long.\n", *pos, i - *pos + 1);
	debug_knot_response("Alloc: %d\n", alloc);
	if (alloc) {
		question->qname = knot_dname_new_from_wire(
				wire + *pos, i - *pos + 1, NULL);
		if (question->qname == NULL) {
			return KNOT_ENOMEM;
		}
	} else {
		int res = knot_dname_from_wire(wire + *pos, i - *pos + 1,
	                                         NULL, question->qname);
		if (res != KNOT_EOK) {
			assert(res != KNOT_EBADARG);
			return res;
		}
	}

	*pos = i + 1;
	question->qtype = knot_wire_read_u16(wire + i + 1);
	//*pos += 2;
	question->qclass = knot_wire_read_u16(wire + i + 3);
	//*pos += 2;

	*pos += 4;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Reallocate space for RRSets.
 *
 * \param rrsets Space for RRSets.
 * \param max_count Size of the space available for the RRSets.
 * \param default_max_count Size of the space pre-allocated for the RRSets when
 *        the response structure was initialized.
 * \param step How much the space should be increased.
 *
 * \retval KNOT_EOK
 * \retval KNOT_ENOMEM
 */
static int knot_packet_realloc_rrsets(const knot_rrset_t ***rrsets,
                                        short *max_count,
                                        short default_max_count, short step)
{
	debug_knot_packet("Max count: %d, default max count: %d\n",
	       *max_count, default_max_count);
	int free_old = (*max_count) != default_max_count;
	const knot_rrset_t **old = *rrsets;

	short new_max_count = *max_count + step;
	const knot_rrset_t **new_rrsets = (const knot_rrset_t **)malloc(
		new_max_count * sizeof(knot_rrset_t *));
	CHECK_ALLOC_LOG(new_rrsets, KNOT_ENOMEM);

	memcpy(new_rrsets, *rrsets, (*max_count) * sizeof(knot_rrset_t *));

	*rrsets = new_rrsets;
	*max_count = new_max_count;

	if (free_old) {
		free(old);
	}

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

static knot_rdata_t *knot_packet_parse_rdata(const uint8_t *wire,
	size_t *pos, size_t total_size, size_t rdlength,
	const knot_rrtype_descriptor_t *desc)
{
	if (desc->type == 0) {
		debug_knot_packet("Unknown RR type.\n");
		return NULL;
	}

	knot_rdata_t *rdata = knot_rdata_new();
	if (rdata == NULL) {
		return NULL;
	}

	int rc = knot_rdata_from_wire(rdata, wire, pos, total_size, rdlength,
	                                desc);
	if (rc != KNOT_EOK) {
		debug_knot_packet("rdata_from_wire() returned: %s\n",
		                    knot_strerror2(rc));
		knot_rdata_free(&rdata);
		return NULL;
	}

	return rdata;
}

/*----------------------------------------------------------------------------*/

static knot_rrset_t *knot_packet_parse_rr(const uint8_t *wire, size_t *pos,
                                              size_t size)
{
//	knot_rrset_t *rrset =
//		(knot_rrset_t *)malloc(sizeof(knot_rrset_t));
//	CHECK_ALLOC_LOG(rrset, NULL);

	knot_dname_t *owner = knot_dname_parse_from_wire(wire, pos, size,
	                                                     NULL);
	debug_knot_packet("Created owner: %p\n", owner);
	if (owner == NULL) {
		return NULL;
	}

DEBUG_KNOT_PACKET(
	char *name = knot_dname_to_str(owner);
	debug_knot_packet("Parsed name: %s\n", name);
	free(name);
);

	//*remaining -= knot_dname_size(rrset->owner);

	/*! @todo Get rid of the numerical constant. */
	if (size - *pos < 10) {
		debug_knot_packet("Malformed RR: Not enough data to parse RR"
		                    " header.\n");
		knot_dname_release(owner);
		return NULL;
	}

	debug_knot_packet("Reading type from position %zu\n", *pos);

	uint16_t type = knot_wire_read_u16(wire + *pos);
	uint16_t rclass = knot_wire_read_u16(wire + *pos + 2);
	uint32_t ttl = knot_wire_read_u32(wire + *pos + 4);

	knot_rrset_t *rrset = knot_rrset_new(owner, type, rclass, ttl);

	/* Owner is either referenced in rrset or rrset creation failed. */
	knot_dname_release(owner);

	/* Check rrset allocation. */
	if (rrset == NULL) {
		return NULL;
	}

	uint16_t rdlength = knot_wire_read_u16(wire + *pos + 8);

	debug_knot_packet("Read RR header: type %u, class %u, ttl %u, "
	                    "rdlength %u\n", rrset->type, rrset->rclass,
	                    rrset->ttl, rdlength);

	*pos += 10;

	if (size - *pos < rdlength) {
		debug_knot_packet("Malformed RR: Not enough data to parse RR"
		                    " RDATA.\n");
		knot_rrset_deep_free(&rrset, 1, 1, 0);
//		free(rrset);
		return NULL;
	}

	rrset->rrsigs = NULL;

	if (rdlength == 0) {
		return rrset;
	}

	// parse RDATA
	knot_rdata_t *rdata = knot_packet_parse_rdata(wire, pos, size,
	                         rdlength,
	                         knot_rrtype_descriptor_by_type(rrset->type));
	if (rdata == NULL) {
		debug_knot_packet("Malformed RR: Could not parse RDATA.\n");
		knot_rrset_deep_free(&rrset, 1, 1, 0);
//		free(rrset);
		return NULL;
	}

	if (knot_rrset_add_rdata(rrset, rdata) != KNOT_EOK) {
		debug_knot_packet("Malformed RR: Could not add RDATA to RRSet"
		                    ".\n");
		knot_rdata_free(&rdata);
		knot_rrset_deep_free(&rrset, 1, 1, 0);
//		free(rrset);
		return NULL;
	}

	return rrset;
}

/*----------------------------------------------------------------------------*/

static int knot_packet_add_rrset(knot_rrset_t *rrset,
                                   const knot_rrset_t ***rrsets,
                                   short *rrset_count,
                                   short *max_rrsets,
                                   short default_rrsets,
                                   const knot_packet_t *packet,
                                   knot_packet_duplicate_handling_t dupl)
{

	assert(rrset != NULL);
	assert(rrsets != NULL);
	assert(rrset_count != NULL);
	assert(max_rrsets != NULL);

DEBUG_KNOT_PACKET(
	char *name = knot_dname_to_str(rrset->owner);
	debug_knot_packet("packet_add_rrset(), owner: %s, type: %s\n",
	                    name, knot_rrtype_to_string(rrset->type));
	free(name);
);

	if (*rrset_count == *max_rrsets
	    && knot_packet_realloc_rrsets(rrsets, max_rrsets, default_rrsets,
	                                    STEP_ANCOUNT) != KNOT_EOK) {
		return KNOT_ENOMEM;
	}

	if (dupl == KNOT_PACKET_DUPL_SKIP &&
	    knot_packet_contains(packet, rrset, KNOT_RRSET_COMPARE_PTR)) {
		/*! \todo This should also return > 0, as it means that the
		          RRSet was not used actually. */
		return KNOT_EOK;
	}

	if (dupl == KNOT_PACKET_DUPL_MERGE) {
		// try to find the RRSet in this array of RRSets
		for (int i = 0; i < *rrset_count; ++i) {

DEBUG_KNOT_PACKET(
			char *name = knot_dname_to_str((*rrsets)[i]->owner);
			debug_knot_packet("Comparing to RRSet: owner: %s, "
			                    "type: %s\n", name,
			                    knot_rrtype_to_string(
			                        (*rrsets)[i]->type));
			free(name);
);

			if (knot_rrset_compare((*rrsets)[i], rrset,
			                         KNOT_RRSET_COMPARE_HEADER)) {
				//const knot_rrset_t *r = (*rrsets)
				/*! \todo Test this!!! */
				int rc = knot_rrset_merge(
				    (void **)((*rrsets) + i), (void **)&rrset);
				if (rc != KNOT_EOK) {
					return rc;
				}
				return 1;
			}
		}
	}

	(*rrsets)[*rrset_count] = rrset;
	++(*rrset_count);

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

static int knot_packet_parse_rrs(const uint8_t *wire, size_t *pos,
                                   size_t size, uint16_t rr_count,
                                   const knot_rrset_t ***rrsets,
                                   short *rrset_count, short *max_rrsets,
                                   short default_rrsets,
                                   knot_packet_t *packet)
{
	assert(pos != NULL);
	assert(wire != NULL);
	assert(rrsets != NULL);
	assert(rrset_count != NULL);
	assert(max_rrsets != NULL);
	assert(packet != NULL);

	debug_knot_packet("Parsing RRSets starting on position: %zu\n",
	                    *pos);

//	if (*rrsets == NULL) {
//		knot_packet_realloc_rrsets(rrsets, max_rrsets, 0, 1);
//	}

	/*
	 * The RRs from one RRSet may be scattered in the current section.
	 * We must parse all RRs separately and try to add them to already
	 * parsed RRSets.
	 */
	int err = KNOT_EOK;
	knot_rrset_t *rrset = NULL;

	for (int i = 0; i < rr_count; ++i) {
		rrset = knot_packet_parse_rr(wire, pos, size);
		if (rrset == NULL) {
			debug_knot_packet("Failed to parse RR!\n");
			err = KNOT_EMALF;
			break;
		}

		err = knot_packet_add_rrset(rrset, rrsets, rrset_count,
		                             max_rrsets, default_rrsets, packet,
		                             KNOT_PACKET_DUPL_MERGE);
		if (err < 0) {
			break;
		} else if (err > 0) {	// merged
			debug_knot_packet("RRSet merged, freeing.\n");
			knot_rrset_deep_free(&rrset, 1, 0, 0);  // TODO: ok??
			continue;
		}

		err = knot_packet_add_tmp_rrset(packet, rrset);
		if (err != KNOT_EOK) {
			// remove the last RRSet from the list of RRSets
			// - just decrement the count
			--(*rrset_count);
			knot_rrset_deep_free(&rrset, 1, 1, 1);
			break;
		}
	}

	return (err < 0) ? err : KNOT_EOK;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Deallocates all space which was allocated additionally to the
 *        pre-allocated space of the response structure.
 *
 * \param resp Response structure that holds pointers to the allocated space.
 */
static void knot_packet_free_allocated_space(knot_packet_t *pkt)
{
	debug_knot_packet("Freeing additional space in packet.\n");
	if (pkt->prealloc_type == KNOT_PACKET_PREALLOC_NONE) {
		debug_knot_packet("Freeing QNAME.\n");
		knot_dname_release(pkt->question.qname);
	}

	if (pkt->max_an_rrsets > DEFAULT_RRSET_COUNT(ANCOUNT, pkt)) {
		free(pkt->answer);
	}
	if (pkt->max_ns_rrsets > DEFAULT_RRSET_COUNT(NSCOUNT, pkt)) {
		free(pkt->authority);
	}
	if (pkt->max_ar_rrsets > DEFAULT_RRSET_COUNT(ARCOUNT, pkt)) {
		free(pkt->additional);
	}

	if (pkt->compression.max > DEFAULT_DOMAINS_IN_RESPONSE) {
		free(pkt->compression.dnames);
		free(pkt->compression.offsets);
	}

	if (pkt->tmp_rrsets_max > DEFAULT_RRSET_COUNT(TMP_RRSETS, pkt)) {
		free(pkt->tmp_rrsets);
	}
}

/*----------------------------------------------------------------------------*/

static int knot_packet_parse_rr_sections(knot_packet_t *packet,
                                           size_t *pos)
{
	assert(packet != NULL);
	assert(packet->wireformat != NULL);
	assert(packet->size > 0);
	assert(pos != NULL);
	assert(*pos > 0);

	int err;

	debug_knot_packet("Parsing Answer RRs...\n");
	if ((err = knot_packet_parse_rrs(packet->wireformat, pos,
	   packet->size, packet->header.ancount, &packet->answer,
	   &packet->an_rrsets, &packet->max_an_rrsets,
	   DEFAULT_RRSET_COUNT(ANCOUNT, packet), packet)) != KNOT_EOK) {
		return err;
	}

	debug_knot_packet("Parsing Authority RRs...\n");
	if ((err = knot_packet_parse_rrs(packet->wireformat, pos,
	   packet->size, packet->header.nscount, &packet->authority,
	   &packet->ns_rrsets, &packet->max_ns_rrsets,
	   DEFAULT_RRSET_COUNT(NSCOUNT, packet), packet)) != KNOT_EOK) {
		return err;
	}

	debug_knot_packet("Parsing Additional RRs...\n");
	if ((err = knot_packet_parse_rrs(packet->wireformat, pos,
	   packet->size, packet->header.arcount, &packet->additional,
	   &packet->ar_rrsets, &packet->max_ar_rrsets,
	   DEFAULT_RRSET_COUNT(ARCOUNT, packet), packet)) != KNOT_EOK) {
		return err;
	}

	debug_knot_packet("Trying to find OPT RR in the packet.\n");

	for (int i = 0; i < packet->header.arcount; ++i) {
		if (knot_rrset_type(packet->additional[i])
		    == KNOT_RRTYPE_OPT) {
			debug_knot_packet("Found OPT RR, filling.\n");
			err = knot_edns_new_from_rr(&packet->opt_rr,
			                              packet->additional[i]);
			if (err != KNOT_EOK) {
				return err;
			}
			break;
		}
	}

	if (*pos < packet->size) {
		// some trailing garbage; ignore, but log
		debug_knot_response("Packet: %zu bytes of trailing garbage "
		                      "in packet.\n", (*pos) - packet->size);
	}

	packet->parsed = packet->size;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/
/* API functions                                                              */
/*----------------------------------------------------------------------------*/

knot_packet_t *knot_packet_new(knot_packet_prealloc_type_t prealloc)
{
	knot_packet_t *pkt;
	void (*init_pointers)(knot_packet_t *pkt) = NULL;
	size_t size = 0;

	switch (prealloc) {
	case KNOT_PACKET_PREALLOC_NONE:
		size = sizeof(knot_packet_t);
		break;
	case KNOT_PACKET_PREALLOC_QUERY:
		size = PREALLOC_QUERY;
		init_pointers = knot_packet_init_pointers_query;
		break;
	case KNOT_PACKET_PREALLOC_RESPONSE:
		size = PREALLOC_RESPONSE;
		init_pointers = knot_packet_init_pointers_response;
		break;
	}

	pkt = (knot_packet_t *)malloc(size);
	CHECK_ALLOC_LOG(pkt, NULL);
	memset(pkt, 0, size);
	if (init_pointers != NULL) {
		init_pointers(pkt);
	}

	pkt->prealloc_type = prealloc;

	// set EDNS version to not supported
	pkt->opt_rr.version = EDNS_NOT_SUPPORTED;

	return pkt;
}

/*----------------------------------------------------------------------------*/

int knot_packet_parse_from_wire(knot_packet_t *packet,
                                  const uint8_t *wireformat, size_t size,
                                  int question_only)
{
	if (packet == NULL || wireformat == NULL) {
		return KNOT_EBADARG;
	}

	int err;

	// save the wireformat in the packet
	// TODO: can we just save the pointer, or we have to copy the data??
	assert(packet->wireformat == NULL);
	packet->wireformat = (uint8_t*)wireformat;
	packet->size = size;
	packet->free_wireformat = 0;

	//uint8_t *pos = wireformat;
	size_t pos = 0;
	//size_t remaining = size;

	debug_knot_packet("Parsing wire format of packet.\nHeader...\n");
	if ((err = knot_packet_parse_header(wireformat, &pos, size,
	                                      &packet->header)) != KNOT_EOK) {
		return err;
	}

	packet->parsed = pos;

	debug_knot_packet("Question (prealloc type: %d)...\n",
	                    packet->prealloc_type);
	if ((err = knot_packet_parse_question(wireformat, &pos, size,
	                   &packet->question,
	                   packet->prealloc_type == KNOT_PACKET_PREALLOC_NONE)
	           ) != KNOT_EOK) {
		return err;
	}
	packet->header.qdcount = 1;

	packet->parsed = pos;

	if (question_only) {
		return KNOT_EOK;
	}

	/*! \todo Replace by call to parse_rest()? */
	err = knot_packet_parse_rr_sections(packet, &pos);

//	debug_knot_packet("Answer RRs...\n");
//	if ((err = knot_packet_parse_rrs(wireformat, &pos, size,
//	    packet->header.ancount, &packet->answer, &packet->an_rrsets,
//	    &packet->max_an_rrsets, DEFAULT_RRSET_COUNT(ANCOUNT, packet),
//	    packet)) != KNOT_EOK) {
//		return err;
//	}

//	debug_knot_packet("Authority RRs...\n");
//	if ((err = knot_packet_parse_rrs(wireformat, &pos, size,
//	    packet->header.nscount, &packet->authority, &packet->ns_rrsets,
//	    &packet->max_ns_rrsets, DEFAULT_RRSET_COUNT(NSCOUNT, packet),
//	    packet)) != KNOT_EOK) {
//		return err;
//	}

//	debug_knot_packet("Additional RRs...\n");
//	if ((err = knot_packet_parse_rrs(wireformat, &pos, size,
//	    packet->header.arcount, &packet->additional, &packet->ar_rrsets,
//	    &packet->max_ar_rrsets, DEFAULT_RRSET_COUNT(ARCOUNT, packet),
//	    packet)) != KNOT_EOK) {
//		return err;
//	}

//	if (pos < size) {
//		// some trailing garbage; ignore, but log
//		debug_knot_response("Packet: %zu bytes of trailing garbage "
//		                      "in packet.\n", pos - size);
//	}

//	packet->parsed = size;

#ifdef KNOT_PACKET_DEBUG
	knot_packet_dump(packet);
#endif /* KNOT_RESPONSE_DEBUG */

	return err;
}

/*----------------------------------------------------------------------------*/

int knot_packet_parse_rest(knot_packet_t *packet)
{
	if (packet == NULL) {
		return KNOT_EBADARG;
	}

	if (packet->parsed >= packet->size) {
		return KNOT_EOK;
	}

	size_t pos = packet->parsed;

	return knot_packet_parse_rr_sections(packet, &pos);
}

/*----------------------------------------------------------------------------*/

int knot_packet_parse_next_rr_answer(knot_packet_t *packet,
                                       knot_rrset_t **rr)
{
	if (packet == NULL || rr == NULL) {
		return KNOT_EBADARG;
	}

	if (packet->parsed >= packet->size
	    || packet->an_rrsets == packet->header.ancount) {
		return KNOT_EOK;
	}

	size_t pos = packet->parsed;

	debug_knot_packet("Parsing next Answer RR...\n");
	*rr = knot_packet_parse_rr(packet->wireformat, &pos, packet->size);
	if (*rr == NULL) {
		debug_knot_packet("Failed to parse RR!\n");
		return KNOT_EMALF;
	}

	packet->parsed = pos;
	// increment the number of answer RRSets, though there are no saved
	// in the packet; it is OK, because packet->answer is NULL
	++packet->an_rrsets;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

int knot_packet_set_max_size(knot_packet_t *packet, int max_size)
{
	if (packet == NULL || max_size <= 0) {
		return KNOT_EBADARG;
	}

	if (packet->max_size < max_size) {
		// reallocate space for the wire format (and copy anything
		// that might have been there before
		uint8_t *wire_new = (uint8_t *)malloc(max_size);
		if (wire_new == NULL) {
			return KNOT_ENOMEM;
		}

		uint8_t *wire_old = packet->wireformat;

		memcpy(wire_new, packet->wireformat, packet->max_size);
		packet->wireformat = wire_new;

		if (packet->max_size > 0 && packet->free_wireformat) {
			free(wire_old);
		}

		packet->free_wireformat = 1;
	}

	// set max size
	packet->max_size = max_size;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

uint16_t knot_packet_id(const knot_packet_t *packet)
{
	assert(packet != NULL);
	return packet->header.id;
}

/*----------------------------------------------------------------------------*/

uint8_t knot_packet_opcode(const knot_packet_t *packet)
{
	assert(packet != NULL);
	return knot_wire_flags_get_opcode(packet->header.flags1);
}

/*----------------------------------------------------------------------------*/

const knot_dname_t *knot_packet_qname(const knot_packet_t *packet)
{
	if (packet == NULL) {
		return NULL;
	}

	return packet->question.qname;
}

/*----------------------------------------------------------------------------*/

uint16_t knot_packet_qtype(const knot_packet_t *packet)
{
	assert(packet != NULL);
	return packet->question.qtype;
}

/*----------------------------------------------------------------------------*/

uint16_t knot_packet_qclass(const knot_packet_t *packet)
{
	assert(packet != NULL);
	return packet->question.qclass;
}

/*----------------------------------------------------------------------------*/

int knot_packet_is_query(const knot_packet_t *packet)
{
	if (packet == NULL) {
		return KNOT_EBADARG;
	}

	return (knot_wire_flags_get_qr(packet->header.flags1) == 0);
}

/*----------------------------------------------------------------------------*/

const knot_packet_t *knot_packet_query(const knot_packet_t *packet)
{
	if (packet == NULL) {
		return NULL;
	}

	return packet->query;
}

/*----------------------------------------------------------------------------*/

short knot_packet_answer_rrset_count(const knot_packet_t *packet)
{
	if (packet == NULL) {
		return KNOT_EBADARG;
	}

	return packet->an_rrsets;
}

/*----------------------------------------------------------------------------*/

short knot_packet_authority_rrset_count(const knot_packet_t *packet)
{
	if (packet == NULL) {
		return KNOT_EBADARG;
	}

	return packet->ns_rrsets;
}

/*----------------------------------------------------------------------------*/

short knot_packet_additional_rrset_count(const knot_packet_t *packet)
{
	if (packet == NULL) {
		return KNOT_EBADARG;
	}

	return packet->ar_rrsets;
}

/*----------------------------------------------------------------------------*/

const knot_rrset_t *knot_packet_answer_rrset(
	const knot_packet_t *packet, short pos)
{
	if (packet == NULL || pos > packet->an_rrsets) {
		return NULL;
	}

	return packet->answer[pos];
}

/*----------------------------------------------------------------------------*/

const knot_rrset_t *knot_packet_authority_rrset(
	knot_packet_t *packet, short pos)
{
	if (packet == NULL || pos > packet->ns_rrsets) {
		return NULL;
	}

	return packet->authority[pos];
}

/*----------------------------------------------------------------------------*/

const knot_rrset_t *knot_packet_additional_rrset(
	knot_packet_t *packet, short pos)
{
	if (packet == NULL || pos > packet->ar_rrsets) {
		return NULL;
	}

	return packet->additional[pos];
}

/*----------------------------------------------------------------------------*/

int knot_packet_contains(const knot_packet_t *packet,
                           const knot_rrset_t *rrset,
                           knot_rrset_compare_type_t cmp)
{
	if (packet == NULL || rrset == NULL) {
		return KNOT_EBADARG;
	}

	for (int i = 0; i < packet->header.ancount; ++i) {
		if (knot_rrset_compare(packet->answer[i], rrset, cmp)) {
			return 1;
		}
	}

	for (int i = 0; i < packet->header.nscount; ++i) {
		if (knot_rrset_compare(packet->authority[i], rrset, cmp)) {
			return 1;
		}
	}

	for (int i = 0; i < packet->header.arcount; ++i) {
		if (knot_rrset_compare(packet->additional[i], rrset, cmp)) {
			return 1;
		}
	}

	return 0;
}

/*----------------------------------------------------------------------------*/

int knot_packet_add_tmp_rrset(knot_packet_t *packet,
                                knot_rrset_t *tmp_rrset)
{
	if (packet == NULL || tmp_rrset == NULL) {
		return KNOT_EBADARG;
	}

	if (packet->tmp_rrsets_count == packet->tmp_rrsets_max
	    && knot_packet_realloc_rrsets(&packet->tmp_rrsets,
			&packet->tmp_rrsets_max,
			DEFAULT_RRSET_COUNT(TMP_RRSETS, packet),
			STEP_TMP_RRSETS) != KNOT_EOK) {
		return KNOT_ENOMEM;
	}

	packet->tmp_rrsets[packet->tmp_rrsets_count++] = tmp_rrset;
	debug_knot_packet("Current tmp RRSets count: %d, max count: %d\n",
	                    packet->tmp_rrsets_count, packet->tmp_rrsets_max);

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/
/*!
 * \brief Frees all temporary RRSets stored in the response structure.
 *
 * \param resp Response structure to free the temporary RRSets from.
 */
void knot_packet_free_tmp_rrsets(knot_packet_t *pkt)
{
	if (pkt == NULL) {
		return;
	}

	for (int i = 0; i < pkt->tmp_rrsets_count; ++i) {
DEBUG_KNOT_PACKET(
		char *name = knot_dname_to_str(
			(((knot_rrset_t **)(pkt->tmp_rrsets))[i])->owner);
		debug_knot_packet("Freeing tmp RRSet on ptr: %p (ptr to ptr:"
		       " %p, type: %s, owner: %s)\n",
		       (((knot_rrset_t **)(pkt->tmp_rrsets))[i]),
		       &(((knot_rrset_t **)(pkt->tmp_rrsets))[i]),
		       knot_rrtype_to_string(
		            (((knot_rrset_t **)(pkt->tmp_rrsets))[i])->type),
		       name);
		free(name);
);
		// TODO: this is quite ugly, but better than copying whole
		// function (for reallocating rrset array)
		knot_rrset_deep_free(
			&(((knot_rrset_t **)(pkt->tmp_rrsets))[i]), 1, 1, 1);
	}
}

/*----------------------------------------------------------------------------*/

void knot_packet_header_to_wire(const knot_header_t *header,
                                  uint8_t **pos, size_t *size)
{
	if (header == NULL || pos == NULL || *pos == NULL || size == NULL) {
		return;
	}

	knot_wire_set_id(*pos, header->id);
	knot_wire_set_flags1(*pos, header->flags1);
	knot_wire_set_flags2(*pos, header->flags2);
	knot_wire_set_qdcount(*pos, header->qdcount);
	knot_wire_set_ancount(*pos, header->ancount);
	knot_wire_set_nscount(*pos, header->nscount);
	knot_wire_set_arcount(*pos, header->arcount);

	*pos += KNOT_WIRE_HEADER_SIZE;
	*size += KNOT_WIRE_HEADER_SIZE;
}

/*----------------------------------------------------------------------------*/

int knot_packet_question_to_wire(knot_packet_t *packet)
{
	if (packet == NULL) {
		return KNOT_EBADARG;
	}

	if (packet->size > KNOT_WIRE_HEADER_SIZE) {
		return KNOT_ERROR;
	}

	// TODO: get rid of the numeric constants
	size_t qsize = 4 + knot_dname_size(packet->question.qname);
	if (qsize > packet->max_size - KNOT_WIRE_HEADER_SIZE) {
		return KNOT_ESPACE;
	}

	// create the wireformat of Question
	uint8_t *pos = packet->wireformat + KNOT_WIRE_HEADER_SIZE;
	memcpy(pos, knot_dname_name(packet->question.qname),
	       knot_dname_size(packet->question.qname));

	pos += knot_dname_size(packet->question.qname);
	knot_wire_write_u16(pos, packet->question.qtype);
	pos += 2;
	knot_wire_write_u16(pos, packet->question.qclass);

//	int err = 0;
	// TODO: put the qname into the compression table
//	// TODO: get rid of the numeric constants
//	if ((err = knot_response_store_dname_pos(&packet->compression,
//	              packet->question.qname,0, 12, 12)) != KNOT_EOK) {
//		return err;
//	}

	packet->size += knot_dname_size(packet->question.qname) + 4;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

int knot_packet_edns_to_wire(knot_packet_t *packet)
{
	if (packet == NULL) {
		return KNOT_EBADARG;
	}

	packet->size += knot_edns_to_wire(&packet->opt_rr,
	                                  packet->wireformat + packet->size,
	                                  packet->max_size - packet->size);

	packet->header.arcount += 1;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

int knot_packet_to_wire(knot_packet_t *packet,
                          uint8_t **wire, size_t *wire_size)
{
	if (packet == NULL || wire == NULL || wire_size == NULL
	    || *wire != NULL) {
		return KNOT_EBADARG;
	}

	assert(packet->size <= packet->max_size);

	// if there are no additional RRSets, add EDNS OPT RR
	if (packet->header.arcount == 0
	    && packet->opt_rr.version != EDNS_NOT_SUPPORTED) {
	    knot_packet_edns_to_wire(packet);
	}

	// set QDCOUNT (in response it is already set, in query it is needed)
	knot_wire_set_qdcount(packet->wireformat, packet->header.qdcount);
	// set ANCOUNT to the packet
	knot_wire_set_ancount(packet->wireformat, packet->header.ancount);
	// set NSCOUNT to the packet
	knot_wire_set_nscount(packet->wireformat, packet->header.nscount);
	// set ARCOUNT to the packet
	knot_wire_set_arcount(packet->wireformat, packet->header.arcount);

	//assert(response->size == size);
	*wire = packet->wireformat;
	*wire_size = packet->size;

	return KNOT_EOK;
}

/*----------------------------------------------------------------------------*/

void knot_packet_free(knot_packet_t **packet)
{
	if (packet == NULL || *packet == NULL) {
		return;
	}

	// free temporary domain names
	debug_knot_packet("Freeing tmp RRSets...\n");
	knot_packet_free_tmp_rrsets(*packet);

	// check if some additional space was allocated for the packet
	debug_knot_packet("Freeing additional allocated space...\n");
	knot_packet_free_allocated_space(*packet);

	// free the space for wireformat
//	assert((*packet)->wireformat != NULL);
//	free((*packet)->wireformat);
	if ((*packet)->wireformat != NULL && (*packet)->free_wireformat) {
		free((*packet)->wireformat);
	}

	debug_knot_packet("Freeing packet structure\n");
	free(*packet);
	*packet = NULL;
}

/*----------------------------------------------------------------------------*/
#ifdef KNOT_PACKET_DEBUG
static void knot_packet_dump_rrsets(const knot_rrset_t **rrsets,
                                      int count)
{
	assert(rrsets != NULL && *rrsets != NULL);

	for (int i = 0; i < count; ++i) {
		knot_rrset_dump(rrsets[i], 0);
	}
}
#endif
/*----------------------------------------------------------------------------*/

void knot_packet_dump(const knot_packet_t *packet)
{
	if (packet == NULL) {
		return;
	}

#ifdef KNOT_PACKET_DEBUG
	debug_knot_packet("DNS packet:\n-----------------------------\n");

	debug_knot_packet("\nHeader:\n");
	debug_knot_packet("  ID: %u", packet->header.id);
	debug_knot_packet("  FLAGS: %s %s %s %s %s %s %s\n",
	       knot_wire_flags_get_qr(packet->header.flags1) ? "qr" : "",
	       knot_wire_flags_get_aa(packet->header.flags1) ? "aa" : "",
	       knot_wire_flags_get_tc(packet->header.flags1) ? "tc" : "",
	       knot_wire_flags_get_rd(packet->header.flags1) ? "rd" : "",
	       knot_wire_flags_get_ra(packet->header.flags2) ? "ra" : "",
	       knot_wire_flags_get_ad(packet->header.flags2) ? "ad" : "",
	       knot_wire_flags_get_cd(packet->header.flags2) ? "cd" : "");
	debug_knot_packet("  QDCOUNT: %u\n", packet->header.qdcount);
	debug_knot_packet("  ANCOUNT: %u\n", packet->header.ancount);
	debug_knot_packet("  NSCOUNT: %u\n", packet->header.nscount);
	debug_knot_packet("  ARCOUNT: %u\n", packet->header.arcount);

	debug_knot_packet("\nQuestion:\n");
	char *qname = knot_dname_to_str(packet->question.qname);
	debug_knot_packet("  QNAME: %s\n", qname);
	free(qname);
	debug_knot_packet("  QTYPE: %u (%s)\n", packet->question.qtype,
	       knot_rrtype_to_string(packet->question.qtype));
	debug_knot_packet("  QCLASS: %u (%s)\n", packet->question.qclass,
	       knot_rrclass_to_string(packet->question.qclass));

	debug_knot_packet("\nAnswer RRSets:\n");
	knot_packet_dump_rrsets(packet->answer, packet->an_rrsets);
	debug_knot_packet("\nAuthority RRSets:\n");
	knot_packet_dump_rrsets(packet->authority, packet->ns_rrsets);
	debug_knot_packet("\nAdditional RRSets:\n");
	knot_packet_dump_rrsets(packet->additional, packet->ar_rrsets);

	/*! \todo Dumping of Answer, Authority and Additional sections. */

	debug_knot_packet("\nEDNS:\n");
	debug_knot_packet("  Version: %u\n", packet->opt_rr.version);
	debug_knot_packet("  Payload: %u\n", packet->opt_rr.payload);
	debug_knot_packet("  Extended RCODE: %u\n",
	                      packet->opt_rr.ext_rcode);

	debug_knot_packet("\nPacket size: %d\n", packet->size);
	debug_knot_packet("\n-----------------------------\n");
#endif
}
