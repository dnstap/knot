/*  Copyright (C) 2014 Farsight Security, Inc. <software@farsightsecurity.com>

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
 * \file message.h
 *
 * \author Robert Edmonds <edmonds@fsi.io>
 *
 * \brief dnstap message interface.
 *
 * \addtogroup dnstap
 * @{
 */

#ifndef _DNSTAP__MESSAGE_H_
#define _DNSTAP__MESSAGE_H_

#include <sys/socket.h>                 // struct sockaddr
#include <sys/time.h>                   // struct timeval
#include <stddef.h>                     // size_t

#include "dnstap/dnstap.pb-c.h"

/*!
 * \brief Fill a Dnstap__Message structure with the given parameters.
 * 
 * \param[out] m
 *      Dnstap__Message structure to fill. Will be zeroed first.
 * \param type
 *      One of the DNSTAP__MESSAGE__TYPE__* values.
 * \param response_sa
 *      sockaddr_in or sockaddr_in6 to use when filling the 'socket_family',
 *      'response_address', 'response_port' fields.
 * \param protocol
 *      \c IPPROTO_UDP or \c IPPROTO_TCP.
 * \param wire
 *	Wire-format query message or response message (depending on 'type').
 * \param len_wire
 *	Length in bytes of 'wire'.
 * \param qtime
 *	Query time. May be NULL.
 * \param rtime
 *	Response time. May be NULL.
 *
 * \retval KNOT_EOK
 * \retval KNOT_EINVAL
 */
int dt_message_fill(Dnstap__Message *m,
                    const Dnstap__Message__Type type,
                    const struct sockaddr *response_sa,
                    const int protocol,
                    const void *wire,
                    const size_t len_wire,
                    const struct timeval *qtime,
                    const struct timeval *rtime);

#endif // _DNSTAP__MESSAGE_H_

/*! @} */
