
/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

Program:	Compact flash bus interface
Version:	25/5/05

*/

#include "skp_bsp.h"	// include SKP board support package
#include "bus.h"
#include <stdio.h>

//
// Initialise external memory interface 
// to compact flash card
//
char bus_init(void)
{
	prc1 =1;	// set protection register to allow writing to mode control registers

	pm0 = 1; 	// expansion mode, all /CS active, non multiplexed
					// BCLK active, /RD, /BHE, /WR mode

	pm10 = 1;
	pm11 = 1;		// MODE3

	pm14 = 0;		// P53=BCLK
	pm15 = 0;
	 
	ds0 = 1;		// 16/8-bit address space for /CS0

	ewcr0 = 16+ 4+ 2; // 3+6 BCLK cycles

	prc1 =0;	// protect mode control registers

    prc0 =1;    // set protection register to allow writing to clock control registers

	cm00 = 1;
	cm01 = 0;	// output Fc on BCLK output

	prc0=0;		// protect clock control registers

	return 0;
}



