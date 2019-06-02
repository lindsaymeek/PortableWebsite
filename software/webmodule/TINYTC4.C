/* TINYTC4.C - Tiny-TCP source fragment
	Part 4 of the TCP layer.
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


/* ----- Write data to a connection. -------------------------------- */
 
	/* Returns number of bytes written, == 0 when connection is not in
 		established state. */

int tcp_Write(struct tcp_Socket * s, Byte *dp,int len )
{
	int	x;

	/* if the connection is not established, don't send anything */

	if( s -> state != TS_ESTAB ) return 0;

	/* figure out how much I can move */

	if( len > ( x = TCP_MAXDATA - s -> dataSize ) ) len = x;
	if( len > 0 ) {
		memcpy( &s -> data[ s -> dataSize ], dp, len );
		s -> dataSize += len;
		tcp_Flush( s );
	}

	return len;	/* return count of bytes sent */
}

/* ----- Send pending data ------------------------------------------ */

Void tcp_Flush(struct tcp_Socket * s ) 
{

	if(( s -> state == 0 ) || ( s -> state == TS_CLOSED ))
		return;

	if( s -> dataSize > 0 ) {
		s -> flags |= TCPF_PUSH;
		tcp_Send( s );
	}
}

/* ----- Handler for incoming packets. ------------------------------ */

Void tcp_Handler(struct in_Header * ip ) 
{

	struct tcp_Header *	tp;
	struct tcp_Pseudoheader ph;
	int			len;
	int			diff;
	struct tcp_Socket *	s;
	Word			flags;
	Longword		lw;

	len = IP_HBYTES( ip );
	tp = ( struct tcp_Header * )(( Byte * ) ip + len );
	len = rev_word( ip -> length ) - len;

	/* demux to active sockets */

	for( s = tcp_allsocs; s!=NULL; s = s -> next )
		if(( s -> hisport != 0 )
			&& ( rev_word( tp -> dstPort ) == s -> myport )
			&& ( rev_word( tp -> srcPort ) == s -> hisport )
			&& ( rev_longword( ip -> source )
				== s -> hisaddr ))
			break;

	if( s == NULL ) 
	{	/* didn't find an matching socket */

		/* demux to passive sockets */

		for ( s = tcp_allsocs; s!=NULL; s = s -> next )
			if(( s -> hisport == 0 )
				&& ( rev_word( tp -> dstPort )
					== s -> myport ))
				break;
	}

	if( s == NULL ) 
	{	/* Still didn't find a matching socket */
#ifdef DEBUG_TCP
		tcp_DumpHeader( ip, tp, "Discarding" );
#endif
		return;
	}

#ifdef DEBUG_TCP
	tcp_DumpHeader( ip, tp, "Received" );
#endif

#ifdef ETHERNET
	/* save his ethernet address */

	s -> hisethaddr = ((( struct eth_Header *) ip ) - 1) -> source;

#endif

	ph.src = ip -> source;
	ph.dst = ip -> destination;
	ph.mbz = 0;
	ph.protocol = 6;
	ph.length = rev_word( len );

	lw = lchecksum( ( Word * ) &ph, sizeof ph - 2 );
	while( lw & 0xFFFF0000L )
		lw = ( lw & 0xFFFFL ) + (( lw >> 16 ) & 0xFFFFL );
	lw += lchecksum( ( Word * ) tp, len );
	while( lw & 0xFFFF0000L )
		lw = ( lw & 0xFFFFL ) + (( lw >> 16 ) & 0xFFFFL );

	if( lw != 0x0000FFFFL )
	{
#ifdef DEBUG_TCP
		debug( "tcp:bad checksum (%lx), received anyway\n", lw );
#endif
	}

	flags = rev_word( tp -> flags );

	if( flags & TCPF_RST ) {
#ifdef DEBUG_TCP
		debug("tcp:connection reset\n");
#endif

		s -> state = TS_CLOSED;
		if( s -> dataHandler != NULL )		/* cast removed */
			( s -> dataHandler )(( void * ) s, ( Byte * ) 0, -1);
		else	
		{
#ifdef DEBUG_TCP
			debug( "tcp:got close, no handler\n" );
#endif
		}

		tcp_Unthread( s );
		return;
	}

	switch( s -> state ) {

	case TS_LISTEN:
		/* Initial state of a Listen port */

		if( flags & TCPF_SYN ) {
			/* We expect to get Syn back */

			s -> acknum = rev_longword( tp -> seqnum ) + 1;
			s -> hisport = rev_word( tp -> srcPort );
			s -> hisaddr = rev_longword( ip -> source );
			s -> flags = TCPF_SYN | TCPF_ACK;

			tcp_Send( s );

			s -> state = TS_RSYN;
			s -> unhappy = True;
			s -> timeout = tcp_TIMEOUT;

#ifdef DEBUG_TCP
			debug("tcp:Syn from 0x%x#%d (seq 0x%lx)\n",
				s -> hisaddr, s -> hisport,
				rev_longword( tp -> seqnum ));
#endif
		}
		break;

	case TS_SSYN:
		/* Initial state of a Open port */

		if( flags & TCPF_SYN ) {
			s -> acknum++;
			s -> flags = TCPF_ACK;
			s -> timeout = tcp_TIMEOUT;

			if(( flags & TCPF_ACK )
				&& rev_longword( tp -> acknum )
					== ( s -> seqnum + 1 ) ) {
#ifdef DEBUG_TCP
				debug( "tcp:--- Open! ---\n" );
#endif

				s -> state = TS_ESTAB;
				s -> seqnum++;
				s -> acknum = rev_longword( tp -> seqnum )
					+ 1;
				s -> unhappy = False;
			} else {
				s -> state = TS_RSYN;
			}
		} else {
#ifdef DEBUG_TCP
			debug( "tcp:Sent Syn, didn't get Syn back\n" );
#endif
			/* This sounds to rr like the half-open
				condition in fig. 10, p. 74 (sec. 3.4). */

			s -> flags = TCPF_RST;
			s -> seqnum = rev_longword( tp -> acknum );
			tcp_Send( s );
#ifdef DEBUG_TCP
			debug( "tcp:Sent RST!\n" );
#endif
			/* We expect no response. Reset the flags to send
				SYN when the timeout occurs. */

			s -> flags = TCPF_SYN;
			s -> timeout = tcp_TIMEOUT;
			s -> seqnum = 0;	/* Start over */ 

			/* Stay in SYNSENT and wait for timeout. */
		}
		break;

	case TS_RSYN:
		if( flags & TCPF_SYN ) {
			s -> flags = TCPF_SYN | TCPF_ACK;
			tcp_Send(s);
			s -> timeout = tcp_TIMEOUT;

#ifdef DEBUG_TCP
			debug("tcp:retransmit of original syn\n");
#endif
		}
		if(( flags & TCPF_ACK )
			&& rev_longword( tp -> acknum )
				== ( s -> seqnum + 1L ) ) {

			s -> flags = TCPF_ACK;
			tcp_Send( s );
			s -> seqnum++;
			s -> unhappy = False;
			s -> state = TS_ESTAB;
			s -> timeout = tcp_TIMEOUT;

#ifdef DEBUG_TCP
			debug( "tcp:Synack received - connection established\n" );
#endif
		} else {
#ifdef DEBUG_TCP
			debug( "tcp:Wrong syn. flags %04x ack %ld seq %ld\n",
				flags,
				rev_longword( tp -> acknum ),
				s -> seqnum + 1L );
#endif
		}
		break;

	case TS_ESTAB:
		if(( flags & TCPF_ACK ) == 0 ) return;

		/* process ack value in packet */

		diff = ( int )( rev_longword( tp -> acknum ) - s -> seqnum );

		if( diff > 0 ) {

			/* The diff value is the number of bytes of MY data
				which he is acknowledging. */

			if( diff > TCP_MAXDATA ) 
				diff = TCP_MAXDATA;
			else 
			{
				memcpy( &s -> data[ 0 ], &s -> data[ diff ], TCP_MAXDATA - diff );
			}

			s -> dataSize -= diff;	/* bytes to send remaining */
			s -> seqnum += diff;	/* my next sequence number */
		}
		s -> flags = ( Word ) TCPF_ACK;

		tcp_ProcessData( s, tp, len );
		break;

	case TS_SFIN:
		if(( flags & TCPF_ACK ) == 0 ) return;
		diff = ( int )( rev_longword( tp -> acknum )
			- s -> seqnum - 1 );
		s -> flags = ( Word )( TCPF_ACK | TCPF_FIN );
		if( diff == 0 ) {
			s -> state = TS_AFIN;
			s -> flags = TCPF_ACK;
#ifdef DEBUG_TCP
			debug("tcp:finack received.\n");
#endif
		}
		tcp_ProcessData(s, tp, len);
		break;

	case TS_AFIN:
		s -> flags = TCPF_ACK;
		tcp_ProcessData( s, tp, len );
		break;

	case TS_RFIN:
		if( rev_longword( tp -> acknum ) == (s -> seqnum + 1) ) {
			s -> state = TS_TIMEWT;
			s -> timeout = tcp_TIMEOUT;
		}
		break;

	case TS_LASTACK:
		if( rev_longword( tp -> acknum ) == (s -> seqnum + 1) ) {
			s -> state = TS_CLOSED;
			s -> unhappy = False;
			s -> dataSize = 0;

			if( s -> dataHandler != NULL )	/* cast removed */
				( s -> dataHandler )(( void * ) s,
					( Byte * ) 0, 0 );
			else
			{
#ifdef DEBUG_TCP
				debug( "tcp:got close, no handler\n" );
#endif
			}

			tcp_Unthread( s );

#ifdef DEBUG_TCP
			debug( "tcp:Closed. [626]\n" );
#endif
		} else {
			s -> flags = TCPF_ACK | TCPF_FIN;
			tcp_Send( s );
			s -> timeout = tcp_TIMEOUT;
#ifdef DEBUG_TCP
			debug( "tcp:retransmitting FIN\n" );
#endif
		}
		break;

	case TS_TIMEWT:
		s -> flags = TCPF_ACK;
		tcp_Send(s);
	}
}

/* end of tinytc4.c */
