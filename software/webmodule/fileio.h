/* FILEIO.H - definitions for simple file I/O

|===================================================================|
|  My changes can be considered public domain.  Geof's statement    |
|  will cover everything.                                           |
|              - Rick Rodman 09/02/97                               |
|===================================================================|

	940513	rr	initial file
*/

#define	MY_OPEN_READ	0
#define	MY_OPEN_WRITE	1

typedef signed char FH;

FH		my_open ( char *p_filename, int open_mode );
void	my_close ( FH fh );
signed short my_read( FH fh, unsigned char *p_buffer, unsigned short u_bytes );
signed short my_write ( FH fh, unsigned char *p_buffer, unsigned short u_bytes );
unsigned long my_filelength ( FH fh );
int my_gets(FH fh,char *str,int maxlen);

/* end of fileio.h */

