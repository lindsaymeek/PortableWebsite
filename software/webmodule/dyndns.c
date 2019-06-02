/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

Dynamic DNS TCP client

*/


#include "tinytcp.h"
#include "http.h"
#include "ppp.h"
#include "kernel.h"
#include "config.h"
#include <string.h>

struct tcp_Socket dyndns_s;	/* outgoing connection socket */

static time_t dyndns_timer,dyndns_timeout;	// refresh timer, timeout value
static bool dyndns_active;					// socket active?
static char dyndns_buffer[256];				// buffer for creating string

void dyndns_client_init(void)
{
	mark(dyndns_timer);
	dyndns_active=FALSE;
	dyndns_timeout=HZ*5;	// connect 5 seconds after starting at initialisation
}

/* ----- dyndns control handler ---------------------------------------- */

void dyndns_ctlHandler(struct tcp_Socket *s, u8 *dp, int len )
{
	// ignore incoming data
	
}

void dyndns_client_poll(void)
{
	int i;

	// only supports the dynu dynamic dns at the moment 
	if(config.dyndns_type==DYNDNS_TYPE_DYNU)
	{
	 if(dyndns_active==FALSE) 
	 {
	    // refresh every 5 mins or initially at connection
		if(elapsed(dyndns_timer)>dyndns_timeout )
		{
		 dyndns_active=TRUE;
		 dyndns_timeout=HZ*5*60;	// slow down to refresh every 5 mins

		 tcp_Open( &dyndns_s,		/* socket */
			0,						/* my port - choose dynamically */
			ADDR(config.dyndns_server[0],
			     config.dyndns_server[1],
				 config.dyndns_server[2],
				 config.dyndns_server[3]),			/* host to call */
			config.dyndns_port,		/* his port (hailing freq.) */
			( Procref ) dyndns_ctlHandler );	/* handler */

		  // create the dns update command
			strcpy(dyndns_buffer,"GET http://basicupdate.dynu.com:80/basic/update.asp?domain=");
			strcat(dyndns_buffer,config.domain);
			strcat(dyndns_buffer,"&password=");
			strcat(dyndns_buffer,config.dyndns_password);
			strcat(dyndns_buffer,"&ip_address=");
			strcat(dyndns_buffer,print_ip(ADDR(addr1,addr2,addr3,addr4)));
			strcat(dyndns_buffer," HTTP/1.0\n\n");

		}
	 }
	 else // connection to server is active
	 {
			if(dyndns_s.state == TS_ESTAB)
			{
			// yes, send updated IP address command

			i = strlen(dyndns_buffer);

			if(i)
			{
			 i = tcp_Write( &dyndns_s, ( u8 * ) dyndns_buffer, i );

  		     tcp_Flush( &dyndns_s );

		     /* move down the data which remains to be sent */

		     if( i ) strcpy( &dyndns_buffer[ 0 ], &dyndns_buffer[ i ] );
			}

		   	if(!i)
				tcp_Close(&dyndns_s);
				mark(dyndns_timer);
				dyndns_active=FALSE;
			}

	 }
	}
}
