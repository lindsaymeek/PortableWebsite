/* TINYTC2.C - Tiny-TCP source fragment
	Part 2 of the TCP layer.
	941012	rr	split into fragments
*/

#include "tinytcp.h"

#include <stdio.h>

#include <string.h>

#ifdef M16C
#include "kernel.h"
#include "uart.h"
#endif

/* ----- globals from tinytcp.c ------------------------------------- */

extern	IP_Address	local_IP_address;	/* local IP address */

extern	int		tcp_id;			/* TCP ID, gets incremented */

extern	struct tcp_Socket *	tcp_allsocs;	/* socket linklist */

/* ----- tcp listen ------------------------------------------------- */

/* Passive open: listen for a connection on a particular port */

Void tcp_Listen(struct	tcp_Socket * s, Word port, Procref datahandler, Longword timeout )
{
	s -> state = TS_LISTEN;

	if( timeout == 0L ) s -> timeout = 0x7FFFFFFL; /* forever... */
	else	s -> timeout = timeout;

	s -> myport = port;
	s -> hisport = 0;
	s -> seqnum = 0;
	s -> dataSize = 0;
	s -> flags = 0;
	s -> unhappy = 0;
	s -> dataHandler = datahandler;

	/* Add it to the linklist */

	s -> next = tcp_allsocs;
	tcp_allsocs = s;
}

/* end of tinytc2.c */
