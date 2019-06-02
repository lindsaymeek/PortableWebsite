/* TINYTC5.C - Tiny-TCP source fragment
	Part 5 of the TCP layer.
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

/* ----- Process the data in an incoming packet. -------------------- */

/* Called from all states where incoming data can be received: established,
	fin-wait-1, fin-wait-2 */

Void tcp_ProcessData(	struct tcp_Socket * s,struct tcp_Header * tp,int len )
{
	int		diff, x;
	Word		flags;
	Byte *		dp;

	flags = ( Word ) rev_word( tp -> flags );

	/* Look at the difference between the value I'm acking and the
		sequence number he's sending. The difference (if any) is the
		length of data in the buffer which we have already seen. */

	diff = ( int )( s -> acknum - rev_longword( tp -> seqnum ));

	/* I don't understand the following statement, unless it is supposed
		to be compensating for options. There should not be data
		on a SYN packet. */

	if( flags & TCPF_SYN ) diff--;

	x = TCP_DATAOFFSET( tp ) << 2;	/* mult. times 4 */
	dp = (( Byte * ) tp ) + x;		/* point to data */
	len -= x;				/* subtract offset */

	if( diff >= 0 ) 
	{
		dp += diff;		/* Ignore stuff we've already seen */
		len -= diff;		/* subtract length of data we've
						already seen */
		s -> acknum += len;	/* Add len to acknum to acknowledge
						new data */

		if( s -> dataHandler != NULL )	/* cast removed */
			( s -> dataHandler )(( void * ) s, dp, len );

		else
		{
#ifdef DEBUG_TCP
			debug( "tcp:got data, %d bytes\n", len );
#endif
		}

		if( flags & TCPF_FIN ) {
			s -> acknum++;
#ifdef DEBUG_TCP
			debug( "tcp:consumed fin.\n" );
#endif
			switch(s -> state) {
			case TS_ESTAB:
				/* note: skip state CLOSEWT by automatically closing conn */
				x = TS_LASTACK;
				s -> flags |= TCPF_FIN;
				s -> unhappy = True;
#ifdef DEBUG_TCP
				debug( "tcp:sending fin.\n" );
#endif
				break;
			case TS_SFIN:
				x = TS_RFIN;
				break;
			case TS_AFIN:
				x = TS_TIMEWT;
				break;
			}
			s -> state = x;
		}
	} else {
#ifdef DEBUG_TCP
		debug( "tcp:diff was negative, %d\n", diff );
#endif
	}
	s -> timeout = tcp_TIMEOUT;
	tcp_Send( s );
}

/* ----- Format and send an outgoing segment ------------------------ */

Void tcp_Send(struct tcp_Socket * s ) 
{

	struct tcp_Pseudoheader	ph;

	struct _pkt {
			struct in_Header	in;
			struct tcp_Header	tcp;
			Longword		maxsegopt;
		} * pkt;

	Byte *			dp;
	Longword		lw;

	/* don't do it if the state is Closed or the socket is not
		on the linklist */

	if(( s -> state == 0 ) || ( s -> state == TS_CLOSED ))
		return;

	pkt = ( struct _pkt * ) sed_FormatPacket(&s -> hisethaddr, 0x800 );
	
	dp = ( Byte * ) &( pkt -> maxsegopt );

	if( s -> flags & TCPF_SYN ) 
	{
		/* Should not send data on SYN */
		pkt -> in.length = rev_word( sizeof( struct in_Header )	+ sizeof( struct tcp_Header ));
	} 
	else 
	{
		pkt -> in.length = rev_word( sizeof( struct in_Header )	+ sizeof( struct tcp_Header ) + s -> dataSize );
	}

	/* tcp header */

	pkt -> tcp.srcPort = rev_word( s -> myport );
	pkt -> tcp.dstPort = rev_word( s -> hisport );
	pkt -> tcp.seqnum = rev_longword( s -> seqnum );
	pkt -> tcp.acknum = rev_longword( s -> acknum );
	pkt -> tcp.window = rev_word( 1024 ) ;

		/* This 1024 is the size of data which I am willing to
			receive */

	pkt -> tcp.flags = rev_word( s -> flags | 0x5000 );
	pkt -> tcp.checksum = 0;
	pkt -> tcp.urgentPointer = 0;

	if( s -> flags & TCPF_SYN ) 
	{

		/* If sending the options, add 1 DWORD to the data offset */

		pkt -> tcp.flags = rev_word( rev_word( pkt -> tcp.flags ) + 0x1000 );

		/* Add 4 to the length */

		pkt -> in.length = rev_word( rev_word( pkt -> in.length ) + 4 );

		/* Options. This is really: kind 02, length 04, value 1400.
			See page 42. */

		pkt -> maxsegopt = rev_longword( 0x02040578L );
				/* 1400 bytes */
		dp += 4;
	} 
	else 
	{
		/* Only send the data if NOT a SYN. */

		memcpy(dp, s->data, s->dataSize);
	}

	/* internet header */

	pkt -> in.vht = rev_word( 0x4500 );
				/* version 4, hdrlen 5, tos 0 */
	pkt -> in.identification = rev_word( tcp_id++ );
	pkt -> in.frag = 0;
	pkt -> in.ttlProtocol = rev_word(( 250 << 8 ) + 6 );
	pkt -> in.checksum = 0;
	pkt -> in.source = rev_longword( local_IP_address );
	pkt -> in.destination = rev_longword( s -> hisaddr );
	pkt -> in.checksum = rev_word(	( Word ) ~ checksum( ( Word * ) &pkt -> in,  sizeof( struct in_Header )));

	/* compute tcp checksum */

	ph.src = pkt -> in.source;
	ph.dst = pkt -> in.destination;
	ph.mbz = 0;
	ph.protocol = 6;
	ph.length = rev_word( rev_word( pkt -> in.length ) - sizeof( struct in_Header ));

	/* Actually, the idea is to compute the checksum of the pseudoheader
		plus that of the tcp header and the data portion. By sticking
		the *unreversed* checksum in the pseudoheader itself, we hope
		to achieve the same result. I'm rather doubtful of this,
		because the overflows will have been mangled. It seems to work
		on receive, though. */

	lw = lchecksum( ( Word * ) &ph, sizeof( ph ) - 2 );
	while( lw & 0xFFFF0000L )
		lw = ( lw & 0xFFFFL ) + (( lw >> 16 ) & 0xFFFFL );
	lw += lchecksum( ( Word * ) &pkt -> tcp,
		rev_word( ph.length ));
	while( lw & 0xFFFF0000L )
		lw = ( lw & 0xFFFFL ) + (( lw >> 16 ) & 0xFFFFL );

	pkt -> tcp.checksum = rev_word( ~ ( Word )( lw & 0xFFFFL ));

#ifdef DEBUG_TCP
	tcp_DumpHeader( &pkt -> in, &pkt -> tcp, "Sending" );
#endif

	sed_Send( rev_word( pkt -> in.length ));
}

/* ----- calculate Word checksum ------------------------------------ */

Word checksum( Word *dp,int length )
{
	Longword	sum;

	sum = lchecksum( dp, length );

	while( sum & 0xFFFF0000L )
		sum = ( sum & 0xFFFFL ) + (( sum >> 16 ) & 0xFFFFL );

	return ( Word )( sum & 0xFFFF );
}

/* ----- compute a Longword sum of words in message ----------------- */
	/* (partial checksum) */

Longword lchecksum(Word * dp,int length ) 
{
	int		len;
	Longword	sum;

	len = length >> 1;
	sum = 0L;

	while ( len-- > 0 ) sum += rev_word( *dp++ );
	if( length & 1 ) sum += ( rev_word( *dp ) & 0xFF00 );

	return sum;
}

#ifdef DEBUG_TCP

/* ----- Dump tcp protocol header of a packet ----------------------- */

Void tcp_DumpHeader(struct in_Header * ip,struct tcp_Header * tp,char * mesg )

{
	static const char *flags[] = { "FIN", "SYN", "RST", "PUSH", "ACK", "URG" };
	int		len;
	Word		f;

	len = rev_word( ip -> length )
		- (( TCP_DATAOFFSET( tp ) + IP_HLEN( ip )) << 2 );

	debug( "TCP: %s packet:\nSrcP: %x; DstP: %x; SeqN=%lx AckN=%lx Wind=%d DLen=%d\n",
		mesg,
		rev_word( tp -> srcPort ),
		rev_word( tp -> dstPort ),
		rev_longword( tp -> seqnum ),
		rev_longword( tp -> acknum ),
		rev_word( tp -> window ),
		len );
	debug( "DO=%d, C=%x U=%d",
		TCP_DATAOFFSET( tp ),
		rev_word( tp -> checksum ),
		rev_word( tp -> urgentPointer ));

	/* output flags */

	f = rev_word( tp -> flags );
	for( len = 0; len < 6; len++ )
		if( f & ( 1 << len )) debug( " %s", flags[ len ] );
	debug( "\n" );
}

#endif

#ifndef BIG_ENDIAN

/* ----- reverse Word ----------------------------------------------- */

#ifndef M16C

Word rev_word( Word w ) {
	return ( Word )((( w >> 8 ) & 0xFF )
		| (( w << 8 ) & 0xFF00 ));
}

/* ----- reverse Longword ------------------------------------------- */

Longword rev_longword( Longword l ) {
	return ( Longword )((( l >> 24 ) & 0x000000FFL )
		| (( l >> 8 ) & 0x0000FF00L )
		| (( l << 8 ) & 0x00FF0000L )
		| (( l << 24 ) & 0xFF000000L ));
}

#else

Word rev_word( Word w ) { return swap16(w); }
Longword rev_longword( Longword l ) { return swap32(l); }

#endif

#endif	/* BIG_ENDIAN */

/* end of tinytc5.c */
