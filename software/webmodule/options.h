/* OPTIONS.H - Compiling options
	940807	rr	orig file
	940925	rr	add Far

Eco-C: 6 character limit; does not have 'unsigned long' or 'unsigned short'.
	No typedef.
	Function predeclarations cause the compiler to crash, even if they
	are ifdef'd out.
	Function calls thru pointers apparently don't work ("illegal
	function call").

BDS C (1.42): Conditional compilation doesn't appear to work; at least, it
	doesn't disable #include directives.
*/

/* ----- Machine type ----------------------------------------------- */

	/* This is used in a few places, e.g. for selecting real-time
		clock logic */

// #define	PC

//#define XEROX820
/* #define KAYPRO */

/* #define	ECO */	/* Eco-C Z-80 compiler */

// #define	Z80

#define M16C

/* ----- configuration ---------------------------------------------- */

	/* define ETHERNET if using Ethernet */

// #define ETHERNET

	/* define BIG_ENDIAN if using Motorola CPU */

/* #define BIG_ENDIAN */

        /* default FTP server */

#define HOST_ADDR       ADDR( 192, 168, 1, 1 )

/* ----- S8 --------------------------------------------------------- */

#ifdef Z80
#define S8	static
#else
#define S8
#endif

/* ----- Prototype -------------------------------------------------- */

	/* uncomment following for compilers allowing prototypes */

#define P(x) x

	/* uncomment following for compilers not allowing prototypes */

/* #define P(x) () */

/* ----- Void ------------------------------------------------------- */

	/* uncomment following for compilers supporting void datatype */

#define Void void

	/* uncomment following for compilers not supporting void datatype */

/* #define Void */

/* ----- Far -------------------------------------------------------- */

#ifdef PC
#define Far far
#else
#define Far
#endif

/* ----- Use typedefs? ---------------------------------------------- */

#define USE_TYPEDEFS

/* end of options.h */
