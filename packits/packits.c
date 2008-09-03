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


/* local helper functions */
static inline unsigned int hash(const char *str)
{
	unsigned int i, h = 0;

	for(i=0; i<strlen(str); i++) {
		h += str[i] * 7;
	}

	return h % PACKITS_HASH_SIZE;
}


/* API FUNCTIONS */
struct packit_record *packit_add_header(struct packit *p, const char *key,
		const char *val)
{
	struct packit_record *nr;
	unsigned int h;
	
	/* check key lenght */
	if(strlen(key) > PACKITS_MAX_KEY) {
		return NULL;
	}
	/* allocate record */
	nr = (struct packit_record *)malloc(sizeof(struct packit_record));
	if(nr == NULL) {
		return NULL;
	}
	memset(nr, 0, sizeof(struct packit_record));
	/* allocate val */
	if((nr->val = (char *)malloc(strlen(val) + 1)) == NULL) {
		free(nr);
		return NULL;
	}
	/* copy key and val */
	strcpy((char *)&nr->key, key);
	strcpy(nr->val, val);

	/* hash entry */
	h = hash(key);

	/* insert at beginning */
	nr->next = p->headers[h];
	if(p->headers[h])
		p->headers[h]->prev = nr;
	p->headers[h] = nr;

	return nr;
}

struct packit_record *packit_get_header(struct packit *p, const char *key)
{
	struct packit_record *r;

	r = p->headers[hash(key)];
	while(r) {
		if(strcmp(key, r->key) == 0)
			break;
		r = r->next;
	}

	return r;
}
