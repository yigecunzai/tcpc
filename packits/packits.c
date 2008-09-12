/*
 * packits.c - Simple packet based communication protocol for serial links.
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

#include "packits.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


/* local helper functions */
static inline unsigned int hash(const char *str)
{
	unsigned int h = 0;

	while(*str) {
		h += *str++ * 31;
	}

	return h % PACKITS_HASH_SIZE;
}

static inline struct packit_record *__packit_get_header(const struct packit *p,
		const char *key, unsigned int hash)
{
	struct packit_record *r;
	hl_node_t *n;

	hlist_for_each_entry(r, n, &p->hash_head[hash], hash_list) {
		if(strcmp(key, r->key) == 0) {
			return r;
		}
	}

	return NULL;
}


/* API FUNCTIONS */
struct packit_record *packit_add_header(struct packit *p, const char *key,
		const char *val)
{
	struct packit_record *nr;
	unsigned int h;
	size_t keylen = strlen(key);
	size_t vallen = strlen(val);
	
	/* check lengths */
	if(keylen > PACKITS_MAX_KEY)
		return NULL;
	if(vallen > PACKITS_MAX_HVAL)
		return NULL;

	/* hash entry */
	h = hash(key);

	/* check for existing key */
	nr = __packit_get_header(p, key, h);

	if(nr) { /* key already existed */
		free(nr->_rec);
		nr->_rec = NULL;
		nr->key = NULL;
		nr->val = NULL;
	} else { /* key did not exist */
		/* allocate record */
		nr = (struct packit_record *)
				malloc(sizeof(struct packit_record));
		if(nr == NULL) {
			return NULL;
		}
		memset(nr, 0, sizeof(struct packit_record));

		/* insert */
		hlist_add_head(&nr->hash_list, &p->hash_head[h]);
		list_add_tail(&nr->full_list, &p->full_head);
	}

	/* allocate rec */
	if((nr->_rec = (char *)malloc(keylen + 1 + vallen + 1)) == NULL) {
		hlist_del(&nr->hash_list);
		list_del(&nr->full_list);
		free(nr);
		return NULL;
	}
	nr->key = nr->_rec;
	nr->val = nr->_rec + keylen + 1;
	/* copy key */
	strcpy(nr->key, key);
	/* copy val */
	strcpy(nr->val, val);

	return nr;
}

struct packit_record *packit_add_uint_header(struct packit *p, const char *key,
		unsigned int val)
{
	char ns[21];
	snprintf((char *)&ns, sizeof(ns), "%u", val);
	return packit_add_header(p, key, (const char *)&ns);
}

struct packit_record *packit_add_int_header(struct packit *p, const char *key,
		int val)
{
	char ns[21];
	snprintf((char *)&ns, sizeof(ns), "%d", val);
	return packit_add_header(p, key, (const char *)&ns);
}

struct packit_record *packit_get_header(const struct packit *p, const char *key)
{
	return __packit_get_header(p, key, hash(key));
}

int packit_send(struct packit *p,
		ssize_t (*txf)(const void *buf, size_t len, void *arg),
		void *arg)
{
	struct packit_record *r;

	packit_add_uint_header(p, CLENGTH_KEY, p->clen);

	/* start of packit */
	if((*txf)(PACKITS_HEADER_START, PACKITS_HEADER_START_L, arg) <= 0) {
		return -1;
	}

	forall_packit_headers(p, r) {
		size_t sep = strlen(r->key);
		size_t term = strlen(r->val) + sep + 1;
		/* make the key/value into single record to send */
		r->_rec[sep]=PACKITS_KV;
		r->_rec[term]=PACKITS_RS;
		/* send the packet */
		if((*txf)(r->_rec, term + 1, arg) <= 0) {
			return -1;
		}
		/* fix the record back to separate key/value valid strings */
		r->_rec[sep]='\0';
		r->_rec[term]='\0';
	}
	
	/* end of header */
	if((*txf)(PACKITS_HEADER_END, PACKITS_HEADER_END_L, arg) <= 0) {
		return -1;
	}

	/* send the data */
	if(p->clen) {
		if((*txf)(p->data, p->clen, arg) <= 0) {
			return -1;
		}
	}

	return 0;
}
