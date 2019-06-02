/* TINYTC3.C - Tiny-TCP source fragment
	Part 3 of the TCP layer.
	941012	rr	split into fragments
*/

#include "tinytcp.h"

#include <stdio.h>

#include <string.h>

#ifdef M16C
#include "kernel.h"
#endif

#undef DEBUG


/* ----- globals from tinytcp.c ------------------------------------- */

extern	IP_Address	local_IP_address;	/* local IP address */

extern	int		tcp_id;			/* TCP ID, gets incremented */

extern	struct tcp_Socket *	tcp_allsocs;	/* socket linklist */

/* ----- retransmitter ---------------------------------------------- */

/* Retransmitter - called periodically to perform tcp retransmissions */

Void tcp_Retransmitter( Void ) 
{
	struct tcp_Socket *	s;
	short			x;

	/* Go through all of the open sockets and see if there is
		anything to send */

	for( s = tcp_allsocs; s!=NULL; s = s -> next ) 
	{
		x = False;
		if( s -> dataSize > 0 || s -> unhappy ) {

			/* send the data */

			tcp_Send( s );
			x = True;
		}

		/* anything sent? */

		if( x || s -> state != TS_ESTAB )
			s -> timeout -= tcp_RETRANSMITTIME;

		if( s -> timeout <= 0 ) {
			if( s -> state == TS_TIMEWT ) {
#ifdef DEBUG_TCP
				debug( "tcp:Closed. [222]\n" );
#endif
				s -> state = TS_CLOSED;

				/* cast to Procref removed on following */

				if( s -> dataHandler != NULL )
					( s -> dataHandler )(( void * ) s,
						( Byte * ) 0, 0 );
				else
				{
#ifdef DEBUG_TCP
					debug( "tcp:no handler\n" );
#endif
				}

				/* Unthread this socket from the linklist */

				tcp_Unthread( s );
			} else {
#ifdef DEBUG_TCP
				debug( "tcp:Timeout\n" );
#endif
				tcp_Abort( s );
			}
		}
	}
}



/* ----- Unthread a socket from the socket list, if it's there ------ */

Void tcp_Unthread( struct tcp_Socket *ds )  
{
	struct tcp_Socket *s, **sp;

	if( ds == ( struct tcp_Socket * ) NULL ) return;

	sp = &tcp_allsocs;			/* -> -> socket */
	for(;;) 
	{
		s = *sp;			/* what pointer points to */
		if( s == ( struct tcp_Socket * ) NULL ) 
		 break;	/* end of list? */
		if( s == ds ) 
		{						/* matches one to delete? */
			*sp = s -> next;	/* unlink it from the list */
			break;				/* all done */
		}
		sp = &s -> next;		/* move to the next one */
	}

	ds -> next = ( struct tcp_Socket * ) NULL;	/* clear next pointer */
}

/* protocol 1 is icmp. the first byte of the data
				is an 8 for ping (echo) requests; the reply
				Byte should have a zero */

static void icmp_Handler(struct in_Header *ip)
{
	char type = *(char *)(ip + 1);

	switch(type)
	{
		case 5:	// redirect
		case 4:	// quench
		default:
#ifdef DEBUG_TCP
			debug("icmp: %u?\n", type);
#endif
			break;

		case 8:
			// ping
         *(char *)(ip+1)=0;
#ifdef DEBUG_TCP
			debug("icmp:ping\n");
#endif


			break;
	}

}

/* ----- busy-wait loop for tcp. Also calls an "application proc" --- */

int tcp(Procrefv application )
{
	struct in_Header *	ip;
	long		mark;

	sed_Receive( NULL );

	/* There was a while( tcp_allsocs ) here. However, if we don't
		have any open, we still want the program to loop,
		so I changed it to for(;;). */

	mark = MsecClock() + tcp_RETRANSMITTIME;

	for(;;)
	{

		/* Have we received a packet? */

		ip = ( struct in_Header * ) sed_IsPacket();

		if( ip == NULL )
		{

			/* No, we haven't. */

			if( mark < MsecClock() )
			{

				/* Anything to retransmit? */

				tcp_Retransmitter();

				/* Set next transmit time */

				mark = MsecClock() + tcp_RETRANSMITTIME;
			}

			/* There's nothing to do. Let the user enter a
				command. */

			application();

			continue;
		}

		/* We have received a packet. Process it */

#ifdef ETHERNET
		/* ARP protocol is supported on ethernet only */

		if( sed_CheckPacket( (Word *)ip, 0x806 ) == 1 ) {

			/* do arp */

			sar_CheckPacket( ( struct arp_Header * ) ip );

		} else
#endif	/* ethernet */
		if( sed_CheckPacket( (Word *)ip, 0x800 ) == 1 ) {

			/* do IP */

			if( rev_longword( ip -> destination ) !=
				local_IP_address ) {
#ifdef DEBUG_TCP
				debug( "tcp:IP address doesn't match\n" );
#endif
				/* But ignore that for now (SLIP) */
			}
			else
			{
			 if( checksum( ( Word * ) ip, IP_HBYTES( ip ))
				!= 0xFFFFU ) {
#ifdef DEBUG_TCP
				debug( "tcp:Checksum bad\n" );
#endif
				/* But ignore that, too, for now (SLIP) */
			 }
			 else
			 {
				if( IP_PROTOCOL( ip ) == 6 )		/* tcp/ip */
					tcp_Handler( ip );
				else if( IP_PROTOCOL( ip ) == 1)
					icmp_Handler( ip );


			 } // CRC ok
			} // IP addr ok
		}

		/* recycle buffer */

		sed_Receive( ( Byte * ) ip );

	}

}

/* end of tinytc3.c */
