;*******************************************************************************
;
; C Complier for M32C
; COPYRIGHT(C) 1999(2000-2002) RENESAS TECHNOLOGY CORPORATION
; AND RENESAS SOLUTIONS CORPORATION ALL RIGHTS RESERVED
;
;      sect308_32Cskp.inc     : section definitions and vector table init
;    
;
;     
;******************************************************************************
;Revision History
;1.0  Modified from standard for M32C/83 and 85 SKPs


;---------------------------------------------------------------------
; HEAP SIZE definition
;---------------------------------------------------------------------
.if __HEAP__ == 1		 
HEAPSIZE	.equ	0h
.else

.if __HEAPSIZE__ == 0
HEAPSIZE .equ 300H
.else				
HEAPSIZE	.equ	__HEAPSIZE__
.endif

.endif

;---------------------------------------------------------------------
; STACK SIZE definition
;---------------------------------------------------------------------
.if __USTACKSIZE__ == 0
STACKSIZE	.equ	0h

.else				
STACKSIZE	.equ	__USTACKSIZE__

.endif



;---------------------------------------------------------------------
; INTERRUPT STACK SIZE definition
;---------------------------------------------------------------------
.if __ISTACKSIZE__ == 0
ISTACKSIZE	.equ	300h

.else				
ISTACKSIZE	.equ	__ISTACKSIZE__

.endif

;---------------------------------------------------------------------
; INTERRUPT VECTOR ADDRESS  definition
; Do not set to an area that conflicts with the ROM monitor which start
;at 0fff900h.  Size of the vector table is 256 bytes
;---------------------------------------------------------------------
VECTOR_ADR 	.equ	0fff800h


; Initialize Macro declaration
;---------------------------------------------------------------------
;
; when copy less 64K byte
BZERO	.macro	TOP_ ,SECT_
	mov.b	#00H, R0L
	mov.l	#TOP_, A1
	mov.w	#sizeof SECT_ , R3
	sstr.b	
	.endm

BCOPY .macro	FROM_,TO_,SECT_
	mov.l	#FROM_ ,A0
	mov.l	#TO_ ,A1
	mov.w	#sizeof SECT_ , R3
	smovf.b
	.endm


HEAPINIT .macro
 .glb	__mbase
	.glb	__mnext
	.glb	__msize
	mov.l	#(heap_top&0FFFFFFH), __mbase
	mov.l	#(heap_top&0FFFFFFH), __mnext
	mov.l	#(HEAPSIZE&0FFFFFFH), __msize
  .endm

;---------------------------------------------------------------
; special page vectors.  This macro puts the jump address of 
; functions defined as special page into the special page vector
; table.  See example call below.  See the M16C Software Manual and
; the NC308 manual for more information on special page vectors
;---------------------------------------------------------------
;       macro define for special page
;
;Format:
;	SPECIAL	number
;

SPECIAL	.macro	NUM
	.org	0FFFFFEH-(NUM*2)
	.glb	__SPECIAL_@NUM
	.word	__SPECIAL_@NUM  & 0FFFFH
.endm



;---------------------------------------------------------------
;
; 	Arrangement of section
;
;---------------------------------------------------------------
; Near RAM data area
;---------------------------------------------------------------
; SBDATA area
	.section	data_SE,DATA
	.org	400H
data_SE_top:

	.section	bss_SE,DATA,ALIGN
bss_SE_top:

	.section	data_SO,DATA
data_SO_top:

	.section	bss_SO,DATA
bss_SO_top:

; near RAM area
	.section	data_NE,DATA,ALIGN
data_NE_top:

	.section	bss_NE,DATA,ALIGN
bss_NE_top:

	.section	data_NO,DATA
data_NO_top:

	.section	bss_NO,DATA
bss_NO_top:

	.section	my_program_ram,CODE,ALIGN
my_program_ram:

;---------------------------------------------------------------
; Stack area
;---------------------------------------------------------------
	.section	stack,DATA,ALIGN
	.blkb	STACKSIZE
	.align
stack_top:

	.blkb	ISTACKSIZE
	.align
istack_top:


;---------------------------------------------------------------
;	heap section
;---------------------------------------------------------------
	.section	heap,DATA
heap_top:
	.blkb	HEAPSIZE

;---------------------------------------------------------------
; Near ROM data area
;---------------------------------------------------------------
	.section	rom_NE,ROMDATA,ALIGN
rom_NE_top:

	.section	rom_NO,ROMDATA
rom_NO_top:

	.section	data_FE,DATA
	.org		20000H
data_FE_top:

	.section	bss_FE,DATA,ALIGN
bss_FE_top:
 
	.section	data_FO,DATA
data_FO_top:
 
	.section	bss_FO,DATA
bss_FO_top:


;---------------------------------------------------------------
; Far ROM data area
;---------------------------------------------------------------
	.section	rom_FE,ROMDATA
	.org		0FA0000H
rom_FE_top:

	.section	rom_FO,ROMDATA
rom_FO_top:

;---------------------------------------------------------------
; Initial data of 'data' section
;---------------------------------------------------------------
	.section	data_SEI,ROMDATA
data_SEI_top:

	.section	data_SOI,ROMDATA
data_SOI_top:

	.section	data_NEI,ROMDATA
data_NEI_top:
 
	.section	data_NOI,ROMDATA
data_NOI_top:
 
	.section	data_FEI,ROMDATA
data_FEI_top:
 
	.section	data_FOI,ROMDATA
data_FOI_top:

;---------------------------------------------------------------
; code area
;---------------------------------------------------------------
	.section	interrupt,ALIGN

	.section	program,ALIGN
	;.org  ;special page code must be set internal ROM area
   	.section	program_S

.if	__MVT__ == 0
;---------------------------------------------------------------
; variable vector section
;---------------------------------------------------------------
	.section	vector		; variable vector table
	.org	VECTOR_ADR 

	.lword	dummy_int		; BRK (software int 0)
	.lword	dummy_int		; reserved
	.lword	dummy_int		; reserved
	.lword	dummy_int		; reserved
	.lword	dummy_int		; reserved
	.lword	dummy_int		; reserved
	.lword	dummy_int		; reserved
	.lword	dummy_int		; A-D1 (software int 7)	  
	.lword	dummy_int		; DMA0 (software int 8)
	.lword	dummy_int 		; DMA1 (software int 9)
	.lword	dummy_int 		; DMA2 (software int 10)
	.lword	dummy_int 		; DMA3 (software int 11)
	.glb	_ta0_irq
	.lword	_ta0_irq		; TIMER A0 (software int 12)
	.lword	dummy_int		; TIMER A1 (software int 13)
  	.lword	dummy_int		; TIMER A2 (software int 14)
	.lword	dummy_int		; TIMER A3 (software int 15)
	.lword	dummy_int		; TIMER A4 (software int 16)
	.lword	dummy_int		; uart0 transmit,NACK (software int 17)
	.glb	_U0rec_ISR
	.lword	_U0rec_ISR		; uart0 receive,ACK (software int 18)
	.lword	dummy_int		; uart1 transmit,NACK (software int 19)
	.lword	dummy_int		; uart1 receive,ACK (software int 20)
	.lword	dummy_int		; TIMER B0 (software int 21)
	.lword	dummy_int		; TIMER B1 (software int 22)
	.lword	dummy_int		; TIMER B2 (software int 23)
	.lword	dummy_int		; TIMER B3 (software int 24)
	.lword	dummy_int		; TIMER B4 (software int 25)
	.lword	dummy_int		; INT5 (software int 26)
	.lword	dummy_int		; INT4 (software int 27)
	.lword	dummy_int		; INT3 (software int 28)
	.lword	dummy_int	    ; INT2 (software int 29)
 	.lword	dummy_int		; INT1 (software int 30)
 	.lword	dummy_int		; INT0 (software int 31)
	.lword	dummy_int		; TIMER B5 (software int 32)
	.lword	dummy_int		; uart2 transmit/NACK (software int 33)
	.lword	dummy_int		; uart2 receive/ACK (software int 34)
	.lword	dummy_int		; uart3 transmit/NACK (software int 35)
	.lword	dummy_int		; uart3 receive/ACK (software int 36)
	.lword	dummy_int		; uart4 transmit/NACK (software int 37)
	.lword	dummy_int		; uart4 receive/ACK (software int 38)
	.lword	dummy_int		; uart2 bus collision (software int 39)
	.lword	dummy_int		; uart3 bus collision (software int 40)
	.lword	dummy_int		; uart4 bus collision (software int 41)
	.lword	dummy_int		; A-D0 Convert (software int 42)
	.lword	dummy_int		; key input (software int 43)
	.lword	dummy_int		; IIO 0, CAN3(85 only)  (software int 44)
	.lword	dummy_int		; IIO 1, CAN4 (85 only) (software int 45)
	.lword	dummy_int		; IIO 2 (software int 46)
	.lword	dummy_int		; IIO 3 (software int 47)
	.lword	dummy_int		; IIO 4 (software int 48)
	.lword	dummy_int		; IIO 5(83 only), CAN5(85 only) (software int 49)
	.lword	dummy_int		; IIO 6(83 only) (software int 50)
	.lword	dummy_int		; IIO 7(83 only) (software int 51)
	.lword	dummy_int		; IIO 8 (software int 52)
	.lword	dummy_int		; II0 9,CAN0 (software int 53)
	.lword	dummy_int		; IIO 10, CAN1 (software int 54)
	.lword	dummy_int		; reserved (software int 55)
	.lword	dummy_int		; reserved (software int 56
	.lword	dummy_int		; IIO 11(83 only), CAN2 (software int 57)
	.lword	dummy_int		; software int 58
	.lword	dummy_int		; software int 59
	.lword	dummy_int		; software int 60
	.lword	dummy_int		; software int 61
	.lword	dummy_int		; software int 62
	.lword	dummy_int		; software int 63
.else	; __MVT__
	.section	__NC_rvector,ROMDATA
	.org		VECTOR_ADR
.endif	; __MVT__

.if	__MST__ == 0	
;===============================================================
; special page defination
;---------------------------------------------------------------
;	macro is defined in ncrt0.a30
;	Format: SPECIAL number
;
;---------------------------------------------------------------
;	SPECIAL 255
;	SPECIAL 254
;	SPECIAL 253
; 	   :
;	   :
;	omitted
;	   :
;	   :	
;	SPECIAL 24
;	SPECIAL 23
;	SPECIAL 22
;	SPECIAL 21
;	SPECIAL 20
;	SPECIAL 19
;	SPECIAL 18
;
.else	; __MST__
	.section	__NC_svector,ROMDATA
	.org		SVECTOR_ADR
.endif	; __MST__
;===============================================================
; fixed vector section
;---------------------------------------------------------------
	.section	fvector,ROMDATA
	.org	0FFFFDCh
UDI:
	.lword	dummy_int
OVER_FLOW:
	.lword	dummy_int
BRKI:
	.lword	dummy_int
ADDRESS_MATCH:
	.lword	dummy_int
SINGLE_STEP:
	.lword	dummy_int
WDT:
	.lword	dummy_int
DBC:
	.lword	dummy_int
NMI:
	.lword	dummy_int
RESET:
	.lword	start
;
;******************************************************************
;     C Compiler for M32C/83,85
; COPYRIGHT(C) 1999(2000-2002) RENESAS TECHNOLOGY CORPORATION
; AND RENESAS SOLUTIONS CORPORATION ALL RIGHTS RESERVED
;******************************************************************







 
