_main:
<<<<<<< HEAD
;Pic32mzCNC_V2.c,17 :: 		void main() {
;Pic32mzCNC_V2.c,19 :: 		PinMode();
JAL	_PinMode+0
NOP	
;Pic32mzCNC_V2.c,20 :: 		EI();
EI	R30
;Pic32mzCNC_V2.c,21 :: 		m0 = false;
LBU	R2, Offset(main_m0_L0+0)(GP)
INS	R2, R0, BitPos(main_m0_L0+0), 1
SB	R2, Offset(main_m0_L0+0)(GP)
;Pic32mzCNC_V2.c,22 :: 		setDragOil(20,100,2);
=======
;Pic32mzCNC_V2.c,14 :: 		void main() {
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
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ORI	R27, R0, 2
ORI	R26, R0, 100
ORI	R25, R0, 20
JAL	_setDragOil+0
NOP	
<<<<<<< HEAD
;Pic32mzCNC_V2.c,23 :: 		while(1){
L_main0:
;Pic32mzCNC_V2.c,26 :: 		LED1 = TMR.clock >> 4;
=======
;Pic32mzCNC_V2.c,20 :: 		while(1){
L_main0:
;Pic32mzCNC_V2.c,23 :: 		LED1 = TMR.clock >> 4;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LBU	R2, Offset(_TMR+0)(GP)
SRL	R3, R2, 4
_LX	
INS	R2, R3, BitPos(LED1+0), 1
_SX	
<<<<<<< HEAD
;Pic32mzCNC_V2.c,28 :: 		if(!SW1 & !m0){
=======
;Pic32mzCNC_V2.c,25 :: 		if(!SW1 & !m0){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
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
<<<<<<< HEAD
;Pic32mzCNC_V2.c,29 :: 		m0 = true;
LBU	R2, Offset(main_m0_L0+0)(GP)
ORI	R2, R2, BitMask(main_m0_L0+0)
SB	R2, Offset(main_m0_L0+0)(GP)
;Pic32mzCNC_V2.c,30 :: 		setStepXY(0,0,50,20);
=======
;Pic32mzCNC_V2.c,26 :: 		m0 = true;
LBU	R2, Offset(main_m0_L0+0)(GP)
ORI	R2, R2, BitMask(main_m0_L0+0)
SB	R2, Offset(main_m0_L0+0)(GP)
;Pic32mzCNC_V2.c,27 :: 		setStepXY(0,0,50,20);
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
ORI	R28, R0, 20
ORI	R27, R0, 50
MOVZ	R26, R0, R0
MOVZ	R25, R0, R0
JAL	_setStepXY+0
NOP	
<<<<<<< HEAD
;Pic32mzCNC_V2.c,31 :: 		doline();
JAL	_doline+0
NOP	
;Pic32mzCNC_V2.c,32 :: 		}
L_main2:
;Pic32mzCNC_V2.c,33 :: 		if (SW1 & m0)
=======
;Pic32mzCNC_V2.c,28 :: 		doline();
JAL	_doline+0
NOP	
;Pic32mzCNC_V2.c,29 :: 		}
L_main2:
;Pic32mzCNC_V2.c,30 :: 		if (SW1 & m0)
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
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
<<<<<<< HEAD
;Pic32mzCNC_V2.c,34 :: 		m0 = false;
=======
;Pic32mzCNC_V2.c,31 :: 		m0 = false;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
LBU	R2, Offset(main_m0_L0+0)(GP)
INS	R2, R0, BitPos(main_m0_L0+0), 1
SB	R2, Offset(main_m0_L0+0)(GP)
L_main3:
<<<<<<< HEAD
;Pic32mzCNC_V2.c,35 :: 		}
J	L_main0
NOP	
;Pic32mzCNC_V2.c,36 :: 		}
=======
;Pic32mzCNC_V2.c,32 :: 		}
J	L_main0
NOP	
;Pic32mzCNC_V2.c,33 :: 		}
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
L_end_main:
L__main_end_loop:
J	L__main_end_loop
NOP	
; end of _main
