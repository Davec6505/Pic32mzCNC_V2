_DMA_global:
;Serial_Dma.c,22 :: 		void DMA_global(){
ADDIU	SP, SP, -4
SW	RA, 0(SP)
;Serial_Dma.c,24 :: 		DMACONSET = 0x8000;
ORI	R2, R0, 32768
SW	R2, Offset(DMACONSET+0)(GP)
;Serial_Dma.c,25 :: 		DMA0();
JAL	_DMA0+0
NOP	
;Serial_Dma.c,26 :: 		DMA1();
JAL	_DMA1+0
NOP	
;Serial_Dma.c,27 :: 		}
L_end_DMA_global:
LW	RA, 0(SP)
ADDIU	SP, SP, 4
JR	RA
NOP	
; end of _DMA_global
_DMA0:
;Serial_Dma.c,40 :: 		void  DMA0(){
;Serial_Dma.c,42 :: 		IEC4CLR      = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IEC4CLR+0)(GP)
;Serial_Dma.c,43 :: 		IFS4CLR      = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,46 :: 		DCH0CONCLR = 0x8003;
ORI	R2, R0, 32771
SW	R2, Offset(DCH0CONCLR+0)(GP)
<<<<<<< HEAD
;Serial_Dma.c,50 :: 		DCH0ECON      =  (113 << 8 ) | 0x30;
ORI	R2, R0, 28976
SW	R2, Offset(DCH0ECON+0)(GP)
;Serial_Dma.c,56 :: 		DCH0DAT       = '\n';//'\0' //0x0A0D;//'\r\n';
ORI	R2, R0, 10
SW	R2, Offset(DCH0DAT+0)(GP)
;Serial_Dma.c,60 :: 		DCH0SSA       = KVA_TO_PA(0xBF822030);    //[0xBF822230 = U2RXREG]
LUI	R2, 8066
ORI	R2, R2, 8240
SW	R2, Offset(DCH0SSA+0)(GP)
;Serial_Dma.c,65 :: 		DCH0SSIZ      = 1;                 // source size = 1byte at a time
ORI	R2, R0, 1
SW	R2, Offset(DCH0SSIZ+0)(GP)
;Serial_Dma.c,68 :: 		DCH0DSA       = KVA_TO_PA(0xA0002000);    // virtual address:= IN RAM FOR RECIEVED DATA
ORI	R2, R0, 8192
SW	R2, Offset(DCH0DSA+0)(GP)
;Serial_Dma.c,69 :: 		DCH0DSIZ      = 200  ;  // destination size = Size for the 'rxBuf' to fill up with received characters. It is = 5 in this example...
ORI	R2, R0, 200
SW	R2, Offset(DCH0DSIZ+0)(GP)
;Serial_Dma.c,72 :: 		DCH0CSIZ      = 1  ;  // bytes transferred in the background
ORI	R2, R0, 1
SW	R2, Offset(DCH0CSIZ+0)(GP)
;Serial_Dma.c,76 :: 		IPC33CLR     = 0x160000;
LUI	R2, 22
SW	R2, Offset(IPC33CLR+0)(GP)
;Serial_Dma.c,79 :: 		DCH0INTCLR    = 0x00FF00FF ;
LUI	R2, 255
ORI	R2, R2, 255
SW	R2, Offset(DCH0INTCLR+0)(GP)
;Serial_Dma.c,82 :: 		IPC33SET      = 0x00140000;
LUI	R2, 20
SW	R2, Offset(IPC33SET+0)(GP)
;Serial_Dma.c,85 :: 		DCH0INTSET      =  0xB0000;
LUI	R2, 11
SW	R2, Offset(DCH0INTSET+0)(GP)
;Serial_Dma.c,88 :: 		IEC4SET       = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IEC4SET+0)(GP)
;Serial_Dma.c,89 :: 		IFS4CLR       = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,92 :: 		DCH0INTCLR    = 0x000000FF ;
ORI	R2, R0, 255
SW	R2, Offset(DCH0INTCLR+0)(GP)
;Serial_Dma.c,96 :: 		DCH0CONSET      = 0X0000513;//013 = 1 char || 813 = 2 char e.g. \r\n
ORI	R2, R0, 1299
SW	R2, Offset(DCH0CONSET+0)(GP)
;Serial_Dma.c,99 :: 		serial.head = serial.tail = serial.diff = 0;
SH	R0, Offset(_serial+504)(GP)
SH	R0, Offset(_serial+502)(GP)
SH	R0, Offset(_serial+500)(GP)
;Serial_Dma.c,100 :: 		}
=======
;Serial_Dma.c,49 :: 		DCH0ECON      =  (146 << 8 ) | 0x30;
ORI	R2, R0, 37424
SW	R2, Offset(DCH0ECON+0)(GP)
;Serial_Dma.c,52 :: 		DCH0DAT       = '?';//'\0' //0x0A0D;//'\r\n';
ORI	R2, R0, 63
SW	R2, Offset(DCH0DAT+0)(GP)
;Serial_Dma.c,55 :: 		DCH0SSA       = KVA_TO_PA(0xBF822230);    //[0xBF822230 = U2RXREG]
LUI	R2, 8066
ORI	R2, R2, 8752
SW	R2, Offset(DCH0SSA+0)(GP)
;Serial_Dma.c,56 :: 		DCH0SSIZ      = 1;                 // source size = 1byte at a time
ORI	R2, R0, 1
SW	R2, Offset(DCH0SSIZ+0)(GP)
;Serial_Dma.c,59 :: 		DCH0DSA       = KVA_TO_PA(0xA0002000);    // virtual address:= IN RAM FOR RECIEVED DATA
ORI	R2, R0, 8192
SW	R2, Offset(DCH0DSA+0)(GP)
;Serial_Dma.c,60 :: 		DCH0DSIZ      = 200  ;  // destination size = Size for the 'rxBuf' to fill up with received characters. It is = 5 in this example...
ORI	R2, R0, 200
SW	R2, Offset(DCH0DSIZ+0)(GP)
;Serial_Dma.c,63 :: 		DCH0CSIZ      = 1  ;  // bytes transferred in the background
ORI	R2, R0, 1
SW	R2, Offset(DCH0CSIZ+0)(GP)
;Serial_Dma.c,67 :: 		IPC33CLR     = 0x160000;
LUI	R2, 22
SW	R2, Offset(IPC33CLR+0)(GP)
;Serial_Dma.c,70 :: 		DCH0INTCLR    = 0x00FF00FF ;
LUI	R2, 255
ORI	R2, R2, 255
SW	R2, Offset(DCH0INTCLR+0)(GP)
;Serial_Dma.c,73 :: 		IPC33SET      = 0x00140000;
LUI	R2, 20
SW	R2, Offset(IPC33SET+0)(GP)
;Serial_Dma.c,76 :: 		DCH0INTSET      =  0xB0000;
LUI	R2, 11
SW	R2, Offset(DCH0INTSET+0)(GP)
;Serial_Dma.c,79 :: 		IEC4SET       = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IEC4SET+0)(GP)
;Serial_Dma.c,80 :: 		IFS4CLR       = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,83 :: 		DCH0INTCLR    = 0x000000FF ;
ORI	R2, R0, 255
SW	R2, Offset(DCH0INTCLR+0)(GP)
;Serial_Dma.c,87 :: 		DCH0CONSET      = 0X0000513;//013 = 1 char || 813 = 2 char e.g. \r\n
ORI	R2, R0, 1299
SW	R2, Offset(DCH0CONSET+0)(GP)
;Serial_Dma.c,90 :: 		serial.head = serial.tail = serial.diff = 0;
SH	R0, Offset(_serial+504)(GP)
SH	R0, Offset(_serial+502)(GP)
SH	R0, Offset(_serial+500)(GP)
;Serial_Dma.c,91 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA0:
JR	RA
NOP	
; end of _DMA0
_DMA0_Flag:
<<<<<<< HEAD
;Serial_Dma.c,104 :: 		char DMA0_Flag(){
;Serial_Dma.c,105 :: 		return dma0int_flag;
LBU	R2, Offset(_dma0int_flag+0)(GP)
;Serial_Dma.c,106 :: 		}
=======
;Serial_Dma.c,95 :: 		char DMA0_Flag(){
;Serial_Dma.c,96 :: 		return dma0int_flag;
LBU	R2, Offset(_dma0int_flag+0)(GP)
;Serial_Dma.c,97 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA0_Flag:
JR	RA
NOP	
; end of _DMA0_Flag
_DMA0_Enable:
<<<<<<< HEAD
;Serial_Dma.c,110 :: 		void DMA0_Enable(){
;Serial_Dma.c,113 :: 		DCH0CONSET  = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH0CONSET+0)(GP)
;Serial_Dma.c,114 :: 		IFS4CLR     = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,115 :: 		}
=======
;Serial_Dma.c,101 :: 		void DMA0_Enable(){
;Serial_Dma.c,104 :: 		DCH0CONSET  = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH0CONSET+0)(GP)
;Serial_Dma.c,105 :: 		IFS4CLR     = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,106 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA0_Enable:
JR	RA
NOP	
; end of _DMA0_Enable
_DMA0_Disable:
<<<<<<< HEAD
;Serial_Dma.c,119 :: 		void DMA0_Disable(){
;Serial_Dma.c,121 :: 		DCH0CONCLR  = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH0CONCLR+0)(GP)
;Serial_Dma.c,123 :: 		}
=======
;Serial_Dma.c,110 :: 		void DMA0_Disable(){
;Serial_Dma.c,112 :: 		DCH0CONCLR  = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH0CONCLR+0)(GP)
;Serial_Dma.c,114 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA0_Disable:
JR	RA
NOP	
; end of _DMA0_Disable
_DMA0_ReadDstPtr:
<<<<<<< HEAD
;Serial_Dma.c,128 :: 		unsigned int DMA0_ReadDstPtr(){
;Serial_Dma.c,129 :: 		return DCH0DPTR;
LW	R2, Offset(DCH0DPTR+0)(GP)
;Serial_Dma.c,130 :: 		}
=======
;Serial_Dma.c,119 :: 		unsigned int DMA0_ReadDstPtr(){
;Serial_Dma.c,120 :: 		return DCH0DPTR;
LW	R2, Offset(DCH0DPTR+0)(GP)
;Serial_Dma.c,121 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA0_ReadDstPtr:
JR	RA
NOP	
; end of _DMA0_ReadDstPtr
_DMA0_RstDstPtr:
<<<<<<< HEAD
;Serial_Dma.c,134 :: 		void DMA0_RstDstPtr(){
;Serial_Dma.c,135 :: 		DCH0DPTRCLR = 0xFFFF;
ORI	R2, R0, 65535
SW	R2, Offset(DCH0DPTRCLR+0)(GP)
;Serial_Dma.c,136 :: 		}
=======
;Serial_Dma.c,125 :: 		void DMA0_RstDstPtr(){
;Serial_Dma.c,126 :: 		DCH0DPTRCLR = 0xFFFF;
ORI	R2, R0, 65535
SW	R2, Offset(DCH0DPTRCLR+0)(GP)
;Serial_Dma.c,127 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA0_RstDstPtr:
JR	RA
NOP	
; end of _DMA0_RstDstPtr
_DMA_CH0_ISR:
<<<<<<< HEAD
;Serial_Dma.c,140 :: 		void DMA_CH0_ISR() iv IVT_DMA0 ilevel 5 ics ICS_AUTO{
=======
;Serial_Dma.c,131 :: 		void DMA_CH0_ISR() iv IVT_DMA0 ilevel 5 ics ICS_AUTO{
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
RDPGPR	SP, SP
ADDIU	SP, SP, -16
SW	R30, 12(SP)
MFC0	R30, 12, 2
SW	R30, 8(SP)
MFC0	R30, 14, 0
SW	R30, 4(SP)
MFC0	R30, 12, 0
SW	R30, 0(SP)
INS	R30, R0, 1, 15
ORI	R30, R0, 5120
MTC0	R30, 12, 0
ADDIU	SP, SP, -16
SW	RA, 0(SP)
<<<<<<< HEAD
;Serial_Dma.c,141 :: 		int i = 0;
; i start address is: 24 (R6)
MOVZ	R6, R0, R0
;Serial_Dma.c,144 :: 		dma0int_flag = DCH0INT & 0x00FF;
LW	R2, Offset(DCH0INT+0)(GP)
ANDI	R2, R2, 255
SB	R2, Offset(_dma0int_flag+0)(GP)
;Serial_Dma.c,147 :: 		if( CHERIF_bit == 1){       // test error int flag
=======
;Serial_Dma.c,132 :: 		int i = 0;
; i start address is: 24 (R6)
MOVZ	R6, R0, R0
;Serial_Dma.c,135 :: 		dma0int_flag = DCH0INT & 0x00FF;
LW	R2, Offset(DCH0INT+0)(GP)
ANDI	R2, R2, 255
SB	R2, Offset(_dma0int_flag+0)(GP)
;Serial_Dma.c,138 :: 		if( CHERIF_bit == 1){       // test error int flag
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
_LX	
EXT	R2, R2, BitPos(CHERIF_bit+0), 1
BNE	R2, 1, L__DMA_CH0_ISR67
NOP	
J	L_DMA_CH0_ISR0
NOP	
L__DMA_CH0_ISR67:
<<<<<<< HEAD
;Serial_Dma.c,149 :: 		strcpy(rxBuf,DMAx_err(dma0,cherie));
=======
;Serial_Dma.c,140 :: 		strcpy(rxBuf,DMAx_err(dma0,cherie));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R23, SP, 4
ADDIU	R22, R23, 12
LUI	R24, hi_addr(?ICS?lstr1_Serial_Dma+0)
ORI	R24, R24, lo_addr(?ICS?lstr1_Serial_Dma+0)
JAL	___CC2DW+0
NOP	
ADDIU	R2, SP, 4
MOVZ	R26, R2, R0
LUI	R25, 40960
ORI	R25, R25, 8192
JAL	_strcpy+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,153 :: 		}
L_DMA_CH0_ISR0:
;Serial_Dma.c,160 :: 		if (DCH0INTbits.CHBCIF == 1){
=======
;Serial_Dma.c,144 :: 		}
L_DMA_CH0_ISR0:
;Serial_Dma.c,151 :: 		if (DCH0INTbits.CHBCIF == 1){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LBU	R2, Offset(DCH0INTbits+0)(GP)
EXT	R2, R2, 3, 1
BNE	R2, 1, L__DMA_CH0_ISR69
NOP	
J	L__DMA_CH0_ISR53
NOP	
L__DMA_CH0_ISR69:
; i end address is: 24 (R6)
<<<<<<< HEAD
;Serial_Dma.c,161 :: 		i = strlen(rxBuf);
=======
;Serial_Dma.c,152 :: 		i = strlen(rxBuf);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LUI	R25, 40960
ORI	R25, R25, 8192
JAL	_strlen+0
NOP	
; i start address is: 24 (R6)
SEH	R6, R2
; i end address is: 24 (R6)
<<<<<<< HEAD
;Serial_Dma.c,162 :: 		}
J	L_DMA_CH0_ISR1
NOP	
L__DMA_CH0_ISR53:
;Serial_Dma.c,160 :: 		if (DCH0INTbits.CHBCIF == 1){
;Serial_Dma.c,162 :: 		}
L_DMA_CH0_ISR1:
;Serial_Dma.c,166 :: 		if(serial.head + i > 499)
=======
;Serial_Dma.c,153 :: 		}
J	L_DMA_CH0_ISR1
NOP	
L__DMA_CH0_ISR53:
;Serial_Dma.c,151 :: 		if (DCH0INTbits.CHBCIF == 1){
;Serial_Dma.c,153 :: 		}
L_DMA_CH0_ISR1:
;Serial_Dma.c,157 :: 		if(serial.head + i > 499)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; i start address is: 24 (R6)
LH	R2, Offset(_serial+500)(GP)
ADDU	R2, R2, R6
SEH	R2, R2
SLTI	R2, R2, 500
BEQ	R2, R0, L__DMA_CH0_ISR70
NOP	
J	L_DMA_CH0_ISR2
NOP	
L__DMA_CH0_ISR70:
<<<<<<< HEAD
;Serial_Dma.c,167 :: 		serial.head = 0;
SH	R0, Offset(_serial+500)(GP)
L_DMA_CH0_ISR2:
;Serial_Dma.c,169 :: 		strncpy(serial.temp_buffer+serial.head, rxBuf, i);
=======
;Serial_Dma.c,158 :: 		serial.head = 0;
SH	R0, Offset(_serial+500)(GP)
L_DMA_CH0_ISR2:
;Serial_Dma.c,160 :: 		strncpy(serial.temp_buffer+serial.head, rxBuf, i);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R3, Offset(_serial+500)(GP)
LUI	R2, hi_addr(_serial+0)
ORI	R2, R2, lo_addr(_serial+0)
ADDU	R2, R2, R3
SEH	R27, R6
LUI	R26, 40960
ORI	R26, R26, 8192
MOVZ	R25, R2, R0
JAL	_strncpy+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,170 :: 		serial.head += i;
LH	R2, Offset(_serial+500)(GP)
ADDU	R2, R2, R6
SH	R2, Offset(_serial+500)(GP)
;Serial_Dma.c,171 :: 		memset(rxBuf,0,i+2);
=======
;Serial_Dma.c,161 :: 		serial.head += i;
LH	R2, Offset(_serial+500)(GP)
ADDU	R2, R2, R6
SH	R2, Offset(_serial+500)(GP)
;Serial_Dma.c,162 :: 		memset(rxBuf,0,i+2);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, R6, 2
; i end address is: 24 (R6)
SEH	R27, R2
MOVZ	R26, R0, R0
LUI	R25, 40960
ORI	R25, R25, 8192
JAL	_memset+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,174 :: 		DCH0INTCLR    = 0x000000ff;
ORI	R2, R0, 255
SW	R2, Offset(DCH0INTCLR+0)(GP)
;Serial_Dma.c,175 :: 		IFS4CLR       = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,177 :: 		}
=======
;Serial_Dma.c,165 :: 		DCH0INTCLR    = 0x000000ff;
ORI	R2, R0, 255
SW	R2, Offset(DCH0INTCLR+0)(GP)
;Serial_Dma.c,166 :: 		IFS4CLR       = 0x40;
ORI	R2, R0, 64
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,168 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_CH0_ISR:
LW	RA, 0(SP)
ADDIU	SP, SP, 16
DI	
EHB	
LW	R30, 4(SP)
MTC0	R30, 14, 0
LW	R30, 0(SP)
MTC0	R30, 12, 0
LW	R30, 8(SP)
MTC0	R30, 12, 2
LW	R30, 12(SP)
ADDIU	SP, SP, 16
WRPGPR	SP, SP
ERET	
; end of _DMA_CH0_ISR
Serial_Dma_Reset_rxBuff:
<<<<<<< HEAD
;Serial_Dma.c,180 :: 		static void Reset_rxBuff(int dif){
ADDIU	SP, SP, -16
SW	RA, 0(SP)
;Serial_Dma.c,181 :: 		memset(rxBuf,0,dif);
=======
;Serial_Dma.c,171 :: 		static void Reset_rxBuff(int dif){
ADDIU	SP, SP, -16
SW	RA, 0(SP)
;Serial_Dma.c,172 :: 		memset(rxBuf,0,dif);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
SEH	R27, R25
MOVZ	R26, R0, R0
LUI	R25, 40960
ORI	R25, R25, 8192
JAL	_memset+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,182 :: 		}
=======
;Serial_Dma.c,173 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_Reset_rxBuff:
LW	R27, 12(SP)
LW	R26, 8(SP)
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 16
JR	RA
NOP	
; end of Serial_Dma_Reset_rxBuff
_Get_Head_Value:
<<<<<<< HEAD
;Serial_Dma.c,185 :: 		int Get_Head_Value(){
;Serial_Dma.c,186 :: 		return serial.head;
LH	R2, Offset(_serial+500)(GP)
;Serial_Dma.c,187 :: 		}
=======
;Serial_Dma.c,176 :: 		int Get_Head_Value(){
;Serial_Dma.c,177 :: 		return serial.head;
LH	R2, Offset(_serial+500)(GP)
;Serial_Dma.c,178 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_Get_Head_Value:
JR	RA
NOP	
; end of _Get_Head_Value
_Get_Tail_Value:
<<<<<<< HEAD
;Serial_Dma.c,190 :: 		int Get_Tail_Value(){
;Serial_Dma.c,191 :: 		return serial.tail;
LH	R2, Offset(_serial+502)(GP)
;Serial_Dma.c,192 :: 		}
=======
;Serial_Dma.c,181 :: 		int Get_Tail_Value(){
;Serial_Dma.c,182 :: 		return serial.tail;
LH	R2, Offset(_serial+502)(GP)
;Serial_Dma.c,183 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_Get_Tail_Value:
JR	RA
NOP	
; end of _Get_Tail_Value
_Get_Difference:
<<<<<<< HEAD
;Serial_Dma.c,195 :: 		int Get_Difference(){
;Serial_Dma.c,197 :: 		if(serial.head > serial.tail)
=======
;Serial_Dma.c,186 :: 		int Get_Difference(){
;Serial_Dma.c,188 :: 		if(serial.head > serial.tail)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R3, Offset(_serial+502)(GP)
LH	R2, Offset(_serial+500)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__Get_Difference75
NOP	
J	L_Get_Difference3
NOP	
L__Get_Difference75:
<<<<<<< HEAD
;Serial_Dma.c,198 :: 		serial.diff = serial.head - serial.tail;
=======
;Serial_Dma.c,189 :: 		serial.diff = serial.head - serial.tail;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R3, Offset(_serial+502)(GP)
LH	R2, Offset(_serial+500)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(_serial+504)(GP)
J	L_Get_Difference4
NOP	
L_Get_Difference3:
<<<<<<< HEAD
;Serial_Dma.c,199 :: 		else if(serial.tail > serial.head)
=======
;Serial_Dma.c,190 :: 		else if(serial.tail > serial.head)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R3, Offset(_serial+500)(GP)
LH	R2, Offset(_serial+502)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__Get_Difference76
NOP	
J	L_Get_Difference5
NOP	
L__Get_Difference76:
<<<<<<< HEAD
;Serial_Dma.c,200 :: 		serial.diff =  serial.head;
=======
;Serial_Dma.c,191 :: 		serial.diff =  serial.head;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R2, Offset(_serial+500)(GP)
SH	R2, Offset(_serial+504)(GP)
J	L_Get_Difference6
NOP	
L_Get_Difference5:
<<<<<<< HEAD
;Serial_Dma.c,202 :: 		serial.diff = 0;
SH	R0, Offset(_serial+504)(GP)
L_Get_Difference6:
L_Get_Difference4:
;Serial_Dma.c,204 :: 		return serial.diff;
LH	R2, Offset(_serial+504)(GP)
;Serial_Dma.c,205 :: 		}
=======
;Serial_Dma.c,193 :: 		serial.diff = 0;
SH	R0, Offset(_serial+504)(GP)
L_Get_Difference6:
L_Get_Difference4:
;Serial_Dma.c,195 :: 		return serial.diff;
LH	R2, Offset(_serial+504)(GP)
;Serial_Dma.c,196 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_Get_Difference:
JR	RA
NOP	
; end of _Get_Difference
_Reset_Ring:
<<<<<<< HEAD
;Serial_Dma.c,207 :: 		void Reset_Ring(){
;Serial_Dma.c,208 :: 		serial.tail = serial.head = 0;
SH	R0, Offset(_serial+500)(GP)
SH	R0, Offset(_serial+502)(GP)
;Serial_Dma.c,209 :: 		}
=======
;Serial_Dma.c,198 :: 		void Reset_Ring(){
;Serial_Dma.c,199 :: 		serial.tail = serial.head = 0;
SH	R0, Offset(_serial+500)(GP)
SH	R0, Offset(_serial+502)(GP)
;Serial_Dma.c,200 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_Reset_Ring:
JR	RA
NOP	
; end of _Reset_Ring
_Get_Line:
<<<<<<< HEAD
;Serial_Dma.c,212 :: 		void Get_Line(char *str,int dif){
ADDIU	SP, SP, -12
SW	RA, 0(SP)
;Serial_Dma.c,214 :: 		if(serial.tail + dif > 499)
=======
;Serial_Dma.c,203 :: 		void Get_Line(char *str,int dif){
ADDIU	SP, SP, -12
SW	RA, 0(SP)
;Serial_Dma.c,205 :: 		if(serial.tail + dif > 499)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SW	R27, 4(SP)
LH	R2, Offset(_serial+502)(GP)
ADDU	R2, R2, R26
SEH	R2, R2
SLTI	R2, R2, 500
BEQ	R2, R0, L__Get_Line79
NOP	
J	L_Get_Line7
NOP	
L__Get_Line79:
<<<<<<< HEAD
;Serial_Dma.c,215 :: 		serial.tail = 0;
SH	R0, Offset(_serial+502)(GP)
L_Get_Line7:
;Serial_Dma.c,217 :: 		strncpy(str,serial.temp_buffer+serial.tail,dif);
=======
;Serial_Dma.c,206 :: 		serial.tail = 0;
SH	R0, Offset(_serial+502)(GP)
L_Get_Line7:
;Serial_Dma.c,208 :: 		strncpy(str,serial.temp_buffer+serial.tail,dif);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R3, Offset(_serial+502)(GP)
LUI	R2, hi_addr(_serial+0)
ORI	R2, R2, lo_addr(_serial+0)
ADDU	R2, R2, R3
SH	R26, 8(SP)
SEH	R27, R26
MOVZ	R26, R2, R0
JAL	_strncpy+0
NOP	
LH	R26, 8(SP)
<<<<<<< HEAD
;Serial_Dma.c,221 :: 		serial.tail += dif;
LH	R2, Offset(_serial+502)(GP)
ADDU	R2, R2, R26
SH	R2, Offset(_serial+502)(GP)
;Serial_Dma.c,222 :: 		}
=======
;Serial_Dma.c,212 :: 		serial.tail += dif;
LH	R2, Offset(_serial+502)(GP)
ADDU	R2, R2, R26
SH	R2, Offset(_serial+502)(GP)
;Serial_Dma.c,213 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_Get_Line:
LW	R27, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 12
JR	RA
NOP	
; end of _Get_Line
_Loopback:
<<<<<<< HEAD
;Serial_Dma.c,225 :: 		int  Loopback(){
ADDIU	SP, SP, -72
SW	RA, 0(SP)
;Serial_Dma.c,229 :: 		dif = Get_Difference();
=======
;Serial_Dma.c,216 :: 		int  Loopback(){
ADDIU	SP, SP, -72
SW	RA, 0(SP)
;Serial_Dma.c,220 :: 		dif = Get_Difference();
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
JAL	_Get_Difference+0
NOP	
; dif start address is: 52 (R13)
SEH	R13, R2
<<<<<<< HEAD
;Serial_Dma.c,231 :: 		if(serial.tail + dif > 499)
=======
;Serial_Dma.c,222 :: 		if(serial.tail + dif > 499)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R3, Offset(_serial+502)(GP)
ADDU	R2, R3, R2
SEH	R2, R2
SLTI	R2, R2, 500
BEQ	R2, R0, L__Loopback81
NOP	
J	L_Loopback8
NOP	
L__Loopback81:
<<<<<<< HEAD
;Serial_Dma.c,232 :: 		serial.tail = 0;
SH	R0, Offset(_serial+502)(GP)
L_Loopback8:
;Serial_Dma.c,234 :: 		strncpy(str,serial.temp_buffer+serial.tail,dif);
=======
;Serial_Dma.c,223 :: 		serial.tail = 0;
SH	R0, Offset(_serial+502)(GP)
L_Loopback8:
;Serial_Dma.c,225 :: 		strncpy(str,serial.temp_buffer+serial.tail,dif);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R3, Offset(_serial+502)(GP)
LUI	R2, hi_addr(_serial+0)
ORI	R2, R2, lo_addr(_serial+0)
ADDU	R3, R2, R3
ADDIU	R2, SP, 16
SEH	R27, R13
MOVZ	R26, R3, R0
MOVZ	R25, R2, R0
JAL	_strncpy+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,235 :: 		dma_printf("\n\t%s",str);
=======
;Serial_Dma.c,226 :: 		dma_printf("\n\t%s",str);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R3, SP, 16
ORI	R30, R0, 10
SB	R30, 66(SP)
ORI	R30, R0, 9
SB	R30, 67(SP)
ORI	R30, R0, 37
SB	R30, 68(SP)
ORI	R30, R0, 115
SB	R30, 69(SP)
MOVZ	R30, R0, R0
SB	R30, 70(SP)
ADDIU	R2, SP, 66
ADDIU	SP, SP, -8
SW	R3, 4(SP)
SW	R2, 0(SP)
JAL	_dma_printf+0
NOP	
ADDIU	SP, SP, 8
<<<<<<< HEAD
;Serial_Dma.c,237 :: 		serial.tail += dif;
=======
;Serial_Dma.c,228 :: 		serial.tail += dif;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LH	R2, Offset(_serial+502)(GP)
ADDU	R2, R2, R13
; dif end address is: 52 (R13)
SH	R2, Offset(_serial+502)(GP)
<<<<<<< HEAD
;Serial_Dma.c,238 :: 		}
=======
;Serial_Dma.c,229 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_Loopback:
LW	R27, 12(SP)
LW	R26, 8(SP)
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 72
JR	RA
NOP	
; end of _Loopback
_DMA1:
<<<<<<< HEAD
;Serial_Dma.c,253 :: 		void DMA1(){
;Serial_Dma.c,256 :: 		IPC33CLR      = 0x17000000;
LUI	R2, 5888
SW	R2, Offset(IPC33CLR+0)(GP)
;Serial_Dma.c,257 :: 		IEC4CLR       = 0x7;
ORI	R2, R0, 7
SW	R2, Offset(IEC4CLR+0)(GP)
;Serial_Dma.c,260 :: 		DCH1CONCLR = 0x8003;
ORI	R2, R0, 32771
SW	R2, Offset(DCH1CONCLR+0)(GP)
;Serial_Dma.c,264 :: 		DCH1ECON=(114 << 8)| 0x30;
ORI	R2, R0, 29232
SW	R2, Offset(DCH1ECON+0)(GP)
;Serial_Dma.c,271 :: 		DCH1DAT       = '\0';
SW	R0, Offset(DCH1DAT+0)(GP)
;Serial_Dma.c,274 :: 		DCH1SSA   = KVA_TO_PA(0xA0002200) ;  //0xA0002200 virtual address of txBuf
ORI	R2, R0, 8704
SW	R2, Offset(DCH1SSA+0)(GP)
;Serial_Dma.c,275 :: 		DCH1SSIZ  = 1000;  //' This is how many bytes you want to send out in a block transfer for UART transmitter
ORI	R2, R0, 1000
SW	R2, Offset(DCH1SSIZ+0)(GP)
;Serial_Dma.c,279 :: 		U1TXREG   = 0x00;
SW	R0, Offset(U1TXREG+0)(GP)
;Serial_Dma.c,281 :: 		DCH1DSA   = KVA_TO_PA(0xBF822020) ;
LUI	R2, 8066
ORI	R2, R2, 8224
SW	R2, Offset(DCH1DSA+0)(GP)
;Serial_Dma.c,285 :: 		DCH1DSIZ  = 1;
ORI	R2, R0, 1
SW	R2, Offset(DCH1DSIZ+0)(GP)
;Serial_Dma.c,288 :: 		DCH1CSIZ  = 1;    //' x bytes from txBuf in a cell waiting to send out 1 byte at a time to U1TXREG / DCH1DSIZ
ORI	R2, R0, 1
SW	R2, Offset(DCH1CSIZ+0)(GP)
;Serial_Dma.c,292 :: 		IPC33CLR  = 0x16000000;
LUI	R2, 5632
SW	R2, Offset(IPC33CLR+0)(GP)
;Serial_Dma.c,295 :: 		DCH1INTCLR = 0x00FF00FF ;
LUI	R2, 255
ORI	R2, R2, 255
SW	R2, Offset(DCH1INTCLR+0)(GP)
;Serial_Dma.c,298 :: 		IPC33SET  = 0x16000000;
LUI	R2, 5632
SW	R2, Offset(IPC33SET+0)(GP)
;Serial_Dma.c,304 :: 		IEC4SET   = 0x80;
ORI	R2, R0, 128
SW	R2, Offset(IEC4SET+0)(GP)
;Serial_Dma.c,306 :: 		IFS4CLR   = 0x80;
ORI	R2, R0, 128
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,308 :: 		DCH1INTCLR = 0x00FF ;
ORI	R2, R0, 255
SW	R2, Offset(DCH1INTCLR+0)(GP)
;Serial_Dma.c,312 :: 		DCH1CONSET    = 0x00000003;
ORI	R2, R0, 3
SW	R2, Offset(DCH1CONSET+0)(GP)
;Serial_Dma.c,314 :: 		}
=======
;Serial_Dma.c,244 :: 		void DMA1(){
;Serial_Dma.c,247 :: 		IPC33CLR      = 0x17000000;
LUI	R2, 5888
SW	R2, Offset(IPC33CLR+0)(GP)
;Serial_Dma.c,248 :: 		IEC4CLR       = 0x7;
ORI	R2, R0, 7
SW	R2, Offset(IEC4CLR+0)(GP)
;Serial_Dma.c,251 :: 		DCH1CONCLR = 0x8003;
ORI	R2, R0, 32771
SW	R2, Offset(DCH1CONCLR+0)(GP)
;Serial_Dma.c,254 :: 		DCH1ECON=(147 << 8)| 0x30;
ORI	R2, R0, 37680
SW	R2, Offset(DCH1ECON+0)(GP)
;Serial_Dma.c,258 :: 		DCH1DAT       = '\0';
SW	R0, Offset(DCH1DAT+0)(GP)
;Serial_Dma.c,261 :: 		DCH1SSA   = KVA_TO_PA(0xA0002200) ;  //0xA0002200 virtual address of txBuf
ORI	R2, R0, 8704
SW	R2, Offset(DCH1SSA+0)(GP)
;Serial_Dma.c,262 :: 		DCH1SSIZ  = 1000;  //' This is how many bytes you want to send out in a block transfer for UART transmitter
ORI	R2, R0, 1000
SW	R2, Offset(DCH1SSIZ+0)(GP)
;Serial_Dma.c,266 :: 		U1TXREG   = 0x00;
SW	R0, Offset(U1TXREG+0)(GP)
;Serial_Dma.c,267 :: 		DCH1DSA   = KVA_TO_PA(0xBF822220) ;
LUI	R2, 8066
ORI	R2, R2, 8736
SW	R2, Offset(DCH1DSA+0)(GP)
;Serial_Dma.c,268 :: 		DCH1DSIZ  = 1;
ORI	R2, R0, 1
SW	R2, Offset(DCH1DSIZ+0)(GP)
;Serial_Dma.c,271 :: 		DCH1CSIZ  = 1;    //' x bytes from txBuf in a cell waiting to send out 1 byte at a time to U1TXREG / DCH1DSIZ
ORI	R2, R0, 1
SW	R2, Offset(DCH1CSIZ+0)(GP)
;Serial_Dma.c,275 :: 		IPC33CLR  = 0x16000000;
LUI	R2, 5632
SW	R2, Offset(IPC33CLR+0)(GP)
;Serial_Dma.c,278 :: 		DCH1INTCLR = 0x00FF00FF ;
LUI	R2, 255
ORI	R2, R2, 255
SW	R2, Offset(DCH1INTCLR+0)(GP)
;Serial_Dma.c,281 :: 		IPC33SET  = 0x16000000;
LUI	R2, 5632
SW	R2, Offset(IPC33SET+0)(GP)
;Serial_Dma.c,287 :: 		IEC4SET   = 0x80;
ORI	R2, R0, 128
SW	R2, Offset(IEC4SET+0)(GP)
;Serial_Dma.c,289 :: 		IFS4CLR   = 0x80;
ORI	R2, R0, 128
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,291 :: 		DCH1INTCLR = 0x00FF ;
ORI	R2, R0, 255
SW	R2, Offset(DCH1INTCLR+0)(GP)
;Serial_Dma.c,295 :: 		DCH1CONSET    = 0x00000003;
ORI	R2, R0, 3
SW	R2, Offset(DCH1CONSET+0)(GP)
;Serial_Dma.c,297 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA1:
JR	RA
NOP	
; end of _DMA1
_DMA1_Flag:
<<<<<<< HEAD
;Serial_Dma.c,318 :: 		char DMA1_Flag(){
;Serial_Dma.c,319 :: 		return dma1int_flag;
LBU	R2, Offset(_dma1int_flag+0)(GP)
;Serial_Dma.c,320 :: 		}
=======
;Serial_Dma.c,301 :: 		char DMA1_Flag(){
;Serial_Dma.c,302 :: 		return dma1int_flag;
LBU	R2, Offset(_dma1int_flag+0)(GP)
;Serial_Dma.c,303 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA1_Flag:
JR	RA
NOP	
; end of _DMA1_Flag
_DMA1_Enable:
<<<<<<< HEAD
;Serial_Dma.c,326 :: 		unsigned int DMA1_Enable(){
;Serial_Dma.c,327 :: 		DCH1CONSET = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH1CONSET+0)(GP)
;Serial_Dma.c,328 :: 		return (DCH1CON & 0x80) >> 7;
LW	R2, Offset(DCH1CON+0)(GP)
ANDI	R2, R2, 128
SRL	R2, R2, 7
;Serial_Dma.c,329 :: 		}
=======
;Serial_Dma.c,309 :: 		unsigned int DMA1_Enable(){
;Serial_Dma.c,310 :: 		DCH1CONSET = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH1CONSET+0)(GP)
;Serial_Dma.c,311 :: 		return (DCH1CON & 0x80) >> 7;
LW	R2, Offset(DCH1CON+0)(GP)
ANDI	R2, R2, 128
SRL	R2, R2, 7
;Serial_Dma.c,312 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA1_Enable:
JR	RA
NOP	
; end of _DMA1_Enable
_DMA1_Disable:
<<<<<<< HEAD
;Serial_Dma.c,334 :: 		void DMA1_Disable(){
;Serial_Dma.c,335 :: 		DCH1CONCLR = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH1CONCLR+0)(GP)
;Serial_Dma.c,336 :: 		}
=======
;Serial_Dma.c,317 :: 		void DMA1_Disable(){
;Serial_Dma.c,318 :: 		DCH1CONCLR = 1<<7;
ORI	R2, R0, 128
SW	R2, Offset(DCH1CONCLR+0)(GP)
;Serial_Dma.c,319 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA1_Disable:
JR	RA
NOP	
; end of _DMA1_Disable
_DMA_Abort:
<<<<<<< HEAD
;Serial_Dma.c,340 :: 		unsigned int DMA_Abort(int channel){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Serial_Dma.c,341 :: 		if(channel == 0){
=======
;Serial_Dma.c,323 :: 		unsigned int DMA_Abort(int channel){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Serial_Dma.c,324 :: 		if(channel == 0){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R2, R25
BEQ	R2, R0, L__DMA_Abort87
NOP	
J	L_DMA_Abort9
NOP	
L__DMA_Abort87:
<<<<<<< HEAD
;Serial_Dma.c,345 :: 		DCH0ECONSET |= 1<<6;
LW	R2, Offset(DCH0ECONSET+0)(GP)
ORI	R2, R2, 64
SW	R2, Offset(DCH0ECONSET+0)(GP)
;Serial_Dma.c,348 :: 		while(DMA_IsOn(0));
=======
;Serial_Dma.c,328 :: 		DCH0ECONSET |= 1<<6;
LW	R2, Offset(DCH0ECONSET+0)(GP)
ORI	R2, R2, 64
SW	R2, Offset(DCH0ECONSET+0)(GP)
;Serial_Dma.c,331 :: 		while(DMA_IsOn(0));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_DMA_Abort10:
SH	R25, 4(SP)
MOVZ	R25, R0, R0
JAL	_DMA_IsOn+0
NOP	
LH	R25, 4(SP)
BNE	R2, R0, L__DMA_Abort89
NOP	
J	L_DMA_Abort11
NOP	
L__DMA_Abort89:
J	L_DMA_Abort10
NOP	
L_DMA_Abort11:
<<<<<<< HEAD
;Serial_Dma.c,352 :: 		DMA0_Enable();
JAL	_DMA0_Enable+0
NOP	
;Serial_Dma.c,353 :: 		while(!DMA_IsOn(0));
=======
;Serial_Dma.c,335 :: 		DMA0_Enable();
JAL	_DMA0_Enable+0
NOP	
;Serial_Dma.c,336 :: 		while(!DMA_IsOn(0));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_DMA_Abort12:
SH	R25, 4(SP)
MOVZ	R25, R0, R0
JAL	_DMA_IsOn+0
NOP	
LH	R25, 4(SP)
BEQ	R2, R0, L__DMA_Abort90
NOP	
J	L_DMA_Abort13
NOP	
L__DMA_Abort90:
J	L_DMA_Abort12
NOP	
L_DMA_Abort13:
<<<<<<< HEAD
;Serial_Dma.c,356 :: 		return (DCH0CON & 0x0080 ) >> 7;
=======
;Serial_Dma.c,339 :: 		return (DCH0CON & 0x0080 ) >> 7;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LW	R2, Offset(DCH0CON+0)(GP)
ANDI	R2, R2, 128
SRL	R2, R2, 7
J	L_end_DMA_Abort
NOP	
<<<<<<< HEAD
;Serial_Dma.c,358 :: 		}else if(channel == 1){
=======
;Serial_Dma.c,341 :: 		}else if(channel == 1){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_DMA_Abort9:
SEH	R3, R25
ORI	R2, R0, 1
BEQ	R3, R2, L__DMA_Abort91
NOP	
J	L_DMA_Abort15
NOP	
L__DMA_Abort91:
<<<<<<< HEAD
;Serial_Dma.c,360 :: 		DCH1ECONSET |= 1<<6;
LW	R2, Offset(DCH1ECONSET+0)(GP)
ORI	R2, R2, 64
SW	R2, Offset(DCH1ECONSET+0)(GP)
;Serial_Dma.c,363 :: 		while(DMA_IsOn(1));
=======
;Serial_Dma.c,343 :: 		DCH1ECONSET |= 1<<6;
LW	R2, Offset(DCH1ECONSET+0)(GP)
ORI	R2, R2, 64
SW	R2, Offset(DCH1ECONSET+0)(GP)
;Serial_Dma.c,346 :: 		while(DMA_IsOn(1));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_DMA_Abort16:
SH	R25, 4(SP)
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
LH	R25, 4(SP)
BNE	R2, R0, L__DMA_Abort93
NOP	
J	L_DMA_Abort17
NOP	
L__DMA_Abort93:
J	L_DMA_Abort16
NOP	
L_DMA_Abort17:
<<<<<<< HEAD
;Serial_Dma.c,366 :: 		while(!DMA_IsOn(1));
=======
;Serial_Dma.c,349 :: 		while(!DMA_IsOn(1));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_DMA_Abort18:
SH	R25, 4(SP)
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
LH	R25, 4(SP)
BEQ	R2, R0, L__DMA_Abort94
NOP	
J	L_DMA_Abort19
NOP	
L__DMA_Abort94:
J	L_DMA_Abort18
NOP	
L_DMA_Abort19:
<<<<<<< HEAD
;Serial_Dma.c,368 :: 		DMA1_Enable();
JAL	_DMA1_Enable+0
NOP	
;Serial_Dma.c,369 :: 		return (DCH1CON & 0x0080 ) >> 7;
=======
;Serial_Dma.c,351 :: 		DMA1_Enable();
JAL	_DMA1_Enable+0
NOP	
;Serial_Dma.c,352 :: 		return (DCH1CON & 0x0080 ) >> 7;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LW	R2, Offset(DCH1CON+0)(GP)
ANDI	R2, R2, 128
SRL	R2, R2, 7
J	L_end_DMA_Abort
NOP	
<<<<<<< HEAD
;Serial_Dma.c,370 :: 		}
L_DMA_Abort15:
;Serial_Dma.c,372 :: 		return 255;
ORI	R2, R0, 255
;Serial_Dma.c,373 :: 		}
=======
;Serial_Dma.c,353 :: 		}
L_DMA_Abort15:
;Serial_Dma.c,355 :: 		return 255;
ORI	R2, R0, 255
;Serial_Dma.c,356 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_Abort:
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of _DMA_Abort
_DMA_IsOn:
<<<<<<< HEAD
;Serial_Dma.c,377 :: 		unsigned int DMA_IsOn(int channel){
;Serial_Dma.c,378 :: 		if(channel == 0)
=======
;Serial_Dma.c,360 :: 		unsigned int DMA_IsOn(int channel){
;Serial_Dma.c,361 :: 		if(channel == 0)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R2, R25
BEQ	R2, R0, L__DMA_IsOn96
NOP	
J	L_DMA_IsOn20
NOP	
L__DMA_IsOn96:
<<<<<<< HEAD
;Serial_Dma.c,379 :: 		return (DCH0CON & 0x8000)>>15;
=======
;Serial_Dma.c,362 :: 		return (DCH0CON & 0x8000)>>15;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LW	R2, Offset(DCH0CON+0)(GP)
ANDI	R2, R2, 32768
SRL	R2, R2, 15
J	L_end_DMA_IsOn
NOP	
L_DMA_IsOn20:
<<<<<<< HEAD
;Serial_Dma.c,381 :: 		return (DCH1CON & 0x8000)>>15;
LW	R2, Offset(DCH1CON+0)(GP)
ANDI	R2, R2, 32768
SRL	R2, R2, 15
;Serial_Dma.c,382 :: 		}
=======
;Serial_Dma.c,364 :: 		return (DCH1CON & 0x8000)>>15;
LW	R2, Offset(DCH1CON+0)(GP)
ANDI	R2, R2, 32768
SRL	R2, R2, 15
;Serial_Dma.c,365 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_IsOn:
JR	RA
NOP	
; end of _DMA_IsOn
_DMA_CH_Busy:
<<<<<<< HEAD
;Serial_Dma.c,389 :: 		unsigned int DMA_CH_Busy(int channel){
;Serial_Dma.c,390 :: 		if(channel == 0)
=======
;Serial_Dma.c,372 :: 		unsigned int DMA_CH_Busy(int channel){
;Serial_Dma.c,373 :: 		if(channel == 0)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R2, R25
BEQ	R2, R0, L__DMA_CH_Busy98
NOP	
J	L_DMA_CH_Busy22
NOP	
L__DMA_CH_Busy98:
<<<<<<< HEAD
;Serial_Dma.c,391 :: 		return (DCH0CON & 0x8000)>>15;
=======
;Serial_Dma.c,374 :: 		return (DCH0CON & 0x8000)>>15;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LW	R2, Offset(DCH0CON+0)(GP)
ANDI	R2, R2, 32768
SRL	R2, R2, 15
J	L_end_DMA_CH_Busy
NOP	
L_DMA_CH_Busy22:
<<<<<<< HEAD
;Serial_Dma.c,393 :: 		return (DCH1CON & 0x8000)>>15;
LW	R2, Offset(DCH1CON+0)(GP)
ANDI	R2, R2, 32768
SRL	R2, R2, 15
;Serial_Dma.c,394 :: 		}
=======
;Serial_Dma.c,376 :: 		return (DCH1CON & 0x8000)>>15;
LW	R2, Offset(DCH1CON+0)(GP)
ANDI	R2, R2, 32768
SRL	R2, R2, 15
;Serial_Dma.c,377 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_CH_Busy:
JR	RA
NOP	
; end of _DMA_CH_Busy
_DMA_Suspend:
<<<<<<< HEAD
;Serial_Dma.c,400 :: 		unsigned int DMA_Suspend(){
;Serial_Dma.c,401 :: 		DMACONSET = (1 << 12);
ORI	R2, R0, 4096
SW	R2, Offset(DMACONSET+0)(GP)
;Serial_Dma.c,404 :: 		return ((DMACON & 0x1000)>>12);
LW	R2, Offset(DMACON+0)(GP)
ANDI	R2, R2, 4096
SRL	R2, R2, 12
;Serial_Dma.c,405 :: 		}
=======
;Serial_Dma.c,383 :: 		unsigned int DMA_Suspend(){
;Serial_Dma.c,384 :: 		DMACONSET = (1 << 12);
ORI	R2, R0, 4096
SW	R2, Offset(DMACONSET+0)(GP)
;Serial_Dma.c,387 :: 		return ((DMACON & 0x1000)>>12);
LW	R2, Offset(DMACON+0)(GP)
ANDI	R2, R2, 4096
SRL	R2, R2, 12
;Serial_Dma.c,388 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_Suspend:
JR	RA
NOP	
; end of _DMA_Suspend
_DMA_Resume:
<<<<<<< HEAD
;Serial_Dma.c,410 :: 		unsigned int DMA_Resume(){
;Serial_Dma.c,411 :: 		DMACONCLR = (1 << 12);
ORI	R2, R0, 4096
SW	R2, Offset(DMACONCLR+0)(GP)
;Serial_Dma.c,414 :: 		return (DMACON & 0x1000)>>12;
LW	R2, Offset(DMACON+0)(GP)
ANDI	R2, R2, 4096
SRL	R2, R2, 12
;Serial_Dma.c,415 :: 		}
=======
;Serial_Dma.c,393 :: 		unsigned int DMA_Resume(){
;Serial_Dma.c,394 :: 		DMACONCLR = (1 << 12);
ORI	R2, R0, 4096
SW	R2, Offset(DMACONCLR+0)(GP)
;Serial_Dma.c,397 :: 		return (DMACON & 0x1000)>>12;
LW	R2, Offset(DMACON+0)(GP)
ANDI	R2, R2, 4096
SRL	R2, R2, 12
;Serial_Dma.c,398 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_Resume:
JR	RA
NOP	
; end of _DMA_Resume
_DMA_Busy:
<<<<<<< HEAD
;Serial_Dma.c,420 :: 		unsigned int DMA_Busy(){
;Serial_Dma.c,421 :: 		return ((DMACON & 0x800)>>11);
LW	R2, Offset(DMACON+0)(GP)
ANDI	R2, R2, 2048
SRL	R2, R2, 11
;Serial_Dma.c,422 :: 		}
=======
;Serial_Dma.c,403 :: 		unsigned int DMA_Busy(){
;Serial_Dma.c,404 :: 		return ((DMACON & 0x800)>>11);
LW	R2, Offset(DMACON+0)(GP)
ANDI	R2, R2, 2048
SRL	R2, R2, 11
;Serial_Dma.c,405 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_Busy:
JR	RA
NOP	
; end of _DMA_Busy
_DMA_CH1_ISR:
<<<<<<< HEAD
;Serial_Dma.c,430 :: 		void DMA_CH1_ISR() iv IVT_DMA1 ilevel 5 ics ICS_SRS {
=======
;Serial_Dma.c,413 :: 		void DMA_CH1_ISR() iv IVT_DMA1 ilevel 5 ics ICS_SRS {
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
RDPGPR	SP, SP
ADDIU	SP, SP, -12
MFC0	R30, 12, 2
SW	R30, 8(SP)
MFC0	R30, 14, 0
SW	R30, 4(SP)
MFC0	R30, 12, 0
SW	R30, 0(SP)
INS	R30, R0, 1, 15
ORI	R30, R0, 5120
MTC0	R30, 12, 0
<<<<<<< HEAD
;Serial_Dma.c,433 :: 		dma1int_flag = DCH1INT & 0x00FF;
LW	R2, Offset(DCH1INT+0)(GP)
ANDI	R2, R2, 255
SB	R2, Offset(_dma1int_flag+0)(GP)
;Serial_Dma.c,435 :: 		if (DCH1INTbits.CHBCIF){
=======
;Serial_Dma.c,416 :: 		dma1int_flag = DCH1INT & 0x00FF;
LW	R2, Offset(DCH1INT+0)(GP)
ANDI	R2, R2, 255
SB	R2, Offset(_dma1int_flag+0)(GP)
;Serial_Dma.c,418 :: 		if (DCH1INTbits.CHBCIF){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LBU	R2, Offset(DCH1INTbits+0)(GP)
EXT	R2, R2, 3, 1
BNE	R2, R0, L__DMA_CH1_ISR104
NOP	
J	L_DMA_CH1_ISR24
NOP	
L__DMA_CH1_ISR104:
<<<<<<< HEAD
;Serial_Dma.c,437 :: 		}
L_DMA_CH1_ISR24:
;Serial_Dma.c,439 :: 		if( CHERIF_DCH1INT_bit == 1){
=======
;Serial_Dma.c,420 :: 		}
L_DMA_CH1_ISR24:
;Serial_Dma.c,422 :: 		if( CHERIF_DCH1INT_bit == 1){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
_LX	
EXT	R2, R2, BitPos(CHERIF_DCH1INT_bit+0), 1
BNE	R2, 1, L__DMA_CH1_ISR106
NOP	
J	L_DMA_CH1_ISR25
NOP	
L__DMA_CH1_ISR106:
<<<<<<< HEAD
;Serial_Dma.c,440 :: 		CABORT_DCH1ECON_bit = 1;
LUI	R2, BitMask(CABORT_DCH1ECON_bit+0)
ORI	R2, R2, BitMask(CABORT_DCH1ECON_bit+0)
_SX	
;Serial_Dma.c,441 :: 		}
L_DMA_CH1_ISR25:
;Serial_Dma.c,446 :: 		DCH1INTCLR  = 0x00FF;
ORI	R2, R0, 255
SW	R2, Offset(DCH1INTCLR+0)(GP)
;Serial_Dma.c,447 :: 		IFS4CLR     = 0x80;
ORI	R2, R0, 128
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,449 :: 		}
=======
;Serial_Dma.c,423 :: 		CABORT_DCH1ECON_bit = 1;
LUI	R2, BitMask(CABORT_DCH1ECON_bit+0)
ORI	R2, R2, BitMask(CABORT_DCH1ECON_bit+0)
_SX	
;Serial_Dma.c,424 :: 		}
L_DMA_CH1_ISR25:
;Serial_Dma.c,429 :: 		DCH1INTCLR  = 0x00FF;
ORI	R2, R0, 255
SW	R2, Offset(DCH1INTCLR+0)(GP)
;Serial_Dma.c,430 :: 		IFS4CLR     = 0x80;
ORI	R2, R0, 128
SW	R2, Offset(IFS4CLR+0)(GP)
;Serial_Dma.c,432 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_DMA_CH1_ISR:
DI	
EHB	
LW	R30, 8(SP)
MTC0	R30, 12, 2
LW	R30, 4(SP)
MTC0	R30, 14, 0
LW	R30, 0(SP)
MTC0	R30, 12, 0
ADDIU	SP, SP, 12
WRPGPR	SP, SP
ERET	
; end of _DMA_CH1_ISR
_dma_printf:
<<<<<<< HEAD
;Serial_Dma.c,454 :: 		int dma_printf(const char* str,...){
ADDIU	SP, SP, -1568
SW	RA, 0(SP)
;Serial_Dma.c,457 :: 		int i = 0, j = 0;
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
;Serial_Dma.c,458 :: 		char buff[1500]={0},tmp[20],tmp1[20];
=======
;Serial_Dma.c,437 :: 		int dma_printf(const char* str,...){
ADDIU	SP, SP, -1568
SW	RA, 0(SP)
;Serial_Dma.c,440 :: 		int i = 0, j = 0;
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
;Serial_Dma.c,441 :: 		char buff[1500]={0},tmp[20],tmp1[20];
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R23, SP, 64
ADDIU	R22, R23, 1500
LUI	R24, hi_addr(?ICSdma_printf_buff_L0+0)
ORI	R24, R24, lo_addr(?ICSdma_printf_buff_L0+0)
JAL	___CC2DW+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,462 :: 		if(str == 0)
=======
;Serial_Dma.c,445 :: 		if(str == 0)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LW	R2, 1568(SP)
BEQ	R2, R0, L__dma_printf108
NOP	
J	L_dma_printf26
NOP	
L__dma_printf108:
<<<<<<< HEAD
;Serial_Dma.c,463 :: 		return;
J	L_end_dma_printf
NOP	
L_dma_printf26:
;Serial_Dma.c,468 :: 		if(DMA_CH_Busy(1)){
=======
;Serial_Dma.c,446 :: 		return;
J	L_end_dma_printf
NOP	
L_dma_printf26:
;Serial_Dma.c,451 :: 		if(DMA_CH_Busy(1)){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ORI	R25, R0, 1
JAL	_DMA_CH_Busy+0
NOP	
BNE	R2, R0, L__dma_printf110
NOP	
J	L_dma_printf27
NOP	
L__dma_printf110:
<<<<<<< HEAD
;Serial_Dma.c,469 :: 		return 0;
MOVZ	R2, R0, R0
J	L_end_dma_printf
NOP	
;Serial_Dma.c,470 :: 		}
L_dma_printf27:
;Serial_Dma.c,474 :: 		va_start(va,str);
=======
;Serial_Dma.c,452 :: 		return 0;
MOVZ	R2, R0, R0
J	L_end_dma_printf
NOP	
;Serial_Dma.c,453 :: 		}
L_dma_printf27:
;Serial_Dma.c,457 :: 		va_start(va,str);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R3, SP, 16
ADDIU	R2, SP, 1568
ADDIU	R2, R2, 4
SW	R2, 0(R3)
<<<<<<< HEAD
;Serial_Dma.c,476 :: 		i = j = 0;
=======
;Serial_Dma.c,459 :: 		i = j = 0;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
MOVZ	R12, R0, R0
; i start address is: 20 (R5)
MOVZ	R5, R0, R0
; j end address is: 48 (R12)
; i end address is: 20 (R5)
<<<<<<< HEAD
;Serial_Dma.c,477 :: 		while(*(str+i) != '\0'){
=======
;Serial_Dma.c,460 :: 		while(*(str+i) != '\0'){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_dma_printf28:
; i start address is: 20 (R5)
; j start address is: 48 (R12)
SEH	R3, R5
LW	R2, 1568(SP)
ADDU	R2, R2, R3
LBU	R2, 0(R2)
ANDI	R2, R2, 255
BNE	R2, R0, L__dma_printf112
NOP	
J	L_dma_printf29
NOP	
L__dma_printf112:
<<<<<<< HEAD
;Serial_Dma.c,478 :: 		if(*(str+i) == '%'){
=======
;Serial_Dma.c,461 :: 		if(*(str+i) == '%'){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R3, R5
LW	R2, 1568(SP)
ADDU	R2, R2, R3
LBU	R2, 0(R2)
ANDI	R3, R2, 255
ORI	R2, R0, 37
BEQ	R3, R2, L__dma_printf113
NOP	
J	L_dma_printf30
NOP	
L__dma_printf113:
<<<<<<< HEAD
;Serial_Dma.c,479 :: 		i++;  //step over % char
=======
;Serial_Dma.c,462 :: 		i++;  //step over % char
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, R5, 1
; i end address is: 20 (R5)
; i start address is: 44 (R11)
SEH	R11, R2
<<<<<<< HEAD
;Serial_Dma.c,480 :: 		switch(*(str+i)){
=======
;Serial_Dma.c,463 :: 		switch(*(str+i)){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R3, R2
LW	R2, 1568(SP)
ADDU	R2, R2, R3
SW	R2, 1564(SP)
J	L_dma_printf31
NOP	
<<<<<<< HEAD
;Serial_Dma.c,481 :: 		case 'c':
L_dma_printf33:
;Serial_Dma.c,483 :: 		buff[j] = (char)va_arg(va,char);
=======
;Serial_Dma.c,464 :: 		case 'c':
L_dma_printf33:
;Serial_Dma.c,466 :: 		buff[j] = (char)va_arg(va,char);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R5, R3, R2
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LBU	R2, 0(R3)
SB	R2, 0(R5)
<<<<<<< HEAD
;Serial_Dma.c,484 :: 		j++;
ADDIU	R2, R12, 1
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,485 :: 		break;
=======
;Serial_Dma.c,467 :: 		j++;
ADDIU	R2, R12, 1
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,468 :: 		break;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
<<<<<<< HEAD
;Serial_Dma.c,486 :: 		case 'd':
L_dma_printf34:
;Serial_Dma.c,488 :: 		sprintf(tmp1,"%d",va_arg(va,int));
=======
;Serial_Dma.c,469 :: 		case 'd':
L_dma_printf34:
;Serial_Dma.c,471 :: 		sprintf(tmp1,"%d",va_arg(va,int));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LH	R2, 0(R3)
ADDIU	R3, SP, 40
ADDIU	SP, SP, -12
SH	R2, 8(SP)
LUI	R2, hi_addr(?lstr_3_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_3_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
<<<<<<< HEAD
;Serial_Dma.c,489 :: 		strcat(buff+j, tmp1);
=======
;Serial_Dma.c,472 :: 		strcat(buff+j, tmp1);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R4, SP, 40
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,490 :: 		j += strlen(tmp1);
=======
;Serial_Dma.c,473 :: 		j += strlen(tmp1);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, SP, 40
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
<<<<<<< HEAD
;Serial_Dma.c,491 :: 		break;
=======
;Serial_Dma.c,474 :: 		break;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
<<<<<<< HEAD
;Serial_Dma.c,492 :: 		case 'u':
L_dma_printf35:
;Serial_Dma.c,494 :: 		sprintf(tmp1,"%u",va_arg(va,unsigned int));
=======
;Serial_Dma.c,475 :: 		case 'u':
L_dma_printf35:
;Serial_Dma.c,477 :: 		sprintf(tmp1,"%u",va_arg(va,unsigned int));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LHU	R2, 0(R3)
ADDIU	R3, SP, 40
ADDIU	SP, SP, -12
SH	R2, 8(SP)
LUI	R2, hi_addr(?lstr_4_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_4_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
<<<<<<< HEAD
;Serial_Dma.c,495 :: 		strcat(buff+j, tmp1);
=======
;Serial_Dma.c,478 :: 		strcat(buff+j, tmp1);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R4, SP, 40
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,496 :: 		j += strlen(tmp1);
=======
;Serial_Dma.c,479 :: 		j += strlen(tmp1);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, SP, 40
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
SEH	R12, R2
; j end address is: 48 (R12)
<<<<<<< HEAD
;Serial_Dma.c,497 :: 		case 'l':
J	L_dma_printf36
NOP	
L__dma_printf54:
;Serial_Dma.c,537 :: 		}
;Serial_Dma.c,497 :: 		case 'l':
L_dma_printf36:
;Serial_Dma.c,499 :: 		sprintf(tmp,"%ld",va_arg(va,long));
=======
;Serial_Dma.c,480 :: 		case 'l':
J	L_dma_printf36
NOP	
L__dma_printf54:
;Serial_Dma.c,520 :: 		}
;Serial_Dma.c,480 :: 		case 'l':
L_dma_printf36:
;Serial_Dma.c,482 :: 		sprintf(tmp,"%ld",va_arg(va,long));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LW	R2, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SW	R2, 8(SP)
LUI	R2, hi_addr(?lstr_5_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_5_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
<<<<<<< HEAD
=======
;Serial_Dma.c,484 :: 		strcat(buff+j, tmp);
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
;Serial_Dma.c,485 :: 		j += strlen(tmp);
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,486 :: 		break;
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
;Serial_Dma.c,487 :: 		case 'X':
L_dma_printf37:
;Serial_Dma.c,489 :: 		sprintf(tmp,"%X",va_arg(va,int));
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LH	R2, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SH	R2, 8(SP)
LUI	R2, hi_addr(?lstr_6_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_6_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
;Serial_Dma.c,490 :: 		strcat(buff+j, tmp);
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
;Serial_Dma.c,491 :: 		j += strlen(tmp);
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,492 :: 		break;
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
;Serial_Dma.c,493 :: 		case 'X':
L_dma_printf38:
;Serial_Dma.c,495 :: 		sprintf(tmp,"%lX",va_arg(va,long));
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LW	R2, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SW	R2, 8(SP)
LUI	R2, hi_addr(?lstr_7_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_7_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
;Serial_Dma.c,496 :: 		strcat(buff+j, tmp);
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
;Serial_Dma.c,497 :: 		j += strlen(tmp);
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,498 :: 		break;
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
;Serial_Dma.c,499 :: 		case 'f':
L_dma_printf39:
;Serial_Dma.c,500 :: 		sprintf(tmp,"%08.3f",va_arg(va,float));
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LWC1	S0, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SWC1	S0, 8(SP)
LUI	R2, hi_addr(?lstr_8_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_8_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
;Serial_Dma.c,501 :: 		strcat(buff+j, tmp);
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
;Serial_Dma.c,502 :: 		j += strlen(tmp);
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,503 :: 		break;
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
<<<<<<< HEAD
;Serial_Dma.c,504 :: 		case 'X':
L_dma_printf37:
;Serial_Dma.c,506 :: 		sprintf(tmp,"%X",va_arg(va,int));
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LH	R2, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SH	R2, 8(SP)
LUI	R2, hi_addr(?lstr_6_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_6_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
;Serial_Dma.c,507 :: 		strcat(buff+j, tmp);
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
;Serial_Dma.c,508 :: 		j += strlen(tmp);
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,509 :: 		break;
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
;Serial_Dma.c,510 :: 		case 'X':
L_dma_printf38:
;Serial_Dma.c,512 :: 		sprintf(tmp,"%lX",va_arg(va,long));
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LW	R2, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SW	R2, 8(SP)
LUI	R2, hi_addr(?lstr_7_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_7_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
;Serial_Dma.c,513 :: 		strcat(buff+j, tmp);
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
;Serial_Dma.c,514 :: 		j += strlen(tmp);
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,515 :: 		break;
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
;Serial_Dma.c,516 :: 		case 'f':
L_dma_printf39:
;Serial_Dma.c,517 :: 		sprintf(tmp,"%08.3f",va_arg(va,float));
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LWC1	S0, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SWC1	S0, 8(SP)
LUI	R2, hi_addr(?lstr_8_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_8_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
;Serial_Dma.c,518 :: 		strcat(buff+j, tmp);
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
;Serial_Dma.c,519 :: 		j += strlen(tmp);
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
;Serial_Dma.c,520 :: 		break;
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
;Serial_Dma.c,521 :: 		case 'F':
L_dma_printf40:
;Serial_Dma.c,522 :: 		sprintf(tmp,"%E",va_arg(va,double));
=======
;Serial_Dma.c,504 :: 		case 'F':
L_dma_printf40:
;Serial_Dma.c,505 :: 		sprintf(tmp,"%E",va_arg(va,double));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LWC1	S0, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SWC1	S0, 8(SP)
LUI	R2, hi_addr(?lstr_9_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_9_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
<<<<<<< HEAD
;Serial_Dma.c,523 :: 		strcat(buff+j, tmp);
=======
;Serial_Dma.c,506 :: 		strcat(buff+j, tmp);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,524 :: 		j += strlen(tmp);
=======
;Serial_Dma.c,507 :: 		j += strlen(tmp);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
<<<<<<< HEAD
;Serial_Dma.c,525 :: 		break;
=======
;Serial_Dma.c,508 :: 		break;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
<<<<<<< HEAD
;Serial_Dma.c,526 :: 		case 'p':
L_dma_printf41:
;Serial_Dma.c,527 :: 		sprintf(tmp,"%p",va_arg(va,void*));
=======
;Serial_Dma.c,509 :: 		case 'p':
L_dma_printf41:
;Serial_Dma.c,510 :: 		sprintf(tmp,"%p",va_arg(va,void*));
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LW	R2, 0(R3)
ADDIU	R3, SP, 20
ADDIU	SP, SP, -12
SW	R2, 8(SP)
LUI	R2, hi_addr(?lstr_10_Serial_Dma+0)
ORI	R2, R2, lo_addr(?lstr_10_Serial_Dma+0)
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_sprintf+0
NOP	
ADDIU	SP, SP, 12
<<<<<<< HEAD
;Serial_Dma.c,528 :: 		strcat(buff+j, tmp);
=======
;Serial_Dma.c,511 :: 		strcat(buff+j, tmp);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R4, SP, 20
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,529 :: 		j += strlen(tmp);
=======
;Serial_Dma.c,512 :: 		j += strlen(tmp);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, SP, 20
MOVZ	R25, R2, R0
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
<<<<<<< HEAD
;Serial_Dma.c,530 :: 		break;
=======
;Serial_Dma.c,513 :: 		break;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
<<<<<<< HEAD
;Serial_Dma.c,531 :: 		case 's':
L_dma_printf42:
;Serial_Dma.c,533 :: 		str_arg = va_arg( va, char* );
=======
;Serial_Dma.c,514 :: 		case 's':
L_dma_printf42:
;Serial_Dma.c,516 :: 		str_arg = va_arg( va, char* );
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
ADDIU	R4, SP, 16
LW	R3, 0(R4)
ADDIU	R2, R3, 4
SW	R2, 0(R4)
LW	R4, 0(R3)
SW	R4, 60(SP)
<<<<<<< HEAD
;Serial_Dma.c,534 :: 		strcat(buff+j, str_arg);
=======
;Serial_Dma.c,517 :: 		strcat(buff+j, str_arg);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
MOVZ	R26, R4, R0
MOVZ	R25, R2, R0
JAL	_strcat+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,535 :: 		j += strlen(str_arg);
=======
;Serial_Dma.c,518 :: 		j += strlen(str_arg);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LW	R25, 60(SP)
JAL	_strlen+0
NOP	
ADDU	R2, R12, R2
; j end address is: 48 (R12)
; j start address is: 8 (R2)
<<<<<<< HEAD
;Serial_Dma.c,536 :: 		break;
=======
;Serial_Dma.c,519 :: 		break;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R12, R2
; j end address is: 8 (R2)
J	L_dma_printf32
NOP	
<<<<<<< HEAD
;Serial_Dma.c,537 :: 		}
=======
;Serial_Dma.c,520 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_dma_printf31:
; j start address is: 48 (R12)
LW	R4, 1564(SP)
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 99
BNE	R3, R2, L__dma_printf115
NOP	
J	L_dma_printf33
NOP	
L__dma_printf115:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 100
BNE	R3, R2, L__dma_printf117
NOP	
J	L_dma_printf34
NOP	
L__dma_printf117:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 117
BNE	R3, R2, L__dma_printf119
NOP	
J	L_dma_printf35
NOP	
L__dma_printf119:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 108
BNE	R3, R2, L__dma_printf121
NOP	
J	L__dma_printf54
NOP	
L__dma_printf121:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 88
BNE	R3, R2, L__dma_printf123
NOP	
J	L_dma_printf37
NOP	
L__dma_printf123:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 88
BNE	R3, R2, L__dma_printf125
NOP	
J	L_dma_printf38
NOP	
L__dma_printf125:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 102
BNE	R3, R2, L__dma_printf127
NOP	
J	L_dma_printf39
NOP	
L__dma_printf127:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 70
BNE	R3, R2, L__dma_printf129
NOP	
J	L_dma_printf40
NOP	
L__dma_printf129:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 112
BNE	R3, R2, L__dma_printf131
NOP	
J	L_dma_printf41
NOP	
L__dma_printf131:
LBU	R2, 0(R4)
ANDI	R3, R2, 255
ORI	R2, R0, 115
BNE	R3, R2, L__dma_printf133
NOP	
J	L_dma_printf42
NOP	
L__dma_printf133:
; j end address is: 48 (R12)
L_dma_printf32:
<<<<<<< HEAD
;Serial_Dma.c,538 :: 		}else{
=======
;Serial_Dma.c,521 :: 		}else{
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
SEH	R3, R11
; i end address is: 44 (R11)
J	L_dma_printf43
NOP	
L_dma_printf30:
<<<<<<< HEAD
;Serial_Dma.c,539 :: 		*(buff+j) = *(str+i);
=======
;Serial_Dma.c,522 :: 		*(buff+j) = *(str+i);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; i start address is: 20 (R5)
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R4, R3, R2
SEH	R3, R5
LW	R2, 1568(SP)
ADDU	R2, R2, R3
LBU	R2, 0(R2)
SB	R2, 0(R4)
<<<<<<< HEAD
;Serial_Dma.c,540 :: 		j++;
=======
;Serial_Dma.c,523 :: 		j++;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, R12, 1
SEH	R12, R2
; j end address is: 48 (R12)
; i end address is: 20 (R5)
SEH	R3, R5
<<<<<<< HEAD
;Serial_Dma.c,541 :: 		}
L_dma_printf43:
;Serial_Dma.c,542 :: 		i++;
=======
;Serial_Dma.c,524 :: 		}
L_dma_printf43:
;Serial_Dma.c,525 :: 		i++;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; j start address is: 48 (R12)
; i start address is: 12 (R3)
ADDIU	R2, R3, 1
; i end address is: 12 (R3)
; i start address is: 20 (R5)
SEH	R5, R2
<<<<<<< HEAD
;Serial_Dma.c,543 :: 		}
=======
;Serial_Dma.c,526 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; i end address is: 20 (R5)
J	L_dma_printf28
NOP	
L_dma_printf29:
<<<<<<< HEAD
;Serial_Dma.c,544 :: 		*(buff+j+1) = 0;
=======
;Serial_Dma.c,527 :: 		*(buff+j+1) = 0;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R3, SP, 64
SEH	R2, R12
ADDU	R2, R3, R2
ADDIU	R2, R2, 1
SB	R0, 0(R2)
<<<<<<< HEAD
;Serial_Dma.c,545 :: 		strncpy(txBuf,buff,j+1);
=======
;Serial_Dma.c,528 :: 		strncpy(txBuf,buff,j+1);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, R12, 1
SEH	R27, R2
MOVZ	R26, R3, R0
LUI	R25, 40960
ORI	R25, R25, 8704
JAL	_strncpy+0
NOP	
<<<<<<< HEAD
;Serial_Dma.c,546 :: 		DCH1SSIZ    = j ;
=======
;Serial_Dma.c,529 :: 		DCH1SSIZ    = j ;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R2, R12
SW	R2, Offset(DCH1SSIZ+0)(GP)
; j end address is: 48 (R12)
SEH	R3, R12
<<<<<<< HEAD
;Serial_Dma.c,547 :: 		while(!DMA1_Enable());
=======
;Serial_Dma.c,530 :: 		while(!DMA1_Enable());
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_dma_printf44:
; j start address is: 12 (R3)
JAL	_DMA1_Enable+0
NOP	
BEQ	R2, R0, L__dma_printf134
NOP	
J	L_dma_printf45
NOP	
L__dma_printf134:
J	L_dma_printf44
NOP	
L_dma_printf45:
<<<<<<< HEAD
;Serial_Dma.c,549 :: 		return j;
SEH	R2, R3
; j end address is: 12 (R3)
;Serial_Dma.c,551 :: 		}
;Serial_Dma.c,549 :: 		return j;
;Serial_Dma.c,551 :: 		}
=======
;Serial_Dma.c,532 :: 		return j;
SEH	R2, R3
; j end address is: 12 (R3)
;Serial_Dma.c,534 :: 		}
;Serial_Dma.c,532 :: 		return j;
;Serial_Dma.c,534 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_dma_printf:
LW	R27, 12(SP)
LW	R26, 8(SP)
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 1568
JR	RA
NOP	
; end of _dma_printf
_lTrim:
<<<<<<< HEAD
;Serial_Dma.c,555 :: 		void lTrim(char *d,char* s){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Serial_Dma.c,557 :: 		int i=0,j,k;
; i start address is: 20 (R5)
MOVZ	R5, R0, R0
;Serial_Dma.c,558 :: 		k = i;
; k start address is: 24 (R6)
SEH	R6, R5
;Serial_Dma.c,559 :: 		j = strlen(s);
=======
;Serial_Dma.c,538 :: 		void lTrim(char *d,char* s){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Serial_Dma.c,540 :: 		int i=0,j,k;
; i start address is: 20 (R5)
MOVZ	R5, R0, R0
;Serial_Dma.c,541 :: 		k = i;
; k start address is: 24 (R6)
SEH	R6, R5
;Serial_Dma.c,542 :: 		j = strlen(s);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SW	R25, 4(SP)
MOVZ	R25, R26, R0
JAL	_strlen+0
NOP	
LW	R25, 4(SP)
; j start address is: 16 (R4)
SEH	R4, R2
; k end address is: 24 (R6)
; j end address is: 16 (R4)
; i end address is: 20 (R5)
SEH	R3, R6
<<<<<<< HEAD
;Serial_Dma.c,560 :: 		while(*s != '\0'){
=======
;Serial_Dma.c,543 :: 		while(*s != '\0'){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_lTrim46:
; j start address is: 16 (R4)
; k start address is: 12 (R3)
; i start address is: 20 (R5)
LBU	R2, 0(R26)
ANDI	R2, R2, 255
BNE	R2, R0, L__lTrim137
NOP	
J	L_lTrim47
NOP	
L__lTrim137:
<<<<<<< HEAD
;Serial_Dma.c,561 :: 		if((*s > 0x30)||(k>0)){
=======
;Serial_Dma.c,544 :: 		if((*s > 0x30)||(k>0)){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LBU	R2, 0(R26)
ANDI	R2, R2, 255
SLTIU	R2, R2, 49
BNE	R2, R0, L__lTrim138
NOP	
J	L__lTrim57
NOP	
L__lTrim138:
SEH	R2, R3
SLTI	R2, R2, 1
BNE	R2, R0, L__lTrim139
NOP	
J	L__lTrim56
NOP	
L__lTrim139:
J	L_lTrim50
NOP	
; k end address is: 12 (R3)
L__lTrim57:
L__lTrim56:
<<<<<<< HEAD
;Serial_Dma.c,562 :: 		k = 1;
; k start address is: 12 (R3)
ORI	R3, R0, 1
;Serial_Dma.c,563 :: 		*d = *s;
LBU	R2, 0(R26)
SB	R2, 0(R25)
;Serial_Dma.c,564 :: 		d++;
ADDIU	R2, R25, 1
MOVZ	R25, R2, R0
;Serial_Dma.c,565 :: 		}else
J	L_lTrim51
NOP	
L_lTrim50:
;Serial_Dma.c,566 :: 		i++;
=======
;Serial_Dma.c,545 :: 		k = 1;
; k start address is: 12 (R3)
ORI	R3, R0, 1
;Serial_Dma.c,546 :: 		*d = *s;
LBU	R2, 0(R26)
SB	R2, 0(R25)
;Serial_Dma.c,547 :: 		d++;
ADDIU	R2, R25, 1
MOVZ	R25, R2, R0
;Serial_Dma.c,548 :: 		}else
J	L_lTrim51
NOP	
L_lTrim50:
;Serial_Dma.c,549 :: 		i++;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ADDIU	R2, R5, 1
SEH	R5, R2
; k end address is: 12 (R3)
; i end address is: 20 (R5)
L_lTrim51:
<<<<<<< HEAD
;Serial_Dma.c,567 :: 		s++;
=======
;Serial_Dma.c,550 :: 		s++;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; k start address is: 12 (R3)
; i start address is: 20 (R5)
ADDIU	R2, R26, 1
MOVZ	R26, R2, R0
<<<<<<< HEAD
;Serial_Dma.c,568 :: 		}
=======
;Serial_Dma.c,551 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
; k end address is: 12 (R3)
J	L_lTrim46
NOP	
L_lTrim47:
<<<<<<< HEAD
;Serial_Dma.c,569 :: 		if(i == j){
=======
;Serial_Dma.c,552 :: 		if(i == j){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
SEH	R3, R5
; i end address is: 20 (R5)
SEH	R2, R4
; j end address is: 16 (R4)
BEQ	R3, R2, L__lTrim140
NOP	
J	L_lTrim52
NOP	
L__lTrim140:
<<<<<<< HEAD
;Serial_Dma.c,570 :: 		*d = '0';
ORI	R2, R0, 48
SB	R2, 0(R25)
;Serial_Dma.c,571 :: 		d++;
ADDIU	R2, R25, 1
MOVZ	R25, R2, R0
;Serial_Dma.c,572 :: 		}
L_lTrim52:
;Serial_Dma.c,573 :: 		*d = 0;
SB	R0, 0(R25)
;Serial_Dma.c,574 :: 		}
=======
;Serial_Dma.c,553 :: 		*d = '0';
ORI	R2, R0, 48
SB	R2, 0(R25)
;Serial_Dma.c,554 :: 		d++;
ADDIU	R2, R25, 1
MOVZ	R25, R2, R0
;Serial_Dma.c,555 :: 		}
L_lTrim52:
;Serial_Dma.c,556 :: 		*d = 0;
SB	R0, 0(R25)
;Serial_Dma.c,557 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_lTrim:
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of _lTrim
