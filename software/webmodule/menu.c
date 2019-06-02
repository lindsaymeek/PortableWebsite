
/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

Configuration Loader / Default'er

*/

#include "tinytcp.h"
#include "skp_bsp.h"	// include SKP board support package
#include "uart.h"
#include "kernel.h"
#include "config.h"
#include "fileio.h"


#include <string.h>
#include <ctype.h>
#include <stdlib.h>

// Define this to dump config data
//#define DEBUG_CONFIG

// Global configuration structure
struct CONFIG config;

// The name of the configuration file on the CF drive

#define CONFIG_FILENAME "webcfg.txt"

//
// dumb parse a.b.c.d string into 4 octets
//
bool get_ip(char *str, u8 *octets)
{
	int i,j;

	for(j=0;j<4;j++)
	{
	
	 i=0;
	 // scan for a '.' or end of string
	 while(str[i])
	 {
		if(str[i]=='.') 
		 break;
		i++;
	 }

	 // look for terminating octet
	 if(j < 3 && str[i]!='.')
		return FALSE;
	
	 // terminate
	 str[i]=0;

	 // convert ascii to number
	 octets[j]=atoi(str);

	 // copy back and repeat
	 strcpy(str,str+i+1);

	}
	return TRUE;
}

//
// alloc string from the heap and copy to it (saves memory)
//
char *stralloc(char *base)
{
	char *new=NULL;
	
	if(strlen(base)>0)
	{
		new=malloc(strlen(base)+1);
		if(new!=NULL)
			strcpy(new,base);

	}

	return new;

}

//
// search for a parameter in the list and return the index
//
int match_parameter(char *arg)
{
	static const char *param[] = {
		"isp_phone", 
		"isp_username", 
		"isp_password", 
		"dyndns_server",
		"dyndns_password",
		"domain", 
		"ppp_direct",
		"dyndns_type",
		"dyndns_port",
		"baudrate",
		NULL };
	int i;


	i=0;
	while(param[i]!=NULL)
	{
		if(!stricmp(param[i],arg))
			return i;
		i++;
	}
	return -1;
}

//
// load the global configuration from file, or just use defaults if that fails
//
bool config_init(bool disk_ok)
{
	FH cf;
	int len;
	char *eq;
	bool read_ok=FALSE;

		// direct PPP connection  @ 57600 to local linux test server with IP fw/masq
		config.isp_phone= NULL;
		config.isp_username = NULL;
		config.isp_password = NULL;
		config.dial_enabled = 0;
		config.baudrate = 57600L;
		config.dyndns_port = 80;
		config.dyndns_type = 0;
		config.dyndns_password = "renesas";
		config.domain = "renesas.dynu.com";
		strcpy(buffer,"209.189.239.227");
		get_ip(buffer,config.dyndns_server);
	

	if(disk_ok)
	{
		// open config file
		cf=my_open(CONFIG_FILENAME,MY_OPEN_READ);
		if(cf >= 0)
		{
			len=my_gets(cf,buffer,BUFFERSIZE-1);
			while(len != 0)
			{
				// if not a comment
				if(buffer[0]!=';' && buffer[0]!='#')
				{
				
					eq=strstr(buffer,"=");
					if(eq!=NULL)
					{
						read_ok=TRUE;
						*eq=0;
						switch(match_parameter(buffer)) {
							default:
								break; // ignore unknown parameters
							case 0:  config.isp_phone=stralloc(eq+1); break;
							case 1:  config.isp_username=stralloc(eq+1); break;
							case 2:  config.isp_password=stralloc(eq+1); break;
							case 3:  get_ip(eq+1, config.dyndns_server); break;
							case 4:  config.dyndns_password=stralloc(eq+1); break;
							case 5:  config.domain=stralloc(eq+1); break;
							case 6:  config.dial_enabled=!atoi(eq+1); break;
							case 7:  config.dyndns_type=atoi(eq+1); break;
							case 8:  config.dyndns_port=atoi(eq+1); break;
							case 9:  config.baudrate=atol(eq+1); break;
						}
					}	
				}
				len=my_gets(cf,buffer,BUFFERSIZE-1);
			}
			my_close(cf);
		}
	}

#ifdef DEBUG_CONFIG

	debug("CONFIGURATION read ok %d\n\r",(int)read_ok);

	// dump parameters
	if(config.dial_enabled)
	{
	 if(config.isp_phone!=NULL) debug("isp_phone:%s\n\r",config.isp_phone);
	 if(config.isp_username!=NULL) debug("isp_username:%s\n\r",config.isp_username);
	 if(config.isp_password!=NULL) debug("isp_password:%s\n\r",config.isp_password);
	}
	else
	{
		debug("direct ppp connection\n\r");
	}

	debug("uart baudrate %lu bps\n\r", config.baudrate);

	debug("dyndns type %d port %u ip %s\n\r", 	
			(int)config.dyndns_type,
			(int)config.dyndns_port,
			print_ip(ADDR(config.dyndns_server[0],config.dyndns_server[1],config.dyndns_server[2],config.dyndns_server[3])));

	if(config.dyndns_password!=NULL) debug("dyndns password %s\n\r",config.dyndns_password);
	if(config.domain!=NULL) debug("dyndns domain %s\n\r",config.domain);
	 	

#endif

	return read_ok;
}
