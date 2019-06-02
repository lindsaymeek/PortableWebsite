/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

Mainline

*/

#include "skp_bsp.h"	// include SKP board support package
#include "uart.h"
#include "bus.h"
#include "config.h"
#include "ppp.h"
#include "http.h"
#include <stdio.h>
#include "kernel.h"
#include "ffs.h"

/* interrupt routine used for system ticker - vector modified in sect30_skpdemo.inc */ 
#pragma INTERRUPT 	ta0_irq

/* Function prototypes */
void main(void);
void mcu_init(void);
void ta0_irq(void);

void led_display(char);
void periph_init(void);

/* global variables */

char hd_ok;

volatile time_t jiffies = 0;			// this value will be incremented every ms

/* Main loop */
void main(void)
{
	/* Initializations */
	mcu_init();					// Initialize MCU clocks, GPIO, ADC & timers
	bus_init();					// Initialise compact flash memory interface
    InitDisplay();				// Initialize LCD
    periph_init();				// Initialise tick timer
	led_display(4);				// Initialise LEDs off

	/* Display Renesas Splash Screen */
	DisplayString(LCD_LINE1,RENESAS_LOGO);	// Display Renesas bitmapped logo on Line 1
	DisplayString(LCD_LINE2,"Technlgy");
	
	uart_init(57600L);			// Initialise UART
//	debug_control(OUTPUT_UART);		// Redirect debugging to UART display
	debug_control(OUTPUT_LCD);		// Redirect debugging to LCD display

	cf_config();

	hd_ok=FALSE;
	// fire up IDE & compact flash interface
	if(TRUE==hd_qry())
	{
		if(TRUE==hd_mbr())
		{
			if(TRUE==hd_bpb())
			{
				hd_ok=TRUE;
			}
		}
	}

	// load configuration parameters from disk, or defaults if file not loaded
	config_init(hd_ok);

	if(config.baudrate != 57600L)
		uart_init(config.baudrate);		// Reinitialise UART with configuration setting speed

	debug_control(OUTPUT_LCD);		// Redirect debugging to LCD display

	// connect and fire up ppp
	PPPInit(config.dial_enabled);

	// start main FTP, HTTP protocol server engine
	httpd();

	//	monitor();

}

/*****************************************************************************
Name:		ta0_irq   
Parameters:	None				 
Returns:	None
Description: This is the timer A0 interrupt routine. It occurs every 1 ms. 

*****************************************************************************/
void ta0_irq(void)
{
	jiffies++;
}

/*****************************************************************************
Name:		periph_init   
Parameters:	None				 
Returns:	None
Description: This function initializes all GPIO and peripherals for the demo 

			  
*****************************************************************************/
void periph_init(void)
{
 	unsigned int i;

	/* Start the 32Khz crystal sub clock */
    prc0 = 1;  		// Unlock CM0 and CM1
    pd8_7 = 0;		// setting GPIO to inputs (XCin/XCout)
    pd8_6 = 0;
    cm04 = 1; 		// Start the 32KHz crystal
    
	/* add some time delay for 32kHz to stabilize */
    for (i=0; i<0x7ff; i++)
    {
 	   _asm("NOP");
    }
    cm03 = 0;		// Set Xc clock to low drive

	/* Switch initialization - macro defined in skp_bsp.h */
 	ENABLE_SWITCHES 

	/* LED initialization - macro defined in skp_bsp.h */
 	ENABLE_LEDS 	

/* Timer initialization */
	/* Configure Timer A0 - 10ms (millisecond) counter */
	ta0mr = 0xc0;								// Timer mode, fc32, no pulse output

	ta0 = 1;		// 1023 = 1 second

	/* Change interrupt priority levels to enable maskable interrupts */
	DISABLE_IRQ		// disable irqs before setting irq registers - macro defined in skp_bsp.h
	ta0ic = 2;		// Set the timer's interrupt priority to level 2
	ENABLE_IRQ		// enable interrupts macro defined in skp_bsp.h

	/* Start timer */
	ta0s = 1;		// Start timer A0


}


/*****************************************************************************
Name:		led_display   
Parameters:	count				 
Returns:	None
Description: Controls LED display.
*****************************************************************************/
void led_display(char count)
{

	switch (count)
	{
		case 1:							/* green on */
   			RED_LED = LED_OFF;			
			YLW_LED = LED_OFF;
			GRN_LED = LED_ON;
			break;
		case 2:							/* yellow on */
		case 4:
   			RED_LED = LED_OFF;			
			YLW_LED = LED_ON;
			GRN_LED = LED_OFF;
			break;
		case 3:							/* red on */
   			RED_LED = LED_ON;			
			YLW_LED = LED_OFF;
			GRN_LED = LED_OFF;
			break;
		default:						/* all LED's off */
   			RED_LED = LED_OFF;			
			YLW_LED = LED_OFF;
			GRN_LED = LED_OFF;
	}
}




	