/* TINYFT4.C - Part 4 of tiny FTP.
	941012	rr	split into 4 parts
*/

/* #define DEBUG_FTP */

#define	RECEIVE_DATA_PORT	0x8080

	/* This is working now. */
#define	IMMEDIATE_OPEN 

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

/* ----- ftp -------------------------------------------------------- */

void ftp_server_init(void)
{
	/* Set up listen for FTP server */

	tcp_Listen( &s_ic_ctl,	/* socket */
		21,			/* my port */
		( Procref ) ftp_server_handler,	/* handler */
		0L );			/* timeout = forever */

	/* Set up listen for data coming back. (Do we have to
		do a new Listen everytime this socket gets closed?) */

	tcp_Listen( &s_og_data,		/* socket */
		RECEIVE_DATA_PORT,	/* my port */
		( Procref ) ftp_dataHandler,	/* handler */
		0L );			/* timeout = forever */


	recv_filename[0]=0;
	p_recv_file =  -1;

	p_send_file =  -1;

}

/* end of tinyft4.c */
