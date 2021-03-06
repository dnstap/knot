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
/*!
 * \file net.h
 *
 * \author Marek Vavrusa <marek.vavusa@nic.cz>
 *
 * \brief Generic sockets APIs.
 *
 * This file provides higher-level API for creating connections and listeners.
 *
 * \addtogroup network
 * @{
 */

#ifndef _KNOTD_NET_H_
#define _KNOTD_NET_H_

/* POSIX only. */
#include "common/sockaddr.h"

/*!
 * \brief Create unbound socket of given family and type.
 *
 * \param type  Socket transport type (SOCK_STREAM, SOCK_DGRAM).
 * \param ss    Socket address storage.
 *
 * \return socket or error code
 */
int net_unbound_socket(int type, struct sockaddr_storage *ss);

/*!
 * \brief Create socket bound to given address.
 *
 * \param type  Socket transport type (SOCK_STREAM, SOCK_DGRAM).
 * \param ss    Socket address storage.
 *
 * \return socket or error code
 */
int net_bound_socket(int type, struct sockaddr_storage *ss);

/*!
 * \brief Create socket connected (asynchronously) to destination address.
 *
 * \param type     Socket transport type (SOCK_STREAM, SOCK_DGRAM).
 * \param dst_addr Destination address.
 * \param src_addr Source address (can be NULL).
 *
 * \return socket or error code
 */
int net_connected_socket(int type, struct sockaddr_storage *dst_addr,
                         struct sockaddr_storage *src_addr);


#endif // _KNOTD_NET_H_

/*! @} */
