#ifndef SPEED_CONTROL_H
#define SPEED_CONTROL_H

#include <errno.h>
#include "global.h"
#include "plib_tmr4.h"

/*! \brief Holding data used by timer interrupt for speed ramp calculation.
 *
 *  Contains data used by timer interrupt to calculate speed profile.
 *  Data is written to it by move(), when stepper motor is moving (timer
 *  interrupt running) data is read/updated when calculating a new step_delay
 */
typedef struct {
  //! What part of the speed ramp we are in.
  uint8_t run_state : 3;
  //! Direction stepper motor should move.
  uint8_t dir : 1;
  //!Dummy bits to condume the balance of the byte
  uint8_t dummy : 4;
  //! Peroid of next timer delay. At start this value set the accelration rate.
  uint16_t step_delay;
  //! What step_pos to start decelaration
  uint16_t decel_start;
  //! Sets deceleration rate.
  int16_t decel_val;
  //! Minimum time delay (max speed)
  int16_t min_delay;
  //! Counter used when accelerateing/decelerateing to calculate step_delay.
  int16_t accel_count;
  //! Keep track of remainder from new_step-delay calculation to incrase accurancy
  uint16_t rest;
} speedRampData;

/*! \Brief Frequency of timer1 in [Hz].
 *
 * Modify this according to frequency used. Because of the prescaler setting,
 * the timer1 frequency is the clock frequency divided by 8.
 */
// Timer/Counter 1 running on 3,686MHz / 8 = 460,75kHz (2,17uS). (T1-FREQ 460750)
#define T1_FREQ TMR4_FrequencyGet()

//! Number of (full)steps per round on stepper motor in use.
#define FSPR 200
#define MICROSTEPS 16

#ifdef HALFSTEPS
  #define SPR (FSPR*2)
  #pragma message("[speed_cntr.c] *** Using Halfsteps ***")
#endif
#ifdef FULLSTEPS
  #define SPR FSPR
  #pragma message("[speed_cntr.c] *** Using Fullsteps ***")
#endif
#ifdef MICROSTEPS
  #define SPR (FSPR*MICROSTEPS)
  #pragma message("[speed_cntr.c] *** Using Halfsteps ***")
#endif

#ifndef HALFSTEPS
  #ifndef FULLSTEPS
    #ifndef MICROSTEPS
        #error FULLSTEPS/HALFSTEPS not defined!
    #endif
  #endif
#endif


// Direction of stepper motor movement
#define CW  0
#define CCW 1



// Maths constants. To simplify maths when calculating in speed_cntr_Move().
#define ALPHA (2*3.14159/SPR)                    // 2*pi/spr
#define A_T_x100 ((long)(ALPHA*T1_FREQ*100))     // (ALPHA / T1_FREQ)*100
#define T1_FREQ_148 ((int)((T1_FREQ*0.676)/100)) // divided by 100 and scaled by 0.676
#define A_SQ (long)(ALPHA*2*10000000000)         // ALPHA*2*10000000000
#define A_x20000 (int)(ALPHA*20000)              // ALPHA*20000

// Speed ramp states
#define STOP  0
#define ACCEL 1
#define DECEL 2
#define RUN   3

void speed_cntr_Move(signed int step, unsigned int accel, unsigned int decel, unsigned int speed);
void speed_cntr_Init_Timer1(void);
void speed_cntr_TIMER1_COMPA( uintptr_t context );

//! Global status flags
extern struct GLOBAL_FLAGS status;

#endif /* _SPEED_CONTROL_H */

/* *****************************************************************************
 End of File
 */
