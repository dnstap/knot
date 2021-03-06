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
 * \file reader.h
 *
 * \brief dnstap file reader.
 *
 * \addtogroup dnstap
 * @{
 */

#ifndef _DNSTAP__READER_H_
#define _DNSTAP__READER_H_

#include <fstrm.h>
#include <protobuf-c/protobuf-c.h>

#include "dnstap.pb-c.h"

/*! \brief Structure for dnstap file reader. */
typedef struct {
	/*!< Input reader. */
	struct fstrm_reader	*fr;
} dt_reader_t;

/*!
 * \brief Creates dnstap file reader structure.
 *
 * \param file_path		Name of file to read input from.
 *
 * \retval reader		if success.
 * \retval NULL			if error.
 */
dt_reader_t* dt_reader_create(const char *file_path);

/*!
 * \brief Close dnstap file reader.
 *
 * \param reader		dnstap file reader structure.
 *
 * \retval KNOT_EOK
 * \retval errcode		if error.
 */
int dt_reader_free(dt_reader_t *reader);

/*!
 * \brief Read a dnstap protobuf from a dnstap file reader.
 *
 * Caller must deallocate the returned protobuf with the
 * dnstap__dnstap__free_unpacked() function.
 *
 * \param reader		dnstap file reader structure.
 * \param d[out]		Unpacked dnstap protobuf.
 *
 * \retval KNOT_EOK
 * \retval KNOT_ERROR
 * \retval KNOT_EOF
 * \retval KNOT_ENOMEM
 */
int dt_reader_read(dt_reader_t *reader, Dnstap__Dnstap **d);

#endif // _DNSTAP__DNSTAP_H_

/*! @} */
