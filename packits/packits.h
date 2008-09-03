/*
 * packits.h - Simple packet based communication protocol for serial links.
 *
 * This file is part of TCPC.
 *
 * Copyright (C) 2008 Robert C. Curtis
 *
 * TCPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * TCPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with TCPC.  If not, see <http://www.gnu.org/licenses/>.
 */

/****************************************************************************/

#ifndef I__PACKITS_H__
	#define I__PACKITS_H__


/* Header Record Separator */
#define PACKITS_RS		'\n'
/* Header Key/Value Separator */
#define PACKITS_KV		':'
/* Header Beginning Line */
#define PACKITS_HEADER_START	"PACKIT\n"
/* Header Ending Line - line separating header and content */
#define PACKITS_HEADER_END	"\n"
/* Max Key Size */
#define PACKITS_MAX_KEY		64

/* Required Length Header Field */
#define CLENGTH_KEY		"Content-Length"


/* Packits Header Record */
struct packit_record {
	struct packit_record *next;
	struct packit_record *prev;
	char key[PACKITS_MAX_KEY + 1];
	char *val;
}

#define PACKITS_HASH_SIZE	32

/* Packit Structure */
struct packit {
	struct packit_record *headers[PACKITS_HASH_SIZE];
	unsigned int clen;
	char *data;
}


/* Packits API */

/* packit_add_header
 *     RETURNS:
 *         pointer to new packit_record on success
 *         NULL on failure
 */
struct packit_record *packit_add_header(struct packit *p, const char *key,
		const char *val);

/* packit_get_key
 *     RETURNS:
 *         pointer to packit_record on success
 *         NULL on failure
 */
struct packit_record *packit_get_header(struct packit *p, const char *key);

#endif /* I__PACKITS_H__ */
