
/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

UART serial driver & simple command line monitor

*/

/****************************  Global variables ***************************/
#include "skp_bsp.h"	// include SKP board support package

#include <stdio.h>
#include <ctype.h>

#include "uart.h"
#include "bus.h"
#include "kernel.h"
#include "ide.h"

#define RX_BUFSIZE 256

static unsigned char rx_buffer[RX_BUFSIZE];
static volatile unsigned int rx_read_i=0,rx_write_i=0;

/*This must also be setup in the vector table in SECT30.INC */
#pragma INTERRUPT U0rec_ISR

void U0rec_ISR (void);

void uart_init(long baud)
{

	unsigned char dummy;
	DISABLE_IRQ      // disable irqs before setting irq registers - macro defined in skp_bsp.h 
    

/*  Configure Uart0 for 8 data bits, 1 stop bit, no parity */

  	u0mr = 0x05;		// set mode register
    /* 00000101         loads the uart 0 mode register 
       ||||||||___SMD[2:0]:   SELECTS UART MODE, 8 BIT DATA TRANSFER
	   |||||______CKDIR:      INTERNAL CLOCK SELECTED    
	   ||||_______STPS ;      ONE STOP BIT 
	   |||________PRY:        ODD PARITY
	   ||_________PRYE:       DISABLE PARITY  
	   |__________IOPOL: 	  TRANSMITTER RECEIVER OUTPUT NOT INVERSED */

  	u0c0 = 0x10; 		// set control register 0
	/* 00000101         loads the uart 0 control register 0 
       ||||||||___CLK[1:0]:   COUNT SOURCE DIVIDED BY 8
	   ||||||_____CRS;        CTS RTS ENABLED WHEN CRD=0  
	   |||||______TXEPT:      TRANSMIT REGISTER EMPTY FLAG           
	   ||||_______CRD ;       CTS/RTS FUNCTION DISABLED
	   |||________NCH:        DATA OUTPUT SELECT BIT
	   ||_________CKPOL:      CLOCK POLARITY SELECTED,TX FALLING EDGE,RX RISING EDGE  
	   |__________UFORM: 	  MSB FIRST */
			
	u0brg = (unsigned char) (f1_CLK_SPEED/(16*baud)-1);	// set bit rate generator
				 		//  
  	u0tb = 0;			// clear transmit buffer
  	dummy = u0rbl;		// clear receive buffer by reading
      	

	s0ric = 0x03;		// Enable UART0 receive interrupt,priority level 3	
  
  	// flush buffers

  	rx_read_i = rx_write_i = 0;


    ENABLE_IRQ          // enable interrupts through macro defined in skp_bsp.h

	// pin settings for making pin p6_3 as transmitter pin of Uart 0
	ps0_3=1;           
     
    // pin settings for making pin p6_2 as receiver pin of Uart 0
    ps0_2=0;           
	pd6_2=0;
		
	// pin setting for making pin p6_5 rts
	ps0_5=0;
	pd6_5=1;

	// pin setting for making pin p6_4 cd
	ps0_4=0;
	pd6_4=0;

	p6_5=0;	// assert RTS
	 	
	te_u0c1 = 1;			// Enable UART0 transmit
	re_u0c1 = 1; 			// Enable UART0 receive

}


/*****************************************************************************
Name       : UART0 Receive Interrupt Routine       
Parameters : none                   
Returns    : none   
Description: Interrupt routine for UART0 receive
	   
*****************************************************************************/
void U0rec_ISR(void)
{
	unsigned int next_i;

	next_i = (rx_write_i+1)&(RX_BUFSIZE-1);

	while(ri_u0c1 == 0);	  	// make sure receive is complete

	if(next_i != rx_read_i)
	{
	 rx_buffer[rx_write_i] = u0rb;

	 rx_write_i = next_i;

	}
	else
	{
		next_i = u0rb; // discard
		pd6_5=1;	   // deassert RTS
	}

}	

void putch(int x)
{
		GRN_LED = LED_ON;
		while(ti_u0c1 == 0); 		//  puts it in the UART 0 transmit buffer 
		u0tb = (unsigned char )x;
		GRN_LED = LED_OFF;
}

//
// return true if characters waiting
//
char kbhit(void)
{
		if(rx_read_i != rx_write_i)
		{
			YLW_LED = LED_ON;
			return 1;
		}
		else
		{
		
			YLW_LED = LED_OFF;
			return 0;
		}
}

int getch(void)
{
	int x;

	if(kbhit()) 
	{
		x=rx_buffer[rx_read_i];
		rx_read_i = (rx_read_i+1) & (RX_BUFSIZE-1);	
		pd6_5=0;	// assert RTS

					
		return x;
	}	
	else
		return 0;
}

char check_carrier_detect(void)
{
	if(p6_4)
		return 1;
	else
		return 0;
}

int puts(const char _far *p)
{
	while(*p)
		putch(*p++);
	return 0;
}

// read common memory in 8-bit mode
static void dump_common(long start, long end)
{
	char index;
	long addr;

	for(addr=start;addr<end;addr+=16)
	{
		debug("%08lX ",addr);

	
		 for(index=0;index<16;index++)
	 	 {
			debug("%04X ",readw(addr+index));
		 }
		

		debug("\n\r");
	}	
}

// read attribute memory in 8-bit mode
static void dump_attrib(long start, long end)
{
	char index;
	long addr;

	for(addr=start;addr<end;addr+=16)
	{
		debug("%08lX ",addr);

		 for(index=0;index<16;index+=2)
	 	 {
			debug("%02X ",rd_cf_reg(addr+index));
		 }
		
		debug("\n\r");
	}	
}

// read i/o memory in 8-bit mode
static void dump_io8(long start, long end)
{
	char index;
	long addr;

	for(addr=start;addr<end;addr+=16)
	{
		debug("%08lX ",addr);

		 for(index=0;index<16;index++)
	 	 {
			debug("%02X ",inb(addr+index));
		 }
	
		debug("\n\r");
	}	
}

// read i/o memory in 16-bit mode
static void dump_io16(long start, long end)
{
	char index;
	long addr;

	for(addr=start;addr<end;addr+=16)
	{
		debug("%08lX ",addr);

		 for(index=0;index<16;index+=2)
	 	 {
			debug("%04X ",inw(addr+index));
		 }
	
		debug("\n\r");
	}	
}

// read m16c memory in 8-bit mode
static void dump_mem(long start, long end)
{
	char index;
	long addr;

	for(addr=start;addr<end;addr+=16)
	{
		debug("%08lX ",addr);

		 for(index=0;index<16;index++)
	 	 {
			debug("%02X ",*((u8*)(addr+index)));
		 }
		
		debug("\n\r");
	}	
}

// parse the ascii hex value into a number
static long parse_hex(char *str,char *index)
{
	long val=0;
	char c,leading;

	leading=1;
	c=tolower(str[*index]);
	while(c!=0)
	{
	
		if(c >= '0' && c <= '9') 
		{
			val = (val << 4) | (c-'0');
			leading=0;
		}
		else
		{
			if(c >= 'a' && c <= 'f')
			{
				val = (val << 4) | (c-'a'+10);
				leading=0;
			}
			else
			{
				if(c==' ')
				{
					if(!leading)
						return val;
				}
				else
					return -1;
			}
		}
					
		(*index)++;
		c=tolower(str[*index]);
	}

	if(!leading)
		return val;
	else
		return -1;
}

//
// very simple memory monitor for testing interfaces
//
// a = dump cf attrib mem
// r = dump cf common mem
// i = dump cf i/o mem 8-bit mode
// j = dump cf i/o mem 16-bit mode
// s = reset cf card
// d = read cf sector
//
void monitor(void)
{
	char inbuf[32],index=0;
	char c;
	long start,end;
	static long last_start=0;
	unsigned short x;

	debug_control(OUTPUT_UART);			// Redirect debugging to UART

	debug("Monitor ready.\n\r:");
	
	for(;;) 
	{
	
	while(!kbhit()) ;

	c=getch();

	if(c!=8 && c!=127)
		inbuf[index++]=c;
	else
	{
		if(index) index--;
		c=8;
	}

	putch(c);

	if(c==13 || c==10 || index > 31)
	{
		inbuf[index-1]=0;

		debug("\n\r");

		switch(tolower(inbuf[0])) {
			default:
				debug("Unknown command '%s'\n\r", inbuf);
				break;

			case 'a':
				index = 1;
				start = parse_hex(inbuf,&index);				
				if(start < 0)
					start = last_start;
			
				 end = parse_hex(inbuf,&index);
				 if(end < 0)
					end=start+256;

  				 dump_attrib(start,end);
			
				last_start=end;

				break;
			case 'r':
				index = 1;
				start = parse_hex(inbuf,&index);				
				if(start < 0)
					start = last_start;
			
				 end = parse_hex(inbuf,&index);
				 if(end < 0)
					end=start+256;

  				 dump_common(start,end);
			
				last_start=end;

				break;

			case 'i':
				index = 1;
				start = parse_hex(inbuf,&index);				
				if(start < 0)
					start = last_start;
			
				 end = parse_hex(inbuf,&index);
				 if(end < 0)
					end=start+256;

  				 dump_io8(start,end);
			
				last_start=end;

				break;

			case 'j':
				index = 1;
				start = parse_hex(inbuf,&index);				
				if(start < 0)
					start = last_start;
			
				 end = parse_hex(inbuf,&index);
				 if(end < 0)
					end=start+256;

  				 dump_io16(start,end);
			
				last_start=end;

				break;
						
			case 's':
				if(TRUE==ide_Initialise())
					dump_mem((u32)sector,512+(u32)sector);
				else
					debug("IDE init failed\n\r");
				break;

			case 'd':
				index = 1;
				start = parse_hex(inbuf,&index);				
				if(start < 0)
					start = last_start;
			
				if(TRUE==ide_SectorRead(start))
					dump_mem((u32)sector,512+(u32)sector);
				else
					debug("IDE read failed\n\r");
					

				break;

			case 'q':
				return;
			
		}
		index=0;
		debug(":");
	 }
	}
}


