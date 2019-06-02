
/* FILEIO.C - simple file I/O routines for portability
	940513	rr	orig file

The purpose of this routine is portability. All file I/O will be confined
	to this module.
*/

/* ----- include files ---------------------------------------------- */

#include "tinytcp.h"	/* for the P() macro */
#include "fileio.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ffs.h"
#ifdef M16C
#include "kernel.h"
#endif

/* ----- internals -------------------------------------------------- */

static char *fixup_filename P(( char *p_filename ));

/* ----- my_open ---------------------------------------------------- */

FH	my_open( char *p_filename, int open_mode )  {

	u8 p_file;

	/* At this point, we may wish to do some work on the filename. */

	p_filename = fixup_filename( p_filename );

	switch( open_mode ) {

	case MY_OPEN_READ:
		p_file = open( p_filename, O_RDONLY, 0 );	
		return ( FH ) p_file;

	case MY_OPEN_WRITE:
		p_file = open( p_filename, O_WRONLY, S_IREAD | S_IWRITE );	/* Microsoft */
		return ( FH ) p_file;

	default:
		return ( FH ) 0;
	}
}

/* ----- return the filelength --------------------------------------------- */

unsigned long	my_filelength( FH fh )  {

	return filelength(fh);
}

/* ----- close the file --------------------------------------------- */

Void	my_close( FH fh )  {

	close( fh );
}

/* ----- read bytes of file ----------------------------------------- */

signed short my_read( FH fh, unsigned char *p_buffer, unsigned short u_bytes )
 {

	return read( fh, p_buffer, u_bytes ); 
}

/* ----- write bytes of file ---------------------------------------- */

signed short my_write( FH fh,unsigned char *p_buffer, unsigned short u_bytes )
{

	return write( fh, p_buffer, u_bytes );
}

/* ----- internal to fix up filename -------------------------------- */

static char *fixup_filename( char *p_filename )  {

	char *		p;
	char		c;
	static	char	fixed_filename[ 42 ];
	int		dots;

	/* Here, I just move to the character after the last slash
		or forward slash. */

	p = strrchr( p_filename, '/' );
	if( p != ( char * ) 0 ) p_filename = p + 1;

	p = strrchr( p_filename, '\\' );
	if( p != ( char * ) 0 ) p_filename = p + 1;

	/* Force it into 8.3 style, uppercase only, one dot only */

	dots = 0;
	p = &fixed_filename[ 0 ];
	while( *p_filename ) {

		c = *p_filename++;

		if(( c <= ' ' ) || ( c > 126 )) continue;
 
		if(( c >= 'a' ) && ( c <= 'z' )) c -= ( 'a' - 'A' );
		/* else if( c == '/' ) c = '\\'; */
		else if( c == '.' ) {
			++dots;
			if( dots > 1 ) break;
		}

		/* could check for bad characters too. */

		*p++ = c;
	}

	return &fixed_filename[ 0 ];
}

//
// get terminated string 
//
int my_gets(FH fh,char *str,int maxlen)
{
	int i=0;
	char c;

	while(1==my_read(fh,&c,1) && i<maxlen)
	{
		if(c==13)
			break;
		if(isprint(c))
			str[i++]=c;
	}

	str[i]=0;

	return i;
}

/* end of fileio.c */
