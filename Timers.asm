_InitTimer1:
;Timers.c,14 :: 		void InitTimer1(){
;Timers.c,16 :: 		Clock = ClockPulse;
LUI	R2, hi_addr(Timers_ClockPulse+0)
ORI	R2, R2, lo_addr(Timers_ClockPulse+0)
SW	R2, Offset(_Clock+0)(GP)
;Timers.c,18 :: 		T1CON         = 0x8010;
ORI	R2, R0, 32784
SW	R2, Offset(T1CON+0)(GP)
;Timers.c,20 :: 		IPC1SET       = 0x1A;
ORI	R2, R0, 26
SW	R2, Offset(IPC1SET+0)(GP)
;Timers.c,22 :: 		IEC0       |= 1<<4;
LW	R2, Offset(IEC0+0)(GP)
ORI	R2, R2, 16
SW	R2, Offset(IEC0+0)(GP)
;Timers.c,24 :: 		IFS0       |= ~(1<<4);
LW	R3, Offset(IFS0+0)(GP)
LUI	R2, 65535
ORI	R2, R2, 65519
OR	R2, R3, R2
SW	R2, Offset(IFS0+0)(GP)
;Timers.c,26 :: 		PR1           = 62500;
ORI	R2, R0, 62500
SW	R2, Offset(PR1+0)(GP)
;Timers.c,27 :: 		TMR1          = 0;
SW	R0, Offset(TMR1+0)(GP)
;Timers.c,29 :: 		}
L_end_InitTimer1:
JR	RA
NOP	
; end of _InitTimer1
_InitTimer2:
;Timers.c,33 :: 		void InitTimer2(){
;Timers.c,34 :: 		T2CON         = 0x8000;
ORI	R2, R0, 32768
SW	R2, Offset(T2CON+0)(GP)
;Timers.c,35 :: 		T2IP0_bit     = 1;
LUI	R2, BitMask(T2IP0_bit+0)
ORI	R2, R2, BitMask(T2IP0_bit+0)
_SX	
;Timers.c,36 :: 		T2IP1_bit     = 0;
LUI	R2, BitMask(T2IP1_bit+0)
ORI	R2, R2, BitMask(T2IP1_bit+0)
_SX	
;Timers.c,37 :: 		T2IP2_bit     = 1;
LUI	R2, BitMask(T2IP2_bit+0)
ORI	R2, R2, BitMask(T2IP2_bit+0)
_SX	
;Timers.c,38 :: 		T2IS0_bit     = 0;
LUI	R2, BitMask(T2IS0_bit+0)
ORI	R2, R2, BitMask(T2IS0_bit+0)
_SX	
;Timers.c,39 :: 		T2IS1_bit     = 0;
LUI	R2, BitMask(T2IS1_bit+0)
ORI	R2, R2, BitMask(T2IS1_bit+0)
_SX	
;Timers.c,41 :: 		T2IF_bit      = 0;
LUI	R2, BitMask(T2IF_bit+0)
ORI	R2, R2, BitMask(T2IF_bit+0)
_SX	
;Timers.c,42 :: 		T2IE_bit      = 1;
LUI	R2, BitMask(T2IE_bit+0)
ORI	R2, R2, BitMask(T2IE_bit+0)
_SX	
;Timers.c,43 :: 		PR2           = 50;
ORI	R2, R0, 50
SW	R2, Offset(PR2+0)(GP)
;Timers.c,44 :: 		TMR2         = 0;
SW	R0, Offset(TMR2+0)(GP)
;Timers.c,45 :: 		}
L_end_InitTimer2:
JR	RA
NOP	
; end of _InitTimer2
_InitTimer8:
;Timers.c,48 :: 		void InitTimer8(void (*dly)()){
;Timers.c,49 :: 		Dly = dly;
SW	R25, Offset(_Dly+0)(GP)
;Timers.c,50 :: 		T8CON            = 0x0050;
ORI	R2, R0, 80
SW	R2, Offset(T8CON+0)(GP)
;Timers.c,51 :: 		T8IP0_bit        = 1;
LUI	R2, BitMask(T8IP0_bit+0)
ORI	R2, R2, BitMask(T8IP0_bit+0)
_SX	
;Timers.c,52 :: 		T8IP1_bit        = 0;
LUI	R2, BitMask(T8IP1_bit+0)
ORI	R2, R2, BitMask(T8IP1_bit+0)
_SX	
;Timers.c,53 :: 		T8IP2_bit        = 1;
LUI	R2, BitMask(T8IP2_bit+0)
ORI	R2, R2, BitMask(T8IP2_bit+0)
_SX	
;Timers.c,54 :: 		T8IS0_bit        = 0;
LUI	R2, BitMask(T8IS0_bit+0)
ORI	R2, R2, BitMask(T8IS0_bit+0)
_SX	
;Timers.c,55 :: 		T8IS1_bit        = 1;
LUI	R2, BitMask(T8IS1_bit+0)
ORI	R2, R2, BitMask(T8IS1_bit+0)
_SX	
;Timers.c,56 :: 		T8IF_bit         = 0;
LUI	R2, BitMask(T8IF_bit+0)
ORI	R2, R2, BitMask(T8IF_bit+0)
_SX	
;Timers.c,57 :: 		T8IE_bit         = 1;
LUI	R2, BitMask(T8IE_bit+0)
ORI	R2, R2, BitMask(T8IE_bit+0)
_SX	
;Timers.c,58 :: 		PR8              = 50000;
ORI	R2, R0, 50000
SW	R2, Offset(PR8+0)(GP)
;Timers.c,59 :: 		TMR8             = 0;
SW	R0, Offset(TMR8+0)(GP)
;Timers.c,60 :: 		uSec             = 0;
SW	R0, Offset(Timers_uSec+0)(GP)
;Timers.c,61 :: 		}
L_end_InitTimer8:
JR	RA
NOP	
; end of _InitTimer8
_InitTimer9:
;Timers.c,65 :: 		void InitTimer9(){
;Timers.c,66 :: 		T9CON         = 0x0050;
ORI	R2, R0, 80
SW	R2, Offset(T9CON+0)(GP)
;Timers.c,67 :: 		T9IP0_bit     = 1;
LUI	R2, BitMask(T9IP0_bit+0)
ORI	R2, R2, BitMask(T9IP0_bit+0)
_SX	
;Timers.c,68 :: 		T9IP1_bit     = 0;
LUI	R2, BitMask(T9IP1_bit+0)
ORI	R2, R2, BitMask(T9IP1_bit+0)
_SX	
;Timers.c,69 :: 		T9IP2_bit     = 1;
LUI	R2, BitMask(T9IP2_bit+0)
ORI	R2, R2, BitMask(T9IP2_bit+0)
_SX	
;Timers.c,70 :: 		T9IS0_bit     = 1;
LUI	R2, BitMask(T9IS0_bit+0)
ORI	R2, R2, BitMask(T9IS0_bit+0)
_SX	
;Timers.c,71 :: 		T9IS1_bit     = 0;
LUI	R2, BitMask(T9IS1_bit+0)
ORI	R2, R2, BitMask(T9IS1_bit+0)
_SX	
;Timers.c,73 :: 		T9IF_bit      = 0;
LUI	R2, BitMask(T9IF_bit+0)
ORI	R2, R2, BitMask(T9IF_bit+0)
_SX	
;Timers.c,74 :: 		T9IE_bit      = 1;
LUI	R2, BitMask(T9IE_bit+0)
ORI	R2, R2, BitMask(T9IE_bit+0)
_SX	
;Timers.c,75 :: 		PR9           = 5000;
ORI	R2, R0, 5000
SW	R2, Offset(PR9+0)(GP)
;Timers.c,76 :: 		TMR9          = 0;
SW	R0, Offset(TMR9+0)(GP)
;Timers.c,77 :: 		}
L_end_InitTimer9:
JR	RA
NOP	
; end of _InitTimer9
_Timer1Interrupt:
;Timers.c,82 :: 		void Timer1Interrupt() iv IVT_TIMER_1 ilevel 6 ics ICS_SRS {
RDPGPR	SP, SP
ADDIU	SP, SP, -12
MFC0	R30, 12, 2
SW	R30, 8(SP)
MFC0	R30, 14, 0
SW	R30, 4(SP)
MFC0	R30, 12, 0
SW	R30, 0(SP)
INS	R30, R0, 1, 15
ORI	R30, R0, 6144
MTC0	R30, 12, 0
ADDIU	SP, SP, -4
SW	RA, 0(SP)
;Timers.c,83 :: 		T1IF_bit  = 0;
LUI	R2, BitMask(T1IF_bit+0)
ORI	R2, R2, BitMask(T1IF_bit+0)
_SX	
;Timers.c,85 :: 		Clock();
LW	R30, Offset(_Clock+0)(GP)
JALR	RA, R30
NOP	
;Timers.c,86 :: 		}
L_end_Timer1Interrupt:
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
ADDIU	SP, SP, 12
WRPGPR	SP, SP
ERET	
; end of _Timer1Interrupt
_Timer2Interrupt:
;Timers.c,90 :: 		void Timer2Interrupt() iv IVT_TIMER_2 ilevel 5 ics ICS_OFF {
;Timers.c,91 :: 		T2IF_bit  = 0;
LUI	R2, BitMask(T2IF_bit+0)
ORI	R2, R2, BitMask(T2IF_bit+0)
_SX	
;Timers.c,93 :: 		uSec++;
LW	R2, Offset(Timers_uSec+0)(GP)
ADDIU	R2, R2, 1
SW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,94 :: 		}
L_end_Timer2Interrupt:
ERET	
; end of _Timer2Interrupt
_getUsec:
;Timers.c,98 :: 		long getUsec(){
;Timers.c,99 :: 		return uSec;
LW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,100 :: 		}
L_end_getUsec:
JR	RA
NOP	
; end of _getUsec
_setUsec:
;Timers.c,102 :: 		long setUsec(long usec){
;Timers.c,103 :: 		uSec = usec;
SW	R25, Offset(Timers_uSec+0)(GP)
;Timers.c,104 :: 		return uSec;
LW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,105 :: 		}
L_end_setUsec:
JR	RA
NOP	
; end of _setUsec
_SetPR8Value:
;Timers.c,109 :: 		void SetPR8Value(unsigned int value){
;Timers.c,110 :: 		PR8  = value;
ANDI	R2, R25, 65535
SW	R2, Offset(PR8+0)(GP)
;Timers.c,111 :: 		TMR8 = 0;
SW	R0, Offset(TMR8+0)(GP)
;Timers.c,112 :: 		T8IF_bit = false;
LUI	R2, BitMask(T8IF_bit+0)
ORI	R2, R2, BitMask(T8IF_bit+0)
_SX	
;Timers.c,113 :: 		}
L_end_SetPR8Value:
JR	RA
NOP	
; end of _SetPR8Value
_RestartTmr8:
;Timers.c,114 :: 		void RestartTmr8(){
;Timers.c,115 :: 		T8CONbits.TON = true;
ORI	R2, R0, 32768
SW	R2, Offset(T8CONbits+8)(GP)
;Timers.c,116 :: 		T8IF_bit      = false;
LUI	R2, BitMask(T8IF_bit+0)
ORI	R2, R2, BitMask(T8IF_bit+0)
_SX	
;Timers.c,117 :: 		T8IE_bit      = true;
LUI	R2, BitMask(T8IE_bit+0)
ORI	R2, R2, BitMask(T8IE_bit+0)
_SX	
;Timers.c,118 :: 		PR8           = 50000;
ORI	R2, R0, 50000
SW	R2, Offset(PR8+0)(GP)
;Timers.c,119 :: 		TMR8          = 0;
SW	R0, Offset(TMR8+0)(GP)
;Timers.c,120 :: 		}
L_end_RestartTmr8:
JR	RA
NOP	
; end of _RestartTmr8
_StopTmr8:
;Timers.c,122 :: 		void StopTmr8(){
;Timers.c,123 :: 		T8CONbits.TON = false;
ORI	R2, R0, 32768
SW	R2, Offset(T8CONbits+4)(GP)
;Timers.c,124 :: 		}
L_end_StopTmr8:
JR	RA
NOP	
; end of _StopTmr8
_Timer8Interrupt:
;Timers.c,126 :: 		void Timer8Interrupt() iv IVT_TIMER_8 ilevel 5 ics ICS_SRS {
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
ADDIU	SP, SP, -4
SW	RA, 0(SP)
;Timers.c,127 :: 		T8IF_bit  = false;
LUI	R2, BitMask(T8IF_bit+0)
ORI	R2, R2, BitMask(T8IF_bit+0)
_SX	
;Timers.c,129 :: 		Dly();
LW	R30, Offset(_Dly+0)(GP)
JALR	RA, R30
NOP	
;Timers.c,130 :: 		}
L_end_Timer8Interrupt:
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
ADDIU	SP, SP, 12
WRPGPR	SP, SP
ERET	
; end of _Timer8Interrupt
_RestartTmr9:
;Timers.c,134 :: 		void RestartTmr9(){
;Timers.c,135 :: 		T9CONbits.TON = true;
ORI	R2, R0, 32768
SW	R2, Offset(T9CONbits+8)(GP)
;Timers.c,136 :: 		T9IF_bit      = 0;
LUI	R2, BitMask(T9IF_bit+0)
ORI	R2, R2, BitMask(T9IF_bit+0)
_SX	
;Timers.c,137 :: 		T9IE_bit      = 1;
LUI	R2, BitMask(T9IE_bit+0)
ORI	R2, R2, BitMask(T9IE_bit+0)
_SX	
;Timers.c,138 :: 		PR9           = 5000;
ORI	R2, R0, 5000
SW	R2, Offset(PR9+0)(GP)
;Timers.c,139 :: 		TMR9          = 0;
SW	R0, Offset(TMR9+0)(GP)
;Timers.c,140 :: 		}
L_end_RestartTmr9:
JR	RA
NOP	
; end of _RestartTmr9
_StopTmr9:
;Timers.c,142 :: 		void StopTmr9(){
;Timers.c,143 :: 		T9CONbits.TON = false;
ORI	R2, R0, 32768
SW	R2, Offset(T9CONbits+4)(GP)
;Timers.c,144 :: 		}
L_end_StopTmr9:
JR	RA
NOP	
; end of _StopTmr9
_Timer9Interrupt:
;Timers.c,146 :: 		void Timer9Interrupt() iv IVT_TIMER_9 ilevel 5 ics ICS_SRS {
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
ADDIU	SP, SP, -4
SW	RA, 0(SP)
;Timers.c,147 :: 		T9IF_bit  = false;
LUI	R2, BitMask(T9IF_bit+0)
ORI	R2, R2, BitMask(T9IF_bit+0)
_SX	
;Timers.c,149 :: 		StopTmr9();//T9IE_bit  = false;
JAL	_StopTmr9+0
NOP	
;Timers.c,150 :: 		LED2      = false;
_LX	
INS	R2, R0, BitPos(LED2+0), 1
_SX	
;Timers.c,151 :: 		}
L_end_Timer9Interrupt:
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
ADDIU	SP, SP, 12
WRPGPR	SP, SP
ERET	
; end of _Timer9Interrupt
Timers_ClockPulse:
;Timers.c,155 :: 		static void ClockPulse(){
;Timers.c,156 :: 		ms100++;
LHU	R2, Offset(Timers_ms100+0)(GP)
ADDIU	R4, R2, 1
SH	R4, Offset(Timers_ms100+0)(GP)
;Timers.c,157 :: 		ms300++;
LHU	R2, Offset(Timers_ms300+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_ms300+0)(GP)
;Timers.c,158 :: 		ms500++;
LHU	R2, Offset(Timers_ms500+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_ms500+0)(GP)
;Timers.c,159 :: 		sec1++;
LHU	R2, Offset(Timers_sec1+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_sec1+0)(GP)
;Timers.c,161 :: 		TMR.clock.B0 = !TMR.clock.B0;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 0, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 0, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,162 :: 		if(ms100 > 9){
ANDI	R2, R4, 65535
SLTIU	R2, R2, 10
BEQ	R2, R0, L_Timers_ClockPulse20
NOP	
J	L_Timers_ClockPulse0
NOP	
L_Timers_ClockPulse20:
;Timers.c,163 :: 		ms100 = 0;
SH	R0, Offset(Timers_ms100+0)(GP)
;Timers.c,164 :: 		TMR.clock.B1 = !TMR.clock.B1;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 1, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 1, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,165 :: 		}
L_Timers_ClockPulse0:
;Timers.c,166 :: 		if(ms300 > 29){
LHU	R2, Offset(Timers_ms300+0)(GP)
SLTIU	R2, R2, 30
BEQ	R2, R0, L_Timers_ClockPulse21
NOP	
J	L_Timers_ClockPulse1
NOP	
L_Timers_ClockPulse21:
;Timers.c,167 :: 		ms300 = 0;
SH	R0, Offset(Timers_ms300+0)(GP)
;Timers.c,168 :: 		TMR.clock.B2 = !TMR.clock.B2;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 2, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 2, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,169 :: 		}
L_Timers_ClockPulse1:
;Timers.c,170 :: 		if(ms500 > 49){
LHU	R2, Offset(Timers_ms500+0)(GP)
SLTIU	R2, R2, 50
BEQ	R2, R0, L_Timers_ClockPulse22
NOP	
J	L_Timers_ClockPulse2
NOP	
L_Timers_ClockPulse22:
;Timers.c,171 :: 		ms500 = 0;
SH	R0, Offset(Timers_ms500+0)(GP)
;Timers.c,172 :: 		TMR.clock.B3 = !TMR.clock.B3;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 3, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 3, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,173 :: 		}
L_Timers_ClockPulse2:
;Timers.c,174 :: 		if(sec1 > 99){
LHU	R2, Offset(Timers_sec1+0)(GP)
SLTIU	R2, R2, 100
BEQ	R2, R0, L_Timers_ClockPulse23
NOP	
J	L_Timers_ClockPulse3
NOP	
L_Timers_ClockPulse23:
;Timers.c,175 :: 		sec1 = 0;
SH	R0, Offset(Timers_sec1+0)(GP)
;Timers.c,176 :: 		TMR.clock.B4 = !TMR.clock.B4;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 4, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 4, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,177 :: 		}
L_Timers_ClockPulse3:
;Timers.c,179 :: 		}
L_end_ClockPulse:
JR	RA
NOP	
; end of Timers_ClockPulse
