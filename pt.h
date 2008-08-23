/*
 * pt.h - Protothreads Implementation.
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

#ifndef I__PT_H__
	#define I__PT_H__

/* Protothread Type */
typedef unsigned int pt_t;

/* Protothread Return Values */
#define PT_YIELDED	0
#define PT_ENDED	1

/* Protothread Implementation */
#define PT_INIT(pt)		pt = 0
#define PT_THREAD(name_args)	int name_args
#define PT_BEGIN(pt)		{ switch(pt) { case 0:
#define PT_END(pt)		}; PT_INIT(pt); return PT_ENDED; }

#define PT_RESTART(pt)		\
	do { \
		PT_INIT(pt); \
		return PT_YIELDED; \
	} while(0)

#define PT_EXIT(pt)		\
	do { \
		PT_INIT(pt); \
		return PT_ENDED; \
	} while(0)

#define PT_WAIT_UNTIL(pt, cond)	\
	do { \
		_PT_SET(pt); \
		if(!(cond)) { \
			return PT_YIELDED; \
		} \
	} while(0)

#define PT_WAIT_WHILE(pt, cond)	PT_WAIT_UNTIL((pt), !(cond))


#define _PT_SET(pt)		pt = __LINE__; case __LINE__:


#endif /* I__PT_H__ */
