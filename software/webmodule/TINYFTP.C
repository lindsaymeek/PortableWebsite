/*
 * tinyftp.c - user ftp built on tinytcp.c
 *
 * Written March 31, 1986 by Geoffrey Cooper
 *
 * Copyright (C) 1986, IMAGEN Corporation
 * "This code may be duplicated in whole or in part provided that [1] there
 * is no commercial gain involved in the duplication, and [2] that this
 * copyright notice is preserved on all copies.  Any other duplication
 * requires written notice of the author."

940213	rr	minor mods
940403	rr	some bug fixes. receive side working.
940424	rr	start working on the server side
940513	rr	more coding on server side - may work, who knows
940529	rr	simplifications and cleanup
940925	rr	shorten some names
941012	rr	split into 2 parts

Notes:
	940424: This is the CLIENT side of the FTP connection. Currently it
	is only capable of requesting that files be RETRieved. It cannot
	retrieve more than one file. The file logic needs beefing up. (rr)

	940424: Names in this program often appear to have been chosen
	quite poorly - that is, they either are unintelligible, or don't
	reflect the true meaning of what they represent. I have been changing
	them slowly, but consider this to be a low priority. (rr)

	940424: There is something funny about the kbhit stuff. It seems to
	sometimes wait for a key even if I haven't pressed one. (rr)
	There is also something wrong with receiving long multi-line responses
	such as to HELP, like the data gets overwritten or unterminated.

	940513: file should be opened when socket opens.

	940513: Need some work on the interactive client; it doesn't work.
	The server is coming along nicely.

	940529: Do we need to do a new listen after a file is received?
*/

/* #define DEBUG_FTP */

#define	RECEIVE_DATA_PORT	0x8080

	/* This is working now. */
/* #define	IMMEDIATE_OPEN */

#include "tinytcp.h"
#include "fileio.h"

#include <stdio.h>	/* for printf, etc */

#include <string.h>	/* for strcpy, etc. */

#ifdef M16C
#include "kernel.h"
#include "uart.h"
#endif
	/* Sockets */

struct tcp_Socket 	/* outgoing connection socket */
			s_og_data,	/* data socket */
			s_ic_ctl,	/* server control socket */
			s_ic_data;	/* server data socket */

	/* Server output buffer */

char		b_s_response[ 128 ] = "";
					/* server output buffer */

	/* Server file transfer buffer, index, and length */

Byte		b_s_data[ 128 ];	/* server output buffer */
int		n_s_sent,	/* bytes sent of buffer */
			n_s_left;	/* bytes left in serv2 buffer */

	/* file handle for retrieve */

char		recv_filename[ 82 ];
FH		p_recv_file =  -1;

	/* file handle for server */

FH		p_send_file =  -1;

/* ----- ftp data handler ------------------------------------------- */

Void ftp_dataHandler( struct tcp_Socket *s, Byte *dp, int len )
{
#ifdef DEBUG_FTP
	debug( "in ftp_dataHandler %08lx %08lx %d\n", s, dp, len );
#endif
	/* When a file is retr'd, it comes in here. */

	if( len <= 0 ) 
	{	/* 0 = EOF, -1 = close */
		if( p_recv_file >= 0 ) 
		{
			my_close( p_recv_file );
			p_recv_file = -1;
		}
		return;
	}

	/* if the file isn't open yet, try to open it */

	if(( p_recv_file < 0 ) &&  strlen( recv_filename )) 
	{
		/* open the file for retrieve */

		p_recv_file = my_open( recv_filename, MY_OPEN_WRITE ); 

		if( p_recv_file < 0 ) 
		{
#ifdef DEBUG_TCP
			debug( "** Can't open file %s **",
				recv_filename );
#endif
			recv_filename[ 0 ] = '\0';
		}
	}

	/* write the data to the file */

	if( p_recv_file >= 0 ) 
	{
		my_write( p_recv_file, dp, len );
		dp += len;
		return;
	}

}

/* end of tinyftp.c */
