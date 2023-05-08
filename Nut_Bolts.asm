_flt2ulong:
;Nut_Bolts.c,8 :: 		unsigned long flt2ulong(float f_){
ADDIU	SP, SP, -24
SW	RA, 0(SP)
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
SWC1	S12, 20(SP)
;Nut_Bolts.c,9 :: 		unsigned long ul_ = 0;
MOVZ	R30, R0, R0
SW	R30, 16(SP)
;Nut_Bolts.c,10 :: 		memcpy(&ul_,&f_,sizeof(float));
ADDIU	R3, SP, 20
ADDIU	R2, SP, 16
ORI	R27, R0, 4
MOVZ	R26, R3, R0
MOVZ	R25, R2, R0
JAL	_memcpy+0
NOP	
;Nut_Bolts.c,11 :: 		return ul_;
LW	R2, 16(SP)
;Nut_Bolts.c,12 :: 		}
;Nut_Bolts.c,11 :: 		return ul_;
;Nut_Bolts.c,12 :: 		}
L_end_flt2ulong:
LW	R27, 12(SP)
LW	R26, 8(SP)
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 24
JR	RA
NOP	
; end of _flt2ulong
_ulong2flt:
;Nut_Bolts.c,15 :: 		float ulong2flt(unsigned long ul_){
ADDIU	SP, SP, -24
SW	RA, 0(SP)
SW	R25, 4(SP)
SW	R26, 8(SP)
SW	R27, 12(SP)
SW	R25, 20(SP)
;Nut_Bolts.c,16 :: 		float f_ = 0.0;
MOVZ	R30, R0, R0
SW	R30, 16(SP)
;Nut_Bolts.c,17 :: 		memcpy(&f_,&ul_,sizeof(unsigned long ));
ADDIU	R3, SP, 20
ADDIU	R2, SP, 16
ORI	R27, R0, 4
MOVZ	R26, R3, R0
MOVZ	R25, R2, R0
JAL	_memcpy+0
NOP	
;Nut_Bolts.c,19 :: 		return f_;
LWC1	S0, 16(SP)
;Nut_Bolts.c,20 :: 		}
;Nut_Bolts.c,19 :: 		return f_;
;Nut_Bolts.c,20 :: 		}
L_end_ulong2flt:
LW	R27, 12(SP)
LW	R26, 8(SP)
LW	R25, 4(SP)
LW	RA, 0(SP)
ADDIU	SP, SP, 24
JR	RA
NOP	
; end of _ulong2flt
_fround:
;Nut_Bolts.c,23 :: 		float fround(float val){
;Nut_Bolts.c,24 :: 		float value = (long)(val * 100.00 + 0.5);
LUI	R2, 17096
ORI	R2, R2, 0
MTC1	R2, S0
MUL.S 	S1, S12, S0
LUI	R2, 16128
ORI	R2, R2, 0
MTC1	R2, S0
ADD.S 	S0, S1, S0
CVT36.S 	S0, S0
MFC1	R2, S0
MTC1	R2, S0
CVT32.W 	S1, S0
;Nut_Bolts.c,25 :: 		return (float)(value / 100.00);
LUI	R2, 17096
ORI	R2, R2, 0
MTC1	R2, S0
DIV.S 	S0, S1, S0
;Nut_Bolts.c,26 :: 		}
L_end_fround:
JR	RA
NOP	
; end of _fround
_round:
;Nut_Bolts.c,29 :: 		int round(float val){
ADDIU	SP, SP, -12
SW	RA, 0(SP)
;Nut_Bolts.c,30 :: 		float temp = 0.00,tempC = 0.00,tempF = 0.00,dec = 0.00;
;Nut_Bolts.c,31 :: 		tempC = ceil(val);
JAL	_ceil+0
NOP	
SWC1	S0, 8(SP)
;Nut_Bolts.c,32 :: 		tempF = floor(val);
JAL	_floor+0
NOP	
; tempF start address is: 16 (R4)
MOV.S 	S2, S0
;Nut_Bolts.c,33 :: 		dec = val - tempF;
SUB.S 	S1, S12, S0
;Nut_Bolts.c,34 :: 		temp = (dec > 0.5)? tempC : tempF;
LUI	R2, 16128
ORI	R2, R2, 0
MTC1	R2, S0
C.LE.S 	0, S1, S0
BC1F	0, L__round8
NOP	
J	L_round0
NOP	
L__round8:
; tempF end address is: 16 (R4)
LWC1	S0, 8(SP)
SWC1	S0, 4(SP)
J	L_round1
NOP	
L_round0:
; tempF start address is: 16 (R4)
SWC1	S2, 4(SP)
; tempF end address is: 16 (R4)
L_round1:
;Nut_Bolts.c,35 :: 		return (int)temp;
LWC1	S0, 4(SP)
CVT36.S 	S0, S0
MFC1	R2, S0
;Nut_Bolts.c,36 :: 		}
L_end_round:
LW	RA, 0(SP)
ADDIU	SP, SP, 12
JR	RA
NOP	
; end of _round
_lround:
;Nut_Bolts.c,39 :: 		long lround(float val){
ADDIU	SP, SP, -12
SW	RA, 0(SP)
;Nut_Bolts.c,40 :: 		float temp = 0.00,tempC = 0.00,tempF = 0.00,dec = 0.00;
;Nut_Bolts.c,41 :: 		tempC = ceil(val);
JAL	_ceil+0
NOP	
SWC1	S0, 8(SP)
;Nut_Bolts.c,42 :: 		tempF = floor(val);
JAL	_floor+0
NOP	
; tempF start address is: 16 (R4)
MOV.S 	S2, S0
;Nut_Bolts.c,43 :: 		dec = val - tempF;
SUB.S 	S1, S12, S0
;Nut_Bolts.c,44 :: 		temp = (dec > 0.5)? tempC : tempF;
LUI	R2, 16128
ORI	R2, R2, 0
MTC1	R2, S0
C.LE.S 	0, S1, S0
BC1F	0, L__lround10
NOP	
J	L_lround2
NOP	
L__lround10:
; tempF end address is: 16 (R4)
LWC1	S0, 8(SP)
SWC1	S0, 4(SP)
J	L_lround3
NOP	
L_lround2:
; tempF start address is: 16 (R4)
SWC1	S2, 4(SP)
; tempF end address is: 16 (R4)
L_lround3:
;Nut_Bolts.c,45 :: 		return (long)temp;
LWC1	S0, 4(SP)
CVT36.S 	S0, S0
MFC1	R2, S0
;Nut_Bolts.c,46 :: 		}
L_end_lround:
LW	RA, 0(SP)
ADDIU	SP, SP, 12
JR	RA
NOP	
; end of _lround
