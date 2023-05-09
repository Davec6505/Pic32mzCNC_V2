_InitTimer1:
;Timers.c,13 :: 		void InitTimer1(){
;Timers.c,15 :: 		Clock = ClockPulse;
LUI	R2, hi_addr(Timers_ClockPulse+0)
ORI	R2, R2, lo_addr(Timers_ClockPulse+0)
SW	R2, Offset(_Clock+0)(GP)
;Timers.c,17 :: 		T1CON         = 0x8010;
ORI	R2, R0, 32784
SW	R2, Offset(T1CON+0)(GP)
;Timers.c,19 :: 		IPC1SET       = 0x1A;
ORI	R2, R0, 26
SW	R2, Offset(IPC1SET+0)(GP)
;Timers.c,21 :: 		IEC0       |= 1<<4;
LW	R2, Offset(IEC0+0)(GP)
ORI	R2, R2, 16
SW	R2, Offset(IEC0+0)(GP)
;Timers.c,23 :: 		IFS0       |= ~(1<<4);
LW	R3, Offset(IFS0+0)(GP)
LUI	R2, 65535
ORI	R2, R2, 65519
OR	R2, R3, R2
SW	R2, Offset(IFS0+0)(GP)
;Timers.c,25 :: 		PR1           = 62500;
ORI	R2, R0, 62500
SW	R2, Offset(PR1+0)(GP)
;Timers.c,26 :: 		TMR1          = 0;
SW	R0, Offset(TMR1+0)(GP)
;Timers.c,28 :: 		}
L_end_InitTimer1:
JR	RA
NOP	
; end of _InitTimer1
_InitTimer8:
;Timers.c,33 :: 		void InitTimer8(){
;Timers.c,40 :: 		T8IF_bit         = 0;
SW	R25, Offset(_Dly+0)(GP)
;Timers.c,41 :: 		T8IE_bit         = 1;
SW	R0, Offset(T8CON+0)(GP)
;Timers.c,42 :: 		PR8              = 500;
ORI	R2, R0, 20
SW	R2, Offset(IPC9SET+0)(GP)
;Timers.c,48 :: 		///////////////////////////////////////////
LW	R3, Offset(IFS1+0)(GP)
LUI	R2, 65535
ORI	R2, R2, 65519
OR	R2, R3, R2
SW	R2, Offset(IFS1+0)(GP)
;Timers.c,50 :: 		void Timer1Interrupt() iv IVT_TIMER_1 ilevel 6 ics ICS_SRS {
LW	R2, Offset(IEC1+0)(GP)
ORI	R2, R2, 16
SW	R2, Offset(IEC1+0)(GP)
;Timers.c,52 :: 		//Enter your code here
ORI	R2, R0, 50000
SW	R2, Offset(PR8+0)(GP)
;Timers.c,53 :: 		Clock();
SW	R0, Offset(TMR8+0)(GP)
;Timers.c,54 :: 		}
SW	R0, Offset(Timers_uSec+0)(GP)
;Timers.c,55 :: 		
ORI	R2, R0, 32848
SW	R2, Offset(T8CONSET+0)(GP)
;Timers.c,56 :: 		//////////////////////////////////////////
L_end_InitTimer8:
JR	RA
NOP	
; end of _InitTimer8
_Timer1Interrupt:
;Timers.c,61 :: 		
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
;Timers.c,62 :: 		long setUsec(long usec){
LUI	R2, BitMask(T1IF_bit+0)
ORI	R2, R2, BitMask(T1IF_bit+0)
_SX	
;Timers.c,64 :: 		return uSec;
LW	R30, Offset(_Clock+0)(GP)
JALR	RA, R30
NOP	
;Timers.c,65 :: 		}
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
_getUsec:
;Timers.c,69 :: 		
;Timers.c,70 :: 		void void Timer8Interrupt() iv IVT_TIMER_8 ilevel 5 ics ICS_SRS {
LW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,71 :: 		T8IF_bit  = 0;
L_end_getUsec:
JR	RA
NOP	
; end of _getUsec
_setUsec:
;Timers.c,73 :: 		//oneShot to start the steppers runnin
;Timers.c,74 :: 		uSec++;
SW	R25, Offset(Timers_uSec+0)(GP)
;Timers.c,75 :: 		}
LW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,76 :: 		
L_end_setUsec:
JR	RA
NOP	
; end of _setUsec
_Timer8Interrupt:
;Timers.c,81 :: 		ms300++;
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
;Timers.c,82 :: 		ms500++;
LUI	R2, BitMask(T8IF_bit+0)
ORI	R2, R2, BitMask(T8IF_bit+0)
_SX	
;Timers.c,85 :: 		TMR.clock.B0 = !TMR.clock.B0;
LW	R30, Offset(_Dly+0)(GP)
JALR	RA, R30
NOP	
;Timers.c,86 :: 		if(ms100 > 9){
LW	R2, Offset(Timers_uSec+0)(GP)
ADDIU	R2, R2, 1
SW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,87 :: 		ms100 = 0;
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
Timers_ClockPulse:
;Timers.c,91 :: 		ms300 = 0;
;Timers.c,92 :: 		TMR.clock.B2 = !TMR.clock.B2;
LHU	R2, Offset(Timers_ms100+0)(GP)
ADDIU	R4, R2, 1
SH	R4, Offset(Timers_ms100+0)(GP)
;Timers.c,93 :: 		}
LHU	R2, Offset(Timers_ms300+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_ms300+0)(GP)
;Timers.c,94 :: 		if(ms500 > 49){
LHU	R2, Offset(Timers_ms500+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_ms500+0)(GP)
;Timers.c,95 :: 		ms500 = 0;
LHU	R2, Offset(Timers_sec1+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_sec1+0)(GP)
;Timers.c,97 :: 		}
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 0, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 0, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,98 :: 		if(sec1 > 99){
ANDI	R2, R4, 65535
SLTIU	R2, R2, 10
BEQ	R2, R0, L_Timers_ClockPulse11
NOP	
J	L_Timers_ClockPulse0
NOP	
L_Timers_ClockPulse11:
;Timers.c,99 :: 		sec1 = 0;
SH	R0, Offset(Timers_ms100+0)(GP)
;Timers.c,100 :: 		TMR.clock.B4 = !TMR.clock.B4;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 1, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 1, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,101 :: 		}
L_Timers_ClockPulse0:
;Timers.c,102 :: 		
LHU	R2, Offset(Timers_ms300+0)(GP)
SLTIU	R2, R2, 30
BEQ	R2, R0, L_Timers_ClockPulse12
NOP	
J	L_Timers_ClockPulse1
NOP	
L_Timers_ClockPulse12:
;Timers.c,103 :: 		}
SH	R0, Offset(Timers_ms300+0)(GP)
;Timers.c,104 :: 		unresolved line
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 2, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 2, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,105 :: 		unresolved line
L_Timers_ClockPulse1:
;Timers.c,106 :: 		unresolved line
LHU	R2, Offset(Timers_ms500+0)(GP)
SLTIU	R2, R2, 50
BEQ	R2, R0, L_Timers_ClockPulse13
NOP	
J	L_Timers_ClockPulse2
NOP	
L_Timers_ClockPulse13:
;Timers.c,107 :: 		unresolved line
SH	R0, Offset(Timers_ms500+0)(GP)
;Timers.c,108 :: 		unresolved line
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 3, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 3, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,109 :: 		unresolved line
L_Timers_ClockPulse2:
;Timers.c,110 :: 		unresolved line
LHU	R2, Offset(Timers_sec1+0)(GP)
SLTIU	R2, R2, 100
BEQ	R2, R0, L_Timers_ClockPulse14
NOP	
J	L_Timers_ClockPulse3
NOP	
L_Timers_ClockPulse14:
;Timers.c,111 :: 		unresolved line
SH	R0, Offset(Timers_sec1+0)(GP)
;Timers.c,112 :: 		unresolved line
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 4, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 4, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,113 :: 		unresolved line
L_Timers_ClockPulse3:
;Timers.c,115 :: 		unresolved line
L_end_ClockPulse:
JR	RA
NOP	
; end of Timers_ClockPulse
