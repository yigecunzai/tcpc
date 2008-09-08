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

#include <stdlib.h>
#include <string.h>
#include "ll.h"

#ifndef I__PACKITS_H__
	#define I__PACKITS_H__

/* Header Record Separator */
#define PACKITS_RS		'\n'
/* Header Key/Value Separator */
#define PACKITS_KV		':'
/* Header Beginning Line */
#define PACKITS_HEADER_START	"PACKIT\n"
#define PACKITS_HEADER_START_L	7
/* Header Ending Line - line separating header and content */
#define PACKITS_HEADER_END	"\n"
#define PACKITS_HEADER_END_L	1
/* Max Key Size */
#define PACKITS_MAX_KEY		64
/* Max Value Size */
#define PACKITS_MAX_HVAL	1024

/* Header Length Field */
#define CLENGTH_KEY		"Content-Length"


/* Packits Header Record */
struct packit_record {
	ll_t hash_list; /* hash table linked list of records */
	ll_t full_list; /* full linked list of records */
	char *key;
	char *val;
	char *_rec;
	size_t _rec_size;
};

#define PACKITS_HASH_SIZE	32

/* Packit Structure */
struct packit {
	ll_t hash_head[PACKITS_HASH_SIZE];
	ll_t full_head;
	unsigned int clen;
	char *data;
};

/* Packit Interface Structure */
/* struct packit_if {
	char pbuf[PACKITS_MAX_KEY + PACKITS_MAX_HVAL + 2];
};*/


/* Packits API */

/* packit_new
 *     RETURNS:
 *         pointer to new packit on success
 *         NULL on failure
 */
static inline struct packit *packit_new(void)
{
	struct packit *p;
	unsigned int i;
	if((p = (struct packit *)malloc(sizeof(struct packit))) == NULL)
		return NULL;
	for(i = 0; i < PACKITS_HASH_SIZE; i++) {
		INIT_LIST_HEAD(&p->hash_head[i]);
	}
	INIT_LIST_HEAD(&p->full_head);
	return p;
}

/* packit_free
 *     NOTE: you must free your own data if necessary BEFORE this call
 */
static inline void packit_free(struct packit *p)
{
	while(!list_empty(&p->full_head)) {
		struct packit_record *r = list_first_entry(&p->full_head,
				struct packit_record, full_list);
		list_del(&r->full_list);
		free(r->_rec);
		free(r);
	}
	free(p);
}

/* packit_add_header
 *     RETURNS:
 *         pointer to new packit_record on success
 *         NULL on failure
 */
struct packit_record *packit_add_header(struct packit *p, const char *key,
		const char *val);

/* packit_add_uint_header
 *     RETURNS:
 *         pointer to new packit_record on success
 *         NULL on failure
 */
struct packit_record *packit_add_uint_header(struct packit *p, const char *key,
		unsigned int val);

/* packit_add_int_header
 *     RETURNS:
 *         pointer to new packit_record on success
 *         NULL on failure
 */
struct packit_record *packit_add_int_header(struct packit *p, const char *key,
		int val);

/* packit_get_key
 *     RETURNS:
 *         pointer to packit_record on success
 *         NULL on failure
 */
struct packit_record *packit_get_header(struct packit *p, const char *key);

/* packit_get_key
 *     RETURNS:
 *         0 on success
 *         -1 on failure
 *
 *         **buf and *len will be filled in with the packet to send. you must
 *         free buf when you are done with it.
 */
//int packit_send(struct packit *p, void **buf, size_t *len);
int packit_send(struct packit *p,
		ssize_t (*txf)(const void *buf, size_t len, void *arg),
		void *arg);

/* forall_packit_headers
 */
#define forall_packit_headers(packitp, recordp) \
	list_for_each_entry(recordp, &(packitp)->full_head, \
			full_list)

#endif /* I__PACKITS_H__ */
