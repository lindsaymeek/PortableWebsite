/*
 * tinytcp.c - Tiny Implementation of the Transmission Control Protocol
 *
 * Written March 28, 1986 by Geoffrey Cooper, IMAGEN Corporation.
 *
 * This code is a small implementation of the TCP and IP protocols, suitable
 * for burning into ROM. The implementation is bare-bones and represents
 * two days' coding efforts. A timer and an ethernet board are assumed. The
 * implementation is based on busy-waiting, but the tcp_handler procedure
 * could easily be integrated into an interrupt driven scheme.
 *
 * IP routing is accomplished on active opens by broadcasting the tcp SYN
 * packet when ARP mapping fails. If anyone answers, the ethernet address
 * used is saved for future use. This also allows IP routing on incoming
 * connections.
 *
 * The TCP does not implement urgent pointers (easy to add), and discards
 * segments that are received out of order. It ignores the received window
 * and always offers a fixed window size on input (i.e., it is not flow
 * controlled).
 *
 * Special care is taken to access the ethernet buffers only in Word
 * mode. This is to support boards that only allow Word accesses.
 *
 * Copyright (C) 1986, IMAGEN Corporation
 * "This code may be duplicated in whole or in part provided that [1] there
 * is no commercial gain involved in the duplication, and [2] that this
 * copyright notice is preserved on all copies. Any other duplication
 * requires written notice of the author."
Comment: The original version of this code was virtually uncommented and
	contained numerous errors. In any contact with the original author
	these points need to be made.

Reference is RFC-793, available through the Internet.

931216	rr	try to fix send checksum problem
940213	rr	fix various problems. Checked against RFC.
940529	rr	minor cleanup and name changes
941012	rr	broken into chunks

Notes:
	940213 Things seem to be working a little better. There are still
		problems.

	940403 When the os/2 side sends FIN ACK, I send ACK and the two sides
		loop. This is still a problem.
*/


#include "tinytcp.h"

#include <stdio.h>

#include <string.h>

#ifdef M16C
#include "kernel.h"
#include "uart.h"
#endif

/* ----- static data ------------------------------------------------ */

IP_Address		local_IP_address;	/* local IP address */

int			tcp_id;		/* TCP ID, gets incremented */

struct tcp_Socket *	tcp_allsocs;	/* make global for sharing */

IP_Address ADDR(Byte A,Byte B,Byte C,Byte D)
{
	IP_Address t;

	t=A;
	t<<=8;
	t+=B;
	t<<=8;
	t+=C;
	t<<=8;
	t+=D;

	return t;
}

char *print_ip(IP_Address ip)
{
 	static char buf[18];

	sprintf(buf,"%u.%u.%u.%u",
			(int)((ip >> 24) & 255),
			(int)((ip >> 16) & 255),
			(int)((ip >> 8) & 255),
			(int)(ip & 255));

	return buf;

}

/* ----- Initialize the tcp implementation -------------------------- */

Void tcp_Init( IP_Address MY_ADDR ) 
{
	tcp_allsocs = ( struct tcp_Socket * ) 0;
	tcp_id = 0;

	local_IP_address = MY_ADDR;
}

/* ----- tcp open --------------------------------------------------- */

/* Actively open a TCP connection to a particular destination. */

Void tcp_Open(struct tcp_Socket * s, Word lport, IP_Address ina, Word port, Procref datahandler )
{
	static Word next_port=1024;

#ifdef ETHERNET
	extern struct Ethernet_Address	broadcast_ethernet_address;
#endif

	s -> state = TS_SSYN;
	s -> timeout = tcp_LONGTIMEOUT;

	if( lport == 0 )
	{
		  //pick a random #
		  lport = next_port++;
		  if(next_port < 1024 || next_port > 65535)
		  	next_port=1024;
	}

	s -> myport = lport;

#ifdef ETHERNET
	/* Perform arp handshake to find out the host's Ethernet address.
		Does not apply to slip. */

	if( ! sar_MapIn2Eth( (IP_Address ) ina,
		& s -> hisethaddr ) ) {

		/* Can't find the ethernet address. Blast it to everyone */
#ifdef DEBUG_TCP
		debug(
"tcp_Open of 0x%x: defaulting ethernet address to broadcast\n", ina );
#endif
			s->hisethaddr = broadcast_ethernet_address;

	}
#endif	/* ETHERNET */ 

	s -> hisaddr = ina;
	s -> hisport = port;
	s -> seqnum = 0;
	s -> dataSize = 0;
	s -> flags = TCPF_SYN;		/* Looking for sync */
	s -> unhappy = True;		/* Flag it as unhappy */
	s -> dataHandler = datahandler;

	/* add the new socket to the linklist */

	s -> next = tcp_allsocs;
	tcp_allsocs = s;

	tcp_Send( s );
}

/* end of tinytc1.c */
