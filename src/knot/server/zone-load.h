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
/*!
 * \file zone-load.h
 *
 * Zone loading functions
 *
 * \addtogroup server
 * @{
 */

#ifndef _KNOTD_ZONE_LOAD_H_
#define _KNOTD_ZONE_LOAD_H_

#include "knot/conf/conf.h"
#include "knot/zone/zonedb.h"

struct server_t;

/*!
 * \brief Load zone from zone file.
 *
 * \param conf Zone configuration.
 *
 * \return Loaded zone, NULL in case of error.
 */
zone_t *load_zone_file(conf_zone_t *conf);

/*!
 * \brief Update zone database according to configuration.
 *
 * Creates a new database, copies references those zones from the old database
 * which are still in the configuration, loads any new zones required and
 * replaces the database inside the namserver.
 *
 * It also creates a list of deprecated zones that should be deleted once the
 * function finishes.
 *
 * This function uses RCU mechanism to guard the access to the config and
 * nameserver and to publish the new database in the nameserver.
 *
 * \param[in] conf Configuration.
 * \param[in] ns Nameserver which holds the zone database.
 *
 * \retval KNOT_EOK
 * \retval KNOT_EINVAL
 * \retval KNOT_ERROR
 */
int load_zones_from_config(const conf_t *conf, struct server_t *server);

#endif // _KNOTD_ZONE_LOAD_H_

/*! @} */
