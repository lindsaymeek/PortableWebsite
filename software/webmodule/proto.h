/* PROTO.H - function prototypes for tiny-tcp
	931208	rr	orig file
	940424	rr	minor changes

Notes:
	Eco-C doesn't like Void, and having functions predeclared with no type
	makes the compiler lock up.
	Personally, I'd rather have all the functions return success/failure
	anyway.	(941010 rr)
*/

	/* in arp.c */

int sar_CheckPacket P(( struct arp_Header *ap ));
int sar_MapIn2Eth P(( IP_Address ina, struct Ethernet_Address *ethap ));

	/* in sed.c or sedslip.c */

Byte * sed_FormatPacket P(( struct Ethernet_Address *destEAddr, int ethType ));
int sed_Send P(( int pkLengthInBytes ));
int sed_Receive P(( Byte *recBufLocation ));
Byte * sed_IsPacket P(( Void ));
int sed_CheckPacket P(( Word *recBufLocation, Word expectedType ));

	/* in tinytcp.c */

char *print_ip P((IP_Address ip));

Void tcp_Init P(( IP_Address MY_ADDR ));

#ifdef USE_TYPEDEFS
Void tcp_Open P(( struct tcp_Socket *s, Word lport, IP_Address ina,
	Word port, Procref datahandler ));
#else
Void tcp_Open P(( struct tcp_Socket *s, Word lport, IP_Address ina,
	Word port, int ( *datahandler )( void *s, Byte *dp, int len ) ));
#endif

#ifdef USE_TYPEDEFS
Void tcp_Listen P(( struct tcp_Socket *s, Word port, Procref datahandler,
	Longword timeout ));
#else
Void tcp_Listen P(( struct tcp_Socket *s, Word port,
	int ( *datahandler )( void *s, Byte *dp, int len ),
	Longword timeout ));
#endif

Void tcp_Close P(( struct tcp_Socket *s ));
Void tcp_Abort P(( struct tcp_Socket *s ));

#ifdef USE_TYPEDEFS
int tcp P(( Procrefv application ));
#else
int tcp P(( void ( *application )( void ) ));
#endif

int tcp_Write P(( struct tcp_Socket *s, Byte *dp, int len ));

#ifndef ECO
Void tcp_Flush P(( struct tcp_Socket *s ));
#endif

#ifndef BIG_ENDIAN
Word rev_word P(( Word w ));
Longword rev_longword P(( Longword l ));
#endif

	/* redefine move as memcpy */

#define	Move(s,d,n)	memmove(d,s,n)

	/* the following are internal to TCP, but because it was broken
		into chunks, they had to become global */

Void tcp_Unthread P(( struct tcp_Socket *ds ));
Void tcp_Retransmitter P(( Void ));
Void tcp_ProcessData P(( struct tcp_Socket *s,
		struct tcp_Header *tp, int len ));
Void tcp_Send P(( struct tcp_Socket *s ));
Void tcp_DumpHeader P(( struct in_Header *ip,
		struct tcp_Header *tp, char *mesg ));
Void tcp_Handler P(( struct in_Header *ip ));

Word checksum P(( Word *dp, int length ));
Longword lchecksum P(( Word *dp, int length ));

	/* in tinyftp.c */

Void ftp_ctlHandler P(( struct tcp_Socket *s, Byte *dp, int len ));
Void ftp_dataHandler P(( struct tcp_Socket *s, Byte *dp, int len ));
Void ftp_commandLine P(( Void ));
Void ftp_Abort P(( Void ));
Void ftp_application P(( Void ));
Void ftp P(( IP_Address host ));

	/* in tinyft2.c */

Void ftp_server_handler P(( struct tcp_Socket *s, Byte *dp, int len ));

	/* in tinyft4.c */

Void ftp_local_command P(( char *s ));

long MsecClock(void);

void httpd(void);

/* end of proto.h */

