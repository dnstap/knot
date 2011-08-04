/*!
 * \file rdata_tests.h
 *
 * \author Lubos Slovak <lubos.slovak@nic.cz>
 *
 * Contains unit tests for RDATA (knot_rdata_t) and RDATA item
 * (knot_rdata_item_t) structures.
 *
 * Contains tests for:
 * - creating empty RDATA structure with or without reserved space.
 * - setting RDATA items one-by-one
 * - setting RDATA items all at once
 *
 * As for now, the tests use several (TEST_RDATAS) RDATA structures, each
 * with different number of RDATA items (given by test_rdatas). These are all
 * initialized to pointers derived from RDATA_ITEM_PTR (first is RDATA_ITEM_PTR,
 * second RDATA_ITEM_PTR + 1, etc.). The functions only test if the pointer
 * is set properly.
 *
 * \todo It may be better to test also some RDATAs with predefined contents,
 *       such as some numbers, some domain name, etc. For this purpose, we'd
 *       need RDATA descriptors (telling the types of each RDATA item within an
 *       RDATA).
 *
 * \todo It will be fine to test all possible output values of all functions,
 *       e.g. test whether knot_rdata_get_item() returns NULL when passed an
 *       illegal position, etc.
 */
#ifndef _KNOTDRDATA_TESTS_H_
#define _KNOTDRDATA_TESTS_H_

#include "common/libtap/tap_unit.h"

/* Unit API. */
unit_api rdata_tests_api;

#endif /* _KNOTDRDATA_TESTS_H_ */