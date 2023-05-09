_main:
;Pic32mzCNC_V2.c,14 :: 		void main() {
ADDIU	SP, SP, -8
;Pic32mzCNC_V2.c,16 :: 		PinMode();
JAL	_PinMode+0
NOP	
;Pic32mzCNC_V2.c,17 :: 		EI();
EI	R30
;Pic32mzCNC_V2.c,18 :: 		m0 = false;
LBU	R2, Offset(main_m0_L0+0)(GP)
INS	R2, R0, BitPos(main_m0_L0+0), 1
SB	R2, Offset(main_m0_L0+0)(GP)
;Pic32mzCNC_V2.c,19 :: 		setDragOil(20,100,2);
ORI	R27, R0, 2
ORI	R26, R0, 100
ORI	R25, R0, 20
JAL	_setDragOil+0
NOP	
;Pic32mzCNC_V2.c,20 :: 		while(1){
JAL	_Initialize_Stepper+0
NOP	
;Pic32mzCNC_V2.c,22 :: 		#ifdef LED_STATUS
L_main0:
;Pic32mzCNC_V2.c,25 :: 		if(!SW1 & !m0){
LBU	R2, Offset(_TMR+0)(GP)
SRL	R3, R2, 4
_LX	
INS	R2, R3, BitPos(LED1+0), 1
_SX	
;Pic32mzCNC_V2.c,27 :: 		setStepXY(0,0,50,20);
_LX	
EXT	R2, R2, BitPos(SW1+0), 1
XORI	R3, R2, 1
LBU	R2, Offset(main_m0_L0+0)(GP)
EXT	R2, R2, BitPos(main_m0_L0+0), 1
XORI	R2, R2, 1
AND	R2, R3, R2
BNE	R2, R0, L__main6
NOP	
J	L_main2
NOP	
L__main6:
;Pic32mzCNC_V2.c,28 :: 		doline();
LBU	R2, Offset(main_m0_L0+0)(GP)
ORI	R2, R2, BitMask(main_m0_L0+0)
SB	R2, Offset(main_m0_L0+0)(GP)
;Pic32mzCNC_V2.c,29 :: 		}
ORI	R30, R0, 72
SB	R30, 0(SP)
ORI	R30, R0, 101
SB	R30, 1(SP)
ORI	R30, R0, 108
SB	R30, 2(SP)
ORI	R30, R0, 108
SB	R30, 3(SP)
ORI	R30, R0, 111
SB	R30, 4(SP)
MOVZ	R30, R0, R0
SB	R30, 5(SP)
ADDIU	R2, SP, 0
MOVZ	R25, R2, R0
JAL	_UART3_Write_Text+0
NOP	
;Pic32mzCNC_V2.c,33 :: 		}
L_main2:
;Pic32mzCNC_V2.c,34 :: 		unresolved line
LBU	R2, Offset(main_m0_L0+0)(GP)
EXT	R3, R2, BitPos(main_m0_L0+0), 1
_LX	
EXT	R2, R2, BitPos(SW1+0), 1
AND	R2, R2, R3
BNE	R2, R0, L__main8
NOP	
J	L_main3
NOP	
L__main8:
;Pic32mzCNC_V2.c,35 :: 		unresolved line
LBU	R2, Offset(main_m0_L0+0)(GP)
INS	R2, R0, BitPos(main_m0_L0+0), 1
SB	R2, Offset(main_m0_L0+0)(GP)
L_main3:
;Pic32mzCNC_V2.c,36 :: 		unresolved line
J	L_main0
NOP	
;Pic32mzCNC_V2.c,37 :: 		unresolved line
L_end_main:
L__main_end_loop:
J	L__main_end_loop
NOP	
; end of _main
