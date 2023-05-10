_PinMode:
;Config.c,3 :: 		void PinMode(){
ADDIU	SP, SP, -16
SW	RA, 0(SP)
;Config.c,6 :: 		DI();
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
DI	R30
;Config.c,10 :: 		SYSKEY = 0xAA996655;
LUI	R2, 43673
ORI	R2, R2, 26197
SW	R2, Offset(SYSKEY+0)(GP)
;Config.c,11 :: 		SYSKEY = 0x556699AA;
LUI	R2, 21862
ORI	R2, R2, 39338
SW	R2, Offset(SYSKEY+0)(GP)
;Config.c,12 :: 		CFGCONbits.OCACLK = 1;
LBU	R2, Offset(CFGCONbits+2)(GP)
ORI	R2, R2, 1
SB	R2, Offset(CFGCONbits+2)(GP)
;Config.c,13 :: 		SYSKEY = 0x33333333;
LUI	R2, 13107
ORI	R2, R2, 13107
SW	R2, Offset(SYSKEY+0)(GP)
;Config.c,17 :: 		JTAGEN_bit = 0;
_LX	
INS	R2, R0, BitPos(JTAGEN_bit+0), 1
_SX	
;Config.c,18 :: 		Delay_ms(100);
LUI	R24, 101
ORI	R24, R24, 47530
L_PinMode0:
ADDIU	R24, R24, -1
BNE	R24, R0, L_PinMode0
NOP	
;Config.c,22 :: 		ANSELA = 0X0000;
SW	R0, Offset(ANSELA+0)(GP)
;Config.c,23 :: 		TRISA  = 0X0000;
SW	R0, Offset(TRISA+0)(GP)
;Config.c,24 :: 		ANSELB = 0X0000;
SW	R0, Offset(ANSELB+0)(GP)
;Config.c,25 :: 		TRISB  = 0X0000;
SW	R0, Offset(TRISB+0)(GP)
;Config.c,26 :: 		ANSELC = 0X0000;
SW	R0, Offset(ANSELC+0)(GP)
;Config.c,27 :: 		TRISC  = 0X0000;
SW	R0, Offset(TRISC+0)(GP)
;Config.c,28 :: 		ANSELD = 0X0000;
SW	R0, Offset(ANSELD+0)(GP)
;Config.c,29 :: 		TRISD  = 0X0000;
SW	R0, Offset(TRISD+0)(GP)
;Config.c,30 :: 		ANSELE = 0X0000;
SW	R0, Offset(ANSELE+0)(GP)
;Config.c,31 :: 		TRISE  = 0X0000;
SW	R0, Offset(TRISE+0)(GP)
;Config.c,32 :: 		ANSELG = 0X0000;
SW	R0, Offset(ANSELG+0)(GP)
;Config.c,33 :: 		TRISG  = 0X0000;
SW	R0, Offset(TRISG+0)(GP)
;Config.c,35 :: 		CNPUB  = 0;//1 << 15;
SW	R0, Offset(CNPUB+0)(GP)
;Config.c,36 :: 		CNPUF  = 0;//1 << 3;
SW	R0, Offset(CNPUF+0)(GP)
;Config.c,39 :: 		LED1_Dir = 0;
_LX	
INS	R2, R0, BitPos(LED1_Dir+0), 1
_SX	
;Config.c,40 :: 		LED2_Dir = 0;
_LX	
INS	R2, R0, BitPos(LED2_Dir+0), 1
_SX	
;Config.c,44 :: 		SW1_Dir = 1;
_LX	
ORI	R2, R2, BitMask(SW1_Dir+0)
_SX	
;Config.c,45 :: 		SW2_Dir = 1;
_LX	
ORI	R2, R2, BitMask(SW2_Dir+0)
_SX	
;Config.c,47 :: 		TRISG7_bit = 1;
LUI	R2, BitMask(TRISG7_bit+0)
ORI	R2, R2, BitMask(TRISG7_bit+0)
_SX	
;Config.c,48 :: 		TRISG8_bit = 1;
LUI	R2, BitMask(TRISG8_bit+0)
ORI	R2, R2, BitMask(TRISG8_bit+0)
_SX	
;Config.c,51 :: 		set_performance_mode();
JAL	_set_performance_mode+0
NOP	
;Config.c,55 :: 		Unlock_IOLOCK();
JAL	_Unlock_IOLOCK+0
NOP	
;Config.c,57 :: 		PPS_Mapping_NoLock(_RPD15, _OUTPUT, _U1TX);    // Sets pin PORTD.B15 to be Output and maps UART1 Transmit
ORI	R27, R0, 1
MOVZ	R26, R0, R0
ORI	R25, R0, 27
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,58 :: 		PPS_Mapping_NoLock(_RPD14, _INPUT,  _U1RX);    // Sets pin PORTE.B14 to be Input and maps UART1 Receive
ORI	R27, R0, 5
ORI	R26, R0, 1
ORI	R25, R0, 11
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,60 :: 		PPS_Mapping_NoLock(_RPE8, _OUTPUT, _U2TX);    // Sets pin PORTE.B8 to be Output and maps UART2 Transmit
ORI	R27, R0, 2
MOVZ	R26, R0, R0
ORI	R25, R0, 61
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,61 :: 		PPS_Mapping_NoLock(_RPE9, _INPUT,  _U2RX);    // Sets pin PORTE.B9 to be Input and maps UART2 Receive
ORI	R27, R0, 32
ORI	R26, R0, 1
ORI	R25, R0, 45
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,63 :: 		PPS_Mapping_NoLock(_RPA14, _OUTPUT, _U3TX);    // Sets pin PORTE.B8 to be Output and maps UART2 Transmit
ORI	R27, R0, 1
MOVZ	R26, R0, R0
ORI	R25, R0, 13
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,64 :: 		PPS_Mapping_NoLock(_RPF5, _INPUT,  _U3RX);    // Sets pin PORTE.B9 to be Input and maps UART2 Receive
ORI	R27, R0, 19
ORI	R26, R0, 1
ORI	R25, R0, 18
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,66 :: 		PPS_Mapping_NoLock(_RPB9, _OUTPUT, _NULL);
MOVZ	R27, R0, R0
MOVZ	R26, R0, R0
ORI	R25, R0, 5
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,67 :: 		PPS_Mapping_NoLock(_RPB10, _OUTPUT, _NULL);
MOVZ	R27, R0, R0
MOVZ	R26, R0, R0
ORI	R25, R0, 6
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,80 :: 		PPS_Mapping_NoLock(_RPF3, _INPUT, _INT1);     //X_Min_Limit
ORI	R27, R0, 39
ORI	R26, R0, 1
ORI	R25, R0, 56
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,81 :: 		PPS_Mapping_NoLock(_RPB15, _INPUT, _INT2);    //Y_Min_Limit
ORI	R27, R0, 25
ORI	R26, R0, 1
ORI	R25, R0, 35
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,82 :: 		PPS_Mapping_NoLock(_RPB1, _INPUT, _INT4);    //Z_Min_Limit
ORI	R27, R0, 14
ORI	R26, R0, 1
ORI	R25, R0, 21
JAL	_PPS_Mapping_NoLock+0
NOP	
;Config.c,83 :: 		Lock_IOLOCK();
JAL	_Lock_IOLOCK+0
NOP	
;Config.c,87 :: 		MM_Init();
JAL	_MM_Init+0
NOP	
;Config.c,91 :: 		InitTimer1();
JAL	_InitTimer1+0
NOP	
;Config.c,97 :: 		Init_Steppers();
JAL	_Init_Steppers+0
NOP	
;Config.c,110 :: 		DMA_global();
JAL	_DMA_global+0
NOP	
;Config.c,111 :: 		DMA0_Enable();
JAL	_DMA0_Enable+0
NOP	
;Config.c,116 :: 		UartConfig();
JAL	_UartConfig+0
NOP	
;Config.c,120 :: 		Uart1InterruptSetup();
JAL	_Uart1InterruptSetup+0
NOP	
;Config.c,124 :: 		EnableInterrupts();
EI	R30
;Config.c,130 :: 		}
L_end_PinMode:
LW	R27, 12(SP)
LW	R26, 8(SP)
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 16
JR	RA
NOP	
; end of _PinMode
_UartConfig:
;Config.c,132 :: 		void UartConfig(){
ADDIU	SP, SP, -20
SW	RA, 0(SP)
;Config.c,135 :: 		UART1_Init_Advanced(115200, 200000/*PBClk x 2*/, _UART_LOW_SPEED, _UART_8BIT_NOPARITY, _UART_ONE_STOPBIT);
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
SW	R28, 16(SP)
MOVZ	R28, R0, R0
ORI	R27, R0, 1
LUI	R26, 3
ORI	R26, R26, 3392
LUI	R25, 1
ORI	R25, R25, 49664
ADDIU	SP, SP, -4
SB	R0, 0(SP)
JAL	_UART1_Init_Advanced+0
NOP	
ADDIU	SP, SP, 4
;Config.c,136 :: 		UART_Set_Active(&UART1_Read, &UART1_Write, &UART1_Data_Ready, &UART1_Tx_Idle);
LUI	R28, hi_addr(_UART1_Tx_Idle+0)
ORI	R28, R28, lo_addr(_UART1_Tx_Idle+0)
LUI	R27, hi_addr(_UART1_Data_Ready+0)
ORI	R27, R27, lo_addr(_UART1_Data_Ready+0)
LUI	R26, hi_addr(_UART1_Write+0)
ORI	R26, R26, lo_addr(_UART1_Write+0)
LUI	R25, hi_addr(_UART1_Read+0)
ORI	R25, R25, lo_addr(_UART1_Read+0)
JAL	_UART_Set_Active+0
NOP	
;Config.c,137 :: 		Delay_ms(10);                  // Wait for UART module to stabilize
LUI	R24, 10
ORI	R24, R24, 11306
L_UartConfig2:
ADDIU	R24, R24, -1
BNE	R24, R0, L_UartConfig2
NOP	
;Config.c,141 :: 		UART2_Init_Advanced(115200, 200000/*PBClk x 2*/, _UART_LOW_SPEED, _UART_8BIT_NOPARITY, _UART_ONE_STOPBIT);
MOVZ	R28, R0, R0
ORI	R27, R0, 1
LUI	R26, 3
ORI	R26, R26, 3392
LUI	R25, 1
ORI	R25, R25, 49664
ADDIU	SP, SP, -4
SB	R0, 0(SP)
JAL	_UART2_Init_Advanced+0
NOP	
ADDIU	SP, SP, 4
;Config.c,142 :: 		UART_Set_Active(&UART2_Read, &UART2_Write, &UART2_Data_Ready, &UART2_Tx_Idle); // set UART2 active
LUI	R28, hi_addr(_UART2_Tx_Idle+0)
ORI	R28, R28, lo_addr(_UART2_Tx_Idle+0)
LUI	R27, hi_addr(_UART2_Data_Ready+0)
ORI	R27, R27, lo_addr(_UART2_Data_Ready+0)
LUI	R26, hi_addr(_UART2_Write+0)
ORI	R26, R26, lo_addr(_UART2_Write+0)
LUI	R25, hi_addr(_UART2_Read+0)
ORI	R25, R25, lo_addr(_UART2_Read+0)
JAL	_UART_Set_Active+0
NOP	
;Config.c,143 :: 		Delay_ms(10);                  // Wait for UART module to stabilize
LUI	R24, 10
ORI	R24, R24, 11306
L_UartConfig4:
ADDIU	R24, R24, -1
BNE	R24, R0, L_UartConfig4
NOP	
;Config.c,145 :: 		}
L_end_UartConfig:
LW	R28, 16(SP)
LW	R27, 12(SP)
LW	R26, 8(SP)
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 20
JR	RA
NOP	
; end of _UartConfig
_Uart1InterruptSetup:
;Config.c,154 :: 		void Uart1InterruptSetup(){
;Config.c,157 :: 		UTXISEL0_bit =  0 ;
LUI	R2, BitMask(UTXISEL0_bit+0)
ORI	R2, R2, BitMask(UTXISEL0_bit+0)
_SX	
;Config.c,158 :: 		UTXISEL1_bit =  0 ;
LUI	R2, BitMask(UTXISEL1_bit+0)
ORI	R2, R2, BitMask(UTXISEL1_bit+0)
_SX	
;Config.c,161 :: 		URXISEL0_bit =  0 ;
LUI	R2, BitMask(URXISEL0_bit+0)
ORI	R2, R2, BitMask(URXISEL0_bit+0)
_SX	
;Config.c,162 :: 		URXISEL1_bit =  0 ;
LUI	R2, BitMask(URXISEL1_bit+0)
ORI	R2, R2, BitMask(URXISEL1_bit+0)
_SX	
;Config.c,165 :: 		IPC28CLR     = 0x1F00;
ORI	R2, R0, 7936
SW	R2, Offset(IPC28CLR+0)(GP)
;Config.c,167 :: 		IPC28SET      = 0x1400;
ORI	R2, R0, 5120
SW	R2, Offset(IPC28SET+0)(GP)
;Config.c,169 :: 		IEC3SET       = 0x20000;
LUI	R2, 2
SW	R2, Offset(IEC3SET+0)(GP)
;Config.c,170 :: 		IFS3CLR       = 0x20000;
LUI	R2, 2
SW	R2, Offset(IFS3CLR+0)(GP)
;Config.c,172 :: 		}
L_end_Uart1InterruptSetup:
JR	RA
NOP	
; end of _Uart1InterruptSetup
_set_performance_mode:
;Config.c,174 :: 		void set_performance_mode(){
;Config.c,178 :: 		DI(); // Disable all interrupts
DI	R30
;Config.c,183 :: 		INTCONSET = 0x00001400;
ORI	R2, R0, 5120
SW	R2, Offset(INTCONSET+0)(GP)
;Config.c,185 :: 		SYSKEY = 0xAA996655;
LUI	R2, 43673
ORI	R2, R2, 26197
SW	R2, Offset(SYSKEY+0)(GP)
;Config.c,186 :: 		SYSKEY = 0x556699AA;
LUI	R2, 21862
ORI	R2, R2, 39338
SW	R2, Offset(SYSKEY+0)(GP)
;Config.c,189 :: 		PB1DIVbits.PBDIV = 1; // Peripheral Bus 1 Clock Divisor Control (PBCLK1 is SYSCLK divided by 2)
ORI	R3, R0, 1
LBU	R2, Offset(PB1DIVbits+0)(GP)
INS	R2, R3, 0, 7
SB	R2, Offset(PB1DIVbits+0)(GP)
;Config.c,192 :: 		UEN0_bit = 1;
LUI	R2, BitMask(UEN0_bit+0)
ORI	R2, R2, BitMask(UEN0_bit+0)
_SX	
;Config.c,193 :: 		UEN1_bit = 1;
LUI	R2, BitMask(UEN1_bit+0)
ORI	R2, R2, BitMask(UEN1_bit+0)
_SX	
;Config.c,194 :: 		PB2DIVbits.ON = 1; // Peripheral Bus 2 Output Clock Enable (Output clock is enabled)
ORI	R2, R0, 32768
SWR	R2, Offset(PB2DIVbits+8)(GP)
SWL	R2, Offset(PB2DIVbits+11)(GP)
;Config.c,195 :: 		while(!PB2DIVbits.PBDIVRDY);
L_set_performance_mode6:
LBU	R2, Offset(PB2DIVbits+1)(GP)
EXT	R2, R2, 3, 1
BEQ	R2, R0, L__set_performance_mode22
NOP	
J	L_set_performance_mode7
NOP	
L__set_performance_mode22:
J	L_set_performance_mode6
NOP	
L_set_performance_mode7:
;Config.c,196 :: 		PB2DIVbits.PBDIV = 0x07; // Peripheral Bus 2 Clock Divisor Control (PBCLK2 is SYSCLK "200MHZ" / 8)
ORI	R3, R0, 7
LBU	R2, Offset(PB2DIVbits+0)(GP)
INS	R2, R3, 0, 7
SB	R2, Offset(PB2DIVbits+0)(GP)
;Config.c,199 :: 		PB3DIVbits.ON = 1; // Peripheral Bus 2 Output Clock Enable (Output clock is enabled)
ORI	R2, R0, 32768
SWR	R2, Offset(PB3DIVbits+8)(GP)
SWL	R2, Offset(PB3DIVbits+11)(GP)
;Config.c,200 :: 		while(!PB3DIVbits.PBDIVRDY);
L_set_performance_mode8:
LBU	R2, Offset(PB3DIVbits+1)(GP)
EXT	R2, R2, 3, 1
BEQ	R2, R0, L__set_performance_mode23
NOP	
J	L_set_performance_mode9
NOP	
L__set_performance_mode23:
J	L_set_performance_mode8
NOP	
L_set_performance_mode9:
;Config.c,201 :: 		PB3DIVbits.PBDIV = 3; // Peripheral Bus 3 Clock Divisor Control (PBCLK3 is SYSCLK divided by 4)
ORI	R3, R0, 3
LBU	R2, Offset(PB3DIVbits+0)(GP)
INS	R2, R3, 0, 7
SB	R2, Offset(PB3DIVbits+0)(GP)
;Config.c,204 :: 		PB4DIVbits.ON = 1; // Peripheral Bus 4 Output Clock Enable (Output clock is enabled)
ORI	R2, R0, 32768
SWR	R2, Offset(PB4DIVbits+8)(GP)
SWL	R2, Offset(PB4DIVbits+11)(GP)
;Config.c,205 :: 		while (!PB4DIVbits.PBDIVRDY); // Wait until it is ready to write to
L_set_performance_mode10:
LBU	R2, Offset(PB4DIVbits+1)(GP)
EXT	R2, R2, 3, 1
BEQ	R2, R0, L__set_performance_mode24
NOP	
J	L_set_performance_mode11
NOP	
L__set_performance_mode24:
J	L_set_performance_mode10
NOP	
L_set_performance_mode11:
;Config.c,206 :: 		PB4DIVbits.PBDIV = 0; // Peripheral Bus 4 Clock Divisor Control (PBCLK4 is SYSCLK divided by 1)
ORI	R2, R0, 127
SB	R2, Offset(PB4DIVbits+4)(GP)
;Config.c,209 :: 		PB5DIVbits.ON = 1; // Peripheral Bus 5 Output Clock Enable (Output clock is enabled)
ORI	R2, R0, 32768
SWR	R2, Offset(PB5DIVbits+8)(GP)
SWL	R2, Offset(PB5DIVbits+11)(GP)
;Config.c,210 :: 		while(!PB5DIVbits.PBDIVRDY);
L_set_performance_mode12:
LBU	R2, Offset(PB5DIVbits+1)(GP)
EXT	R2, R2, 3, 1
BEQ	R2, R0, L__set_performance_mode25
NOP	
J	L_set_performance_mode13
NOP	
L__set_performance_mode25:
J	L_set_performance_mode12
NOP	
L_set_performance_mode13:
;Config.c,211 :: 		PB5DIVbits.PBDIV = 1; // Peripheral Bus 5 Clock Divisor Control (PBCLK5 is SYSCLK divided by 2)
ORI	R3, R0, 1
LBU	R2, Offset(PB5DIVbits+0)(GP)
INS	R2, R3, 0, 7
SB	R2, Offset(PB5DIVbits+0)(GP)
;Config.c,214 :: 		PB7DIVbits.ON = 1; // Peripheral Bus 7 Output Clock Enable (Output clock is enabled)
ORI	R2, R0, 32768
SWR	R2, Offset(PB7DIVbits+8)(GP)
SWL	R2, Offset(PB7DIVbits+11)(GP)
;Config.c,215 :: 		while(!PB7DIVbits.PBDIVRDY);
L_set_performance_mode14:
LBU	R2, Offset(PB7DIVbits+1)(GP)
EXT	R2, R2, 3, 1
BEQ	R2, R0, L__set_performance_mode26
NOP	
J	L_set_performance_mode15
NOP	
L__set_performance_mode26:
J	L_set_performance_mode14
NOP	
L_set_performance_mode15:
;Config.c,216 :: 		PB7DIVbits.PBDIV = 0; // Peripheral Bus 7 Clock Divisor Control (PBCLK7 is SYSCLK divided by 1)
ORI	R2, R0, 127
SB	R2, Offset(PB7DIVbits+4)(GP)
;Config.c,219 :: 		PB8DIVbits.ON = 1; // Peripheral Bus 8 Output Clock Enable (Output clock is enabled)
ORI	R2, R0, 32768
SWR	R2, Offset(PB8DIVbits+8)(GP)
SWL	R2, Offset(PB8DIVbits+11)(GP)
;Config.c,220 :: 		while(!PB8DIVbits.PBDIVRDY);
L_set_performance_mode16:
LBU	R2, Offset(PB8DIVbits+1)(GP)
EXT	R2, R2, 3, 1
BEQ	R2, R0, L__set_performance_mode27
NOP	
J	L_set_performance_mode17
NOP	
L__set_performance_mode27:
J	L_set_performance_mode16
NOP	
L_set_performance_mode17:
;Config.c,221 :: 		PB8DIVbits.PBDIV = 1; // Peripheral Bus 8 Clock Divisor Control (PBCLK8 is SYSCLK divided by 2)
ORI	R3, R0, 1
LBU	R2, Offset(PB8DIVbits+0)(GP)
INS	R2, R3, 0, 7
SB	R2, Offset(PB8DIVbits+0)(GP)
;Config.c,224 :: 		PRECONbits.PFMSECEN = 0; // Flash SEC Interrupt Enable (Do not generate an interrupt when the PFMSEC bit is set)
LUI	R2, 1024
SW	R2, Offset(PRECONbits+4)(GP)
;Config.c,225 :: 		PRECONbits.PREFEN = 0b11; // Predictive Prefetch Enable (Enable predictive prefetch for any address)
ORI	R2, R0, 48
SB	R2, Offset(PRECONbits+8)(GP)
;Config.c,226 :: 		PRECONbits.PFMWS = 0b100; // PFM Access Time Defined in Terms of SYSCLK Wait States (Two wait states)
ORI	R3, R0, 4
LBU	R2, Offset(PRECONbits+0)(GP)
INS	R2, R3, 0, 3
SB	R2, Offset(PRECONbits+0)(GP)
;Config.c,230 :: 		cp0 = CP0_GET(CP0_CONFIG);
MFC0	R30, 16, 0
MOVZ	R2, R30, R0
; cp0 start address is: 12 (R3)
MOVZ	R3, R2, R0
;Config.c,231 :: 		cp0 &= ~0x07;
LUI	R2, 65535
ORI	R2, R2, 65528
AND	R2, R3, R2
; cp0 end address is: 12 (R3)
;Config.c,232 :: 		cp0 |= 0b011; // K0 = Cacheable, non-coherent, write-back, write allocate
ORI	R2, R2, 3
;Config.c,233 :: 		CP0_SET(CP0_CONFIG,cp0);
MOVZ	R30, R2, R0
MTC0	R30, 16, 0
;Config.c,236 :: 		SYSKEY = 0x33333333;
LUI	R2, 13107
ORI	R2, R2, 13107
SW	R2, Offset(SYSKEY+0)(GP)
;Config.c,237 :: 		PRISS = 0x76543210;   //SRS all priorities
LUI	R2, 30292
ORI	R2, R2, 12816
SW	R2, Offset(PRISS+0)(GP)
;Config.c,239 :: 		PREFEN0_bit = 1;
LUI	R2, BitMask(PREFEN0_bit+0)
ORI	R2, R2, BitMask(PREFEN0_bit+0)
_SX	
;Config.c,240 :: 		PREFEN1_bit = 1;
LUI	R2, BitMask(PREFEN1_bit+0)
ORI	R2, R2, BitMask(PREFEN1_bit+0)
_SX	
;Config.c,241 :: 		PFMWS0_bit = 0;
LUI	R2, BitMask(PFMWS0_bit+0)
ORI	R2, R2, BitMask(PFMWS0_bit+0)
_SX	
;Config.c,242 :: 		PFMWS1_bit = 1;
LUI	R2, BitMask(PFMWS1_bit+0)
ORI	R2, R2, BitMask(PFMWS1_bit+0)
_SX	
;Config.c,243 :: 		PFMWS2_bit = 0;
LUI	R2, BitMask(PFMWS2_bit+0)
ORI	R2, R2, BitMask(PFMWS2_bit+0)
_SX	
;Config.c,244 :: 		}
L_end_set_performance_mode:
JR	RA
NOP	
; end of _set_performance_mode
_OutPutPulseXYZ:
;Config.c,247 :: 		void OutPutPulseXYZ(){
;Config.c,253 :: 		OC5CON = 0x0000; // disable OC3 module |_using TMR2_3 in 32bit mode
SW	R0, Offset(OC5CON+0)(GP)
;Config.c,254 :: 		OC2CON = 0x0000; // disable OC5 module |
SW	R0, Offset(OC2CON+0)(GP)
;Config.c,255 :: 		OC7CON = 0X0000; // disable OC8 module |_using tmr6
SW	R0, Offset(OC7CON+0)(GP)
;Config.c,256 :: 		OC3CON = 0x0000; // disable OC3 module |_using TMR2_3 in 32bit mode
SW	R0, Offset(OC3CON+0)(GP)
;Config.c,257 :: 		OC6CON = 0x0000; // disable OC5 module |
SW	R0, Offset(OC6CON+0)(GP)
;Config.c,258 :: 		OC8CON = 0X0000; // disable OC8 module |_using tmr6
SW	R0, Offset(OC8CON+0)(GP)
;Config.c,261 :: 		T2CON  = 0x0000;  // disable Timer2  OC5
SW	R0, Offset(T2CON+0)(GP)
;Config.c,262 :: 		T3CON  = 0x0000;  // disable Timer4  OC3
SW	R0, Offset(T3CON+0)(GP)
;Config.c,263 :: 		T4CON  = 0x0000;  // disable Timer6  OC8
SW	R0, Offset(T4CON+0)(GP)
;Config.c,264 :: 		T5CON  = 0x0000;  // disable Timer2  OC5
SW	R0, Offset(T5CON+0)(GP)
;Config.c,265 :: 		T6CON  = 0x0000;  // disable Timer4  OC3
SW	R0, Offset(T6CON+0)(GP)
;Config.c,266 :: 		T7CON  = 0x0000;  // disable Timer6  OC8
SW	R0, Offset(T7CON+0)(GP)
;Config.c,268 :: 		T2CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
ORI	R2, R0, 48
SW	R2, Offset(T2CON+0)(GP)
;Config.c,269 :: 		T3CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
ORI	R2, R0, 48
SW	R2, Offset(T3CON+0)(GP)
;Config.c,270 :: 		T4CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
ORI	R2, R0, 48
SW	R2, Offset(T4CON+0)(GP)
;Config.c,271 :: 		T5CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
ORI	R2, R0, 48
SW	R2, Offset(T5CON+0)(GP)
;Config.c,272 :: 		T6CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
ORI	R2, R0, 48
SW	R2, Offset(T6CON+0)(GP)
;Config.c,273 :: 		T7CON  = 0x0030;  //   a prescaler of 1:8 to get 1.28usec tick resolution
ORI	R2, R0, 48
SW	R2, Offset(T7CON+0)(GP)
;Config.c,276 :: 		PR2    = 0xFFFF;   //OC5
ORI	R2, R0, 65535
SW	R2, Offset(PR2+0)(GP)
;Config.c,277 :: 		PR3    = 0xFFFF;   //OC6
ORI	R2, R0, 65535
SW	R2, Offset(PR3+0)(GP)
;Config.c,278 :: 		PR4    = 0xFFFF;   //OC2
ORI	R2, R0, 65535
SW	R2, Offset(PR4+0)(GP)
;Config.c,279 :: 		PR5    = 0xFFFF;   //OC3
ORI	R2, R0, 65535
SW	R2, Offset(PR5+0)(GP)
;Config.c,280 :: 		PR6    = 0xFFFF;   //OC7
ORI	R2, R0, 65535
SW	R2, Offset(PR6+0)(GP)
;Config.c,281 :: 		PR7    = 0xFFFF;   //OC8
ORI	R2, R0, 65535
SW	R2, Offset(PR7+0)(GP)
;Config.c,284 :: 		OC5CON = 0x0004; //X Conf OC5 module for dual single Pulse output 16bit tmr2
ORI	R2, R0, 4
SW	R2, Offset(OC5CON+0)(GP)
;Config.c,285 :: 		OC2CON = 0x0004; //Y Conf OC2 module for dual single Pulse output 16bit tmr4
ORI	R2, R0, 4
SW	R2, Offset(OC2CON+0)(GP)
;Config.c,286 :: 		OC7CON = 0x0004; //Z Conf OC7 module for dual single Pulse output 16bit tmr6
ORI	R2, R0, 4
SW	R2, Offset(OC7CON+0)(GP)
;Config.c,287 :: 		OC3CON = 0x000C; //A Conf OC3 module for dual single Pulse output 16bit tmr5
ORI	R2, R0, 12
SW	R2, Offset(OC3CON+0)(GP)
;Config.c,288 :: 		OC6CON = 0x000C; //B Conf OC6 module for dual single Pulse output 16bit tmr3
ORI	R2, R0, 12
SW	R2, Offset(OC6CON+0)(GP)
;Config.c,289 :: 		OC8CON = 0x000C; //C Conf OC8 module for dual single Pulse output 16bit tmr7
ORI	R2, R0, 12
SW	R2, Offset(OC8CON+0)(GP)
;Config.c,297 :: 		OC5R   = 0x5;        // X_Axis Initialize Compare Register 1
ORI	R2, R0, 5
SW	R2, Offset(OC5R+0)(GP)
;Config.c,298 :: 		OC5RS  = 0x234;      // X_Axis Initialize Secondary Compare Register 1
ORI	R2, R0, 564
SW	R2, Offset(OC5RS+0)(GP)
;Config.c,299 :: 		OC2R   = 0x5;        // Y_Axis Initialize Compare Register 1
ORI	R2, R0, 5
SW	R2, Offset(OC2R+0)(GP)
;Config.c,300 :: 		OC2RS  = 0x234;      // Y_Axis Initialize Secondary Compare Register 1
ORI	R2, R0, 564
SW	R2, Offset(OC2RS+0)(GP)
;Config.c,301 :: 		OC7R   = 0x5;        // Z_Axis Initialize Compare Register 1
ORI	R2, R0, 5
SW	R2, Offset(OC7R+0)(GP)
;Config.c,302 :: 		OC7RS  = 0x234;      // Z_Axis Initialize Secondary Compare Register 1
ORI	R2, R0, 564
SW	R2, Offset(OC7RS+0)(GP)
;Config.c,303 :: 		OC3R   = 0x5;        // A_Axis Initialize Compare Register 1
ORI	R2, R0, 5
SW	R2, Offset(OC3R+0)(GP)
;Config.c,304 :: 		OC3RS  = 0x234;      // A_Axis Initialize Secondary Compare Register 1
ORI	R2, R0, 564
SW	R2, Offset(OC3RS+0)(GP)
;Config.c,305 :: 		OC6R   = 0x5;        // B_Axis Initialize Compare Register 1
ORI	R2, R0, 5
SW	R2, Offset(OC6R+0)(GP)
;Config.c,306 :: 		OC6RS  = 0x234;      // B_Axis Initialize Secondary Compare Register 1
ORI	R2, R0, 564
SW	R2, Offset(OC6RS+0)(GP)
;Config.c,307 :: 		OC8R   = 0x5;        // C_Axis Initialize Compare Register 1
ORI	R2, R0, 5
SW	R2, Offset(OC8R+0)(GP)
;Config.c,308 :: 		OC8RS  = 0x234;      // C_Axis Initialize Secondary Compare Register 1
ORI	R2, R0, 564
SW	R2, Offset(OC8RS+0)(GP)
;Config.c,312 :: 		OC5IP0_bit = 1;  // Set OC5 interrupt priority to 3
LUI	R2, BitMask(OC5IP0_bit+0)
ORI	R2, R2, BitMask(OC5IP0_bit+0)
_SX	
;Config.c,313 :: 		OC5IP1_bit = 1;
LUI	R2, BitMask(OC5IP1_bit+0)
ORI	R2, R2, BitMask(OC5IP1_bit+0)
_SX	
;Config.c,314 :: 		OC5IP2_bit = 0;
LUI	R2, BitMask(OC5IP2_bit+0)
ORI	R2, R2, BitMask(OC5IP2_bit+0)
_SX	
;Config.c,315 :: 		OC5IS0_bit = 0;  // Set OC5 sub priority 0
LUI	R2, BitMask(OC5IS0_bit+0)
ORI	R2, R2, BitMask(OC5IS0_bit+0)
_SX	
;Config.c,316 :: 		OC5IS1_bit = 0;
LUI	R2, BitMask(OC5IS1_bit+0)
ORI	R2, R2, BitMask(OC5IS1_bit+0)
_SX	
;Config.c,317 :: 		OC5IF_bit  = 0;  // reset interrupt flag
LUI	R2, BitMask(OC5IF_bit+0)
ORI	R2, R2, BitMask(OC5IF_bit+0)
_SX	
;Config.c,318 :: 		OC5IE_bit  = 0;  // enable interrupt
LUI	R2, BitMask(OC5IE_bit+0)
ORI	R2, R2, BitMask(OC5IE_bit+0)
_SX	
;Config.c,321 :: 		OC2IP0_bit = 1;  // Set OC3 interrupt priority to 3
LUI	R2, BitMask(OC2IP0_bit+0)
ORI	R2, R2, BitMask(OC2IP0_bit+0)
_SX	
;Config.c,322 :: 		OC2IP1_bit = 1;
LUI	R2, BitMask(OC2IP1_bit+0)
ORI	R2, R2, BitMask(OC2IP1_bit+0)
_SX	
;Config.c,323 :: 		OC2IP2_bit = 0;
LUI	R2, BitMask(OC2IP2_bit+0)
ORI	R2, R2, BitMask(OC2IP2_bit+0)
_SX	
;Config.c,324 :: 		OC2IS0_bit = 1;  // Set OC3 sub priority 1
LUI	R2, BitMask(OC2IS0_bit+0)
ORI	R2, R2, BitMask(OC2IS0_bit+0)
_SX	
;Config.c,325 :: 		OC2IS1_bit = 0;
LUI	R2, BitMask(OC2IS1_bit+0)
ORI	R2, R2, BitMask(OC2IS1_bit+0)
_SX	
;Config.c,326 :: 		OC2IF_bit  = 0;   // reset interrupt flag
LUI	R2, BitMask(OC2IF_bit+0)
ORI	R2, R2, BitMask(OC2IF_bit+0)
_SX	
;Config.c,327 :: 		OC2IE_bit  = 0;   // enable interrupt
LUI	R2, BitMask(OC2IE_bit+0)
ORI	R2, R2, BitMask(OC2IE_bit+0)
_SX	
;Config.c,330 :: 		OC7IP0_bit = 1;  // Set OC8 interrupt priority to 6
LUI	R2, BitMask(OC7IP0_bit+0)
ORI	R2, R2, BitMask(OC7IP0_bit+0)
_SX	
;Config.c,331 :: 		OC7IP1_bit = 1;
LUI	R2, BitMask(OC7IP1_bit+0)
ORI	R2, R2, BitMask(OC7IP1_bit+0)
_SX	
;Config.c,332 :: 		OC7IP2_bit = 0;
LUI	R2, BitMask(OC7IP2_bit+0)
ORI	R2, R2, BitMask(OC7IP2_bit+0)
_SX	
;Config.c,333 :: 		OC7IS0_bit = 0;  // Set OC8 sub priority 2
LUI	R2, BitMask(OC7IS0_bit+0)
ORI	R2, R2, BitMask(OC7IS0_bit+0)
_SX	
;Config.c,334 :: 		OC7IS1_bit = 1;
LUI	R2, BitMask(OC7IS1_bit+0)
ORI	R2, R2, BitMask(OC7IS1_bit+0)
_SX	
;Config.c,335 :: 		OC7IF_bit  = 0;  // reset interrupt flag
LUI	R2, BitMask(OC7IF_bit+0)
ORI	R2, R2, BitMask(OC7IF_bit+0)
_SX	
;Config.c,336 :: 		OC7IE_bit  = 0;  // enable interrupt
LUI	R2, BitMask(OC7IE_bit+0)
ORI	R2, R2, BitMask(OC7IE_bit+0)
_SX	
;Config.c,339 :: 		OC3IP0_bit = 1;  // Set OC3 interrupt priority to 6
LUI	R2, BitMask(OC3IP0_bit+0)
ORI	R2, R2, BitMask(OC3IP0_bit+0)
_SX	
;Config.c,340 :: 		OC3IP1_bit = 1;
LUI	R2, BitMask(OC3IP1_bit+0)
ORI	R2, R2, BitMask(OC3IP1_bit+0)
_SX	
;Config.c,341 :: 		OC3IP2_bit = 0;
LUI	R2, BitMask(OC3IP2_bit+0)
ORI	R2, R2, BitMask(OC3IP2_bit+0)
_SX	
;Config.c,342 :: 		OC3IS0_bit = 1;  // Set OC3 sub priority 3
LUI	R2, BitMask(OC3IS0_bit+0)
ORI	R2, R2, BitMask(OC3IS0_bit+0)
_SX	
;Config.c,343 :: 		OC3IS1_bit = 1;
LUI	R2, BitMask(OC3IS1_bit+0)
ORI	R2, R2, BitMask(OC3IS1_bit+0)
_SX	
;Config.c,344 :: 		OC3IF_bit  = 0;   // reset interrupt flag
LUI	R2, BitMask(OC3IF_bit+0)
ORI	R2, R2, BitMask(OC3IF_bit+0)
_SX	
;Config.c,345 :: 		OC3IE_bit  = 0;   // enable interrupt
LUI	R2, BitMask(OC3IE_bit+0)
ORI	R2, R2, BitMask(OC3IE_bit+0)
_SX	
;Config.c,348 :: 		OC6IP0_bit = 1;  // Set OC5 interrupt priority to 6
LUI	R2, BitMask(OC6IP0_bit+0)
ORI	R2, R2, BitMask(OC6IP0_bit+0)
_SX	
;Config.c,349 :: 		OC6IP1_bit = 1;
LUI	R2, BitMask(OC6IP1_bit+0)
ORI	R2, R2, BitMask(OC6IP1_bit+0)
_SX	
;Config.c,350 :: 		OC6IP2_bit = 0;
LUI	R2, BitMask(OC6IP2_bit+0)
ORI	R2, R2, BitMask(OC6IP2_bit+0)
_SX	
;Config.c,351 :: 		OC6IS0_bit = 1;  // Set OC6 sub priority 3
LUI	R2, BitMask(OC6IS0_bit+0)
ORI	R2, R2, BitMask(OC6IS0_bit+0)
_SX	
;Config.c,352 :: 		OC6IS1_bit = 1;
LUI	R2, BitMask(OC6IS1_bit+0)
ORI	R2, R2, BitMask(OC6IS1_bit+0)
_SX	
;Config.c,353 :: 		OC6IF_bit  = 0;  // reset interrupt flag
LUI	R2, BitMask(OC6IF_bit+0)
ORI	R2, R2, BitMask(OC6IF_bit+0)
_SX	
;Config.c,354 :: 		OC6IE_bit  = 0;  // enable interrupt
LUI	R2, BitMask(OC6IE_bit+0)
ORI	R2, R2, BitMask(OC6IE_bit+0)
_SX	
;Config.c,357 :: 		OC8IP0_bit = 1;  // Set OC8 interrupt priority to 6
LUI	R2, BitMask(OC8IP0_bit+0)
ORI	R2, R2, BitMask(OC8IP0_bit+0)
_SX	
;Config.c,358 :: 		OC8IP1_bit = 1;
LUI	R2, BitMask(OC8IP1_bit+0)
ORI	R2, R2, BitMask(OC8IP1_bit+0)
_SX	
;Config.c,359 :: 		OC8IP2_bit = 0;
LUI	R2, BitMask(OC8IP2_bit+0)
ORI	R2, R2, BitMask(OC8IP2_bit+0)
_SX	
;Config.c,360 :: 		OC8IS0_bit = 1;  // Set OC8 sub priority 2
LUI	R2, BitMask(OC8IS0_bit+0)
ORI	R2, R2, BitMask(OC8IS0_bit+0)
_SX	
;Config.c,361 :: 		OC8IS1_bit = 1;
LUI	R2, BitMask(OC8IS1_bit+0)
ORI	R2, R2, BitMask(OC8IS1_bit+0)
_SX	
;Config.c,362 :: 		OC8IF_bit  = 0;  // reset interrupt flag
LUI	R2, BitMask(OC8IF_bit+0)
ORI	R2, R2, BitMask(OC8IF_bit+0)
_SX	
;Config.c,363 :: 		OC8IE_bit  = 0;  // enable interrupt
LUI	R2, BitMask(OC8IE_bit+0)
ORI	R2, R2, BitMask(OC8IE_bit+0)
_SX	
;Config.c,366 :: 		T2CONSET  = 0x8000; //X Enable Timer2 0C5
ORI	R2, R0, 32768
SW	R2, Offset(T2CONSET+0)(GP)
;Config.c,367 :: 		T4CONSET  = 0x8000; //Y Enable Timer4 OC2
ORI	R2, R0, 32768
SW	R2, Offset(T4CONSET+0)(GP)
;Config.c,368 :: 		T6CONSET  = 0x8000; //Z Enable Timer6 OC7
ORI	R2, R0, 32768
SW	R2, Offset(T6CONSET+0)(GP)
;Config.c,369 :: 		T5CONSET  = 0x8000; //A Enable Timer5 0C3
ORI	R2, R0, 32768
SW	R2, Offset(T5CONSET+0)(GP)
;Config.c,370 :: 		T3CONSET  = 0x8000; //B Enable Timer3 OC6
ORI	R2, R0, 32768
SW	R2, Offset(T3CONSET+0)(GP)
;Config.c,371 :: 		T7CONSET  = 0x8000; //C Enable Timer6 OC8
ORI	R2, R0, 32768
SW	R2, Offset(T7CONSET+0)(GP)
;Config.c,377 :: 		}
L_end_OutPutPulseXYZ:
JR	RA
NOP	
; end of _OutPutPulseXYZ
_UART1:
;Config.c,381 :: 		void UART1() iv IVT_UART1_RX ilevel 5 ics ICS_SOFT {
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
ADDIU	SP, SP, -4
SW	RA, 0(SP)
;Config.c,382 :: 		IFS3CLR  = 0x20000;
LUI	R2, 2
SW	R2, Offset(IFS3CLR+0)(GP)
;Config.c,384 :: 		UART1_Write(U1RXREG);
LW	R25, Offset(U1RXREG+0)(GP)
JAL	_UART1_Write+0
NOP	
;Config.c,386 :: 		}
L_end_UART1:
LW	RA, 0(SP)
ADDIU	SP, SP, 4
DI	
EHB	
LW	R30, 8(SP)
MTC0	R30, 12, 2
LW	R30, 4(SP)
MTC0	R30, 14, 0
LW	R30, 0(SP)
MTC0	R30, 12, 0
LW	R30, 12(SP)
ADDIU	SP, SP, 16
WRPGPR	SP, SP
ERET	
; end of _UART1
