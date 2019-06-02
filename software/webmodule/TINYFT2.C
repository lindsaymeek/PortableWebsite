/* TINYFT2.C - Part 2 of tiny FTP.
	941012	rr	split into 3 parts
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

extern	struct tcp_Socket s_og_ctl,	/* outgoing connection socket */
			s_og_data,	/* data socket */
			s_ic_ctl,	/* server control socket */
			s_ic_data;	/* server data socket */

/* Server output buffer */
extern	char		b_s_response[ 128 ];
					/* server output buffer */

/* Server file transfer buffer, index, and length */
extern	Byte		b_s_data[ 128 ];	/* server output buffer */
extern	int		n_s_sent,	/* bytes sent of buffer */
			n_s_left;	/* bytes left in serv2 buffer */

/* file handle for retrieve */
extern	char		recv_filename[ 82 ];
extern	FH		p_recv_file;

/* file handle for server */
extern	FH		p_send_file;

/* ----- get a number (unsigned short) ------------------------------ */

static char * get_a_number( char *ps, unsigned short *pn ) 
{
	unsigned short	us;

	us = 0;

	/* ignore leading blanks */

	while( *ps == ' ' ) 
		++ps;

	/* convert any digits present */

	while(( *ps >= '0' ) && ( *ps <= '9' ))
		us = ( unsigned short )(( 10 * us ) + *ps++ - '0' );

	/* store the converted number */

	*pn = us;

	/* return pointer to next char */

	return ps;
}

/* ----- ftp server data handler ------------------------------------ */

Void ftp_server_handler(struct tcp_Socket * s,Byte * dp,int len )
{
	unsigned short	l, h;
	char *		p;	

	static	int	first_time = 1;		
	static	int	his_data_port = RECEIVE_DATA_PORT;

	/* THINK I NEED TO COLLECT DATA UNTIL I RECEIVE AN ENTIRE
		STRING TERMINATED WITH CR/LF. */

#ifdef DEBUG_FTP
	debug( "ftp:in ftp_server_handler. dp %08lx len %d, his port %04x\n",
		dp, len, s -> hisport );
#endif

	if( len < 0 ) 
	{		/* 0 = EOF, -1 = close */

		/* Close of port. What is there to say?
			I suppose I need to redo the listen. */

		return;
	}

	/* Here's the usual exchange:
		(open)		220 tritium FTP server ready.
		user rickr	331 password required for rickr.
		pass grelber	230 user rickr logged on.
		retr \temp\2	150 opening blah blah
			Data port is opened. Data gets sent.
			Data port is closed.
		quit		221 goodbye. */

	/* When I enter 'get' in FTP on OS/2, he sends me a PORT message... */

	/* His port number is at s -> hisport */

	/* Set up the reply */

	if( len == 0 ) {
		if( first_time ) {
			strcat( b_s_response,
				"220 Tiny-TCP FTP server ready.\r\n" );
			first_time = 0;
		} else return;

	} else if( strnicmp( ( char * ) dp, "USER", 4 ) == 0 ) {
		strcat( b_s_response, "230 No logon required.\r\n" );

	} else if( strnicmp( ( char * ) dp, "PASS", 4 ) == 0 ) {
		strcat( b_s_response, "230 No logon required.\r\n" );

	} else if( strnicmp( ( char * ) dp, "PORT", 4 ) == 0 ) {

		/* I get 192,9,201,2,4,7 */

		p = ( char * ) dp + 4;
		p = get_a_number( p, &h );	/* 192 */
		if( *p != ',' ) goto syntax_error;
		++p;
		p = get_a_number( p, &h );	/* 9 */
		if( *p != ',' ) goto syntax_error;
		++p;
		p = get_a_number( p, &h );	/* 201 */
		if( *p != ',' ) goto syntax_error;
		++p;
		p = get_a_number( p, &h );	/* 2 */
		if( *p != ',' ) goto syntax_error;
		++p;
		p = get_a_number( p, &h );	/* 4 */
		if( *p != ',' ) goto syntax_error;
		++p;
		p = get_a_number( p, &l );	/* 7 */

		his_data_port = ( int )(( h << 8 ) + l );

#ifdef DEBUG_FTP
		debug( "ftp:his data port set to %04x\n", his_data_port );
#endif

		strcat( b_s_response, "200 Okay.\r\n" );

	} else if( strnicmp( ( char * ) dp, "RETR", 4 ) == 0 ) {

		if( *( dp + 4 ) != ' ' ) {
syntax_error:
			strcat( b_s_response, "501 Syntax error.\r\n" );
			return;
		}

		strncpy( buffer, ( char * ) dp + 4, len - 5 );
		buffer[ len - 5 ] = '\0';

#ifdef DEBUG_FTP
	debugs( "ftp:retrieving file: "); debugs( buffer ); debugs("\n\r");
#endif

		/* Open the output file */

		p_send_file = my_open( buffer, MY_OPEN_READ );
		if( p_send_file < 0 ) {
			strcat( b_s_response, "550 File doesn't exist.\r\n" );
			return;
		}

		/* Open a socket for the send data. */

		tcp_Open( &s_ic_data,		/* socket */
			RECEIVE_DATA_PORT,	/* my port */
			s -> hisaddr,		/* host to call */
			his_data_port,		/* his port */
			( Procref ) ftp_dataHandler );
						/* handler for data receive */

		strcat( b_s_response, "150 File open.\r\n" );

		/* When transfer is complete, need to send 226. */

	/* } else if( strncmp( ( char * ) dp, "STOR", 4 ) == 0 ) { */

		/* STOR is like RETR, but needs to use the recv_filename
			instead. I think I can use the same data handler... */

	} else if( strnicmp( ( char * ) dp, "QUIT", 4 ) == 0 ) {
		strcat( b_s_response, "221 Goodbye.\r\n" );

	} else if( strnicmp( ( char * ) dp, "HELP", 4 ) == 0 ) {
		strcat( b_s_response, "211 When we say Tiny, we mean it.\r\n" );

	} else if( len > 0 ) {
		/* dump it to the screen (Hm, to stdout?) */

#ifdef DEBUG_FTP
		debug( "** Unrecognized command: " );

		while( len > 0 ) {
			if(( *dp < 32 ) || ( *dp > 126 ))
				debug( " %02x ", *dp );
			else	debug("%c", *dp );
			++dp;
			--len;
		}
		debug( "\n" );
#endif

		strcat( b_s_response, "502 Command not implemented.\r\n" );
	}
}

/* end of tinyft2.c */
