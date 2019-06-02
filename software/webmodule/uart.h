
#ifndef UART_H
#define UART_H

/*

Program:	UART serial driver
Version:	8/5/05

*/

void uart_init(long baud);

void putch(int x);

int puts(const char _far *s);

char kbhit(void);

int getch(void);

char check_carrier_detect(void);

void monitor(void);


#endif


