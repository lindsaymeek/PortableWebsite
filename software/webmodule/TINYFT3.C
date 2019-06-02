/* TINYFT3.C - Part 3 of tiny FTP.
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

	/* Receive buffer for client side. */

extern	char		b_response[ 120 ];	/* response buffer */
extern	int		i_response;		/* index into response buf */

	/* Client output command buffer */

extern	char		b_c_command[ 82 ];	/* send buffer */

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

/* ----- abort ftp -------------------------------------------------- */

Void ftp_Abort( Void ) 
{
	
	tcp_Abort( &s_og_data );
}


void ftp_server_poll(void)
{
	int i;

	/* Look at the SERVER buffer and send anything that's there */

	i = strlen( b_s_response );
	if( i ) 
	{
		i = tcp_Write( &s_ic_ctl,( Byte * ) b_s_response, i );

		/* move down the data which remains to be sent */
		if(i) strcpy( &b_s_response[ 0 ], &b_s_response[ i ] );

		tcp_Flush( &s_ic_ctl );
	}

	/* If the server (or the client?) is sending a file, send the file */

	if( p_send_file >=  0 ) 
	{

		/* if buffer is empty, read some more data */
		if( n_s_left == 0 ) 
		{
			n_s_left = ( int ) my_read( p_send_file, &b_s_data[ 0 ], sizeof( b_s_data ));
			if( n_s_left <= 0 ) 
			{		/* EOF! */
				my_close( p_send_file );
				p_send_file = -1;
#ifdef DEBUG_TCP
				debug( "ftp:Closing...\n" );
#endif
				tcp_Close( &s_ic_data );

				strcat( b_s_response, "226 File sent.\r\n" );

				return;

			}
			n_s_sent = 0;
		}

		/* send as much as will fit */
  
		i = tcp_Write( &s_ic_data, &b_s_data[ n_s_sent ], n_s_left );

		/* if buffer is empty, read some more data */

		n_s_sent += i;
		n_s_left -= i;

		tcp_Flush( &s_ic_data );
	}

}

/* end of tinyft3.c */
