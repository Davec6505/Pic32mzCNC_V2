_Initialize_Stepper:
;Steppers.c,24 :: 		*/
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,25 :: 		static void delay(){
SW	R25, 4(SP)
LUI	R25, hi_addr(_delay+0)
ORI	R25, R25, lo_addr(_delay+0)
JAL	_InitTimer8+0
NOP	
;Steppers.c,26 :: 		static long ii;
L_end_Initialize_Stepper:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of _Initialize_Stepper
_delay:
;Steppers.c,31 :: 		while (getUsec() <= (long)acc_val)continue;
;Steppers.c,35 :: 		/* drag increases the delay at the beginning */
_LX	
EXT	R2, R2, BitPos(LED2+0), 1
XORI	R3, R2, 1
_LX	
INS	R2, R3, BitPos(LED2+0), 1
_SX	
;Steppers.c,53 :: 		
L_end_delay:
JR	RA
NOP	
; end of _delay
_setStepXY:
;Steppers.c,55 :: 		Dly = delay;
;Steppers.c,56 :: 		feedrate = MAXFEED - _feedrate;
SH	R25, Offset(Steppers_step+0)(GP)
;Steppers.c,57 :: 		drag = _drag;
SH	R26, Offset(Steppers_step+2)(GP)
;Steppers.c,58 :: 		oil = _oil;
SH	R27, Offset(Steppers_step+8)(GP)
;Steppers.c,59 :: 		}
SH	R28, Offset(Steppers_step+10)(GP)
;Steppers.c,60 :: 		
L_end_setStepXY:
JR	RA
NOP	
; end of _setStepXY
_setDragOil:
;Steppers.c,62 :: 		static int rad,radrad,f,a,b,d;
;Steppers.c,63 :: 		int binrep = 0;
ORI	R2, R0, 180
SUBU	R2, R2, R25
SH	R2, Offset(Steppers_feedrate+0)(GP)
;Steppers.c,64 :: 		step.xo = step.yo = 0;
SH	R26, Offset(Steppers_drag+0)(GP)
;Steppers.c,65 :: 		if(d)binrep = binrep + 8;
SH	R27, Offset(Steppers_oil+0)(GP)
;Steppers.c,66 :: 		if(f)binrep = binrep + 4;
L_end_setDragOil:
JR	RA
NOP	
; end of _setDragOil
Steppers_getdir:
;Steppers.c,68 :: 		if(b)binrep = binrep + 1;
;Steppers.c,70 :: 		switch(binrep){
; binrep start address is: 12 (R3)
MOVZ	R3, R0, R0
;Steppers.c,71 :: 		case 0:  step.yo = -1; break;
SH	R0, Offset(Steppers_step+14)(GP)
LH	R2, Offset(Steppers_step+14)(GP)
SH	R2, Offset(Steppers_step+12)(GP)
;Steppers.c,72 :: 		case 1:  step.xo = -1; break;
LH	R2, Offset(Steppers_getdir_d_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir53
NOP	
J	L_Steppers_getdir40
NOP	
L_Steppers_getdir53:
ADDIU	R2, R3, 8
SEH	R3, R2
; binrep end address is: 12 (R3)
J	L_Steppers_getdir0
NOP	
L_Steppers_getdir40:
L_Steppers_getdir0:
;Steppers.c,73 :: 		case 2:  step.xo = 1;break;
; binrep start address is: 12 (R3)
LH	R2, Offset(Steppers_getdir_f_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir55
NOP	
J	L_Steppers_getdir41
NOP	
L_Steppers_getdir55:
ADDIU	R2, R3, 4
SEH	R3, R2
; binrep end address is: 12 (R3)
J	L_Steppers_getdir1
NOP	
L_Steppers_getdir41:
L_Steppers_getdir1:
;Steppers.c,74 :: 		case 3:  step.yo = 1; break;
; binrep start address is: 12 (R3)
LH	R2, Offset(Steppers_getdir_a_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir57
NOP	
J	L_Steppers_getdir42
NOP	
L_Steppers_getdir57:
ADDIU	R2, R3, 2
SEH	R3, R2
; binrep end address is: 12 (R3)
SEH	R4, R3
J	L_Steppers_getdir2
NOP	
L_Steppers_getdir42:
SEH	R4, R3
L_Steppers_getdir2:
;Steppers.c,75 :: 		case 4:  step.xo = 1;break;
; binrep start address is: 16 (R4)
LH	R2, Offset(Steppers_getdir_b_L0+0)(GP)
BNE	R2, R0, L_Steppers_getdir59
NOP	
J	L_Steppers_getdir43
NOP	
L_Steppers_getdir59:
ADDIU	R2, R4, 1
SEH	R4, R2
; binrep end address is: 16 (R4)
J	L_Steppers_getdir3
NOP	
L_Steppers_getdir43:
L_Steppers_getdir3:
;Steppers.c,77 :: 		case 6:  step.yo = 1; break;
; binrep start address is: 16 (R4)
J	L_Steppers_getdir4
NOP	
; binrep end address is: 16 (R4)
;Steppers.c,78 :: 		case 7:  step.xo = -1; break;
L_Steppers_getdir6:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,79 :: 		case 8:  step.xo = -1; break;
L_Steppers_getdir7:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,80 :: 		case 9:  step.yo = 1;  break;
L_Steppers_getdir8:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,81 :: 		case 10: step.yo = -1;break;
L_Steppers_getdir9:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,82 :: 		case 11: step.xo = 1;break;
L_Steppers_getdir10:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,83 :: 		case 12: step.yo = 1;break;
L_Steppers_getdir11:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,84 :: 		case 13: step.xo = 1; break;
L_Steppers_getdir12:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,85 :: 		case 14: step.xo = -1; break;
L_Steppers_getdir13:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,86 :: 		case 15: step.yo = -1; break;
L_Steppers_getdir14:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,87 :: 		}
L_Steppers_getdir15:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,88 :: 		}
L_Steppers_getdir16:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,89 :: 		
L_Steppers_getdir17:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,90 :: 		static void setdirection(){
L_Steppers_getdir18:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,91 :: 		\
L_Steppers_getdir19:
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,92 :: 		step.dy = step.y3 - step.yl;
L_Steppers_getdir20:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,93 :: 		if(step.dy < 0) step.yo = -1;
L_Steppers_getdir21:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_getdir5
NOP	
;Steppers.c,94 :: 		else step.yo = 1;
L_Steppers_getdir4:
; binrep start address is: 16 (R4)
SEH	R2, R4
BNE	R2, R0, L_Steppers_getdir61
NOP	
J	L_Steppers_getdir6
NOP	
L_Steppers_getdir61:
SEH	R3, R4
ORI	R2, R0, 1
BNE	R3, R2, L_Steppers_getdir63
NOP	
J	L_Steppers_getdir7
NOP	
L_Steppers_getdir63:
SEH	R3, R4
ORI	R2, R0, 2
BNE	R3, R2, L_Steppers_getdir65
NOP	
J	L_Steppers_getdir8
NOP	
L_Steppers_getdir65:
SEH	R3, R4
ORI	R2, R0, 3
BNE	R3, R2, L_Steppers_getdir67
NOP	
J	L_Steppers_getdir9
NOP	
L_Steppers_getdir67:
SEH	R3, R4
ORI	R2, R0, 4
BNE	R3, R2, L_Steppers_getdir69
NOP	
J	L_Steppers_getdir10
NOP	
L_Steppers_getdir69:
SEH	R3, R4
ORI	R2, R0, 5
BNE	R3, R2, L_Steppers_getdir71
NOP	
J	L_Steppers_getdir11
NOP	
L_Steppers_getdir71:
SEH	R3, R4
ORI	R2, R0, 6
BNE	R3, R2, L_Steppers_getdir73
NOP	
J	L_Steppers_getdir12
NOP	
L_Steppers_getdir73:
SEH	R3, R4
ORI	R2, R0, 7
BNE	R3, R2, L_Steppers_getdir75
NOP	
J	L_Steppers_getdir13
NOP	
L_Steppers_getdir75:
SEH	R3, R4
ORI	R2, R0, 8
BNE	R3, R2, L_Steppers_getdir77
NOP	
J	L_Steppers_getdir14
NOP	
L_Steppers_getdir77:
SEH	R3, R4
ORI	R2, R0, 9
BNE	R3, R2, L_Steppers_getdir79
NOP	
J	L_Steppers_getdir15
NOP	
L_Steppers_getdir79:
SEH	R3, R4
ORI	R2, R0, 10
BNE	R3, R2, L_Steppers_getdir81
NOP	
J	L_Steppers_getdir16
NOP	
L_Steppers_getdir81:
SEH	R3, R4
ORI	R2, R0, 11
BNE	R3, R2, L_Steppers_getdir83
NOP	
J	L_Steppers_getdir17
NOP	
L_Steppers_getdir83:
SEH	R3, R4
ORI	R2, R0, 12
BNE	R3, R2, L_Steppers_getdir85
NOP	
J	L_Steppers_getdir18
NOP	
L_Steppers_getdir85:
SEH	R3, R4
ORI	R2, R0, 13
BNE	R3, R2, L_Steppers_getdir87
NOP	
J	L_Steppers_getdir19
NOP	
L_Steppers_getdir87:
SEH	R3, R4
ORI	R2, R0, 14
BNE	R3, R2, L_Steppers_getdir89
NOP	
J	L_Steppers_getdir20
NOP	
L_Steppers_getdir89:
SEH	R3, R4
; binrep end address is: 16 (R4)
ORI	R2, R0, 15
BNE	R3, R2, L_Steppers_getdir91
NOP	
J	L_Steppers_getdir21
NOP	
L_Steppers_getdir91:
L_Steppers_getdir5:
;Steppers.c,95 :: 		step.dy = abs(step.dy);
L_end_getdir:
JR	RA
NOP	
; end of Steppers_getdir
Steppers_setdirection:
;Steppers.c,98 :: 		if(step.dx < 0) step.xo = -1;
ADDIU	SP, SP, -8
SW	RA, 0(SP)
;Steppers.c,100 :: 		step.dx = abs(step.dx);
SW	R25, 4(SP)
LH	R3, Offset(Steppers_step+2)(GP)
LH	R2, Offset(Steppers_step+10)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+18)(GP)
;Steppers.c,101 :: 		
LH	R2, Offset(Steppers_step+18)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L_Steppers_setdirection93
NOP	
J	L_Steppers_setdirection22
NOP	
L_Steppers_setdirection93:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+14)(GP)
J	L_Steppers_setdirection23
NOP	
L_Steppers_setdirection22:
;Steppers.c,102 :: 		if(step.dx>step.dy){step.fxy = step.dx - step.dy;step.fm=0;}
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+14)(GP)
L_Steppers_setdirection23:
;Steppers.c,103 :: 		else {step.fxy = step.dy - step.dx; step.fm=1;}
LH	R25, Offset(Steppers_step+18)(GP)
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_step+18)(GP)
;Steppers.c,105 :: 		
LH	R3, Offset(Steppers_step+0)(GP)
LH	R2, Offset(Steppers_step+8)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+16)(GP)
;Steppers.c,106 :: 		void doline(){
LH	R2, Offset(Steppers_step+16)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L_Steppers_setdirection94
NOP	
J	L_Steppers_setdirection24
NOP	
L_Steppers_setdirection94:
ORI	R2, R0, 65535
SH	R2, Offset(Steppers_step+12)(GP)
J	L_Steppers_setdirection25
NOP	
L_Steppers_setdirection24:
;Steppers.c,107 :: 		step.stepnum = step.x2 = step.y2 = step.fxy = 0;
ORI	R2, R0, 1
SH	R2, Offset(Steppers_step+12)(GP)
L_Steppers_setdirection25:
;Steppers.c,108 :: 		setdirection();
LH	R25, Offset(Steppers_step+16)(GP)
JAL	_abs+0
NOP	
SH	R2, Offset(Steppers_step+16)(GP)
;Steppers.c,110 :: 		dma_printf("%s","\nStep\tFXY\tX2\tY2\t\tXO\tYO\toutput\tacc_val\tdrag\toil\n");
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+16)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L_Steppers_setdirection95
NOP	
J	L_Steppers_setdirection26
NOP	
L_Steppers_setdirection95:
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+16)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
SB	R0, Offset(Steppers_step+24)(GP)
J	L_Steppers_setdirection27
NOP	
L_Steppers_setdirection26:
;Steppers.c,111 :: 		while ( (step.dx > step.x2) && (step.dy > step.y2)){ // at endpoint?
LH	R3, Offset(Steppers_step+16)(GP)
LH	R2, Offset(Steppers_step+18)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
ORI	R2, R0, 1
SB	R2, Offset(Steppers_step+24)(GP)
L_Steppers_setdirection27:
;Steppers.c,113 :: 		//if(!T8IE_bit){T8IE_bit = true;TMR8 =
L_end_setdirection:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 8
JR	RA
NOP	
; end of Steppers_setdirection
_doline:
;Steppers.c,116 :: 		++step.x2; step.fxy -= step.dy;
ADDIU	SP, SP, -44
SW	RA, 0(SP)
;Steppers.c,117 :: 		bit_true(out,bit(0));
SW	R25, 4(SP)
SH	R0, Offset(Steppers_step+22)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
SH	R2, Offset(Steppers_step+6)(GP)
LH	R2, Offset(Steppers_step+6)(GP)
SH	R2, Offset(Steppers_step+4)(GP)
LH	R2, Offset(Steppers_step+4)(GP)
SH	R2, Offset(Steppers_step+20)(GP)
;Steppers.c,118 :: 		if(step.fxy < 0){
JAL	Steppers_setdirection+0
NOP	
;Steppers.c,119 :: 		++step.y2; step.fxy += step.dx;
L_doline28:
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
BNE	R2, R0, L__doline98
NOP	
J	L_doline29
NOP	
L__doline98:
J	L_doline28
NOP	
L_doline29:
;Steppers.c,120 :: 		bit_true(out,bit(1));
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
;Steppers.c,121 :: 		}
L_doline30:
LH	R3, Offset(Steppers_step+4)(GP)
LH	R2, Offset(Steppers_step+16)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__doline99
NOP	
J	L__doline46
NOP	
L__doline99:
LH	R3, Offset(Steppers_step+6)(GP)
LH	R2, Offset(Steppers_step+18)(GP)
SLT	R2, R3, R2
BNE	R2, R0, L__doline100
NOP	
J	L__doline45
NOP	
L__doline100:
L__doline44:
;Steppers.c,123 :: 		++step.y2; step.fxy -= step.dx;
SH	R0, Offset(_out+0)(GP)
;Steppers.c,124 :: 		bit_true(out,bit(1));
LB	R2, Offset(Steppers_step+24)(GP)
BEQ	R2, R0, L__doline101
NOP	
J	L_doline34
NOP	
L__doline101:
;Steppers.c,125 :: 		if(step.fxy < 0){
LH	R2, Offset(Steppers_step+4)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+4)(GP)
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,126 :: 		++step.x2;step.fxy += step.dy;
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 1
SH	R2, Offset(_out+0)(GP)
;Steppers.c,127 :: 		bit_true(out,bit(0));
LH	R2, Offset(Steppers_step+22)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L__doline102
NOP	
J	L_doline35
NOP	
L__doline102:
;Steppers.c,128 :: 		}
LH	R2, Offset(Steppers_step+6)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+6)(GP)
LH	R3, Offset(Steppers_step+16)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,129 :: 		}
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 2
SH	R2, Offset(_out+0)(GP)
;Steppers.c,130 :: 		while(DMA_IsOn(1));
L_doline35:
;Steppers.c,131 :: 		dma_printf("\n%d\t%d\t%d\t%d\t\t%d\t%d\t%d\t%d\t%d\t%d"
J	L_doline36
NOP	
L_doline34:
;Steppers.c,132 :: 		,step.stepnum++,step.fxy,step.x2,
LH	R2, Offset(Steppers_step+6)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+6)(GP)
LH	R3, Offset(Steppers_step+16)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
SUBU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,133 :: 		step.y2,step.xo,step.yo,out,acc_val,drag,oil);
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 2
SH	R2, Offset(_out+0)(GP)
;Steppers.c,134 :: 		}
LH	R2, Offset(Steppers_step+22)(GP)
SLTI	R2, R2, 0
BNE	R2, R0, L__doline103
NOP	
J	L_doline37
NOP	
L__doline103:
;Steppers.c,135 :: 		}
LH	R2, Offset(Steppers_step+4)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+4)(GP)
LH	R3, Offset(Steppers_step+18)(GP)
LH	R2, Offset(Steppers_step+22)(GP)
ADDU	R2, R2, R3
SH	R2, Offset(Steppers_step+22)(GP)
;Steppers.c,136 :: 		unresolved line
LHU	R2, Offset(_out+0)(GP)
ORI	R2, R2, 1
SH	R2, Offset(_out+0)(GP)
;Steppers.c,137 :: 		unresolved line
L_doline37:
;Steppers.c,138 :: 		unresolved line
L_doline36:
;Steppers.c,139 :: 		unresolved line
L_doline38:
ORI	R25, R0, 1
JAL	_DMA_IsOn+0
NOP	
BNE	R2, R0, L__doline105
NOP	
J	L_doline39
NOP	
L__doline105:
J	L_doline38
NOP	
L_doline39:
;Steppers.c,140 :: 		unresolved line
ADDIU	R23, SP, 11
ADDIU	R22, R23, 32
LUI	R24, hi_addr(?ICS?lstr3_Steppers+0)
ORI	R24, R24, lo_addr(?ICS?lstr3_Steppers+0)
JAL	___CC2DW+0
NOP	
ADDIU	R3, SP, 11
;Steppers.c,142 :: 		unresolved line
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
;Steppers.c,141 :: 		unresolved line
LH	R2, Offset(Steppers_step+4)(GP)
SH	R2, 12(SP)
LH	R2, Offset(Steppers_step+22)(GP)
SH	R2, 8(SP)
LH	R2, Offset(Steppers_step+20)(GP)
SH	R2, 4(SP)
;Steppers.c,140 :: 		unresolved line
SW	R3, 0(SP)
;Steppers.c,142 :: 		unresolved line
JAL	_dma_printf+0
NOP	
ADDIU	SP, SP, 44
LH	R2, Offset(Steppers_step+20)(GP)
ADDIU	R2, R2, 1
SH	R2, Offset(Steppers_step+20)(GP)
;Steppers.c,143 :: 		unresolved line
J	L_doline30
NOP	
;Steppers.c,121 :: 		}
L__doline46:
L__doline45:
;Steppers.c,144 :: 		unresolved line
L_end_doline:
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 44
JR	RA
NOP	
; end of _doline
