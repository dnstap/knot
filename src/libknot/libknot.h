/*!
 * \file libknot.h
 *
 * \author Jan Kadlec <jan.kadlec@nic.cz>
 *
 * \brief Convenience header for including whole library.
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

#ifndef _KNOT_LIBKNOT_H_
#define _KNOT_LIBKNOT_H_

#include "libknot/consts.h"
#include "libknot/dname.h"
#include "libknot/edns.h"
#include "libknot/packet/wire.h"
#include "libknot/packet/compr.h"
#include "libknot/packet/pkt.h"
#include "libknot/rdataset.h"
#include "libknot/rrset.h"
#include "libknot/rrset-dump.h"
#include "libknot/rdata/rdname.h"
#include "libknot/rdata/dnskey.h"
#include "libknot/rdata/nsec3.h"
#include "libknot/rdata/nsec3param.h"
#include "libknot/rdata/nsec.h"
#include "libknot/rdata/rrsig.h"
#include "libknot/rdata/soa.h"
#include "libknot/rdata/tsig.h"
#include "libknot/tsig-op.h"
#include "libknot/util/tolower.h"
#include "libknot/util/utils.h"

#endif

/*! @} */
