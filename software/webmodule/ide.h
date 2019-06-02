
#ifndef _IDE_H
#define _IDE_H

#include "kernel.h"

bool ide_Initialise(void);

extern u8 sector[512];

bool ide_SectorWrite(u32 lba);

bool ide_SectorRead(u32 lba);

#endif

