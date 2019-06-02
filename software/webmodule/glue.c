
/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

System Interfacing GLUE (Timing, Endian Swapping, Compact Flash Interface)

*/

#include "skp_bsp.h"
#include "kernel.h"
#include "uart.h"
#include "bus.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <asmmacro.h>

/* temporary buffer */
char buffer[BUFFERSIZE];

//
// Big endian to little endian conversion
//
u16  swap16(u16 x)
{
	u16 t;

	((u8 *)&t)[0]=((u8 *)&x)[1];
	((u8 *)&t)[1]=((u8 *)&x)[0];

	return t;
}

u32  swap32(u32 x)
{
	u32 t;

	((u8 *)&t)[0]=((u8 *)&x)[3];
	((u8 *)&t)[1]=((u8 *)&x)[2];
	((u8 *)&t)[2]=((u8 *)&x)[1];
	((u8 *)&t)[3]=((u8 *)&x)[0];

	return t;
}

// Timing functions

void delay_ms(unsigned short ms)
{
	time_t i ;

	if(!ms)
		ms=1;

	mark(i);

	while(elapsed(i) < ms) ;
}

void udelay(u16 us)
{
	us /= 1000;
	if(!us)
		us=1;
	delay_ms(us);
}

u32 MsecClock (void)
{
	return jiffies;
}

// Compact flash memory interface functions

// Access common memory 

void writew(u16 reg_addr, u16 data16)
{
	u16 *reg = (u16 *)(CF_ADDR +  CF_COMMON_OFFSET + reg_addr);
	
	*reg = data16;
}

u16 readw(u16 reg_addr)
{
	u16 *reg = (u16 *)(CF_ADDR +  CF_COMMON_OFFSET + reg_addr);
	
	return *reg;
}

//
// Access attribute memory (/REG asserted in common memory)
//
u8 rd_cf_reg(u16 reg_addr)
{
	u8 *reg = (u8 *)(CF_ADDR + CF_ATTRIB_OFFSET + reg_addr);

	return *reg;
}

void wr_cf_reg(u16 reg_addr,u8 data)
{
	u8 *reg = (u8 *)(CF_ADDR + CF_ATTRIB_OFFSET + reg_addr);

	*reg = data;
}

// Access I/O memory

void outb(u16 reg_addr,u8 data)
{
	u8 *reg = (u8 *)(CF_ADDR +  CF_IO_OFFSET + reg_addr);

	*reg = data;
}

u8 inb(u16 reg_addr)
{
	u8 *reg = (u8 *)(CF_ADDR +  CF_IO_OFFSET + reg_addr);

	return *reg;
}

void outw(u16 reg_addr,u16 data16)
{
	u16 *reg = (u16 *)(CF_ADDR +  CF_IO_OFFSET + reg_addr);

	*reg = data16;
}

u16 inw(u16 reg_addr)
{
	u16 *reg = (u16 *)(CF_ADDR +  CF_IO_OFFSET + reg_addr);

	return *reg;
}

// High speed copy using M16C assembler macro

void outsw(u16 addr,u16 *data16,u16 len)
{
	u16 *reg = (u16 *)(CF_ADDR +  CF_IO_OFFSET + addr);

	sout_w((int*)data16,(int*)reg,len);
}

void insw(u16 addr, u16 *data16,u16 len)
{
	u16 *reg = (u16 *)(CF_ADDR +  CF_IO_OFFSET + addr);

	sin_w((int*)reg,(int*)data16,len);
}

//
// Reset the CF card in PCMCIA mode (RESET is active high)
//
void cf_reset(void)
{
	ps0_6=0;		// P6.6 is I/O

	pu21=0;

	pd6_6=1;		// RESET pin is output

	p6_6=0;			// Deasserted

	delay_ms(1);

	p6_6=1;			// Asserted

	delay_ms(100);

	p6_6=0;			// Deasserted

	delay_ms(100);
}

//
// Configure the PCMCIA interface on the compact flash card
//

#define DEVICE_TUPLE		0x01
#define DEVICE_A_TUPLE		0x17
#define DEVICE_OC_TUPLE		0x1C
#define DEVICE_OA_TUPLE		0x1D
#define VERS_1_TUPLE		0x15  
#define MANFID_TUPLE		0x20
#define FUNCID_TUPLE		0x21
#define FUNCE_TUPLE			0x22	
#define CONFIG_TUPLE		0x1A	
#define CFTABLE_ENTRY_TUPLE	0x1B
#define END_TUPLE			0xFF

bool cf_config(void)
{
	u8 cor_addr_lo=0,cor_addr_hi,cor_data;
	u8 cis_device;
	u16 cis_addr,x;  	// attribute memory index (EVEN ADDRS ONLY)
	u16 corbase;		// address of configuration register base
	u8 code_tuple;			// tuple type
    u8 link_tuple;           	// number of body bytes in this particular tuple
    u8 tuple_link_cnt;
	u8 tuple_data;			// current body, byte value for a given body
    u8 funce_cnt;      	 	// there are 6 CF_CISTPL_FUNCE & the 4th holds the MAC addrs

	cf_reset();

	x = 500;

	do
	{

	delay_ms(1);

		    if(--x == 0)

			{
				// CF card not detected
			
				return FALSE;
			}

		code_tuple = rd_cf_reg(0);	


	  } while(code_tuple!=1);


   cis_device  = 0;

   corbase    = 0;

   funce_cnt = 0;			// reset to the first CF_CISTPL_FUNCE


   cis_addr   = 0;			// CIS starts at location 0 of Attribute memory

 	do{

   		code_tuple = rd_cf_reg(cis_addr);

        cis_addr+=2;	

        link_tuple = rd_cf_reg(cis_addr);		

        cis_addr+=2;  
    

	  if(code_tuple != END_TUPLE)

      {

       for(tuple_link_cnt=0;tuple_link_cnt<link_tuple;++tuple_link_cnt)		

       {

      	 tuple_data = rd_cf_reg(cis_addr);	

         cis_addr+=2;



          switch(code_tuple)	

          {

          	 case DEVICE_TUPLE:			

			  break;	

             case DEVICE_A_TUPLE:		

			  break;	

             case DEVICE_OC_TUPLE:		

			  break;	

             case DEVICE_OA_TUPLE:  		

			  break;	

             case VERS_1_TUPLE:     		

			  break;

             case MANFID_TUPLE:     		

			  break;	

             case FUNCID_TUPLE:			

               	if(tuple_link_cnt == 0)

					cis_device = tuple_data; 		// device type 0x06 = Network Adapter

			    break;

             case CONFIG_TUPLE:				  		// GET THE COR ADDRESS

               if(tuple_link_cnt == 2)

               	cor_addr_lo = tuple_data;		  	// copying low-byte

               else if(tuple_link_cnt == 3)              // shift, then copy hi-byte

			   {

               	cor_addr_hi = tuple_data;

				corbase = (cor_addr_hi<<8)+cor_addr_lo;
			

			   }

              break;

			   

             case CFTABLE_ENTRY_TUPLE:	

			  break;

             default:					

			  break;

          }// end switch

       }// end for loop tuple_link_cnt

     }//if(code_tuple != END_TUPLE)

   

   	}while(code_tuple!=END_TUPLE);


	if(cis_device != 4)
	{
		// incorrect device class
		return FALSE;
	}

	if(!corbase)
	{
		// didn't find corbase
		return FALSE;
	}

	// configure the memory card into configuration # 1 (16 bit I/O boundary)
	wr_cf_reg(corbase,1);

	return TRUE;

}			

