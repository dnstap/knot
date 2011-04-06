#include <config.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <netinet/in.h>

#include "dnslib/dnslib-common.h"
#include "dnslib/zone-dump.h"
#include "dnslib/dnslib.h"
#include "dnslib/debug.h"
#include "common/skip-list.h"
#include "common/base32hex.h"
#include "dnslib/error.h"

#define ZONECHECKS_VERBOSE

/*! \note For space and speed purposes, dname ID (to be later used in loading)
 * is being stored in dname->node field. Not to be confused with dname's actual
 * node.
 */

/*! \note Contents of dump file:
 * MAGIC(knotxx) NUMBER_OF_NORMAL_NODES NUMBER_OF_NSEC3_NODES
 * [normal_nodes] [nsec3_nodes]
 * node has following format:
 * owner_size owner_wire owner_label_size owner_labels owner_id
 * node_flags node_rrset_count [node_rrsets]
 * rrset has following format:
 * rrset_type rrset_class rrset_ttl rrset_rdata_count rrset_rrsig_count
 * [rrset_rdata] [rrset_rrsigs]
 * rdata can either contain full dnames (that is with labels but without ID)
 * or dname ID, if dname is in the zone
 * or raw data stored like this: data_len [data]
 */

static const uint MAX_CNAME_CYCLE_DEPTH = 15;

/*!
 *\brief Internal error constants. General errors are added for convenience,
 *       so that code does not have to change if new errors are added.
 */
enum zonechecks_errors {
	ZC_ERR_ALLOC = -35,
	ZC_ERR_UNKNOWN,

	ZC_ERR_MISSING_SOA,

	ZC_ERR_GENERIC_GENERAL_ERROR, /* isn't there a better name? */

	ZC_ERR_RRSIG_RDATA_TYPE_COVERED,
	ZC_ERR_RRSIG_RDATA_TTL,
	ZC_ERR_RRSIG_RDATA_LABELS,
	ZC_ERR_RRSIG_RDATA_DNSKEY_OWNER,
	ZC_ERR_RRSIG_RDATA_SIGNED_WRONG,
	ZC_ERR_RRSIG_NO_RRSIG,
	ZC_ERR_RRSIG_SIGNED,
	ZC_ERR_RRSIG_OWNER,
	ZC_ERR_RRSIG_CLASS,
	ZC_ERR_RRSIG_TTL,
	ZC_ERR_RRSIG_NOT_ALL,

	ZC_ERR_RRSIG_GENERAL_ERROR,

	ZC_ERR_NO_NSEC,
	ZC_ERR_NSEC_RDATA_BITMAP,
	ZC_ERR_NSEC_RDATA_MULTIPLE,
	ZC_ERR_NSEC_RDATA_CHAIN,
	ZC_ERR_NSEC_RDATA_CHAIN_NOT_CYCLIC,

	ZC_ERR_NSEC_GENERAL_ERROR,

	ZC_ERR_NSEC3_UNSECURED_DELEGATION,
	ZC_ERR_NSEC3_NOT_FOUND,
	ZC_ERR_NSEC3_UNSECURED_DELEGATION_OPT,
	ZC_ERR_NSEC3_RDATA_TTL,
	ZC_ERR_NSEC3_RDATA_CHAIN,
	ZC_ERR_NSEC3_RDATA_BITMAP,

	ZC_ERR_NSEC3_GENERAL_ERROR,

	ZC_ERR_CNAME_CYCLE,
	ZC_ERR_CNAME_EXTRA_RECORDS,
	ZC_ERR_CNAME_EXTRA_RECORDS_DNSSEC,
	ZC_ERR_CNAME_MULTIPLE,

	ZC_ERR_CNAME_GENERAL_ERROR,

	ZC_ERR_GLUE_NODE,
	ZC_ERR_GLUE_RECORD,

	ZC_ERR_GLUE_GENERAL_ERROR,
};

static char *error_messages[(-ZC_ERR_ALLOC) + 1] = {
	[0] = "nil\n",

	[-ZC_ERR_ALLOC] = "Memory allocation error!\n",

	[-ZC_ERR_MISSING_SOA] = "SOA record missing in zone!\n",

	[-ZC_ERR_RRSIG_RDATA_TYPE_COVERED] =
	"RRSIG: Type covered rdata field is wrong!\n",
	[-ZC_ERR_RRSIG_RDATA_TTL] =
	"RRSIG: TTL rdata field is wrong!\n",
	[-ZC_ERR_RRSIG_RDATA_LABELS] =
	"RRSIG: Labels rdata field is wrong!\n",
	[-ZC_ERR_RRSIG_RDATA_DNSKEY_OWNER] =
	"RRSIG: Signer name is different than in DNSKEY!\n",
	[-ZC_ERR_RRSIG_RDATA_SIGNED_WRONG] =
	"RRSIG: Key error!\n",
	[-ZC_ERR_RRSIG_NO_RRSIG] =
	"RRSIG: No RRSIG!\n",
	[-ZC_ERR_RRSIG_SIGNED] =
	"RRSIG: Signed RRSIG!\n",
	[-ZC_ERR_RRSIG_OWNER] =
	"RRSIG: Owner name rdata field is wrong!\n",
	[-ZC_ERR_RRSIG_CLASS] =
	"RRSIG: Class is wrong!\n",
	[-ZC_ERR_RRSIG_TTL] =
	"RRSIG: TTL is wrong!\n",
	[-ZC_ERR_RRSIG_NOT_ALL] =
	"RRSIG: Not all RRs are signed!\n",

	[-ZC_ERR_NO_NSEC] =
	"NSEC: Missing NSEC record\n",
	[-ZC_ERR_NSEC_RDATA_BITMAP] =
	"NSEC: Wrong NSEC bitmap!\n",
	[-ZC_ERR_NSEC_RDATA_MULTIPLE] =
	"NSEC: Multiple NSEC records!\n",
	[-ZC_ERR_NSEC_RDATA_CHAIN] =
	"NSEC: NSEC chain is not coherent!\n",
	[-ZC_ERR_NSEC_RDATA_CHAIN_NOT_CYCLIC] =
	"NSEC: NSEC chain is not cyclic!\n",

	[-ZC_ERR_NSEC3_UNSECURED_DELEGATION] =
	"NSEC3: Zone contains unsecured delegation!\n",
	[-ZC_ERR_NSEC3_NOT_FOUND] =
	"NSEC3: Could not find previous NSEC3 record in the zone!\n",
	[-ZC_ERR_NSEC3_UNSECURED_DELEGATION_OPT] =
	"NSEC3: Unsecured delegation is not part "
	"of the Opt-Out span!\n",
	[-ZC_ERR_NSEC3_RDATA_TTL] =
	"NSEC3: Original TTL rdata field is wrong!\n",
	[-ZC_ERR_NSEC3_RDATA_CHAIN] =
	"NSEC3: NSEC3 chain is not coherent!\n",
	[-ZC_ERR_NSEC3_RDATA_BITMAP] =
	"NSEC3: NSEC3 bitmap error!\n",

	[-ZC_ERR_CNAME_CYCLE] =
	"CNAME: CNAME cycle!\n",
	[-ZC_ERR_CNAME_EXTRA_RECORDS] =
	"CNAME: Node with CNAME record has other records!\n",
	[-ZC_ERR_CNAME_EXTRA_RECORDS_DNSSEC] =
	"CNAME: Node with CNAME record has other "
	"records than RRSIG and NSEC/NSEC3!\n",
	[-ZC_ERR_CNAME_MULTIPLE] = "CNAME: Multiple CNAME records!\n",

	/* ^
	   | Important errors (to be logged on first occurence and counted) */


	/* Below are errors of lesser importance, to be counted unless
	   specified otherwise */

	[-ZC_ERR_GLUE_NODE] =
	"GLUE: Node with Glue record missing!\n",
	[-ZC_ERR_GLUE_RECORD] =
	"GLUE: Record with Glue address missing\n",
};

/*!
 * \brief Structure representing handle options.
 */
struct handler_options {
	char log_cname; /*!< Log all CNAME related semantic errors. */
	char log_glue; /*!< Log all glue related semantic errors. */
	char log_rrsigs; /*!< Log all RRSIG related semantic errors. */
	char log_nsec; /*!< Log all NSEC related semantic errors. */
	char log_nsec3; /*!< Log all NSEC3 related semantic errors. */
};

/*!
 * \brief Structure for handling semantic errors.
 */
struct err_handler {
	/* Consider moving error messages here */
	struct handler_options options; /*!< Handler options. */
	uint errors[(-ZC_ERR_ALLOC) + 1]; /*!< Array with error messages */
};

typedef struct err_handler err_handler_t;

/*!
 * \brief Creates new semantic error handler.
 *
 * \param log_cname If true, log all CNAME related events.
 * \param log_glue If true, log all CNAME related events.
 * \param log_rrsigs If true, log all CNAME related events.
 * \param log_nsec If true, log all CNAME related events.
 * \param log_nsec3 If true, log all CNAME related events.
 *
 * \return err_handler_t * Created error handler.
 */
static err_handler_t *handler_new(char log_cname, char log_glue,
				  char log_rrsigs, char log_nsec,
				  char log_nsec3)
{
	err_handler_t *handler = malloc(sizeof(err_handler_t));
	CHECK_ALLOC_LOG(handler, NULL);

	/* It should be initialized, but to be safe */
	memset(handler->errors, 0, sizeof(uint) * (-ZC_ERR_ALLOC + 1));

	handler->options.log_cname = log_cname;
	handler->options.log_glue = log_glue;
	handler->options.log_rrsigs = log_rrsigs;
	handler->options.log_nsec = log_nsec;
	handler->options.log_nsec3 = log_nsec3;

	return handler;
}

/*!
 * \brief Prints error message with node information.
 *
 * \param handler Error handler.
 * \param node Node with semantic error in it.
 * \param error Type of error.
 */
static void log_error_from_node(err_handler_t *handler,
				const dnslib_node_t *node,
				uint error)
{
	/* TODO not like this */
	if (node != NULL) {
		char *name =
			dnslib_dname_to_str(dnslib_node_owner(node));
		fprintf(stderr, "Semantic error in node: %s: ", name);
		fprintf(stderr, "%s", error_messages[error]);
		free(name);
	} else {
		fprintf(stderr, "Total number of errors: %s: %d",
			error_messages[error],
			handler->errors[error]);
	}
}

/*!
 * \brief Called when error has been encountered in node. Will either log error
 *        or print it, depending on handler's options.
 *
 * \param handler Error handler.
 * \param node Node with semantic error in it.
 * \param error Type of error.
 *
 * \retval DNSLIB_EOK on success.
 * \retval ZC_ERR_UNKNOWN if unknown error.
 * \retval ZC_ERR_ALLOC if memory error.
 */
static int err_handler_handle_error(err_handler_t *handler,
				    const dnslib_node_t *node,
				    uint error)
{
	printf("%d\n", error);
	if ((error != 0) &&
	    (error > ZC_ERR_GLUE_GENERAL_ERROR)) {
		return ZC_ERR_UNKNOWN;
	}

	if (error == ZC_ERR_ALLOC) {
		ERR_ALLOC_FAILED;
		return ZC_ERR_ALLOC;
	}

	/* missing SOA can only occur once, so there
	 * needn't to be an option for it */

	if ((error != 0) &&
	    (error < ZC_ERR_GENERIC_GENERAL_ERROR)) {

		/* The two errors before SOA were handled */
		log_error_from_node(handler, node, error);

	} else if ((error < ZC_ERR_RRSIG_GENERAL_ERROR) &&
		   ((handler->errors[error] == 0) ||
		   (handler->options.log_rrsigs))) {

		log_error_from_node(handler, node, error);

	} else if ((error > ZC_ERR_RRSIG_GENERAL_ERROR) &&
		   (error < ZC_ERR_NSEC_GENERAL_ERROR) &&
		   ((handler->errors[error] == 0) ||
		    (handler->options.log_nsec))) {

		log_error_from_node(handler, node, error);

	} else if ((error > ZC_ERR_NSEC_GENERAL_ERROR) &&
		   (error < ZC_ERR_NSEC3_GENERAL_ERROR) &&
		   ((handler->errors[error] == 0) ||
		    (handler->options.log_nsec3))) {

		log_error_from_node(handler, node, error);

	} else if ((error > ZC_ERR_NSEC3_GENERAL_ERROR) &&
		   (error < ZC_ERR_CNAME_GENERAL_ERROR) &&
		   ((handler->errors[error] == 0) ||
		    (handler->options.log_cname))) {

		log_error_from_node(handler, node, error);

	} else if ((error > ZC_ERR_CNAME_GENERAL_ERROR) &&
		   (error < ZC_ERR_GLUE_GENERAL_ERROR) &&
		    handler->options.log_glue) {

		log_error_from_node(handler, node, error);

	}

	handler->errors[error]++;

	return DNSLIB_EOK;
}

/*!
 * \brief This function prints all errors that occured in zone.
 *
 * \param handler Error handler containing found errors.
 */
static void err_handler_log_all(err_handler_t *handler)
{
	for (int i = 0; i < ZC_ERR_GLUE_GENERAL_ERROR; i++) {
		if (handler->errors[i] > 0) {
			log_error_from_node(handler, NULL, i);
		}
	}
}

/* TODO CHANGE FROM VOID POINTERS */
/*!
 * \brief
 *
 */
struct arg {
	void *arg1; /* FILE *f / zone */
	void *arg2; /* skip_list_t */
	void *arg3; /* zone */
	void *arg4; /* first node */
	void *arg5; /* last node */
	void *arg6; /* error handler */
};

typedef struct arg arg_t;

/*!
 * \brief Simple pointer compare, used for skip list ordering.
 *
 * \note We only need ordering for search purposes, therefore it is OK to
 *       compare pointers directly.
 *
 * \param p1 First pointer.
 * \param p2 Second pointer.
 *
 * \retval 0 when pointers are the same.
 * \retval -1 when first pointer is "bigger" than the first.
 * \retval 1 when second pointer is "bigger" than the first.
 */
static int compare_pointers(void *p1, void *p2)
{
	return ((size_t)p1 == (size_t)p2
		? 0 : (size_t)p1 < (size_t)p2 ? -1 : 1);
}

/*!
 * \brief Saves closest encloser from rdata to given skip list.
 *
 * \param rdata Rdata possibly containing closest encloser.
 * \param zone Zone containing the node.
 * \param pos Position of item in rdata to be searched.
 * \param list Skip list used for storing closest enclosers.
 */
static void dnslib_zone_save_encloser_rdata_item(dnslib_rdata_t *rdata,
						 dnslib_zone_t *zone, uint pos,
						 skip_list_t *list)
{
	const dnslib_rdata_item_t *dname_item
		= dnslib_rdata_item(rdata, pos);

	if (dname_item != NULL) {
		dnslib_dname_t *dname = dname_item->dname;
		const dnslib_node_t *n = NULL;
		const dnslib_node_t *closest_encloser = NULL;
		const dnslib_node_t *prev = NULL;

		int exact = dnslib_zone_find_dname(zone, dname, &n,
						   &closest_encloser, &prev);

//		n = dnslib_zone_find_node(zone, dname);

		assert(!exact || n == closest_encloser);

		if (!exact && (closest_encloser != NULL)) {
			debug_dnslib_zdump("Saving closest encloser to RDATA."
					   "\n");
			// save pointer to the closest encloser
			dnslib_rdata_item_t *item =
				dnslib_rdata_get_item(rdata, pos);
			assert(item->dname != NULL);
			assert(item->dname->node == NULL);
			skip_insert(list, (void *)item->dname,
				    (void *)closest_encloser->owner, NULL);
		}
	}
}

/*!
 * \brief Saves closest enclosers from all entries in rrset.
 *
 * \param rrset RRSet to be searched.
 * \param zone Zone containing the rrset.
 * \param list Skip list used for storing closest enclosers.
 */
static void dnslib_zone_save_enclosers_rrset(dnslib_rrset_t *rrset,
					     dnslib_zone_t *zone,
					     skip_list_t *list)
{
	uint16_t type = dnslib_rrset_type(rrset);

	dnslib_rrtype_descriptor_t *desc =
		dnslib_rrtype_descriptor_by_type(type);
	dnslib_rdata_t *rdata_first = dnslib_rrset_get_rdata(rrset);
	dnslib_rdata_t *rdata = rdata_first;

	if (rdata == NULL) {
		return;
	}

	while (rdata->next != rdata_first) {
		for (int i = 0; i < rdata->count; ++i) {
			if (desc->wireformat[i]
			    == DNSLIB_RDATA_WF_COMPRESSED_DNAME
			    || desc->wireformat[i]
			       == DNSLIB_RDATA_WF_UNCOMPRESSED_DNAME
			    || desc->wireformat[i]
			       == DNSLIB_RDATA_WF_LITERAL_DNAME) {
				debug_dnslib_zdump("Adjusting domain name at "
				  "position %d of RDATA of record with owner "
				  "%s and type %s.\n",
				  i, rrset->owner->name,
				  dnslib_rrtype_to_string(type));

				dnslib_zone_save_encloser_rdata_item(rdata,
								     zone,
								     i,
								     list);
			}
		}
		rdata = rdata->next;
	}

	for (int i = 0; i < rdata->count; ++i) {
		if (desc->wireformat[i]
		    == DNSLIB_RDATA_WF_COMPRESSED_DNAME
		    || desc->wireformat[i]
		       == DNSLIB_RDATA_WF_UNCOMPRESSED_DNAME
		    || desc->wireformat[i]
		       == DNSLIB_RDATA_WF_LITERAL_DNAME) {
			debug_dnslib_zdump("Adjusting domain name at "
			  "position %d of RDATA of record with owner "
			  "%s and type %s.\n",
			  i, rrset->owner->name,
			  dnslib_rrtype_to_string(type));

				dnslib_zone_save_encloser_rdata_item(rdata,
								     zone,
								     i,
								     list);
		}
	}
}

/*!
 * \brief Semantic check - CNAME cycles. Uses constant value with maximum
 *        allowed CNAME chain depth.
 *
 * \param zone Zone containing the RRSet.
 * \param rrset RRSet to be tested.
 *
 * \retval DNSLIB_EOK when there is no cycle.
 * \retval ZC_ERR_CNAME_CYCLE when cycle is present.
 */
static int check_cname_cycles_in_zone(dnslib_zone_t *zone,
				      const dnslib_rrset_t *rrset)
{
	const dnslib_rrset_t *next_rrset = rrset;
	assert(rrset);
	const dnslib_rdata_t *tmp_rdata = dnslib_rrset_rdata(next_rrset);
	const dnslib_node_t *next_node = NULL;

	uint i = 0;

	assert(tmp_rdata);

	const dnslib_dname_t *next_dname =
		dnslib_rdata_cname_name(tmp_rdata);

	assert(next_dname);

	while (i < MAX_CNAME_CYCLE_DEPTH && next_dname != NULL) {
		next_node = dnslib_zone_get_node(zone, next_dname);
		if (next_node == NULL) {
			next_node =
				dnslib_zone_get_nsec3_node(zone, next_dname);
		}

		if (next_node != NULL) {
			next_rrset = dnslib_node_rrset(next_node,
						       DNSLIB_RRTYPE_CNAME);
			if (next_rrset != NULL) {
				next_dname =
				dnslib_rdata_cname_name(next_rrset->rdata);
			} else {
				next_node = NULL;
				next_dname = NULL;
			}
		} else {
			next_dname = NULL;
		}
		i++;
	}

	/* even if the length is 0, i will be 1 */
	if (i >= MAX_CNAME_CYCLE_DEPTH) {
		return ZC_ERR_CNAME_CYCLE;
	}

	return DNSLIB_EOK;
}

/*!
 * \brief Return raw data from rdata item structure (without length).
 *
 * \param item Item to get rdata from.
 * \return uint16_t * raw data without length.
 */
static inline uint16_t *rdata_item_data(const dnslib_rdata_item_t *item)
{
	return (uint16_t *)(item->raw_data + 1);
}

/*!
 * \brief Returns type covered field from RRSIG RRSet's rdata.
 *
 * \param rdata RRSIG rdata.
 * \return uint16_t Type covered.
 */
uint16_t type_covered_from_rdata(const dnslib_rdata_t *rdata)
{
	return ntohs(*(uint16_t *) rdata_item_data(&(rdata->items[0])));
}

/*!
 * \brief Check whether DNSKEY rdata are valid.
 *
 * \param rdata DNSKEY rdata to be checked.
 *
 * \retval DNSLIB_EOK when rdata are OK.
 * \retval ZC_ERR_RRSIG_RDATA_DNSKEY_OWNER when rdata are not OK.
 */
static int check_dnskey_rdata(const dnslib_rdata_t *rdata)
{
	/* check that Zone key bit it set - position 7 in net order */
	/* FIXME endian */
	uint16_t mask = 0b0000000100000000;

	uint16_t flags =
		dnslib_wire_read_u16((uint8_t *)rdata_item_data
				     (dnslib_rdata_item(rdata, 0)));

	if (flags & mask) {
		return DNSLIB_EOK;
	} else {
		/* This error does not exactly fit, but it's better
		 * than a new one */
		return ZC_ERR_RRSIG_RDATA_DNSKEY_OWNER;
	}
}

/*!
 * \brief Calculates keytag for RSA/SHA algorithm.
 *
 * \param key Key wireformat.
 * \param keysize Wireformat size.
 *
 * \return uint16_t Calculated keytag.
 */
static uint16_t keytag_1(uint8_t *key, uint16_t keysize)
{
	uint16_t ac = 0;
	if (keysize > 4) {
		memmove(&ac, key + keysize - 3, 2);
	}

	ac = ntohs(ac);
	return ac;
}

/*!
 * \brief Calculates keytag from key wire.
 *
 * \param key Key wireformat.
 * \param keysize Wireformat size.
 *
 * \return uint16_t Calculated keytag.
 */
static uint16_t keytag(uint8_t *key, uint16_t keysize )
{
	uint32_t ac = 0; /* assumed to be 32 bits or larger */

	/* algorithm RSA/SHA */
	if (key[3] == 1) {
		return keytag_1(key, keysize);
	} else {
		for(int i = 0; i < keysize; i++) {
			ac += (i & 1) ? key[i] : key[i] << 8;
		}

		ac += (ac >> 16) & 0xFFFF;
		return (uint16_t)ac & 0xFFFF;
	}
}

/*!
 * \brief Returns size of raw data item.
 *
 * \param item Raw data item.
 *
 * \return uint16_t Size of raw data item.
 */
static inline uint16_t rdata_item_size(const dnslib_rdata_item_t *item)
{
	return item->raw_data[0];
}

/*!
 * \brief Converts DNSKEY rdata to wireformat.
 *
 * \param rdata DNSKEY rdata to be converted.
 * \param wire Created wire.
 * \param size Size of created wire.
 *
 * \retval DNSLIB_EOK on success.
 * \retval DNSLIB_ENOMEM on memory error.
 */
static int dnskey_to_wire(const dnslib_rdata_t *rdata, uint8_t **wire,
			  uint *size)
{
	assert(*wire == NULL);
	/* flags + algorithm + protocol + keysize */
	*size = 2 + 1 + 1 + dnslib_rdata_item(rdata, 3)->raw_data[0];
	*wire = malloc(sizeof(uint8_t) * *size);
	CHECK_ALLOC_LOG(*wire, DNSLIB_ENOMEM);

	/* copy the wire octet by octet */

	/* TODO check if we really have that many items */

	(*wire)[0] = ((uint8_t *)(dnslib_rdata_item(rdata, 0)->raw_data))[2];
	(*wire)[1] = ((uint8_t *)(dnslib_rdata_item(rdata, 0)->raw_data))[3];

	(*wire)[2] = ((uint8_t *)(dnslib_rdata_item(rdata, 1)->raw_data))[2];
	(*wire)[3] = ((uint8_t *)(dnslib_rdata_item(rdata, 2)->raw_data))[2];

	memcpy(*wire + 4, dnslib_rdata_item(rdata, 3)->raw_data + 1,
	       dnslib_rdata_item(rdata, 3)->raw_data[0]);

	return DNSLIB_EOK;
}

/*!
 * \brief Semantic check - RRSIG rdata.
 *
 * \param rdata_rrsig RRSIG rdata to be checked.
 * \param rrset RRSet containing the rdata.
 * \param dnskey_rrset RRSet containing zone's DNSKEY
 *
 * \retval DNSLIB_EOK if rdata are OK.
 *
 * \return Appropriate error code if error was found.
 */
static int check_rrsig_rdata(const dnslib_rdata_t *rdata_rrsig,
			     const dnslib_rrset_t *rrset,
			     const dnslib_rrset_t *dnskey_rrset)
{
	if (type_covered_from_rdata(rdata_rrsig) !=
	    dnslib_rrset_type(rrset)) {
		/* zoneparser would not let this happen
		 * but to be on the safe side
		 */
		return ZC_ERR_RRSIG_RDATA_TYPE_COVERED;
	}

	/* label number at the 2nd index should be same as owner's */
	uint16_t *raw_data =
		rdata_item_data(dnslib_rdata_item(rdata_rrsig, 2));

	uint8_t labels_rdata = ((uint8_t *)raw_data)[0];

	int tmp = dnslib_dname_label_count(dnslib_rrset_owner(rrset)) -
		  labels_rdata;

	if (tmp != 0) {
		/* if name has wildcard, label must not be included */
		if (!dnslib_dname_is_wildcard(dnslib_rrset_owner(rrset))) {
			return ZC_ERR_RRSIG_RDATA_LABELS;
		} else {
			if (abs(tmp) != 1) {
				return ZC_ERR_RRSIG_RDATA_LABELS;
			}
		}
	}

	/* check original TTL */
	uint32_t original_ttl =
		dnslib_wire_read_u32((uint8_t *)rdata_item_data(
				     dnslib_rdata_item(rdata_rrsig, 3)));

	if (original_ttl != dnslib_rrset_ttl(rrset)) {
		return ZC_ERR_RRSIG_RDATA_TTL;
	}

	/* signer's name is same as in the zone apex */
	dnslib_dname_t *signer_name =
		dnslib_rdata_item(rdata_rrsig, 7)->dname;

	/* dnskey is in the apex node */
	if (dnslib_dname_compare(signer_name,
				 dnslib_rrset_owner(dnskey_rrset)) != 0) {
		return ZC_ERR_RRSIG_RDATA_DNSKEY_OWNER;
	}

	/* Compare algorithm, key tag and signer's name with DNSKEY rrset
	 * one of the records has to match. Signer name has been checked
	 * before */
	char match = 0;
	const dnslib_rdata_t *tmp_dnskey_rdata =
		dnslib_rrset_rdata(dnskey_rrset);
	do {
		uint8_t alg =
		((uint8_t *)(dnslib_rdata_item(rdata_rrsig, 1)->raw_data))[2];
		uint8_t alg_dnskey =
		((uint8_t *)(dnslib_rdata_item(tmp_dnskey_rdata,
					       2)->raw_data))[2];

		raw_data = rdata_item_data(dnslib_rdata_item(rdata_rrsig, 6));
		uint16_t key_tag_rrsig =
			dnslib_wire_read_u16((uint8_t *)raw_data);

/*		raw_data =
			rdata_item_data(dnslib_rdata_item(
					tmp_dnskey_rdata, 3));

		uint16_t raw_length = rdata_item_size(dnslib_rdata_item(
						     tmp_dnskey_rdata, 3)); */

		uint8_t *dnskey_wire = NULL;
		uint dnskey_wire_size = 0;

		if (dnskey_to_wire(tmp_dnskey_rdata, &dnskey_wire,
				   &dnskey_wire_size) != 0) {
			return ZC_ERR_ALLOC;
		}

		uint16_t key_tag_dnskey =
			keytag(dnskey_wire, dnskey_wire_size);

		free(dnskey_wire);

		match = (alg == alg_dnskey) &&
			(key_tag_rrsig == key_tag_dnskey) &&
			!check_dnskey_rdata(tmp_dnskey_rdata);

	} while (!match &&
		 ((tmp_dnskey_rdata =
			dnslib_rrset_rdata_next(dnskey_rrset,
						tmp_dnskey_rdata))
		!= NULL));

	if (!match) {
		return ZC_ERR_RRSIG_RDATA_SIGNED_WRONG;
	}

	return DNSLIB_EOK;
}

/*!
 * \brief Semantic check - RRSet's RRSIG.
 *
 * \param rrset RRSet containing RRSIG.
 * \param dnskey_rrset
 * \param nsec3 NSEC3 active.
 *
 * \retval DNSLIB_EOK on success.
 *
 * \return Appropriate error code if error was found.
 */
static int check_rrsig_in_rrset(const dnslib_rrset_t *rrset,
				const dnslib_rrset_t *dnskey_rrset,
				char nsec3)
{
	assert(dnskey_rrset && rrset);

	const dnslib_rrset_t *rrsigs = dnslib_rrset_rrsigs(rrset);

	if (rrsigs == NULL) {
		return ZC_ERR_RRSIG_NO_RRSIG;
	}

	/* signed rrsig - nonsense */
	if (dnslib_rrset_rrsigs(rrsigs) != NULL) {
		return ZC_ERR_RRSIG_SIGNED;
	}

	/* Different owner, class, ttl */

	if (dnslib_dname_compare(dnslib_rrset_owner(rrset),
				 dnslib_rrset_owner(rrsigs)) != 0) {
		return ZC_ERR_RRSIG_OWNER;
	}

	if (dnslib_rrset_class(rrset) != dnslib_rrset_class(rrsigs)) {
		return ZC_ERR_RRSIG_CLASS;
	}

	if (dnslib_rrset_ttl(rrset) != dnslib_rrset_ttl(rrset)) {
		return ZC_ERR_RRSIG_TTL;
	}

	/* Check whether all rrsets have their rrsigs */
	const dnslib_rdata_t *tmp_rdata = dnslib_rrset_rdata(rrset);
	const dnslib_rdata_t *tmp_rrsig_rdata = dnslib_rrset_rdata(rrsigs);

	assert(tmp_rdata);
	assert(tmp_rrsig_rdata);
	int ret = 0;
	do {
		if ((ret = check_rrsig_rdata(tmp_rrsig_rdata,
					     rrset,
					     dnskey_rrset)) != 0) {
			return ret;
		}
	} while ((tmp_rdata = dnslib_rrset_rdata_next(rrset, tmp_rdata))
		!= NULL &&
		((tmp_rrsig_rdata =
			dnslib_rrset_rdata_next(rrsigs, tmp_rrsig_rdata))
		!= NULL));

	if (tmp_rdata != NULL &&
	    tmp_rrsig_rdata != NULL) {
		/* Not all records in rrset are signed */
		return ZC_ERR_RRSIG_NOT_ALL;
	}

	return DNSLIB_EOK;
}

/*!
 * \brief Returns bit on index from array in network order. Taken from NSD.
 *
 * \param bits Array in network order.
 * \param index Index to return from array.
 *
 * \return int Bit on given index.
 */
int get_bit(uint8_t *bits, size_t index)
{
	/*
	 * The bits are counted from left to right, so bit #0 is the
	 * leftmost bit.
	 */
	return bits[index / 8] & (1 << (7 - index % 8));
}

/*!
 * \brief Converts NSEC bitmap to array of integers. (Inspired by NSD code)
 *
 * \param item Item containing the bitmap.
 * \param array Array to be created.
 * \param count Count of items in array.
 *
 * \retval DNSLIB_OK on success.
 * \retval DNSLIB_NOMEM on memory error.
 */
static int rdata_nsec_to_type_array(const dnslib_rdata_item_t *item,
			      uint16_t **array,
			      uint *count)
{
	assert(*array == NULL);

	uint8_t *data = (uint8_t *)rdata_item_data(item);

	int increment = 0;
	*count = 0;

	for (int i = 0; i < rdata_item_size(item); i += increment) {
		increment = 0;
		uint8_t window = data[i];
		increment++;

		uint8_t bitmap_size = data[i + increment];
		increment++;

		uint8_t *bitmap =
			malloc(sizeof(uint8_t) * (bitmap_size));

		memcpy(bitmap, data + i + increment,
		       bitmap_size);

		increment += bitmap_size;

		for (int j = 0; j < bitmap_size * 8; j++) {
			if (get_bit(bitmap, j)) {
				(*count)++;
				void *tmp = realloc(*array,
						    sizeof(uint16_t) *
						    *count);
				CHECK_ALLOC_LOG(tmp, DNSLIB_ENOMEM);
				*array = tmp;
				(*array)[*count - 1] = j + window * 256;
			}
		}
		free(bitmap);
	}

	return DNSLIB_EOK;
}

/* should write error, not return values !!! */

/*!
 * \brief Semantic check - check node's NSEC node.
 *
 * \param zone Current zone.
 * \param node Node to be checked.
 * \param handler Error handler
 *
 * \retval DNSLIB_EOK if no error was found.
 *
 * \return Appropriate error code if error was found.
 */
static int check_nsec3_node_in_zone(dnslib_zone_t *zone, dnslib_node_t *node,
				    err_handler_t *handler)
{

	assert(handler);
	const dnslib_node_t *nsec3_node = dnslib_node_nsec3_node(node);

	if (nsec3_node == NULL) {
		/* I know it's probably not what RFCs say, but it will have to
		 * do for now. */
		if (dnslib_node_rrset(node, DNSLIB_RRTYPE_DS) != NULL) {
			err_handler_handle_error(handler, node,
					ZC_ERR_NSEC3_UNSECURED_DELEGATION);
		} else {
			/* Unsecured delegation, check whether it is part of
			 * opt-out span */
			const dnslib_node_t *nsec3_previous;
			const dnslib_node_t *nsec3_node;

			if (dnslib_zone_find_nsec3_for_name(zone,
						dnslib_node_owner(node),
						&nsec3_node,
						&nsec3_previous) != 0) {
				err_handler_handle_error(handler, node,
						 ZC_ERR_NSEC3_NOT_FOUND);
			}

/* ???			if (nsec3_node == NULL) {
				return -3;
			} */

			assert(nsec3_node == NULL); /* TODO error */

			assert(nsec3_previous);

			const dnslib_rrset_t *previous_rrset =
				dnslib_node_rrset(nsec3_previous,
						  DNSLIB_RRTYPE_NSEC3);

			assert(previous_rrset);

			/* check for Opt-Out flag */
			uint8_t flags =
		((uint8_t *)(previous_rrset->rdata->items[1].raw_data))[2];

			uint8_t opt_out_mask = 0b00000001;

			if (!(flags & opt_out_mask)) {
				err_handler_handle_error(handler, node,
					ZC_ERR_NSEC3_UNSECURED_DELEGATION_OPT);
			}
		}
	}

	const dnslib_rrset_t *nsec3_rrset =
		dnslib_node_rrset(nsec3_node, DNSLIB_RRTYPE_NSEC3);

	assert(nsec3_rrset);

	uint32_t minimum_ttl =
		dnslib_wire_read_u32((uint8_t *)
		rdata_item_data(
		dnslib_rdata_item(
		dnslib_rrset_rdata(
		dnslib_node_rrset(
		dnslib_zone_apex(zone), DNSLIB_RRTYPE_SOA)), 6)));
	/* are those getters even worth this? */

	if (dnslib_rrset_ttl(nsec3_rrset) != minimum_ttl) {
			err_handler_handle_error(handler, node,
						 ZC_ERR_NSEC3_RDATA_TTL);
	}

	/* check that next dname is in the zone */

	/* TODO should look nicer :) */

	uint8_t *next_dname_decoded = malloc(sizeof(uint8_t) * 34);
	/* 34 because of the "0" at the end */
	size_t next_dname_decoded_size = 33;

	if (base32hex_decode(((char *)
		   (nsec3_rrset->rdata->items[4].raw_data)) + 3,
		   ((uint8_t *)(nsec3_rrset->rdata->items[4].raw_data))[2],
		   (char *)(next_dname_decoded + 1),
		   &next_dname_decoded_size) != 0) {
		fprintf(stderr, "Could not decode base32 string!\n");
		return DNSLIB_ERROR;
	}

	/* !!! TODO - read length from rdata? !!! */

	next_dname_decoded[0] = 32;

	dnslib_dname_t *next_dname =
		dnslib_dname_new_from_wire(next_dname_decoded,
					   next_dname_decoded_size, NULL);

	free(next_dname_decoded);

	/* TODO this should not work, what about 0 at the end??? */

	if (dnslib_dname_cat(next_dname,
		     dnslib_node_owner(dnslib_zone_apex(zone))) == NULL) {
			err_handler_handle_error(handler, node,
						 ZC_ERR_ALLOC);
	}


	if (dnslib_zone_find_nsec3_node(zone, next_dname) == NULL) {
		err_handler_handle_error(handler, node,
					 ZC_ERR_NSEC3_RDATA_CHAIN);
	}

	/* TODO first node in the nsec3 tree - is there a way? */

	dnslib_dname_free(&next_dname);

	/* This is probably not sufficient, but again, it is covered in
	 * zone load time */

	uint count;
	uint16_t *array = NULL;
	if (rdata_nsec_to_type_array(
	    dnslib_rdata_item(
	    dnslib_rrset_rdata(nsec3_rrset), 5),
	    &array, &count) != 0) {
			err_handler_handle_error(handler, node,
						 ZC_ERR_ALLOC);
			return DNSLIB_ERROR;
	}

	uint16_t type = 0;
	for (int j = 0; j < count; j++) {
		/* test for each type's presence */
		type = array[j];
		if (type == DNSLIB_RRTYPE_RRSIG) {
		       continue;
		}
		if (dnslib_node_rrset(node,
				      type) == NULL) {
			err_handler_handle_error(handler, node,
						 ZC_ERR_NSEC3_RDATA_BITMAP);
			break;
/*			char *name =
				dnslib_dname_to_str(
			log_zone_error("Node %s does "
					"not contain RRSet of type %s "
					"but NSEC bitmap says "
					"it does!\n", name,
					dnslib_rrtype_to_string(type));
			free(name); */
		}
	}

	free(array);

	return DNSLIB_EOK;
}

/*!
 * \brief Run semantic checks for node without DNSSEC-related types.
 *
 * \param zone Current zone.
 * \param node Node to be checked.
 * \param do_checks Level of checks to be done.
 * \param handler Error handler.
 *
 * \retval DNSLIB_EOK if no error was found.
 *
 * \return Appropriate error code if error was found.
 */
static int semantic_checks_plain(dnslib_zone_t *zone,
				 dnslib_node_t *node,
				 char do_checks,
				 err_handler_t *handler)
{
	assert(handler);
	const dnslib_rrset_t *cname_rrset =
			dnslib_node_rrset(node, DNSLIB_RRTYPE_CNAME);
	if (cname_rrset != NULL) {
		if (check_cname_cycles_in_zone(zone, cname_rrset) !=
				DNSLIB_EOK) {
			err_handler_handle_error(handler, node,
						 ZC_ERR_CNAME_CYCLE);
		}
	}

	/* TODO move things below to the if above */

	/* No DNSSEC and yet there is more than one rrset in node */
	if (cname_rrset && do_checks == 1 &&
	    dnslib_node_rrset_count(node) != 1) {
		err_handler_handle_error(handler, node,
					 ZC_ERR_CNAME_EXTRA_RECORDS);
	} else if (cname_rrset &&
		   dnslib_node_rrset_count(node) != 1) {
		/* With DNSSEC node can contain RRSIG or NSEC */
		if (!(dnslib_node_rrset(node, DNSLIB_RRTYPE_RRSIG) ||
		      dnslib_node_rrset(node, DNSLIB_RRTYPE_NSEC))) {
			err_handler_handle_error(handler, node,
					 ZC_ERR_CNAME_EXTRA_RECORDS_DNSSEC);
		}
	}

	/* same thing */

	if (cname_rrset &&
	    dnslib_rrset_rdata(cname_rrset)->count != 1) {
		err_handler_handle_error(handler, node,
					 ZC_ERR_CNAME_MULTIPLE);
	}

	/* check for glue records at zone cuts */
	if (dnslib_node_is_deleg_point(node)) {
		const dnslib_rrset_t *ns_rrset =
				dnslib_node_rrset(node, DNSLIB_RRTYPE_NS);
		assert(ns_rrset);
		//FIXME this should be an error as well ! (i guess)

		const dnslib_dname_t *ns_dname =
				dnslib_rdata_get_item(dnslib_rrset_rdata
						      (ns_rrset), 0)->dname;

		assert(ns_dname);

		const dnslib_node_t *glue_node =
				dnslib_zone_find_node(zone, ns_dname);

		if (dnslib_dname_is_subdomain(ns_dname,
			      dnslib_node_owner(dnslib_zone_apex(zone)))) {
			if (glue_node == NULL) {
				err_handler_handle_error(handler, node,
							 ZC_ERR_GLUE_NODE);
			} else {
				if ((dnslib_node_rrset(glue_node,
					       DNSLIB_RRTYPE_A) == NULL) &&
				    (dnslib_node_rrset(glue_node,
					       DNSLIB_RRTYPE_AAAA) == NULL)) {
					err_handler_handle_error(handler, node,
							 ZC_ERR_GLUE_RECORD);
				}
			}
		}
	}
	return DNSLIB_EOK;
}

/*!
 * \brief Run semantic checks for node without DNSSEC-related types.
 *
 * \param zone Current zone.
 * \param node Node to be checked.
 * \param first_node First node in canonical order.
 * \param last_node Last node in canonical order.
 * \param handler Error handler.
 *
 * \retval DNSLIB_EOK if no error was found.
 *
 * \return Appropriate error code if error was found.
 */
static int semantic_checks_dnssec(dnslib_zone_t *zone,
				  dnslib_node_t *node,
				  dnslib_node_t *first_node,
				  dnslib_node_t **last_node,
				  err_handler_t *handler,
				  char nsec3)
{
	assert(handler);
	assert(node);
	char auth = !dnslib_node_is_non_auth(node);
	char deleg = dnslib_node_is_deleg_point(node);
	uint rrset_count = dnslib_node_rrset_count(node);
	const dnslib_rrset_t **rrsets = dnslib_node_rrsets(node);
	const dnslib_rrset_t *dnskey_rrset =
		dnslib_node_rrset(dnslib_zone_apex(zone),
				  DNSLIB_RRTYPE_DNSKEY);

	int ret = 0;

	/* there is no point in checking non_authoritative node */
	for (int i = 0; i < rrset_count && auth; i++) {
		const dnslib_rrset_t *rrset = rrsets[i];
		if (!deleg &&
		    (ret = check_rrsig_in_rrset(rrset, dnskey_rrset,
						nsec3)) != 0) {
/*			log_zone_error("RRSIG %d node %s\n", ret,
				       dnslib_dname_to_str(node->owner));*/

			err_handler_handle_error(handler, node, ret);
		}

		if (!nsec3 && auth) {
			/* check for NSEC record */
			const dnslib_rrset_t *nsec_rrset =
					dnslib_node_rrset(node,
							  DNSLIB_RRTYPE_NSEC);

			if (nsec_rrset == NULL) {
				err_handler_handle_error(handler, node,
							 ZC_ERR_NO_NSEC);
/*				char *name =
					dnslib_dname_to_str(node->owner);
				log_zone_error("Missing NSEC in node: "
					       "%s\n", name);
				free(name);
				return; */
			} else {

				/* check NSEC/NSEC3 bitmap */

				uint count;

				uint16_t *array = NULL;

				if (rdata_nsec_to_type_array(
						dnslib_rdata_item(
						dnslib_rrset_rdata(nsec_rrset),
						1),
						&array, &count) != 0) {
					err_handler_handle_error(handler,
								 NULL,
								 ZC_ERR_ALLOC);
					return ZC_ERR_ALLOC; /* ... */
					/*return; */
				}

				uint16_t type = 0;
				for (int j = 0; j < count; j++) {
					/* test for each type's presence */
					type = array[j];
					if (type == DNSLIB_RRTYPE_RRSIG) {
						continue;
					}
					if (dnslib_node_rrset(node,
							      type) == NULL) {
					err_handler_handle_error(
						handler,
						node,
						ZC_ERR_NSEC_RDATA_BITMAP);
	/*					char *name =
						dnslib_dname_to_str(
						dnslib_node_owner(node));

						log_zone_error("Node %s does "
						"not contain RRSet of type %s "
						"but NSEC bitmap says "
					       "it does!\n", name,
					       dnslib_rrtype_to_string(type));

					free(name); */
					}
				}
				free(array);
			}

			/* Test that only one record is in the
				 * NSEC RRSet */

			if ((nsec_rrset != NULL) &&
			    dnslib_rrset_rdata(nsec_rrset)->next !=
			    dnslib_rrset_rdata(nsec_rrset)) {
				err_handler_handle_error(handler,
						 node,
						 ZC_ERR_NSEC_RDATA_MULTIPLE);
/*				char *name =
					dnslib_dname_to_str(
					dnslib_node_owner(node));
				log_zone_error("Node %s contains more "
					       "than one NSEC "
					       "record!\n", name);
				dnslib_rrset_dump(nsec_rrset, 0);
				free(name); */
			}

			/*
			 * Test that NSEC chain is coherent.
			 * We have already checked that every
			 * authoritative node contains NSEC record
			 * so checking should only be matter of testing
			 * the next link in each node.
			 */

			if (nsec_rrset != NULL) {
				dnslib_dname_t *next_domain =
					dnslib_rdata_item(
					dnslib_rrset_rdata(nsec_rrset),
					0)->dname;

				assert(next_domain);

				if (dnslib_zone_find_node(zone, next_domain) ==
				    NULL) {
					err_handler_handle_error(handler,
						node,
						ZC_ERR_NSEC_RDATA_CHAIN);
/*					log_zone_error("NSEC chain is not "
						       "coherent!\n"); */
				}

				if (dnslib_dname_compare(next_domain,
				    dnslib_node_owner(dnslib_zone_apex(zone)))
					== 0) {
					/* saving the last node */
					*last_node = node;
				}

			}
		} else if (nsec3 && (auth || deleg)) { /* nsec3 */
			int ret = check_nsec3_node_in_zone(zone, node, handler);
			if (ret != DNSLIB_EOK) {
				free(rrsets);
				return ret;
			}
		}
	}
	free(rrsets);

	return DNSLIB_EOK;
}

/*!
 * \brief Function called by zone traversal function. Used to call
 *        dnslib_zone_save_enclosers.
 *
 * \param node Node to be searched.
 * \param data Arguments.
 */
static void dnslib_zone_save_enclosers_in_tree(dnslib_node_t *node, void *data)
{
	assert(data != NULL);
	arg_t *args = (arg_t *)data;

	dnslib_rrset_t **rrsets = dnslib_node_get_rrsets(node);
	short count = dnslib_node_rrset_count(node);

	assert(count == 0 || rrsets != NULL);

	dnslib_zone_t *zone = (dnslib_zone_t *)args->arg1;

	assert(zone);


	for (int i = 0; i < count; ++i) {
		assert(rrsets[i] != NULL);
		dnslib_zone_save_enclosers_rrset(rrsets[i],
						 zone,
						 (skip_list_t *)args->arg2);
	}

	dnslib_node_t *first_node = (dnslib_node_t *)args->arg4;
	dnslib_node_t **last_node = (dnslib_node_t **)args->arg5;

	err_handler_t *handler = (err_handler_t *)args->arg6;

	char do_checks = *((char *)(args->arg3));

	if (do_checks) {
		 semantic_checks_plain(zone, node, do_checks, handler);
	}

	if (do_checks > 1) {
		semantic_checks_dnssec(zone, node, first_node, last_node,
				       handler, do_checks == 3);
	}

	free(rrsets);
}

/*!
 * \brief Helper function - wraps its arguments into arg_t structure and
 *        calls function that does the actual work.
 *
 * \param zone Zone to be searched / checked
 * \param list Skip list of closests enclosers.
 * \param do_checks Level of semantic checks.
 * \param handler Semantic error handler.
 * \param last_node Last checked node, which is part of NSEC(3) chain.
 */
void zone_save_enclosers_sem_check(dnslib_zone_t *zone, skip_list_t *list,
				   char do_checks, err_handler_t *handler,
				   dnslib_node_t **last_node)
{
	assert(handler);
	arg_t arguments;
	arguments.arg1 = zone;
	arguments.arg2 = list;
	arguments.arg3 = &do_checks;
	arguments.arg4 = NULL;
	arguments.arg5 = last_node;
	arguments.arg6 = handler;

	dnslib_zone_tree_apply_inorder(zone,
			   dnslib_zone_save_enclosers_in_tree,
			   (void *)&arguments);
}

/* TODO Think of a better way than a global variable */
static uint node_count = 0;

/*!
 * \brief Dumps dname labels in binary format to given file.
 *
 * \param dname Dname whose labels are to be dumped.
 * \param f Output file.
 */
static void dnslib_labels_dump_binary(dnslib_dname_t *dname, FILE *f)
{
	debug_dnslib_zdump("label count: %d\n", dname->label_count);
	fwrite(&(dname->label_count), sizeof(dname->label_count), 1, f);
	fwrite(dname->labels, sizeof(uint8_t), dname->label_count, f);
}

/*!
 * \brief Dumps dname in binary format to given file.
 *
 * \param dname Dname to be dumped.
 * \param f Output file.
 */
static void dnslib_dname_dump_binary(dnslib_dname_t *dname, FILE *f)
{
	fwrite(&(dname->size), sizeof(uint8_t), 1, f);
	fwrite(dname->name, sizeof(uint8_t), dname->size, f);
	debug_dnslib_zdump("dname size: %d\n", dname->size);
	dnslib_labels_dump_binary(dname, f);
}

/*!
 * \brief Finds wildcard for dname in list of closest enclosers.
 *
 * \param dname Dname to find wildcard for.
 * \param list Skip list of closest enclosers.
 * \return Found wildcard or NULL.
 */
static dnslib_dname_t *dnslib_find_wildcard(dnslib_dname_t *dname,
					    skip_list_t *list)
{
	return (dnslib_dname_t *)skip_find(list, (void *)dname);
}

/*!
 * \brief Dumps given rdata in binary format to given file.
 *
 * \param rdata Rdata to be dumped.
 * \param type Type of rdata.
 * \param data Arguments to be propagated.
 */
static void dnslib_rdata_dump_binary(dnslib_rdata_t *rdata,
				     uint32_t type, void *data)
{
	FILE *f = (FILE *)((arg_t *)data)->arg1;
	skip_list_t *list = (skip_list_t *)((arg_t *)data)->arg2;
	dnslib_rrtype_descriptor_t *desc =
		dnslib_rrtype_descriptor_by_type(type);
	assert(desc != NULL);

	debug_dnslib_zdump("dumping type: %s\n", dnslib_rrtype_to_string(type));

	for (int i = 0; i < desc->length; i++) {
		if (&(rdata->items[i]) == NULL) {
			debug_dnslib_zdump("Item n. %d is not set!\n", i);
			continue;
		}
		debug_dnslib_zdump("Item n: %d\n", i);
		if (desc->wireformat[i] == DNSLIB_RDATA_WF_COMPRESSED_DNAME ||
		desc->wireformat[i] == DNSLIB_RDATA_WF_UNCOMPRESSED_DNAME ||
		desc->wireformat[i] == DNSLIB_RDATA_WF_LITERAL_DNAME )	{
			/*  some temp variables - this is way too long */
			assert(rdata->items[i].dname != NULL);
			dnslib_dname_t *wildcard = NULL;

			if (rdata->items[i].dname->node == NULL) {
				wildcard = dnslib_find_wildcard(
					rdata->items[i].dname, list);
DEBUG_DNSLIB_ZDUMP(
				char *name = dnslib_dname_to_str(
						  rdata->items[i].dname);
				debug_dnslib_zdump("Not in the zone: %s\n",
				       name);
				free(name);
);

				fwrite((uint8_t *)"\0", sizeof(uint8_t), 1, f);
				dnslib_dname_dump_binary(rdata->items[i].dname,
							 f);
				if (wildcard) {
					fwrite((uint8_t *)"\1",
					       sizeof(uint8_t), 1, f);
					fwrite(&wildcard->node,
					       sizeof(void *), 1, f);
				} else {
					fwrite((uint8_t *)"\0", sizeof(uint8_t),
					       1, f);
				}
			} else {
				debug_dnslib_zdump("In the zone\n");
				fwrite((uint8_t *)"\1", sizeof(uint8_t), 1, f);
				fwrite(&(rdata->items[i].dname->node),
				       sizeof(void *), 1, f);
			}

		} else {
			assert(rdata->items[i].raw_data != NULL);
			fwrite(rdata->items[i].raw_data, sizeof(uint8_t),
			       rdata->items[i].raw_data[0] + 2, f);

			debug_dnslib_zdump("Written %d long raw data\n",
				 rdata->items[i].raw_data[0]);
		}
	}
}

/*!
 * \brief Dumps RRSIG in binary format to given file.
 *
 * \param rrsig RRSIG to be dumped.
 * \param data Arguments to be propagated.
 */
static void dnslib_rrsig_set_dump_binary(dnslib_rrset_t *rrsig, arg_t *data)
{
	assert(rrsig->type == DNSLIB_RRTYPE_RRSIG);
	FILE *f = (FILE *)((arg_t *)data)->arg1;
	fwrite(&rrsig->type, sizeof(rrsig->type), 1, f);
	fwrite(&rrsig->rclass, sizeof(rrsig->rclass), 1, f);
	fwrite(&rrsig->ttl, sizeof(rrsig->ttl), 1, f);

	uint8_t rdata_count = 0;

	fpos_t rrdata_count_pos;

	fgetpos(f, &rrdata_count_pos);

	fwrite(&rdata_count, sizeof(rdata_count), 1, f);

	assert(rrsig->rdata);

	dnslib_rdata_t *tmp_rdata = rrsig->rdata;

	while (tmp_rdata->next != rrsig->rdata) {
		dnslib_rdata_dump_binary(tmp_rdata, DNSLIB_RRTYPE_RRSIG, data);
		tmp_rdata = tmp_rdata->next;
		rdata_count++;
	}
	dnslib_rdata_dump_binary(tmp_rdata, DNSLIB_RRTYPE_RRSIG, data);
	rdata_count++;

	fpos_t tmp_pos;

	fgetpos(f, &tmp_pos);

	fsetpos(f, &rrdata_count_pos);

	fwrite(&rdata_count, sizeof(rdata_count), 1, f);

	fsetpos(f, &tmp_pos);
}

/*!
 * \brief Dumps RRSet in binary format to given file.
 *
 * \param rrsig RRSSet to be dumped.
 * \param data Arguments to be propagated.
 */
static void dnslib_rrset_dump_binary(dnslib_rrset_t *rrset, void *data)
{
	FILE *f = (FILE *)((arg_t *)data)->arg1;

	fwrite(&rrset->type, sizeof(rrset->type), 1, f);
	fwrite(&rrset->rclass, sizeof(rrset->rclass), 1, f);
	fwrite(&rrset->ttl, sizeof(rrset->ttl), 1, f);

	uint8_t rdata_count = 0;
	uint8_t rrsig_count = 0;

	fpos_t rrdata_count_pos;

	fgetpos(f, &rrdata_count_pos);

	fwrite(&rdata_count, sizeof(rdata_count), 1, f);
	fwrite(&rrsig_count, sizeof(rrsig_count), 1, f);

	dnslib_rdata_t *tmp_rdata = rrset->rdata;

	while (tmp_rdata->next != rrset->rdata) {
		dnslib_rdata_dump_binary(tmp_rdata, rrset->type, data);
		tmp_rdata = tmp_rdata->next;
		rdata_count++;
	}
	dnslib_rdata_dump_binary(tmp_rdata, rrset->type, data);
	rdata_count++;

	/* This is now obsolete, although I'd rather not use recursion - that
	 * would probably not work */

	if (rrset->rrsigs != NULL) {
		dnslib_rrsig_set_dump_binary(rrset->rrsigs, data);
		rrsig_count = 1;
	}

	fpos_t tmp_pos;

	fgetpos(f, &tmp_pos);

	fsetpos(f, &rrdata_count_pos);

	fwrite(&rdata_count, sizeof(rdata_count), 1, f);
	fwrite(&rrsig_count, sizeof(rrsig_count), 1, f);

	fsetpos(f, &tmp_pos);
}

/*!
 * \brief Dumps all RRSets in node to file in binary format.
 *
 * \param node Node to dumped.
 * \param data Arguments to be propagated.
 */
static void dnslib_node_dump_binary(dnslib_node_t *node, void *data)
{
	arg_t *args = (arg_t *)data;

	dnslib_zone_t *zone = (dnslib_zone_t *)args->arg3;

	FILE *f = (FILE *)args->arg1;


	node_count++;
	/* first write dname */
	assert(node->owner != NULL);

	if (!dnslib_node_is_non_auth(node)) {
		zone->node_count++;
	}

	dnslib_dname_dump_binary(node->owner, f);

	fwrite(&(node->owner->node), sizeof(void *), 1, f);

	debug_dnslib_zdump("Written id: %p\n", node->owner->node);

	/* TODO investigate whether this is necessary */
	if (node->parent != NULL) {
		fwrite(&(node->parent->owner->node), sizeof(void *), 1, f);
	} else {
		fwrite(&(node->parent), sizeof(void *), 1, f);
	}

	fwrite(&(node->flags), sizeof(node->flags), 1, f);

	debug_dnslib_zdump("Written flags: %u\n", node->flags);

	if (node->nsec3_node != NULL) {
		fwrite(&node->nsec3_node->owner->node, sizeof(void *), 1, f);
		debug_dnslib_zdump("Written nsec3 node id: %p\n",
			 node->nsec3_node->owner->node);
	} else {
		fwrite(&node->nsec3_node, sizeof(void *), 1, f);
		debug_dnslib_zdump("Written nsec3 node id: %p\n",
			 node->nsec3_node);
	}

	/* Now we need (or do we?) count of rrsets to be read
	 * but that number is yet unknown */

	fpos_t rrset_count_pos;

	fgetpos(f, &rrset_count_pos);

	debug_dnslib_zdump("Position rrset_count: %ld\n", ftell(f));

	uint8_t rrset_count = 0;

	fwrite(&rrset_count, sizeof(rrset_count), 1, f);

	const skip_node_t *skip_node = skip_first(node->rrsets);

	if (skip_node == NULL) {
		/* we can return, count is set to 0 */
		return;
	}

	dnslib_rrset_t *tmp;

	do {
		tmp = (dnslib_rrset_t *)skip_node->value;
		rrset_count++;
		dnslib_rrset_dump_binary(tmp, data);
	} while ((skip_node = skip_next(skip_node)) != NULL);

	fpos_t tmp_pos;

	fgetpos(f, &tmp_pos);

	debug_dnslib_zdump("Position after all rrsets: %ld\n", ftell(f));

	fsetpos(f, &rrset_count_pos);

	debug_dnslib_zdump("Writing here: %ld\n", ftell(f));

	fwrite(&rrset_count, sizeof(rrset_count), 1, f);

	fsetpos(f, &tmp_pos);

	debug_dnslib_zdump("Function ends with: %ld\n\n", ftell(f));

}

/*!
 * \brief Checks if zone uses DNSSEC and/or NSEC3
 *
 * \param zone Zone to be checked.
 *
 * \retval 0 if zone is not secured.
 * \retval 2 if zone uses NSEC3
 * \retval 1 if zone uses NSEC
 */
static int zone_is_secure(dnslib_zone_t *zone)
{
	if (dnslib_node_rrset(dnslib_zone_apex(zone),
			      DNSLIB_RRTYPE_DNSKEY) == NULL) {
		return 0;
	} else {
		if (dnslib_node_rrset(dnslib_zone_apex(zone),
				      DNSLIB_RRTYPE_NSEC3PARAM) != NULL) {
			return 2;
		} else {
			return 1;
		}
	}
}

/*!
 * \brief Checks if last node in NSEC/NSEC3 chain points to first node in the
 *        chain and prints possible errors.
 *
 * \param handler Semantic error handler.
 * \param zone Current zone.
 * \param last_node Last node in NSEC/NSEC3 chain.
 * \param do_checks Level of semantic checks.
 */
static void log_cyclic_errors_in_zone(err_handler_t *handler,
				      dnslib_zone_t *zone,
				      dnslib_node_t *last_node,
				      char do_checks)
{
	if (do_checks == 3) {
		/* TODO I can check it points somewhere allright, but
		 * to be sure it's really the first node I would have to have
		 * first node of NSEC3 tree as well - impossible without
		 * receiving it explicitely or going through the whole tree.*/
		;
	} else if (do_checks == 2 ) {
		if (last_node == NULL) {
			err_handler_handle_error(handler, last_node,
				ZC_ERR_NSEC_RDATA_CHAIN_NOT_CYCLIC);
				return;
		} else {
			const dnslib_rrset_t *nsec_rrset =
				dnslib_node_rrset(last_node,
						  DNSLIB_RRTYPE_NSEC);

			if (nsec_rrset == NULL) {
				err_handler_handle_error(handler, last_node,
					 ZC_ERR_NSEC_RDATA_CHAIN_NOT_CYCLIC);
				return;
			}

			const dnslib_dname_t *next_dname =
				dnslib_rdata_item(
				dnslib_rrset_rdata(nsec_rrset), 0)->dname;
			assert(next_dname);

			const dnslib_dname_t *apex_dname =
				dnslib_node_owner(dnslib_zone_apex(zone));
			assert(apex_dname);

			if (dnslib_dname_compare(next_dname, apex_dname) !=0) {
				err_handler_handle_error(handler, last_node,
					 ZC_ERR_NSEC_RDATA_CHAIN_NOT_CYCLIC);
			}
		}
	}
}

int dnslib_zdump_binary(dnslib_zone_t *zone, const char *filename,
			char do_checks, const char *sfilename)
{
	FILE *f;

	f = fopen(filename, "wb");

	if (f == NULL) {
		return DNSLIB_EBADARG;
	}

	zone->node_count = 0;

	skip_list_t *encloser_list = skip_create_list(compare_pointers);

	if (do_checks && zone_is_secure(zone)) {
		do_checks += zone_is_secure(zone);
	}

	err_handler_t *handler = NULL;

	if (do_checks) {
		handler = handler_new(1, 0, 1, 1, 1);
		if (handler == NULL) {
			/* disable checks and we can continue */
			do_checks = 0;
		} else { /* Do check for SOA right now */
			if (dnslib_node_rrset(dnslib_zone_apex(zone),
					      DNSLIB_RRTYPE_SOA) == NULL) {
				err_handler_handle_error(handler,
							 dnslib_zone_apex(zone),
							 ZC_ERR_MISSING_SOA);
			}
		}
	}

	dnslib_node_t *last_node = NULL;

	zone_save_enclosers_sem_check(zone, encloser_list, do_checks, handler,
				      &last_node);

	log_cyclic_errors_in_zone(handler, zone, last_node, do_checks);

	err_handler_log_all(handler);

	free(handler);

	/* Start writing header - magic bytes. */
	size_t header_len = MAGIC_LENGTH;
	static const uint8_t MAGIC[MAGIC_LENGTH] = MAGIC_BYTES;
	fwrite(&MAGIC, sizeof(uint8_t), MAGIC_LENGTH, f);

	/* Write source file length. */
	uint32_t sflen = 0;
	if (sfilename) {
		sflen = strlen(sfilename) + 1;
	}
	fwrite(&sflen, sizeof(uint32_t), 1, f);
	header_len += sizeof(uint32_t);

	/* Write source file. */
	fwrite(sfilename, sflen, 1, f);
	header_len += sflen;

	/* Notice: End of header,
	 * length must be marked for future return.
	 */

	/* Start writing compiled data. */
	fwrite(&node_count, sizeof(node_count), 1, f);
	fwrite(&node_count, sizeof(node_count), 1, f);
	fwrite(&zone->node_count,
	       sizeof(zone->node_count),
	       1, f);

	arg_t arguments;


	arguments.arg1 = f;
	arguments.arg2 = encloser_list;
	arguments.arg3 = zone;

	/* TODO is there a way how to stop the traversal upon error? */
	dnslib_zone_tree_apply_inorder(zone, dnslib_node_dump_binary,
				       (void *)&arguments);

	uint tmp_count = node_count;

	node_count = 0;
	dnslib_zone_nsec3_apply_inorder(zone, dnslib_node_dump_binary,
					(void *)&arguments);

	/* Update counters. */
	fseek(f, header_len, SEEK_SET);
	fwrite(&tmp_count, sizeof(tmp_count), 1, f);
	fwrite(&node_count, sizeof(node_count), 1, f);
	fwrite(&zone->node_count,
	       sizeof(zone->node_count),
	       1, f);

	debug_dnslib_zdump("written %d normal nodes\n", tmp_count);

	debug_dnslib_zdump("written %d nsec3 nodes\n", node_count);

	debug_dnslib_zdump("authorative nodes: %u\n", zone->node_count);

	fclose(f);

	return DNSLIB_EOK;
}
