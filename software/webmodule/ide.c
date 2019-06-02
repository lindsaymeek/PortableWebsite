/*

Renesas M16C Design Contest 2005
Project M1747
Portable Website

Compact Flash ATA/IDE interface routines

*/
 
#include "ide.h"

#include "kernel.h"

// sector buffer
u8 sector[512];

// local copy of ATA status register
static u8 status_reg;

//
//Standard ATA commands
//
#define CMD_IDENTIFY                0xec
#define CMD_READ                    0x20
#define CMD_WRITE                   0x30

//
//IDE status bits
//
#define IDE_BSY		7
#define IDE_DRDY	6
#define IDE_DF      5
#define IDE_DRQ		3
#define IDE_ERR		0

#define BUSY_TIMEOUT 1000

//
//ATA registers
//
#define REG_STATUS 7
#define REG_HEAD 6
#define REG_CYLHI 5
#define REG_CYLLO 4
#define REG_SECNO 3
#define REG_SECCNT 2
#define REG_COMMAND 7
#define REG_DATA 0

// Wait for IDE controller to be idle
bool ide_WaitBusy(void)
{
	u16 to=0;

	status_reg=inb(REG_STATUS);

	while(status_reg & (1<<IDE_BSY))
	{
		delay_ms(1);
		if(++to > BUSY_TIMEOUT)
			return FALSE;

		status_reg=inb(REG_STATUS);
	}
	return TRUE;
}

// Initialise the IDE controller. Return FALSE if not found
bool ide_Initialise(void)
{

	// Set Dev, Send Identify command (nb MASTER)
	outb(REG_HEAD,  0xa0);
	outb(REG_COMMAND, CMD_IDENTIFY);

	if(FALSE==ide_WaitBusy())
		return FALSE;

	if(status_reg & (1<<IDE_DRQ))
	{
		insw(REG_DATA,(u16 *)sector,256);

		return TRUE;
	}

	return FALSE;
}

// Read a sector into the buffer

bool ide_SectorRead(u32 lba)
{
	if(FALSE==ide_WaitBusy())
		return FALSE;

	outb(REG_HEAD, 0xe0 | ((u8)(lba>>24)&0xf));
	outb(REG_CYLHI, (u8)(lba >> 16));
	outb(REG_CYLLO, (u8)(lba >> 8));
	outb(REG_SECNO, (u8)lba );
	outb(REG_SECCNT, 1);
	outb(REG_COMMAND, CMD_READ );

	if(FALSE==ide_WaitBusy())
		return FALSE;

	if(!(status_reg & (1<<IDE_DRQ)))
		return FALSE;

	insw(REG_DATA,(u16 *)sector,256);

	return TRUE;
}

// Write a sector

bool ide_SectorWrite(u32 lba)
{
	if(FALSE==ide_WaitBusy())
		return FALSE;

	outb(REG_HEAD, 0xe0 | ((u8)(lba>>24)&0xF));
	outb(REG_CYLHI, (u8)(lba >> 16));
	outb(REG_CYLLO, (u8)(lba >> 8));
	outb(REG_SECNO, (u8)lba );
	outb(REG_SECCNT, 1);
	outb(REG_COMMAND, CMD_WRITE );

	if(FALSE==ide_WaitBusy())
		return FALSE;

	if(!(status_reg & (1<<IDE_DRQ)))
		return FALSE;

	outsw(REG_DATA,(u16 *)sector,256);

	if(FALSE==ide_WaitBusy())
		return FALSE;

	if(status_reg & (1<<IDE_ERR))
		return FALSE;
	else
		return TRUE;
}

