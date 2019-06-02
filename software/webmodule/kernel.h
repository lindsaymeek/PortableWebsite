
#ifndef _KERNEL_H
#define _KERNEL_H

typedef unsigned long u32;
typedef signed long s32;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned char u8;
typedef signed char s8;
typedef char bool;

#define FALSE 0 
#define TRUE 1

// Temporary string buffer
#define BUFFERSIZE 256
extern char buffer[BUFFERSIZE];

// Endian converters
u16  swap16(u16 x);
u32  swap32(u32 x);

// Timing
void delay_ms(unsigned short ms);

typedef u32 time_t;

extern volatile time_t jiffies;

#define HZ 1000
#define mark(x) (x)=jiffies
#define elapsed(x) (jiffies-(x))

// Compact flash (PCMCIA) I/O

// I/O memory access functions

void insw(u16 addr,u16 *data16,u16 len);
void outsw(u16 addr,u16 *data16,u16 len);

void outb(u16 addr,u8 data);
u8 inb(u16 addr);

u16 inw(u16 addr);
void outw(u16 addr,u16 data);

// Common memory access functions
u16 readw(u16 addr);
void writew(u16 addr,u16 data);

// Attribute memory access function
u8 rd_cf_reg(u16 reg_addr);
void wr_cf_reg(u16 reg_addr,u8 data);

// Reset CF card
void cf_reset(void);

// Config CF card
bool cf_config(void);

// Debugging

int debug(const char _far *fmt, ...);
// lightweight stack version
void debugs(const char *str);

// Change debugging display mode
#define OUTPUT_UART 1
#define OUTPUT_LCD 2
#define OUTPUT_NONE 0
void debug_control(char output);

#endif
