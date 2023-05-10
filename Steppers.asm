_Init_Steppers:
;Steppers.c,21 :: 		void Init_Steppers(){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,22 :: 		InitTimer8(&delay);
SW	R25, 4(SP)
LUI	R25, hi_addr(_delay+0)
ORI	R25, R25, lo_addr(_delay+0)
JAL	_InitTimer8+0
NOP	
;Steppers.c,23 :: 		}
L_end_Init_Steppers:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of _Init_Steppers
_delay:
;Steppers.c,29 :: 		void delay(){
;Steppers.c,30 :: 		LED2 = !LED2;
_LX	
EXT	R2, R2, BitPos(LED2+0), 1
XORI	R3, R2, 1
_LX	
INS	R2, R3, BitPos(LED2+0), 1
_SX	
;Steppers.c,31 :: 		}
L_end_delay:
JR	RA
NOP	
; end of _delay
__delay_:
;Steppers.c,34 :: 		void _delay_(){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,38 :: 		acc_val = abs((feedrate + drag) / 10);
SW	R25, 4(SP)
LH	R3, Offset(Steppers_drag+0)(GP)
LH	R2, Offset(Steppers_feedrate+0)(GP)
ADDU	R2, R2, R3
SEH	R3, R2
ORI	R2, R0, 10
DIV	R3, R2
MFLO	R2
SEH	R25, R2
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_acc_val+0)(GP)
;Steppers.c,39 :: 		ii = setUsec(0);
MOVZ	R25, R0, R0
JAL	_setUsec+0
NOP	
;Steppers.c,40 :: 		while (getUsec() <= (long)acc_val)continue;
L__delay_0:
JAL	_getUsec+0
NOP	
LH	R3, Offset(Steppers_acc_val+0)(GP)
SLT	R2, R3, R2
BEQ	R2, R0, L___delay_56
NOP	
J	L__delay_1
NOP	
L___delay_56:
J	L__delay_0
NOP	
L__delay_1:
;Steppers.c,42 :: 		if(i < 10){
LH	R2, Offset(_delay__i_L0+0)(GP)
SLTI	R2, R2, 10
BNE	R2, R0, L___delay_57
NOP	
J	L__delay_2
NOP	
L___delay_57:
;Steppers.c,46 :: 		drag = drag - (oil * oil);
LH	R3, Offset(Steppers_oil+0)(GP)
LH	R2, Offset(Steppers_oil+0)(GP)
MUL	R3, R2, R3
LH	R2, Offset(Steppers_drag+0)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_drag+0)(GP)
;Steppers.c,47 :: 		oil--;
LH	R2, Offset(Steppers_oil+0)(GP)
ADDIU	R2, R2, -1
SH	R2, Offset(Steppers_oil+0)(GP)
;Steppers.c,48 :: 		}else if(i > 40){
J	L__delay_3
NOP	
L__delay_2:
LH	R2, Offset(_delay__i_L0+0)(GP)
SLTI	R2, R2, 41
BEQ	R2, R0, L___delay_58
NOP	
J	L__delay_4
NOP	
L___delay_58:
;Steppers.c,49 :: 		++oil;
LH	R2, Offset(Steppers_oil+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_oil+0)(GP)
;Steppers.c,50 :: 		drag = drag + (oil * oil);
LH	R3, Offset(Steppers_oil+0)(GP)
LH	R2, Offset(Steppers_oil+0)(GP)
MUL	R3, R2, R3
LH	R2, Offset(Steppers_drag+0)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_drag+0)(GP)
;Steppers.c,51 :: 		}
L__delay_4:
L__delay_3:
;Steppers.c,52 :: 		i++;
LH	R2, Offset(_delay__i_L0+0)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(_delay__i_L0+0)(GP)
;Steppers.c,53 :: 		if (drag < 0) drag = 0;
LH	R2, Offset(Steppers_drag+0)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L___delay_59
NOP	
J	L__delay_5
NOP	
L___delay_59:
SH	R0, Offset(Steppers_drag+0)(GP)
L__delay_5:
;Steppers.c,54 :: 		}
L_end__delay_:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of __delay_
_setStepXY:
;Steppers.c,56 :: 		void setStepXY(int _x1,int _y1,int _x3,int _y3){
;Steppers.c,57 :: 		step.xl = _x1;
SH	R25, Offset(Steppers_step+0)(GP)
;Steppers.c,58 :: 		step.yl = _y1;
SH	R26, Offset(Steppers_step+2)(GP)
;Steppers.c,59 :: 		step.x3 = _x3;
SH	R27, Offset(Steppers_step+8)(GP)
;Steppers.c,60 :: 		step.y3 = _y3;
SH	R28, Offset(Steppers_step+10)(GP)
;Steppers.c,61 :: 		}
L_end_setStepXY:
JR	RA
NOP	
; end of _setStepXY
_setDragOil:
;Steppers.c,63 :: 		void setDragOil(int _feedrate,int _drag,int _oil){
;Steppers.c,64 :: 		feedrate = MAXFEED - _feedrate;
ORI	R2, R0, 180
SUBU	R2, R2, R25
SH	R2, Offset(Steppers_feedrate+0)(GP)
;Steppers.c,65 :: 		drag = _drag;
SH	R26, Offset(Steppers_drag+0)(GP)
;Steppers.c,66 :: 		oil = _oil;
SH	R27, Offset(Steppers_oil+0)(GP)
;Steppers.c,67 :: 		}
L_end_setDragOil:
JR	RA
NOP	
; end of _setDragOil
Steppers_getdir:
;Steppers.c,69 :: 		static void getdir(){
;Steppers.c,71 :: 		int binrep = 0;
; binrep start address is: 12 (R3)
MOVZ	R3, R0, R0
;Steppers.c,72 :: 		step.xo = step.yo = 0;
SH	R0, Offset(Steppers_step+14)(GP)
LH	R2, Offset(Steppers_step+14)(GP)
SH	R2, Offset(Steppers_step+12)(GP)
;Steppers.c,73 :: 		if(d)binrep = binrep + 8;
LH	R2, Offset(Steppers_getdir_d_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir64
NOP	
J	L_Steppers_getdir46
NOP	
L_Steppers_getdir64:
ADDIU	R2, R3, 8
SEH	R3, R2
; binrep end address is: 12 (R3)
J	L_Steppers_getdir6
NOP	
L_Steppers_getdir46:
L_Steppers_getdir6:
;Steppers.c,74 :: 		if(f)binrep = binrep + 4;
; binrep start address is: 12 (R3)
LH	R2, Offset(Steppers_getdir_f_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir66
NOP	
J	L_Steppers_getdir47
NOP	
L_Steppers_getdir66:
ADDIU	R2, R3, 4
SEH	R3, R2
; binrep end address is: 12 (R3)
J	L_Steppers_getdir7
NOP	
L_Steppers_getdir47:
L_Steppers_getdir7:
;Steppers.c,75 :: 		if(a)binrep = binrep + 2;
; binrep start address is: 12 (R3)
LH	R2, Offset(Steppers_getdir_a_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir68
NOP	
J	L_Steppers_getdir48
NOP	
L_Steppers_getdir68:
ADDIU	R2, R3, 2
SEH	R3, R2
; binrep end address is: 12 (R3)
SEH	R4, R3
J	L_Steppers_getdir8
NOP	
L_Steppers_getdir48:
SEH	R4, R3
L_Steppers_getdir8:
;Steppers.c,76 :: 		if(b)binrep = binrep + 1;
; binrep start address is: 16 (R4)
LH	R2, Offset(Steppers_getdir_b_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir70
NOP	
J	L_Steppers_getdir49
NOP	
L_Steppers_getdir70:
ADDIU	R2, R4, 1
SEH	R4, R2
; binrep end address is: 16 (R4)
J	L_Steppers_getdir9
NOP	
L_Steppers_getdir49:
L_Steppers_getdir9:
;Steppers.c,78 :: 		switch(binrep){
; binrep start address is: 16 (R4)
J	L_Steppers_getdir10
NOP	
; binrep end address is: 16 (R4)
;Steppers.c,79 :: 		case 0:  step.yo = -1; break;
L_Steppers_getdir12:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,80 :: 		case 1:  step.xo = -1; break;
L_Steppers_getdir13:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,81 :: 		case 2:  step.xo = 1;break;
L_Steppers_getdir14:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,82 :: 		case 3:  step.yo = 1; break;
L_Steppers_getdir15:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,83 :: 		case 4:  step.xo = 1;break;
L_Steppers_getdir16:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,84 :: 		case 5:  step.yo = -1;break;
L_Steppers_getdir17:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,85 :: 		case 6:  step.yo = 1; break;
L_Steppers_getdir18:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,86 :: 		case 7:  step.xo = -1; break;
L_Steppers_getdir19:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,87 :: 		case 8:  step.xo = -1; break;
L_Steppers_getdir20:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,88 :: 		case 9:  step.yo = 1;  break;
L_Steppers_getdir21:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,89 :: 		case 10: step.yo = -1;break;
L_Steppers_getdir22:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,90 :: 		case 11: step.xo = 1;break;
L_Steppers_getdir23:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,91 :: 		case 12: step.yo = 1;break;
L_Steppers_getdir24:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,92 :: 		case 13: step.xo = 1; break;
L_Steppers_getdir25:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,93 :: 		case 14: step.xo = -1; break;
L_Steppers_getdir26:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,94 :: 		case 15: step.yo = -1; break;
L_Steppers_getdir27:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir11
NOP	
;Steppers.c,95 :: 		}
L_Steppers_getdir10:
; binrep start address is: 16 (R4)
SEH	R2, R4
BNE	R2, R0, L_Steppers_getdir72
NOP	
J	L_Steppers_getdir12
NOP	
L_Steppers_getdir72:
SEH	R3, R4
ORI	R2, R0, 1
BNE	R3, R2, L_Steppers_getdir74
NOP	
J	L_Steppers_getdir13
NOP	
L_Steppers_getdir74:
SEH	R3, R4
ORI	R2, R0, 2
BNE	R3, R2, L_Steppers_getdir76
NOP	
J	L_Steppers_getdir14
NOP	
L_Steppers_getdir76:
SEH	R3, R4
ORI	R2, R0, 3
BNE	R3, R2, L_Steppers_getdir78
NOP	
J	L_Steppers_getdir15
NOP	
L_Steppers_getdir78:
SEH	R3, R4
ORI	R2, R0, 4
BNE	R3, R2, L_Steppers_getdir80
NOP	
J	L_Steppers_getdir16
NOP	
L_Steppers_getdir80:
SEH	R3, R4
ORI	R2, R0, 5
BNE	R3, R2, L_Steppers_getdir82
NOP	
J	L_Steppers_getdir17
NOP	
L_Steppers_getdir82:
SEH	R3, R4
ORI	R2, R0, 6
BNE	R3, R2, L_Steppers_getdir84
NOP	
J	L_Steppers_getdir18
NOP	
L_Steppers_getdir84:
SEH	R3, R4
ORI	R2, R0, 7
BNE	R3, R2, L_Steppers_getdir86
NOP	
J	L_Steppers_getdir19
NOP	
L_Steppers_getdir86:
SEH	R3, R4
ORI	R2, R0, 8
BNE	R3, R2, L_Steppers_getdir88
NOP	
J	L_Steppers_getdir20
NOP	
L_Steppers_getdir88:
SEH	R3, R4
ORI	R2, R0, 9
BNE	R3, R2, L_Steppers_getdir90
NOP	
J	L_Steppers_getdir21
NOP	
L_Steppers_getdir90:
SEH	R3, R4
ORI	R2, R0, 10
BNE	R3, R2, L_Steppers_getdir92
NOP	
J	L_Steppers_getdir22
NOP	
L_Steppers_getdir92:
SEH	R3, R4
ORI	R2, R0, 11
BNE	R3, R2, L_Steppers_getdir94
NOP	
J	L_Steppers_getdir23
NOP	
L_Steppers_getdir94:
SEH	R3, R4
ORI	R2, R0, 12
BNE	R3, R2, L_Steppers_getdir96
NOP	
J	L_Steppers_getdir24
NOP	
L_Steppers_getdir96:
SEH	R3, R4
ORI	R2, R0, 13
BNE	R3, R2, L_Steppers_getdir98
NOP	
J	L_Steppers_getdir25
NOP	
L_Steppers_getdir98:
SEH	R3, R4
ORI	R2, R0, 14
BNE	R3, R2, L_Steppers_getdir100
NOP	
J	L_Steppers_getdir26
NOP	
L_Steppers_getdir100:
SEH	R3, R4
; binrep end address is: 16 (R4)
ORI	R2, R0, 15
BNE	R3, R2, L_Steppers_getdir102
NOP	
J	L_Steppers_getdir27
NOP	
L_Steppers_getdir102:
L_Steppers_getdir11:
;Steppers.c,96 :: 		}
L_end_getdir:
JR	RA
NOP	
; end of Steppers_getdir
Steppers_setdirection:
;Steppers.c,98 :: 		static void setdirection(){
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,100 :: 		step.dy = step.y3 - step.yl;
SW	R25, 4(SP)
LH	R3, Offset(Steppers_step+2)(GP)
LH	R2, Offset(Steppers_step+10)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+18)(GP)
;Steppers.c,101 :: 		if(step.dy < 0) step.yo = -1;
LH	R2, Offset(Steppers_step+18)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L_Steppers_setdirection104
NOP	
J	L_Steppers_setdirection28
NOP	
L_Steppers_setdirection104:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_setdirection29
NOP	
L_Steppers_setdirection28:
;Steppers.c,102 :: 		else step.yo = 1;
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
L_Steppers_setdirection29:
;Steppers.c,103 :: 		step.dy = abs(step.dy);
LH	R25, Offset(Steppers_step+18)(GP)
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_step+18)(GP)
;Steppers.c,105 :: 		step.dx = step.x3 - step.xl;
LH	R3, Offset(Steppers_step+0)(GP)
LH	R2, Offset(Steppers_step+8)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+16)(GP)
;Steppers.c,106 :: 		if(step.dx < 0) step.xo = -1;
LH	R2, Offset(Steppers_step+16)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L_Steppers_setdirection105
NOP	
J	L_Steppers_setdirection30
NOP	
L_Steppers_setdirection105:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_setdirection31
NOP	
L_Steppers_setdirection30:
;Steppers.c,107 :: 		else step.xo = 1;
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
L_Steppers_setdirection31:
;Steppers.c,108 :: 		step.dx = abs(step.dx);
LH	R25, Offset(Steppers_step+16)(GP)
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_step+16)(GP)
;Steppers.c,110 :: 		if(step.dx>step.dy){step.fxy = step.dx - step.dy;step.fm=0;}
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+16)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L_Steppers_setdirection106
NOP	
J	L_Steppers_setdirection32
NOP	
L_Steppers_setdirection106:
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+16)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
SB	R0, Offset(Steppers_step+24)(GP)
J	L_Steppers_setdirection33
NOP	
L_Steppers_setdirection32:
;Steppers.c,111 :: 		else {step.fxy = step.dy - step.dx; step.fm=1;}
LH	R3, Offset(Steppers_step+16)(GP)
LH	R2, Offset(Steppers_step+18)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
ORI	R2, R0, 1
SB	R2, Offset(Steppers_step+24)(GP)
L_Steppers_setdirection33:
;Steppers.c,112 :: 		}
L_end_setdirection:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of Steppers_setdirection
_doline:
;Steppers.c,114 :: 		void doline(){
ADDIU	SP, SP, -44
SW	RA, 0(SP)
;Steppers.c,115 :: 		step.stepnum = step.x2 = step.y2 = step.fxy = 0;
SW	R25, 4(SP)
SH	R0, Offset(Steppers_step+22)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
SH	R2, Offset(Steppers_step+6)(GP)
LH	R2, Offset(Steppers_step+6)(GP)
SH	R2, Offset(Steppers_step+4)(GP)
LH	R2, Offset(Steppers_step+4)(GP)
SH	R2, Offset(Steppers_step+20)(GP)
;Steppers.c,116 :: 		setdirection();
JAL	Steppers_setdirection+0
NOP	
;Steppers.c,117 :: 		while(DMA_IsOn(1));
L_doline34:
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
BNE	R2, R0, L__doline109
NOP	
J	L_doline35
NOP	
L__doline109:
J	L_doline34
NOP	
L_doline35:
;Steppers.c,118 :: 		dma_printf("%s","\nStep\tFXY\tX2\tY2\t\tXO\tYO\toutput\tacc_val\tdrag\toil\n");
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
;Steppers.c,119 :: 		while ( (step.dx > step.x2) && (step.dy > step.y2)){ // at endpoint?
L_doline36:
LH	R3, Offset(Steppers_step+4)(GP)
LH	R2, Offset(Steppers_step+16)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__doline110
NOP	
J	L__doline52
NOP	
L__doline110:
LH	R3, Offset(Steppers_step+6)(GP)
LH	R2, Offset(Steppers_step+18)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__doline111
NOP	
J	L__doline51
NOP	
L__doline111:
L__doline50:
;Steppers.c,120 :: 		_delay_();
JAL	__delay_+0
NOP	
;Steppers.c,122 :: 		out = 0;
SH	R0, Offset(_out+0)(GP)
;Steppers.c,123 :: 		if(!step.fm){
LB	R2, Offset(Steppers_step+24)(GP)
BEQ	R2, R0, L__doline112
NOP	
J	L_doline40
NOP	
L__doline112:
;Steppers.c,124 :: 		++step.x2; step.fxy -= step.dy;
LH	R2, Offset(Steppers_step+4)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+4)(GP)
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,125 :: 		bit_true(out,bit(0));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 1
SH	R2, Offset(_out+0)(GP)
;Steppers.c,126 :: 		if(step.fxy < 0){
LH	R2, Offset(Steppers_step+22)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L__doline113
NOP	
J	L_doline41
NOP	
L__doline113:
;Steppers.c,127 :: 		++step.y2; step.fxy += step.dx;
LH	R2, Offset(Steppers_step+6)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+6)(GP)
LH	R3, Offset(Steppers_step+16)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,128 :: 		bit_true(out,bit(1));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 2
SH	R2, Offset(_out+0)(GP)
;Steppers.c,129 :: 		}
L_doline41:
;Steppers.c,130 :: 		}else{
J	L_doline42
NOP	
L_doline40:
;Steppers.c,131 :: 		++step.y2; step.fxy -= step.dx;
LH	R2, Offset(Steppers_step+6)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+6)(GP)
LH	R3, Offset(Steppers_step+16)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,132 :: 		bit_true(out,bit(1));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 2
SH	R2, Offset(_out+0)(GP)
;Steppers.c,133 :: 		if(step.fxy < 0){
LH	R2, Offset(Steppers_step+22)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L__doline114
NOP	
J	L_doline43
NOP	
L__doline114:
;Steppers.c,134 :: 		++step.x2;step.fxy += step.dy;
LH	R2, Offset(Steppers_step+4)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+4)(GP)
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,135 :: 		bit_true(out,bit(0));
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 1
SH	R2, Offset(_out+0)(GP)
;Steppers.c,136 :: 		}
L_doline43:
;Steppers.c,137 :: 		}
L_doline42:
;Steppers.c,138 :: 		while(DMA_IsOn(1));
L_doline44:
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
BNE	R2, R0, L__doline116
NOP	
J	L_doline45
NOP	
L__doline116:
J	L_doline44
NOP	
L_doline45:
;Steppers.c,139 :: 		dma_printf("\n%d\t%d\t%d\t%d\t\t%d\t%d\t%d\t%d\t%d\t%d"
ADDIU	R23, SP, 11
ADDIU	R22, R23, 32
LUI	R24, hi_addr(?ICS?lstr3_Steppers+0)
ORI	R24, R24, lo_addr(?ICS?lstr3_Steppers+0)
JAL	___CC2DW+0
NOP	
ADDIU	R3, SP, 11
;Steppers.c,141 :: 		step.y2,step.xo,step.yo,out,acc_val,drag,oil);
LH	R2, Offset(Steppers_oil+0)(GP)
ADDIU	SP, SP, -44
SH	R2, 40(SP)
LH	R2, Offset(Steppers_drag+0)(GP)
SH	R2, 36(SP)
LH	R2, Offset(Steppers_acc_val+0)(GP)
SH	R2, 32(SP)
LHU	R2, Offset(_out+0)(GP)
SH	R2, 28(SP)
LH	R2, Offset(Steppers_step+14)(GP)
SH	R2, 24(SP)
LH	R2, Offset(Steppers_step+12)(GP)
SH	R2, 20(SP)
LH	R2, Offset(Steppers_step+6)(GP)
SH	R2, 16(SP)
;Steppers.c,140 :: 		,step.stepnum++,step.fxy,step.x2,
LH	R2, Offset(Steppers_step+4)(GP)
SH	R2, 12(SP)
LH	R2, Offset(Steppers_step+22)(GP)
SH	R2, 8(SP)
LH	R2, Offset(Steppers_step+20)(GP)
SH	R2, 4(SP)
;Steppers.c,139 :: 		dma_printf("\n%d\t%d\t%d\t%d\t\t%d\t%d\t%d\t%d\t%d\t%d"
SW	R3, 0(SP)
;Steppers.c,141 :: 		step.y2,step.xo,step.yo,out,acc_val,drag,oil);
JAL	_dma_printf+0
NOP	
ADDIU	SP, SP, 44
LH	R2, Offset(Steppers_step+20)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+20)(GP)
;Steppers.c,142 :: 		}
J	L_doline36
NOP	
;Steppers.c,119 :: 		while ( (step.dx > step.x2) && (step.dy > step.y2)){ // at endpoint?
L__doline52:
L__doline51:
;Steppers.c,143 :: 		}
L_end_doline:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 44
JR	RA
NOP	
; end of _doline
