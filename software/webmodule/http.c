/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

HTTP server with embedded FTP server and DYNDNS client

*/


#include "tinytcp.h"
#include <stdio.h>
#include <string.h>
#include "fileio.h"
#include "http.h"
#include "ppp.h"
#include "dyndns.h"

#include "kernel.h"
#include "skp_bsp.h"	// include SKP board support package
#include "config.h"
#include "uart.h"

//#define	DEBUGM

void mcu_reset(void);

/* ----- main program ----------------------------------------------- */

#define HDR "HTTP/1.0 200 OK\r\n"
#define HDRHTML "Content-type: text/html\r\n"
#define HDRGIF "Content-type: image/gif\r\n"
#define HDRJPEG "Content-type: image/jpeg\r\n"
#define HDRDFT "Content-type: text/plain\r\n"
#define DEFAULTRESOURCE "index.html"
#define NOFILERROR "<HTML><HEAD><TITLE>404 File Not Found</TITLE></HEAD></HTML>\r\n"

//
// Sockets
//
static struct tcp_Socket s_http;

//
// handle for rom file
//
static	FH rfhandle=-1;

#define MAX_REQUESTS 4
#define MAX_STRING 128

//
// Request QUEUE
//
static char request[MAX_REQUESTS][MAX_STRING];

// External FTP server polling loop and initialisation
extern void ftp_server_init(void),ftp_server_poll(void);

//
//This receives data on the HTTP command port, and queues the requests
//
static void http_server_handler(struct tcp_Socket * s,u8 * dp,int len )
{
	int i;
	char *p;

	if(len <= 0)
		return ;

#ifdef DEBUGM
	debug( "httpd: command packet length %d from %s port %d\n",
		len, print_ip(s-> hisaddr), s -> hisport );
#endif

		dp[len] = 0;                   /* change to null terminatd str */

		// strip CR and LF if any
		while((len > 0) && (dp[len-1]==13 || dp[len-1]==10))
			{ len--; dp[len]=0; }


#ifdef DEBUGM
		debug("httpd: request '%s'\n", dp);                  /* display request for debug */
#endif

		p=strstr((char*)dp,"GET ");

		if(p!=NULL)
		{

		 //
		 // Queue valid request if any free space in queue
		 //
		 for(i=0;i<MAX_REQUESTS;i++)
		 {
			if(!request[i][0])
			{
				strncpy(request[i],p+4,MAX_STRING);

#ifdef DEBUGM
		debug("httpd: request queued at entry %d\n",i);
#endif
				break;
			}
		 }
		}
}

#define FILEBUF_SIZE 128
static s16 filebuf_index,filebuf_len;
static u8 file_buf[FILEBUF_SIZE];               

//
// Background task for TCP stack. This does the file copies & buffer transfers
//
static void httpd_main(void)
{
	char *filename;              /* pointer to requested resource */
	char *extension;             /* ptr to extension for requested resource */
	char *dp;
	int actual,i;

		ftp_server_poll();		// run ftp server inner loop

		dyndns_client_poll();	// run dyndns client inner loop

		// is file output active
		if(rfhandle >= 0)
		{
#ifdef M16C
			RED_LED=LED_ON;
#endif
			//
			// Read data from DISK into buffer if its currently empty
			//
			if(!filebuf_len)
			{
				filebuf_len = my_read(rfhandle, file_buf, FILEBUF_SIZE);
				filebuf_index = 0;
			}

			if(filebuf_len <= 0)
			{
					//
					// Close connection if no data left in FILE
					//
					tcp_Abort(&s_http);
					my_close(rfhandle);
					rfhandle = -1;
			}
			else
			{
				//
				// Send data to HTTP TCP data port
				//
				actual = tcp_Write( &s_http, &file_buf[filebuf_index], filebuf_len);
				if(!actual)
				{
					//
					// shut down if TCP is not working
					//
					tcp_Abort(&s_http);
					my_close(rfhandle);
					rfhandle=-1;
				}
				else // advance pointers
				{
				 filebuf_index += actual;
				 filebuf_len -= actual;
				 tcp_Flush(&s_http);
				}
			}
		}
		else // no file send active
		{
#ifdef M16C
			RED_LED = LED_OFF;
#endif
			if(s_http.state == TS_CLOSED)
			{
				//
				// restart listener if socket was closed
				//
#ifdef DEBUGM
				debug("httpd: restarting listener\n");
#endif
				tcp_Listen( &s_http,	/* socket */
					80,			/* my port */
					( Procref ) http_server_handler,	/* handler */
					0L );			/* timeout = forever */

			}


				for(i=0;i<MAX_REQUESTS;i++)
				{
					if(request[i][0])
						break;
				}


				if(i < MAX_REQUESTS)
				{

				 dp=request[i];

#ifdef DEBUGM
				debug("httpd:qrequest%d '%s'\n",i,dp);
#endif

				if (strcmp("/", dp))   	  				/* is this default request 'GET /'? */
				{
					if(dp[0] == '/')
						filename = dp + 1;       		/* no, skip past initial '/' */
					else
						filename = dp;			  		/* if it exists, that is */
				}
				else
					filename = DEFAULTRESOURCE;

				extension = filename;                 /* locate extension */
				while ((*extension) && (*extension != '.'))
					extension++;

				rfhandle = my_open(filename,MY_OPEN_READ);     /* open local resource */
				filebuf_len = 0;
			
				 if(rfhandle >= 0)
				 {
					tcp_Write(&s_http, (u8*)HDR, strlen(HDR));

					if (!stricmp(".htm", extension))  /* write ext specific response */
						tcp_Write(&s_http, (u8*)HDRHTML, strlen(HDRHTML));
					else if (!stricmp(".jpg", extension))
						tcp_Write(&s_http,  (u8*)HDRJPEG, strlen(HDRJPEG));
					else if (!stricmp(".gif", extension))
						tcp_Write(&s_http, (u8*)HDRGIF, strlen(HDRGIF));
					else
						tcp_Write(&s_http, (u8*)HDRDFT, strlen(HDRDFT));

					sprintf(buffer, "Content-length: %lu\r\n\r\n", my_filelength(rfhandle));

					tcp_Write(&s_http, (u8*)buffer, strlen(buffer));  /* write size */
				 }
				 else
				 {

					tcp_Write(&s_http, (u8*)NOFILERROR, strlen(NOFILERROR));

					tcp_Abort(&s_http);

				 } // open not successful

				 // clear request slot
				 request[i][0]=0;

				} // valid request
			} // file not open

	// sense carrier loss and reset CPU if so
	if(config.dial_enabled && !check_carrier_detect())
		mcu_reset();
}

void httpd(void)
{
	int i;

#ifdef DEBUGM
	debug("httpd:%s\n",print_ip(ADDR(addr1,addr2,addr3,addr4)));
#endif


	//
	// clear all requests
	//
	for(i=0;i<MAX_REQUESTS;i++)
		request[i][0]=0;

	//
	// no current request
	//
	rfhandle = -1;

	tcp_Init(ADDR(addr1,addr2,addr3,addr4));		/* init TCP/IP */

	/* Set up listen for HTTP server */

	tcp_Listen( &s_http,	/* socket */
		80,			/* my port */
		( Procref ) http_server_handler,	/* handler */
		0L );			/* timeout = forever */

	/* Set up Dynamic DNS client */
	dyndns_client_init();

	/* Set up FTP server */
	ftp_server_init();

	/* Start protocol stack, with inner loop callback */
	tcp(httpd_main);


}


/* end of http.c */
