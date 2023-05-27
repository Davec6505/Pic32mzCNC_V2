_Init_Steppers:
;Steppers.c,23 :: 		void Init_Steppers(){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,24 :: 		step.move = false;
SW	R25, 4(SP)
LBU	R2, Offset(Steppers_step+0)(GP)
INS	R2, R0, 0, 1
SB	R2, Offset(Steppers_step+0)(GP)
;Steppers.c,25 :: 		InitTimer2();
JAL	_InitTimer2+0
NOP	
;Steppers.c,26 :: 		InitTimer8(&delay); //clock delay
LUI	R25, hi_addr(_delay+0)
ORI	R25, R25, lo_addr(_delay+0)
JAL	_InitTimer8+0
NOP	
;Steppers.c,27 :: 		InitTimer9();       //pulse on time
JAL	_InitTimer9+0
NOP	
;Steppers.c,28 :: 		}
L_end_Init_Steppers:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of _Init_Steppers
_delay:
;Steppers.c,34 :: 		void delay(){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,37 :: 		i = (i>50)? 0:i;
SW	R25, 4(SP)
LH	R2, Offset(delay_i_L0+0)(GP)
SLTI	R2, R2, 51
BEQ	R2, R0, L__delay62
NOP	
J	L_delay0
NOP	
L__delay62:
; ?FLOC___delay?T4 start address is: 8 (R2)
MOVZ	R2, R0, R0
; ?FLOC___delay?T4 end address is: 8 (R2)
J	L_delay1
NOP	
L_delay0:
; ?FLOC___delay?T4 start address is: 8 (R2)
LH	R2, Offset(delay_i_L0+0)(GP)
; ?FLOC___delay?T4 end address is: 8 (R2)
L_delay1:
; ?FLOC___delay?T4 start address is: 8 (R2)
SH	R2, Offset(delay_i_L0+0)(GP)
; ?FLOC___delay?T4 end address is: 8 (R2)
;Steppers.c,39 :: 		LED2   = true;
_LX	
ORI	R2, R2, BitMask(LED2+0)
_SX	
;Steppers.c,40 :: 		step.move = true;
LBU	R2, Offset(Steppers_step+0)(GP)
ORI	R2, R2, 1
SB	R2, Offset(Steppers_step+0)(GP)
;Steppers.c,42 :: 		acc_val = abs((feedrate + drag) >> 2 ); // divide by 4
LH	R3, Offset(Steppers_drag+0)(GP)
LH	R2, Offset(Steppers_feedrate+0)(GP)
ADDU	R2, R2, R3
SEH	R2, R2
SRA	R2, R2, 2
SEH	R25, R2
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_acc_val+0)(GP)
;Steppers.c,43 :: 		acc_val <<= 8;
LH	R2, Offset(Steppers_acc_val+0)(GP)
SLL	R2, R2, 8
SH	R2, Offset(Steppers_acc_val+0)(GP)
;Steppers.c,46 :: 		acc_val = (acc_val < 5000)? 5000:acc_val;
LH	R2, Offset(Steppers_acc_val+0)(GP)
SLTI	R2, R2, 5000
BNE	R2, R0, L__delay63
NOP	
J	L_delay2
NOP	
L__delay63:
; ?FLOC___delay?T11 start address is: 8 (R2)
ORI	R2, R0, 5000
; ?FLOC___delay?T11 end address is: 8 (R2)
J	L_delay3
NOP	
L_delay2:
; ?FLOC___delay?T11 start address is: 8 (R2)
LH	R2, Offset(Steppers_acc_val+0)(GP)
; ?FLOC___delay?T11 end address is: 8 (R2)
L_delay3:
; ?FLOC___delay?T11 start address is: 8 (R2)
SH	R2, Offset(Steppers_acc_val+0)(GP)
; ?FLOC___delay?T11 end address is: 8 (R2)
;Steppers.c,49 :: 		SetPR8Value(acc_val);
LH	R25, Offset(Steppers_acc_val+0)(GP)
JAL	_SetPR8Value+0
NOP	
;Steppers.c,52 :: 		RestartTmr9();
JAL	_RestartTmr9+0
NOP	
;Steppers.c,56 :: 		if(i < 10){
LH	R2, Offset(delay_i_L0+0)(GP)
SLTI	R2, R2, 10
BNE	R2, R0, L__delay64
NOP	
J	L_delay4
NOP	
L__delay64:
;Steppers.c,57 :: 		drag = drag - (oil * oil);
LH	R3, Offset(Steppers_oil+0)(GP)
LH	R2, Offset(Steppers_oil+0)(GP)
MUL	R3, R2, R3
LH	R2, Offset(Steppers_drag+0)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_drag+0)(GP)
;Steppers.c,58 :: 		--oil;
LH	R2, Offset(Steppers_oil+0)(GP)
ADDIU	R2, R2, -1
SH	R2, Offset(Steppers_oil+0)(GP)
;Steppers.c,59 :: 		}else if(i > 40){
J	L_delay5
NOP	
L_delay4:
LH	R2, Offset(delay_i_L0+0)(GP)
SLTI	R2, R2, 41
BEQ	R2, R0, L__delay65
NOP	
J	L_delay6
NOP	
L__delay65:
;Steppers.c,60 :: 		++oil;
LH	R2, Offset(Steppers_oil+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_oil+0)(GP)
;Steppers.c,61 :: 		drag = drag + (oil * oil);
LH	R3, Offset(Steppers_oil+0)(GP)
LH	R2, Offset(Steppers_oil+0)(GP)
MUL	R3, R2, R3
LH	R2, Offset(Steppers_drag+0)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_drag+0)(GP)
;Steppers.c,62 :: 		}
L_delay6:
L_delay5:
;Steppers.c,65 :: 		i++;
LH	R2, Offset(delay_i_L0+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(delay_i_L0+0)(GP)
;Steppers.c,66 :: 		if (drag < 0) drag = 0;
LH	R2, Offset(Steppers_drag+0)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L__delay66
NOP	
J	L_delay7
NOP	
L__delay66:
SH	R0, Offset(Steppers_drag+0)(GP)
L_delay7:
;Steppers.c,67 :: 		if(i == 40)oil = 0;
LH	R3, Offset(delay_i_L0+0)(GP)
ORI	R2, R0, 40
BEQ	R3, R2, L__delay67
NOP	
J	L_delay8
NOP	
L__delay67:
SH	R0, Offset(Steppers_oil+0)(GP)
L_delay8:
;Steppers.c,68 :: 		drag = (drag > 100)? 100:drag;
LH	R2, Offset(Steppers_drag+0)(GP)
SLTI	R2, R2, 101
BEQ	R2, R0, L__delay68
NOP	
J	L_delay9
NOP	
L__delay68:
; ?FLOC___delay?T24 start address is: 8 (R2)
ORI	R2, R0, 100
; ?FLOC___delay?T24 end address is: 8 (R2)
J	L_delay10
NOP	
L_delay9:
; ?FLOC___delay?T24 start address is: 8 (R2)
LH	R2, Offset(Steppers_drag+0)(GP)
; ?FLOC___delay?T24 end address is: 8 (R2)
L_delay10:
; ?FLOC___delay?T24 start address is: 8 (R2)
SH	R2, Offset(Steppers_drag+0)(GP)
; ?FLOC___delay?T24 end address is: 8 (R2)
;Steppers.c,69 :: 		}
L_end_delay:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of _delay
_setStepXY:
;Steppers.c,71 :: 		void setStepXY(int _x1,int _y1,int _x3,int _y3){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,72 :: 		setDragOil(100,1);
SH	R26, 4(SP)
SH	R25, 6(SP)
ORI	R26, R0, 1
ORI	R25, R0, 100
JAL	_setDragOil+0
NOP	
LH	R25, 6(SP)
LH	R26, 4(SP)
;Steppers.c,73 :: 		setFeedrate(180);
SH	R25, 4(SP)
ORI	R25, R0, 180
JAL	_setFeedrate+0
NOP	
LH	R25, 4(SP)
;Steppers.c,74 :: 		step.xl = _x1;
SH	R25, Offset(Steppers_step+2)(GP)
;Steppers.c,75 :: 		step.yl = _y1;
SH	R26, Offset(Steppers_step+4)(GP)
;Steppers.c,76 :: 		step.x3 = _x3;
SH	R27, Offset(Steppers_step+10)(GP)
;Steppers.c,77 :: 		step.y3 = _y3;
SH	R28, Offset(Steppers_step+12)(GP)
;Steppers.c,78 :: 		}
L_end_setStepXY:
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of _setStepXY
_setDragOil:
;Steppers.c,80 :: 		void setDragOil(int _drag,int _oil){
;Steppers.c,81 :: 		drag = _drag;
SH	R25, Offset(Steppers_drag+0)(GP)
;Steppers.c,82 :: 		oil = _oil;
SH	R26, Offset(Steppers_oil+0)(GP)
;Steppers.c,83 :: 		}
L_end_setDragOil:
JR	RA
NOP	
; end of _setDragOil
_setFeedrate:
;Steppers.c,85 :: 		void setFeedrate(int _feedrate){
;Steppers.c,86 :: 		feedrate = _feedrate;
SH	R25, Offset(Steppers_feedrate+0)(GP)
;Steppers.c,87 :: 		}
L_end_setFeedrate:
JR	RA
NOP	
; end of _setFeedrate
Steppers_getdir:
;Steppers.c,89 :: 		static void getdir(){
;Steppers.c,91 :: 		int binrep = 0;
; binrep start address is: 12 (R3)
MOVZ	R3, R0, R0
;Steppers.c,92 :: 		step.xo = step.yo = 0;
SH	R0, Offset(Steppers_step+16)(GP)
LH	R2, Offset(Steppers_step+16)(GP)
SH	R2, Offset(Steppers_step+14)(GP)
;Steppers.c,93 :: 		if(d)binrep = binrep + 8;
LH	R2, Offset(Steppers_getdir_d_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir74
NOP	
J	L_Steppers_getdir53
NOP	
L_Steppers_getdir74:
ADDIU	R2, R3, 8
SEH	R3, R2
; binrep end address is: 12 (R3)
J	L_Steppers_getdir11
NOP	
L_Steppers_getdir53:
L_Steppers_getdir11:
;Steppers.c,94 :: 		if(f)binrep = binrep + 4;
; binrep start address is: 12 (R3)
LH	R2, Offset(Steppers_getdir_f_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir76
NOP	
J	L_Steppers_getdir54
NOP	
L_Steppers_getdir76:
ADDIU	R2, R3, 4
SEH	R3, R2
; binrep end address is: 12 (R3)
J	L_Steppers_getdir12
NOP	
L_Steppers_getdir54:
L_Steppers_getdir12:
;Steppers.c,95 :: 		if(a)binrep = binrep + 2;
; binrep start address is: 12 (R3)
LH	R2, Offset(Steppers_getdir_a_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir78
NOP	
J	L_Steppers_getdir55
NOP	
L_Steppers_getdir78:
ADDIU	R2, R3, 2
SEH	R3, R2
; binrep end address is: 12 (R3)
SEH	R4, R3
J	L_Steppers_getdir13
NOP	
L_Steppers_getdir55:
SEH	R4, R3
L_Steppers_getdir13:
;Steppers.c,96 :: 		if(b)binrep = binrep + 1;
; binrep start address is: 16 (R4)
LH	R2, Offset(Steppers_getdir_b_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir80
NOP	
J	L_Steppers_getdir56
NOP	
L_Steppers_getdir80:
ADDIU	R2, R4, 1
SEH	R4, R2
; binrep end address is: 16 (R4)
J	L_Steppers_getdir14
NOP	
L_Steppers_getdir56:
L_Steppers_getdir14:
;Steppers.c,98 :: 		switch(binrep){
; binrep start address is: 16 (R4)
J	L_Steppers_getdir15
NOP	
; binrep end address is: 16 (R4)
;Steppers.c,99 :: 		case 0:  step.yo = -1; break;
L_Steppers_getdir17:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,100 :: 		case 1:  step.xo = -1; break;
L_Steppers_getdir18:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,101 :: 		case 2:  step.xo = 1;break;
L_Steppers_getdir19:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,102 :: 		case 3:  step.yo = 1; break;
L_Steppers_getdir20:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,103 :: 		case 4:  step.xo = 1;break;
L_Steppers_getdir21:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,104 :: 		case 5:  step.yo = -1;break;
L_Steppers_getdir22:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,105 :: 		case 6:  step.yo = 1; break;
L_Steppers_getdir23:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,106 :: 		case 7:  step.xo = -1; break;
L_Steppers_getdir24:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,107 :: 		case 8:  step.xo = -1; break;
L_Steppers_getdir25:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,108 :: 		case 9:  step.yo = 1;  break;
L_Steppers_getdir26:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,109 :: 		case 10: step.yo = -1;break;
L_Steppers_getdir27:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,110 :: 		case 11: step.xo = 1;break;
L_Steppers_getdir28:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,111 :: 		case 12: step.yo = 1;break;
L_Steppers_getdir29:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,112 :: 		case 13: step.xo = 1; break;
L_Steppers_getdir30:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,113 :: 		case 14: step.xo = -1; break;
L_Steppers_getdir31:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,114 :: 		case 15: step.yo = -1; break;
L_Steppers_getdir32:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_getdir16
NOP	
;Steppers.c,115 :: 		}
L_Steppers_getdir15:
; binrep start address is: 16 (R4)
SEH	R2, R4
BNE	R2, R0, L_Steppers_getdir82
NOP	
J	L_Steppers_getdir17
NOP	
L_Steppers_getdir82:
SEH	R3, R4
ORI	R2, R0, 1
BNE	R3, R2, L_Steppers_getdir84
NOP	
J	L_Steppers_getdir18
NOP	
L_Steppers_getdir84:
SEH	R3, R4
ORI	R2, R0, 2
BNE	R3, R2, L_Steppers_getdir86
NOP	
J	L_Steppers_getdir19
NOP	
L_Steppers_getdir86:
SEH	R3, R4
ORI	R2, R0, 3
BNE	R3, R2, L_Steppers_getdir88
NOP	
J	L_Steppers_getdir20
NOP	
L_Steppers_getdir88:
SEH	R3, R4
ORI	R2, R0, 4
BNE	R3, R2, L_Steppers_getdir90
NOP	
J	L_Steppers_getdir21
NOP	
L_Steppers_getdir90:
SEH	R3, R4
ORI	R2, R0, 5
BNE	R3, R2, L_Steppers_getdir92
NOP	
J	L_Steppers_getdir22
NOP	
L_Steppers_getdir92:
SEH	R3, R4
ORI	R2, R0, 6
BNE	R3, R2, L_Steppers_getdir94
NOP	
J	L_Steppers_getdir23
NOP	
L_Steppers_getdir94:
SEH	R3, R4
ORI	R2, R0, 7
BNE	R3, R2, L_Steppers_getdir96
NOP	
J	L_Steppers_getdir24
NOP	
L_Steppers_getdir96:
SEH	R3, R4
ORI	R2, R0, 8
BNE	R3, R2, L_Steppers_getdir98
NOP	
J	L_Steppers_getdir25
NOP	
L_Steppers_getdir98:
SEH	R3, R4
ORI	R2, R0, 9
BNE	R3, R2, L_Steppers_getdir100
NOP	
J	L_Steppers_getdir26
NOP	
L_Steppers_getdir100:
SEH	R3, R4
ORI	R2, R0, 10
BNE	R3, R2, L_Steppers_getdir102
NOP	
J	L_Steppers_getdir27
NOP	
L_Steppers_getdir102:
SEH	R3, R4
ORI	R2, R0, 11
BNE	R3, R2, L_Steppers_getdir104
NOP	
J	L_Steppers_getdir28
NOP	
L_Steppers_getdir104:
SEH	R3, R4
ORI	R2, R0, 12
BNE	R3, R2, L_Steppers_getdir106
NOP	
J	L_Steppers_getdir29
NOP	
L_Steppers_getdir106:
SEH	R3, R4
ORI	R2, R0, 13
BNE	R3, R2, L_Steppers_getdir108
NOP	
J	L_Steppers_getdir30
NOP	
L_Steppers_getdir108:
SEH	R3, R4
ORI	R2, R0, 14
BNE	R3, R2, L_Steppers_getdir110
NOP	
J	L_Steppers_getdir31
NOP	
L_Steppers_getdir110:
SEH	R3, R4
; binrep end address is: 16 (R4)
ORI	R2, R0, 15
BNE	R3, R2, L_Steppers_getdir112
NOP	
J	L_Steppers_getdir32
NOP	
L_Steppers_getdir112:
L_Steppers_getdir16:
;Steppers.c,116 :: 		}
L_end_getdir:
JR	RA
NOP	
; end of Steppers_getdir
Steppers_setdirection:
;Steppers.c,118 :: 		static void setdirection(){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,119 :: 		step.dy = step.y3 - step.yl;
SW	R25, 4(SP)
LH	R3, Offset(Steppers_step+4)(GP)
LH	R2, Offset(Steppers_step+12)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+20)(GP)
;Steppers.c,120 :: 		if(step.dy < 0) step.yo = -1;
LH	R2, Offset(Steppers_step+20)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L_Steppers_setdirection114
NOP	
J	L_Steppers_setdirection33
NOP	
L_Steppers_setdirection114:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+16)(GP)
J	L_Steppers_setdirection34
NOP	
L_Steppers_setdirection33:
;Steppers.c,121 :: 		else step.yo = 1;
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+16)(GP)
L_Steppers_setdirection34:
;Steppers.c,122 :: 		step.dy = abs(step.dy);
LH	R25, Offset(Steppers_step+20)(GP)
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_step+20)(GP)
;Steppers.c,124 :: 		step.dx = step.x3 - step.xl;
LH	R3, Offset(Steppers_step+2)(GP)
LH	R2, Offset(Steppers_step+10)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+18)(GP)
;Steppers.c,125 :: 		if(step.dx < 0) step.xo = -1;
LH	R2, Offset(Steppers_step+18)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L_Steppers_setdirection115
NOP	
J	L_Steppers_setdirection35
NOP	
L_Steppers_setdirection115:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_setdirection36
NOP	
L_Steppers_setdirection35:
;Steppers.c,126 :: 		else step.xo = 1;
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
L_Steppers_setdirection36:
;Steppers.c,127 :: 		step.dx = abs(step.dx);
LH	R25, Offset(Steppers_step+18)(GP)
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_step+18)(GP)
;Steppers.c,129 :: 		if(step.dx>step.dy){step.fxy = step.dx - step.dy;step.fm=0;}
LH	R3, Offset(Steppers_step+20)(GP)
LH	R2, Offset(Steppers_step+18)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L_Steppers_setdirection116
NOP	
J	L_Steppers_setdirection37
NOP	
L_Steppers_setdirection116:
LH	R3, Offset(Steppers_step+20)(GP)
LH	R2, Offset(Steppers_step+18)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+24)(GP)
SB	R0, Offset(Steppers_step+1)(GP)
J	L_Steppers_setdirection38
NOP	
L_Steppers_setdirection37:
;Steppers.c,130 :: 		else {step.fxy = step.dy - step.dx; step.fm=1;}
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+20)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+24)(GP)
ORI	R2, R0, 1
SB	R2, Offset(Steppers_step+1)(GP)
L_Steppers_setdirection38:
;Steppers.c,131 :: 		}
L_end_setdirection:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of Steppers_setdirection
_doline:
;Steppers.c,134 :: 		void doline(){
ADDIU	SP, SP, -44
SW	RA, 0(SP)
;Steppers.c,135 :: 		step.stepnum = step.x2 = step.y2 = step.fxy = 0;
SW	R25, 4(SP)
SH	R0, Offset(Steppers_step+24)(GP)
LH	R2, Offset(Steppers_step+24)(GP)
SH	R2, Offset(Steppers_step+8)(GP)
LH	R2, Offset(Steppers_step+8)(GP)
SH	R2, Offset(Steppers_step+6)(GP)
LH	R2, Offset(Steppers_step+6)(GP)
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,136 :: 		setdirection();
JAL	Steppers_setdirection+0
NOP	
;Steppers.c,137 :: 		while(DMA_IsOn(1));
L_doline39:
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
BNE	R2, R0, L__doline119
NOP	
J	L_doline40
NOP	
L__doline119:
J	L_doline39
NOP	
L_doline40:
;Steppers.c,138 :: 		dma_printf("%s","\nStep\tFXY\tX2\tY2\t\tXO\tYO\toutput\tacc_val\tdrag\toil\n");
ORI	R30, R0, 37
SB	R30, 8(SP)
ORI	R30, R0, 115
SB	R30, 9(SP)
MOVZ	R30, R0, R0
SB	R30, 10(SP)
ADDIU	R3, SP, 8
LUI	R2, hi_addr(?lstr_2_Steppers+0)
ORI	R2, R2, lo_addr(?lstr_2_Steppers+0)
ADDIU	SP, SP, -8
SW	R2, 4(SP)
SW	R3, 0(SP)
JAL	_dma_printf+0
NOP	
ADDIU	SP, SP, 8
;Steppers.c,139 :: 		RestartTmr8();
JAL	_RestartTmr8+0
NOP	
;Steppers.c,140 :: 		while ( (step.dx > step.x2) && (step.dy > step.y2)){ // at endpoint?
L_doline41:
LH	R3, Offset(Steppers_step+6)(GP)
LH	R2, Offset(Steppers_step+18)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__doline120
NOP	
J	L__doline59
NOP	
L__doline120:
LH	R3, Offset(Steppers_step+8)(GP)
LH	R2, Offset(Steppers_step+20)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__doline121
NOP	
J	L__doline58
NOP	
L__doline121:
L__doline57:
;Steppers.c,141 :: 		out = 0;
SH	R0, Offset(_out+0)(GP)
;Steppers.c,142 :: 		while(!step.move){
L_doline45:
LBU	R2, Offset(Steppers_step+0)(GP)
EXT	R2, R2, 0, 1
BEQ	R2, R0, L__doline122
NOP	
J	L_doline46
NOP	
L__doline122:
;Steppers.c,144 :: 		}
J	L_doline45
NOP	
L_doline46:
;Steppers.c,145 :: 		step.move = false;
LBU	R2, Offset(Steppers_step+0)(GP)
INS	R2, R0, 0, 1
SB	R2, Offset(Steppers_step+0)(GP)
;Steppers.c,146 :: 		if(!step.fm){
LB	R2, Offset(Steppers_step+1)(GP)
BEQ	R2, R0, L__doline123
NOP	
J	L_doline47
NOP	
L__doline123:
;Steppers.c,147 :: 		++step.x2; step.fxy -= step.dy;
LH	R2, Offset(Steppers_step+6)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+6)(GP)
LH	R3, Offset(Steppers_step+20)(GP)
LH	R2, Offset(Steppers_step+24)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+24)(GP)
;Steppers.c,148 :: 		bit_true(out,bit(0));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 1
SH	R2, Offset(_out+0)(GP)
;Steppers.c,149 :: 		if(step.fxy < 0){
LH	R2, Offset(Steppers_step+24)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L__doline124
NOP	
J	L_doline48
NOP	
L__doline124:
;Steppers.c,150 :: 		++step.y2; step.fxy += step.dx;
LH	R2, Offset(Steppers_step+8)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+8)(GP)
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+24)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_step+24)(GP)
;Steppers.c,151 :: 		bit_true(out,bit(1));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 2
SH	R2, Offset(_out+0)(GP)
;Steppers.c,152 :: 		}
L_doline48:
;Steppers.c,153 :: 		}else{
J	L_doline49
NOP	
L_doline47:
;Steppers.c,154 :: 		++step.y2; step.fxy -= step.dx;
LH	R2, Offset(Steppers_step+8)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+8)(GP)
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+24)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+24)(GP)
;Steppers.c,155 :: 		bit_true(out,bit(1));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 2
SH	R2, Offset(_out+0)(GP)
;Steppers.c,156 :: 		if(step.fxy < 0){
LH	R2, Offset(Steppers_step+24)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L__doline125
NOP	
J	L_doline50
NOP	
L__doline125:
;Steppers.c,157 :: 		++step.x2;step.fxy += step.dy;
LH	R2, Offset(Steppers_step+6)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+6)(GP)
LH	R3, Offset(Steppers_step+20)(GP)
LH	R2, Offset(Steppers_step+24)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_step+24)(GP)
;Steppers.c,158 :: 		bit_true(out,bit(0));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 1
SH	R2, Offset(_out+0)(GP)
;Steppers.c,159 :: 		}
L_doline50:
;Steppers.c,160 :: 		}
L_doline49:
;Steppers.c,161 :: 		while(DMA_IsOn(1));
L_doline51:
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
BNE	R2, R0, L__doline127
NOP	
J	L_doline52
NOP	
L__doline127:
J	L_doline51
NOP	
L_doline52:
;Steppers.c,162 :: 		dma_printf("\n%d\t%d\t%d\t%d\t\t%d\t%d\t%d\t%d\t%d\t%d"
ADDIU	R23, SP, 11
ADDIU	R22, R23, 32
LUI	R24, hi_addr(?ICS?lstr3_Steppers+0)
ORI	R24, R24, lo_addr(?ICS?lstr3_Steppers+0)
JAL	___CC2DW+0
NOP	
ADDIU	R3, SP, 11
;Steppers.c,164 :: 		step.y2,step.xo,step.yo,out,acc_val,drag,oil);
LH	R2, Offset(Steppers_oil+0)(GP)
ADDIU	SP, SP, -44
SH	R2, 40(SP)
LH	R2, Offset(Steppers_drag+0)(GP)
SH	R2, 36(SP)
LH	R2, Offset(Steppers_acc_val+0)(GP)
SH	R2, 32(SP)
LHU	R2, Offset(_out+0)(GP)
SH	R2, 28(SP)
LH	R2, Offset(Steppers_step+16)(GP)
SH	R2, 24(SP)
LH	R2, Offset(Steppers_step+14)(GP)
SH	R2, 20(SP)
LH	R2, Offset(Steppers_step+8)(GP)
SH	R2, 16(SP)
;Steppers.c,163 :: 		,step.stepnum++,step.fxy,step.x2,
LH	R2, Offset(Steppers_step+6)(GP)
SH	R2, 12(SP)
LH	R2, Offset(Steppers_step+24)(GP)
SH	R2, 8(SP)
LH	R2, Offset(Steppers_step+22)(GP)
SH	R2, 4(SP)
;Steppers.c,162 :: 		dma_printf("\n%d\t%d\t%d\t%d\t\t%d\t%d\t%d\t%d\t%d\t%d"
SW	R3, 0(SP)
;Steppers.c,164 :: 		step.y2,step.xo,step.yo,out,acc_val,drag,oil);
JAL	_dma_printf+0
NOP	
ADDIU	SP, SP, 44
LH	R2, Offset(Steppers_step+22)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,165 :: 		}
J	L_doline41
NOP	
;Steppers.c,140 :: 		while ( (step.dx > step.x2) && (step.dy > step.y2)){ // at endpoint?
L__doline59:
L__doline58:
;Steppers.c,166 :: 		StopTmr8();
JAL	_StopTmr8+0
NOP	
;Steppers.c,167 :: 		}
L_end_doline:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 44
JR	RA
NOP	
; end of _doline
