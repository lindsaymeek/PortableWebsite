
/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

Debugging interface

*/

#include "skp_lcd.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "uart.h"
#include "kernel.h"
#include <ctype.h>

// debugging is off by default
static char output_mode=OUTPUT_NONE;

char tmp[8];

//
// printf like debugging procedure that dumps to serial port
//
int debug(const char _far *fmt, ...)
{
	va_list va;
	int i,rc;

	va_start(va, fmt);

	if(output_mode!=OUTPUT_NONE)
	{
 	 rc=vsprintf(buffer, fmt, va);

	 if(output_mode==OUTPUT_UART)
		puts(buffer);
	 else if(output_mode==OUTPUT_LCD)
	 {
	  memset(tmp,32,sizeof(tmp));
	  for(i=0;i<strlen(buffer);i++)
	  {
		if(isprint(buffer[i]))
		{
		 if(i < 8)
			tmp[i]=buffer[i];
		 else
		 {
			memcpy(tmp,tmp+1,7);
			tmp[7]=buffer[i];
			delay_ms(200);
		 }
		 DisplayString(LCD_LINE2,tmp);	
		}
	  }
	 }
	}
	else
		rc=0;

	va_end(va);

	return rc;
}

// string version which is lighter on stack usage
void debugs(const char *s)
{
	int i;

	if(output_mode!=OUTPUT_NONE)
	{
 
	 if(output_mode==OUTPUT_UART)
		puts(s);
	 else if(output_mode==OUTPUT_LCD)
	 {
	  memset(tmp,32,sizeof(tmp));
	  for(i=0;i<strlen(s);i++)
	  {
		if(isprint(s[i]))
		{
		 if(i < 8)
			tmp[i]=s[i];
		 else
		 {
			memcpy(tmp,tmp+1,7);
			tmp[7]=s[i];
			delay_ms(200);
		 }
		 DisplayString(LCD_LINE2,tmp);	
		}
	  }
	 }
	}

}

//
// switch debug output between LCD and uart
//
void debug_control(char _output)
{
	output_mode=_output;
}
