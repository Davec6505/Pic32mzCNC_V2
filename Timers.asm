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
_InitTimer8:
;Timers.c,34 :: 		void InitTimer8(void (*dly)()){
;Timers.c,35 :: 		Dly = dly;
SW	R25, Offset(_Dly+0)(GP)
;Timers.c,36 :: 		T8CON            = 0x8050;
ORI	R2, R0, 32848
SW	R2, Offset(T8CON+0)(GP)
;Timers.c,37 :: 		T8IP0_bit        = 1;
LUI	R2, BitMask(T8IP0_bit+0)
ORI	R2, R2, BitMask(T8IP0_bit+0)
_SX	
;Timers.c,38 :: 		T8IP1_bit        = 0;
LUI	R2, BitMask(T8IP1_bit+0)
ORI	R2, R2, BitMask(T8IP1_bit+0)
_SX	
;Timers.c,39 :: 		T8IP2_bit        = 1;
LUI	R2, BitMask(T8IP2_bit+0)
ORI	R2, R2, BitMask(T8IP2_bit+0)
_SX	
;Timers.c,40 :: 		T8IS0_bit        = 0;
LUI	R2, BitMask(T8IS0_bit+0)
ORI	R2, R2, BitMask(T8IS0_bit+0)
_SX	
;Timers.c,41 :: 		T8IS1_bit        = 1;
LUI	R2, BitMask(T8IS1_bit+0)
ORI	R2, R2, BitMask(T8IS1_bit+0)
_SX	
;Timers.c,42 :: 		T8IF_bit         = 0;
LUI	R2, BitMask(T8IF_bit+0)
ORI	R2, R2, BitMask(T8IF_bit+0)
_SX	
;Timers.c,43 :: 		T8IE_bit         = 1;
LUI	R2, BitMask(T8IE_bit+0)
ORI	R2, R2, BitMask(T8IE_bit+0)
_SX	
;Timers.c,44 :: 		PR8              = 50000;
ORI	R2, R0, 50000
SW	R2, Offset(PR8+0)(GP)
;Timers.c,45 :: 		TMR8             = 0;
SW	R0, Offset(TMR8+0)(GP)
;Timers.c,46 :: 		uSec             = 0;
SW	R0, Offset(Timers_uSec+0)(GP)
;Timers.c,47 :: 		}
L_end_InitTimer8:
JR	RA
NOP	
; end of _InitTimer8
_Timer1Interrupt:
;Timers.c,52 :: 		void Timer1Interrupt() iv IVT_TIMER_1 ilevel 6 ics ICS_SRS {
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
;Timers.c,53 :: 		T1IF_bit  = 0;
LUI	R2, BitMask(T1IF_bit+0)
ORI	R2, R2, BitMask(T1IF_bit+0)
_SX	
;Timers.c,55 :: 		Clock();
LW	R30, Offset(_Clock+0)(GP)
JALR	RA, R30
NOP	
;Timers.c,56 :: 		}
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
;Timers.c,60 :: 		long getUsec(){
;Timers.c,61 :: 		return uSec;
LW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,62 :: 		}
L_end_getUsec:
JR	RA
NOP	
; end of _getUsec
_setUsec:
;Timers.c,64 :: 		long setUsec(long usec){
;Timers.c,65 :: 		uSec = usec;
SW	R25, Offset(Timers_uSec+0)(GP)
;Timers.c,66 :: 		return uSec;
LW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,67 :: 		}
L_end_setUsec:
JR	RA
NOP	
; end of _setUsec
_Timer8Interrupt:
;Timers.c,72 :: 		void void Timer8Interrupt() iv IVT_TIMER_8 ilevel 5 ics ICS_SRS {
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
;Timers.c,73 :: 		T8IF_bit  = 0;
LUI	R2, BitMask(T8IF_bit+0)
ORI	R2, R2, BitMask(T8IF_bit+0)
_SX	
;Timers.c,76 :: 		Dly();
LW	R30, Offset(_Dly+0)(GP)
JALR	RA, R30
NOP	
;Timers.c,77 :: 		uSec++;
LW	R2, Offset(Timers_uSec+0)(GP)
ADDIU	R2, R2, 1
SW	R2, Offset(Timers_uSec+0)(GP)
;Timers.c,78 :: 		}
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
;Timers.c,82 :: 		static void ClockPulse(){
;Timers.c,83 :: 		ms100++;
LHU	R2, Offset(Timers_ms100+0)(GP)
ADDIU	R4, R2, 1
SH	R4, Offset(Timers_ms100+0)(GP)
;Timers.c,84 :: 		ms300++;
LHU	R2, Offset(Timers_ms300+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_ms300+0)(GP)
;Timers.c,85 :: 		ms500++;
LHU	R2, Offset(Timers_ms500+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_ms500+0)(GP)
;Timers.c,86 :: 		sec1++;
LHU	R2, Offset(Timers_sec1+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Timers_sec1+0)(GP)
;Timers.c,88 :: 		TMR.clock.B0 = !TMR.clock.B0;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 0, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 0, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,89 :: 		if(ms100 > 9){
ANDI	R2, R4, 65535
SLTIU	R2, R2, 10
BEQ	R2, R0, L_Timers_ClockPulse11
NOP	
J	L_Timers_ClockPulse0
NOP	
L_Timers_ClockPulse11:
;Timers.c,90 :: 		ms100 = 0;
SH	R0, Offset(Timers_ms100+0)(GP)
;Timers.c,91 :: 		TMR.clock.B1 = !TMR.clock.B1;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 1, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 1, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,92 :: 		}
L_Timers_ClockPulse0:
;Timers.c,93 :: 		if(ms300 > 29){
LHU	R2, Offset(Timers_ms300+0)(GP)
SLTIU	R2, R2, 30
BEQ	R2, R0, L_Timers_ClockPulse12
NOP	
J	L_Timers_ClockPulse1
NOP	
L_Timers_ClockPulse12:
;Timers.c,94 :: 		ms300 = 0;
SH	R0, Offset(Timers_ms300+0)(GP)
;Timers.c,95 :: 		TMR.clock.B2 = !TMR.clock.B2;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 2, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 2, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,96 :: 		}
L_Timers_ClockPulse1:
;Timers.c,97 :: 		if(ms500 > 49){
LHU	R2, Offset(Timers_ms500+0)(GP)
SLTIU	R2, R2, 50
BEQ	R2, R0, L_Timers_ClockPulse13
NOP	
J	L_Timers_ClockPulse2
NOP	
L_Timers_ClockPulse13:
;Timers.c,98 :: 		ms500 = 0;
SH	R0, Offset(Timers_ms500+0)(GP)
;Timers.c,99 :: 		TMR.clock.B3 = !TMR.clock.B3;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 3, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 3, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,100 :: 		}
L_Timers_ClockPulse2:
;Timers.c,101 :: 		if(sec1 > 99){
LHU	R2, Offset(Timers_sec1+0)(GP)
SLTIU	R2, R2, 100
BEQ	R2, R0, L_Timers_ClockPulse14
NOP	
J	L_Timers_ClockPulse3
NOP	
L_Timers_ClockPulse14:
;Timers.c,102 :: 		sec1 = 0;
SH	R0, Offset(Timers_sec1+0)(GP)
;Timers.c,103 :: 		TMR.clock.B4 = !TMR.clock.B4;
LBU	R2, Offset(_TMR+0)(GP)
EXT	R2, R2, 4, 1
XORI	R3, R2, 1
LBU	R2, Offset(_TMR+0)(GP)
INS	R2, R3, 4, 1
SB	R2, Offset(_TMR+0)(GP)
;Timers.c,104 :: 		}
L_Timers_ClockPulse3:
;Timers.c,106 :: 		}
L_end_ClockPulse:
JR	RA
NOP	
; end of Timers_ClockPulse
