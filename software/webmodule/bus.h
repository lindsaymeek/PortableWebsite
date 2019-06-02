
#ifndef BUS_H
#define BUS_H

// MODE3 EXTERNAL ADDRESS SPACES
#define CS0_ADDR 0xE00000L
#define CS1_ADDR 0x100000L
#define CS2_ADDR 0x200000L
#define CS3_ADDR 0xC00000L

// CS0 is used for the CF card 

#define CF_ADDR CS0_ADDR

// Address offset for attribute memory (A10=0,A11=0)
#define CF_ATTRIB_OFFSET 0x0
// Address offset for common memory (A10=1,A11=0)
#define CF_COMMON_OFFSET 0x400
// Address offset for 16-bit I/O memory (A10=X,A11=1)
#define CF_IO_OFFSET 0x800 

char bus_init(void);

#endif
