/*
 * tcpc.h
 *
 * AUTHOR: Robert C. Curtis
 */

/****************************************************************************/

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#ifndef I__TCPC_H__
	#define I__TCPC_H__

/****************************************************************************
 * struct tcpc_server 
 * 	DESCRIPTION: Main data structure for a TCP Server. The application 
 * 	must allocate one of these for each server it has running. Use the 
 * 	CREATE_TCPC_SERVER() macro for creating statically allocated data
 * 	structures. Use INIT_TCPC_SERVER() to initialize a dynamically 
 * 	allocated data structure, or one you've statically allocated yourself.
 */
struct tcpc_server {
};

#define CREATE_TCPC_SERVER()
static inline void INIT_TCPC_SERVER() {

}
/****************************************************************************/

#endif /* I__TCPC_H__ */
