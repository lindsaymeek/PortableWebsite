
/*

Very basic functional PPP client and dialup chat script

*/

#include "uart.h"
#include "kernel.h"
#include "config.h"
#include "ppp.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Defines for Internet constants

#define	REQ	 1 	// Request options list for PPP negotiations
#define	ACK	 2	// Acknowledge options list for PPP negotiations
#define	NAK	 3	// Not acknowledged options list for PPP negotiations
#define	REJ	 4	// Reject options list for PPP negotiations
#define	TERM 5	// Termination packet for LCP to close connection
#define	IP		0x0021	// Internet Protocol packet
#define	IPCP	0x8021	// Internet Protocol Configure Protocol packet
#define	CCP		0x80FD	// Compression Configure Protocol packet
#define	LCP		0xC021	// Link Configure Protocol packet
#define	PAP		0xC023	// Password Authenication Protocol packet

u8 addr1, addr2, addr3, addr4;			// Assigned IP address
static u16 rx_ptr, tx_ptr, tx_end;		// pointers into buffers
static u16 checksum1, checksum2;		// Rx and Tx checksums
static u8 number;						// Unique packet id
static u16 packet;						// Type of the last received packet, reused as temp
static u8 state ;						// PPP negotiation state, from dialing=0 to done=6
static u8 lcp_term;						// Set when remote end requested a terminate
static time_t TIME;						// Time stamp
static bool rx_extended,tx_extended;	// PPP Escape processing flags

#define MaxTx 1600						// More buffers
#define MaxRx 256
u8 tx_str[MaxTx];
u8 rx_str[MaxRx];

// Add next character to the CRC checksum for PPP packets
static u16 UpdateCRC(u16 c) 
{
   char i;				// Just a loop index

   c &= 0xFF;				// Only calculate CRC on low byte

   for (i=0;i<8;i++) 
   {		// Loop eight times, once for each bit
      if (c&1) 
	  {			// Is bit high?
         c >>= 1;			// Position for next bit      
         c ^= 0x8408;		// Toggle the feedback bits
      } 
	  else 
	  	c >>= 1;		// Just position for next bit
   }					
   return c;				// Return the 16 bit checksum
}

// Add character to the new packet
static void Insert(u8 c) 
{
   checksum2 = UpdateCRC(c^checksum2) ^ (checksum2>>8); // Add CRC from this char to running total
   tx_str[tx_ptr++] = c;	// Store character in the transmit buffer & advance pointer
}

// Create ICMP/IP packet 
//   length is the size of the packet body
//   *str is the packet data to be added after the header
//   returns the packet as a string in tx_str
static void FrameICMPPacket(u8 *str) 
{
   u16 t,length;		// Just a dual use temp variable or two

   tx_ptr = 1;			// Point to second character in transmit buffer
   tx_str[0] = ' ';			// Set first character to a space for now
   checksum2 = 0xFFFF;		// Initialize checksum
   Insert(0xFF);				// Insert PPP header OxFF
   Insert(3);					// Insert PPP header 0x03
   Insert(IP>>8);				// Insert high byte of protocol field
   Insert(IP&255);			// Insert low byte of protocol field
					
   Insert(0x45);			// Insert header version and length
   Insert(0);				// Insert type of service
   Insert(0);				// Insert total packet length high byte
   Insert(*str+12);			// Insert total packet length low byte
   Insert(0x88);			// Insert identification high byte
   Insert(0x10);			// Insert identification low byte
   Insert(0x40);			// Insert flags and fragement offset
   Insert(0);				// Insert rest of fragment offset
   Insert(127);				// Insert time to live countdown
   Insert(1);				// insert the protocol field

   length = 0x45+0x88+0x40+127+addr1+addr3+str[1]+str[3];  // high byte checksum
   t = *str + 12 + 0x10 + 1 + addr2 + addr4 + str[2] + str[4];  
							// low byte checksum
   t += length>>8;				// make 1's complement
   length = (length&255) + (t>>8);	// by adding low carry to high byte
   t = (t&255) + (length>>8);	// and adding high carry to low byte
   length += t>>8;			// fix new adding carries
   Insert(~length);			// Insert 1's complement checksum high byte
   Insert(~t);			// Insert 1's complement checksum low byte
   Insert(addr1);			// Insert the 4 bytes of this login's IP address
   Insert(addr2);
   Insert(addr3);
   Insert(addr4);
   length = *str - 4;		// save the number of following data bytes
   str++;				// point to the first data byte
 
   while (length) 
   {			// copy the whole string into packet
      length--;			// decrement packet length
      Insert(*str++);			// add current character to packet, point to next character
   }
   length = ~checksum2;		// invert the checksum
   Insert(length&255);			// Insert checksum msb
   Insert(length>>8);			// Insert checksum lsb
   tx_end=tx_ptr;			// Set end of buffer marker to end of packet
   tx_ptr = 0;			// Point to the beginning of the packet
}

// Create packet of type, code, length, and data string specified
//   packet_type is the type, like LCP 
//   code is the LCP type of packet like REQ, not used for IP packets
//   num is the packet ID for LCP, or the IP data type for IP packets
//   length is the size of the packet body
//   *str is the packet data to be added after the header
//   returns the packet as a string in tx_str
static void FramePPPPacket(u16 packet_type, 
					u8 code, 
					u8 num, 
					u8 *str) 
{
   u16 length;		// Just a dual use temp variable
   tx_ptr = 1;			// Point to second character in transmit buffer
   tx_str[0] = ' ';			// Set first character to a space for now
   checksum2 = 0xFFFF;		// Initialize checksum
   Insert(0xFF);				// Insert PPP header OxFF
   Insert(3);					// Insert PPP header 0x03
   Insert(packet_type>>8);		// Insert high byte of protocol field
   Insert(packet_type&255);	// Insert low byte of protocol field
  
   Insert(code);			// Insert packet type, like REQ or NAK
   Insert(num);			// Insert packet ID number
   Insert(0);				// Insert most significant byte of length
   length = *str - 3;		// point to the first data byte

   while (length) 
   {			// copy the whole string into packet
      length--;			// decrement packet length
      Insert(*str++);			// add current character to packet, point to next character
   }
   length = ~checksum2;		// invert the checksum
   Insert(length&255);			// Insert checksum msb
   Insert(length>>8);			// Insert checksum lsb
   tx_end=tx_ptr;			// Set end of buffer marker to end of packet
   tx_ptr = 0;			// Point to the beginning of the packet
}

// Test the option list in packet for valid passwords
//   option is the 16 bit field, where a high accepts the option one greater than the
//   bit #
//   returns 2 for LCP NAK, 1 is only correct fields found, and zero means bad options
//   return also modifies RX_STR to list unacceptable options if NAK or REJ required
static u8 TestOptionsList(u16 option)
{
   u16 size;		// size is length of option string
   unsigned ptr1 = 8,		// ptr1 points data insert location
            ptr2 = 8;		// ptr2 points to data origin
   char pass = 3;			// pass is the return value

   size = rx_str[7]+4;		// size if length of packet
   if (size>MaxRx) size=MaxRx;	// truncate packet if larger than buffer
   while (ptr1<size) 
   {		// scan options in receiver buffer
      if (rx_str[ptr1]==3 && rx_str[ptr1+2]!=0x80  && rx_str[2]==0xc2)
         pass&=0xfd;		// found a CHAP request, mark for NAK
      if (!((1<<(rx_str[ptr1]-1))&option))
         pass=0;			// found illegal options, mark for REJ
      ptr1 += rx_str[ptr1+1];	// point to start of next option
   }
   if (!(pass&2)) 
   {			// If marked for NAK or REJ
      if (pass&1) 
	  {			// save state for NAK
         option=0xfffb;
      }
      for (ptr1=8; ptr1<size;) 
	  {
         if (!((1<<(rx_str[ptr1]-1))&option)) 
		 {	// if illegal option
            for (pass=rx_str[ptr1+1]; ptr1<size && pass; ptr1++) 
			{  // move option
               rx_str[ptr2]=rx_str[ptr1];		// move current byte to new storage
               ptr2++;					// increment storage pointer
               pass--;					// decrement number of characters
            }
         } 
		 else 
		 {
            ptr1+=rx_str[ptr1+1];				// point to next option
         }
      }
      rx_str[7] = ptr2-4;			// save new option string length
      pass=0;					// restore state for REJ
      if (option==0xfffb) pass=1;		// restore state for NAK
   }
   return pass;
}

// Send a string and loop until wait string arrives or it times out
//  send is the string to transmit
//  wait is the string to wait for
//  timeout is in milliseconds/10
//  returns 0 if timeout, returns 1 if wait string is matched
static char SendAndWait(const u8 *send, const char *wait, u16 timeout) 
{

	char i1,i3,i2;

   i2=i3=0;

   // loop until time runs out
   for (mark(TIME); elapsed(TIME)<timeout; ) 
   {		
   	  // is there an incoming character 	
      if (kbhit()) 
	  {		
	  	 // get character	
         i1 = getch();			

         if (toupper(wait[i2])==toupper(i1)) 
		 	i2++;	// does char match wait string
         else 
		 	i2=0;				// otherwise reset match pointer

         if (!wait[i2]) 
		 	return 1;		// finished if string matches
      } 
	  else if (send[i3]) 
	  {  // if char to send and Tx ready
         if (send[i3]=='|') 
		 {	// if pause character
            if (elapsed(TIME)>HZ) 
			{			// has 1 second expired yet?
               mark(TIME);		// if yes clear timer
               i3++;				// and point to next character
            }
         } 
		 else 
		 {
            mark(TIME);		// clear timer, timeout starts after last char
            putch(send[i3++]);	// send the character
           						// point to next char in tx string
         }

         if (!send[i3] && !(*wait))
            return 1; 			// done if end of string and no wait string
      }
   }
   return 0;					// return with 0 to indicate timeout
}

//
// Poll transmit buffer and perform PPP escape encapsulation
//
static void TxPoll(void)
{
	char c;							// serial character temp

	  if (tx_end) 					// Data to send?
	  { 
         c = tx_str[tx_ptr];		// get character from buffer
         if (tx_ptr==tx_end) 
		 {							// was it the last character
            tx_end=0;				// mark buffer empty
            c='~';					// send tilde character last
            
         } 
		 else if (tx_extended) 
		 {							// sending escape sequence?
            c^=0x20;				// yes then convert character
            tx_extended = FALSE;	// clear Tx escape flag
            tx_ptr++;				// point to next char
         } 
		 else if (c<0x20 || c==0x7D || c==0x7E) 
		 { 								// if escape sequence required?
            tx_extended = TRUE;			// set Tx escape flag
            c = 0x7D;					// send escape character
         } 
		 else 
		 {
           if (!tx_ptr) 
		   		c='~';			// send ~ if first character of packet
           tx_ptr++;
         }
         putch(c);				// Put character in transmitter
      }
}

//
// Handle incoming LCP packet
//
static void ProcessLCP(u8 type)
{
	u8 c;

         switch (type) 
		 {			// Switch on packet type
		 	default:
				break;
            case REQ:
               state &= 0xfd;			// clear remote ready state bit
               if (c=TestOptionsList(0x0047))
			   {	// is option request list OK?
                  if (c>1) 
				  {
                     c = ACK;			// ACK packet
                     if (state<3) state |= 2;	// set remote ready state bit
					 debug("LCP ACK");
                  } 
				  else 
				  {
                     rx_str[10]=0xc0;		// else NAK password authentication
                     c = NAK;
					 debug("LCP NAK");
                  }
               } 
			   else 
			   {				// else REJ bad options
                  c = REJ;
				  debug("LCP REJ");
               }
               mark(TIME);
               FramePPPPacket(LCP,c,rx_str[5],rx_str+7); // create LCP packet from Rx buffer
               break;
            case ACK:
               if (rx_str[5]!=number) 
			   	break;	// does reply id match the request..ignore it
			
               if (state<3) 
			   	state |= 1;		// Set the local ready flag
               break;
            case NAK:
               state &= 0xfe;			// Clear the local ready flag
               break;
            case REJ:
               state &= 0xfe;			// Clear the local ready flag
               break;
            case TERM:
				lcp_term=1;				// Remote end requested a terminate
               break;
         }
         if (state==3) 
			state = 4;		// When both ends ready, go to state 4
}

//
// Handle incoming PAP packet
//
static void ProcessPAP(u8 type)
{
         switch (type) 
		 {			// Switch on packet type
		 	default:
				break;
            case REQ:			
			   debug("PAP REQ");	
               break;				// Ignore incoming PAP REQ
            case ACK:
               state = 5;			// PAP ack means this state is done
			   debug("PAP OK");
               break;
            case NAK:
			   debug("PAP NAK");
               break;				// Ignore incoming PAP NAK
         }
}

//
// Handle incoming IPCP packet
//
static void ProcessIPCP(u8 type)
{
	u8 c;

         switch (type) 
		 {			// Switch on packet type
		 	default:
				break;
            case REQ:
               if (TestOptionsList(0x0004)) 
			   {						// move to next state on ACK
                  c = ACK;
                  state = 6;
				  debug("IPCP ACK");
               } 
			   else 
			   {						// otherwise reject bad options
                  c = REJ;
				  debug("IPCP NAK");
               }
               FramePPPPacket(IPCP,c,rx_str[5],rx_str+7);
               break; 				// Create IPCP packet from Rx buffer
            case ACK:
               if (rx_str[5]==number) 
			   {		// If IPCP response id matches request id
                  state = 7;			// Move into final state
                  debug("PPP OK");
               }
               break;
            case NAK:   				// This is where we get our address
               addr1 = rx_str[10];
               addr2 = rx_str[11];   		// Store address for use in IP packets 
               addr3 = rx_str[12];
               addr4 = rx_str[13];
               FramePPPPacket(IPCP,REQ,rx_str[5],rx_str+7);
			   debug("GOT IP");
               break; 				// Make IPCP packet from Rx buffer
            case REJ:
               break;				// Ignore incoming IPCP REJ
            case TERM:
               break;				// Ignore incoming IPCP TERM
         }
}

//
// Process incoming IP packet
//
static bool ProcessIP(void)
{
         if (state<7 || (rx_str[19]==addr4 && rx_str[18]==addr3 && 
                         rx_str[17]==addr2 && rx_str[16]==addr1)) 
		{
                    // ignore echoed packets from our address or before we reach state 7
                    // may power down here because echos are good indications that modem
                    // connection hungup
                    // This would be a good place to insert a traceroute test and 
                    // response
         } 
		 else 
		 {
		  // ICMP payload
		  if (rx_str[13]==1) 
		  {   		
		    // Received PING request
            if (rx_str[24]==8) 
			{		
			   // Copy 4 origin address bytes to destination address	
               rx_str[20]=rx_str[16];		
               rx_str[21]=rx_str[17];
               rx_str[22]=rx_str[18];
               rx_str[23]=rx_str[19];
               rx_str[19]=16;	// Length of IP address(4) + ping protocol(8) + 4
               rx_str[24]=0;	// Change received ping request(8) to ping reply(0)
               packet = rx_str[28]+rx_str[30];	// Calculate 1's comp checksum
               rx_str[26] = packet&255;
               rx_str[27] = packet>>8;
               packet = rx_str[27]+rx_str[29]+rx_str[31];
               rx_str[27] = packet&255;
               packet = (packet>>8) + rx_str[26];
               rx_str[26] = packet&255;
               rx_str[27] += packet>>8;
               rx_str[26] = ~rx_str[26];	// Invert the checksum bits
               rx_str[27] = ~rx_str[27];
               FrameICMPPacket(rx_str+19);	// Make ICMP packet from modified Rx buffer
			   debug("PING IN");
            } 
			else if (rx_str[24]==0) 
			{ 	// Received PING reply to our keep alive request
               if ((rx_str[28]|rx_str[30]|rx_str[31])+rx_str[29]==1)
                  { 		
					debug("PONG");
				  }
            }
		 } // ICMP 
 	     else
		 {
		 	 // handle incoming IP packet
			 packet=0;
			 return TRUE;
		 }
		}
		return FALSE;
}

//
// Process incoming CCP packet
//
static void ProcessCCP(u8 type)
{
	u8 c;

         switch (type) 
		 {		// If CCP response id matches request id
		 	default:
				break;

            case REQ:
               
               if (TestOptionsList(0x0004)) 
			   {
			   	    debug("CCP ACK");
			   		c = ACK; // ACK option 3 only, REJ anything else
				}
			  else
			  	{
					debug("CCP REJ");
			  	    c = REJ;
				}

                FramePPPPacket(CCP,c,rx_str[5],rx_str+7);	// Create CCP ACK or REJ packet
				break;

 								// from Rx buffer
         }
}

//
// Process periodic transmit requests
//
static void ProcessREQUESTS(void)
{
	switch(state) 
	{
		case 0:
		case 2:

	  		if(elapsed(TIME)>HZ) 
	  		{ 
 									// Once every few seconds try negotiating LCP
         		number++;					// Increment Id to make packets unique
         		mark(TIME);				// Reset timer
         		FramePPPPacket(LCP,REQ,number,"\x10\x02\x06\x00\x0A\x00\x00\x07\x02\x01\x04\x00\x80"); //\x08\x02"); 
		 		debug("LCP REQ");
 							// Request LCP options 1 (MRU 128), 2 (Escape codes),7 (pcomp) //,8 (accomp)
      		} 
			break;

		case 4:

	  		if (elapsed(TIME)>HZ) 
	  		{	
 										// Once a second try negotiating password
         		mark(TIME);				// Reset timer
         		number++;

		 		sprintf(buffer,"%c%c%s%c%s",
		    		strlen(config.isp_username)+strlen(config.isp_password)+6,
		 			strlen(config.isp_username),config.isp_username,
					strlen(config.isp_password),config.isp_password);
			

		         FramePPPPacket(PAP,REQ,number,buffer); 
	
				 debug("PAP REQ");
      		}	 
			break;

		case 6:

	  		if (elapsed(TIME)>HZ) 
	  		{
 										// Once a second try negotiating IPCP
         		number++;				// Increment Id to make packets unique
         		mark(TIME);				// Reset timer
         		FramePPPPacket(IPCP,REQ,number,"\xA\x3\x6\x0\x0\x0\x0");
 										// Request IPCP option 3 with addr 0.0.0.0
		 		debug("IPCP REQ");
      		} 
			break;

		case 7:

	 		if (elapsed(TIME)>30*HZ) 
	  		{ // Every 30 seconds do a ping to keep the ISP connection alive
         		mark(TIME);				// Reset timer
         		number++;					// Increment ping count

		 		sprintf(buffer,"\x10%c%c%c%c",
		 					config.dyndns_server[0],
		 					config.dyndns_server[1],
		 					config.dyndns_server[2],
		 					config.dyndns_server[3]);
		 		memcpy(buffer+5,"\x8\x0\xF7\xFE\x0\x1\x0\x0",8);

         		FrameICMPPacket(buffer); 
		 		debug("PING");
 						
      		}
			break;
	}
}

//
// Poll the incoming PPP interface, de-encapsulate and do CRC checks
//
static void RxPoll(void)
{
	int c;

      if (kbhit()) 
	  {								// Incoming character?
         c = getch();				// get the character
         
         if (c == 0x7E) 
		 {				// start or end of a packet 
            if (rx_ptr && (checksum1==0xF0B8))
               packet = (rx_str[2]<<8) + rx_str[3]; // if CRC passes accept packet
            rx_extended =FALSE;			// clear escape character flag
            rx_ptr = 0;					// get ready for next packet
            checksum1 = 0xFFFF;			// start new checksum
         } 
		 else if (c == 0x7D) 
		 {		// if tilde character set escape flag
            rx_extended = TRUE;
         } 
		 else 
		 {
            if (rx_extended) 
			{			// if escape flag
               c ^= 0x20;				// recover next character
               rx_extended = FALSE;		// clear Rx escape flag
            }
            if (rx_ptr==0 && c!=0xff) 
				rx_str[rx_ptr++] = 0xff; // uncompress PPP header
            if (rx_ptr==1 && c!=3) 
				rx_str[rx_ptr++] = 3;
            if (rx_ptr==2 && (c&1)) 
				rx_str[rx_ptr++] = 0;
            rx_str[rx_ptr++] = c;			// insert character in buffer
            if (rx_ptr>MaxRx) 
				rx_ptr = MaxRx;	// Inc pointer up to end of buffer
            checksum1 = UpdateCRC(c^checksum1) ^ (checksum1>>8); // calculate CRC checksum
         }
         
      } 
}

//
// Poll the PPP interface, handle PPP encapsulation 
//
// returns 1 if an incoming 
//
bool PPPPoll(void)
{
  
	  RxPoll();

	  TxPoll();

	  switch(packet) 
	  {

	  	case LCP:
		 ProcessLCP(rx_str[4]);
		break;

		case PAP:
		 ProcessPAP(rx_str[4]);
 	    break;

		case IPCP:
		 ProcessIPCP(rx_str[4]);
	    break;

		 break;

		case IP:
		  if(TRUE==ProcessIP())
		  	return TRUE;
        break;

		break;

		case CCP:
		 ProcessCCP(rx_str[4]);
        break;

		 default:
			// Ignore any other received packet types
	  		debug("ID?%x",packet);
			break;
      } 

	if(!tx_end)
	  ProcessREQUESTS();

	  // Indicate that packet is processed
      packet = 0;					

	  return 0;
}

//
// The login script
//
int PPPInit(int dial) 
{
	time_t config_time;

	if (dial)
	{
     // Redial forever every 30 seconds
     for(;;) 
     {		
		debug("InitDial\n\r");

      if(!SendAndWait("+++|ath\r|atz\r|atdt","atdt",3000))	// Init modem
         return 0;

	  debug("Dialing\n\r");

	  // Create ISP phone number string
	  strcpy(buffer,config.isp_phone);
	  strcat(buffer,"\r");
	  
      if (SendAndWait(buffer,"nnect",3000)) 
	  {
      	debug("Connect\n\r");
		break;
	  } 
    
     } // for loop
    } // if dial

	// initialise ppp state variables
	tx_ptr=tx_end=0;
	state=0;
	packet=0;
	tx_extended=FALSE;
	rx_extended=FALSE;
	lcp_term=0;
	mark(TIME);

	mark(config_time);

	// wait 30 seconds to connect
	while(elapsed(config_time) < 30*HZ && !lcp_term && state!=7)
	{
		PPPPoll();
	}

	// return true if PPP completed configuration
	if(state==7)
		return 1;
	else 
		return 0;
}

// Interface to TinyTCP stack

/* ----- Format an ethernet header in the transmit buffer ----------- */
	/* return pointer to where to build the IP message */

u8 * sed_FormatPacket( u8 *destEAddr, int ethType )
{
	if(ethType!=0x800)
		return NULL;

	// flush out tx buffer if PPP is trying to send something
	while(tx_end)
		TxPoll();

 	tx_ptr = 1;				// Point to second character in transmit buffer (ring buffer mgmt)
   	tx_str[0] = ' ';		// Set first character to a space for now
   	checksum2 = 0xFFFF;		// Initialize checksum
   	Insert(0xFF);			// Insert PPP header OxFF
   	Insert(3);				// Insert PPP header 0x03
   	Insert(IP>>8);			// Insert high byte of protocol field
   	Insert(IP&255);			// Insert low byte of protocol field	

	return tx_str+tx_ptr;	// step past PPP header
}

/* ----- send a packet on the link ---------------------------------- */

int sed_Send( int length )   
{
	u16 cs;

	// update CRC and advance pointer to end of newly created packet
	while(length-- > 0)
		Insert(tx_str[tx_ptr]);

   cs = ~checksum2;			// invert the checksum
   Insert(cs&255);			// Insert checksum msb
   Insert(cs>>8);			// Insert checksum lsb
   tx_end=tx_ptr;			// Set end of buffer marker to end of packet
   tx_ptr=0;				// Point to the beginning of the packet

	// flush out tx buffer if PPP is trying to send something
	while(tx_end)
		TxPoll();

	return 1;
}

/* ----- prepare to receive packets --------------------------------- */

	/* If the argument is zero, enable both buffers.
		If the argument is nonzero,
		take it as the address of the buffer to be enabled.
		(i.e. that message has been processed) */

int sed_Receive( u8 *recBufLocation ) 
{
	/* This call is ignored. */
	return 1;
}

/* ----- check for packet received ---------------------------------- */

	/* Test for the arrival of a packet. The location of the packet is
		returned. If no packet is returned, the routine returns
		zero. (Timeout is irrelevant here.) */

u8 * sed_IsPacket(void) 
{
	// busy wait here until we get an ip packet or the link drops
	if(!lcp_term && PPPPoll())
	{
		// first byte past protocol field (IP)
		return &rx_str[4];	
	}
	else
		return NULL; // indicate no packet has been received

}

/* ----- check received packet for ethernet type -------------------- */

int sed_CheckPacket( u16 *recBufLocation, u16 expectedType )
{
	return 1;			/* IsPacket will only return IP packets so this is always true */
}