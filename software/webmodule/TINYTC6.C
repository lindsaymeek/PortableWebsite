/* TINYTC6.C - Tiny-TCP source fragment
	Part 6 of the TCP layer.
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


/* ----- tcp close -------------------------------------------------- */

/* Send a FIN on a particular port -- only works if it is open */

Void tcp_Close( struct tcp_Socket *s )  
{
	if( s -> state == TS_ESTAB || s -> state == TS_RSYN ) {
		s -> flags = TCPF_ACK | TCPF_FIN;
		s -> state = TS_SFIN;
		s -> unhappy = True;
	}
}

/* ----- Abort a tcp connection ------------------------------------- */

Void tcp_Abort( struct tcp_Socket * s )
{

	if( s -> state != TS_LISTEN && s -> state != TS_CLOSED ) 
	{
		s -> flags = TCPF_RST | TCPF_ACK;
		tcp_Send( s );
	}

	s -> unhappy = 0;
	s -> dataSize = 0;
	s -> state = TS_CLOSED;

	if( s -> dataHandler != NULL )		/* cast to Procref removed */
		( s -> dataHandler )(( void * ) s, ( Byte * ) 0, -1 );
	else
	{
#ifdef DEBUG_TCP
		debug( "tcp:got abort, no handler\n" );
#endif
	}

	tcp_Unthread( s );
}

/* end of tinytc6.c */
