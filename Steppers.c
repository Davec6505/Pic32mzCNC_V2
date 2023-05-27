#include "Steppers.h"



struct stepper{
unsigned short move: 1;
unsigned short dummy: 7; /* consume the ballance of the byte move */
short fm; /* value of function / fm = master [1=y%0=x]*/
int xl,yl; /* starting point */
int x2,y2; /* relative position */
int x3,y3; /* endpoint */
int xo,yo; /* direction of output: +1, -I, or 0 */
int dx,dy; /* differentials of x and y */
int stepnum;
int fxy;
};

volatile static struct stepper step;
/* vars dealing with feedrate and delay func. */
volatile static int feedrate,drag,oil,acc_val;
unsigned int out;

void Init_Steppers(){
   step.move = false;
   InitTimer2();
   InitTimer8(&delay); //clock delay
   InitTimer9();       //pulse on time
}

/* delay must remain in this position for local scope association 
 * Timer8 provides a master freq, feedate is supplied from gcode
 * drag is acc constant and oil provides a form of s curve.
 */
void delay(){
static long ii;
static int i = 0;
 i = (i>50)? 0:i;
 //output on
 LED2   = true;
 step.move = true;
 //calculate next dly
 acc_val = abs((feedrate + drag) >> 2 ); // divide by 4
 acc_val <<= 8;
 
 //acc_val can't be < tmr9 pulse as the run concurrently
 acc_val = (acc_val < 5000)? 5000:acc_val;
 
 //reset tmr8 to count from 0
 SetPR8Value(acc_val);

 //start tmr9 to reset output after 50us
 RestartTmr9();
 
// falls off exponentially with drag? sort of s curve at start
//of change in speed; to allow for inertia in machine startup
 if(i < 10){
   drag = drag - (oil * oil);
   --oil;
 }else if(i > 40){
   ++oil;
   drag = drag + (oil * oil);
 }
 //keep track of drag i = steps moved by stepper must change
 //this to actual stepps
 i++;
 if (drag < 0) drag = 0;
 if(i == 40)oil = 0;
 drag = (drag > 100)? 100:drag;
}

void setStepXY(int _x1,int _y1,int _x3,int _y3){
  setDragOil(100,1);
  setFeedrate(180);
  step.xl = _x1;
  step.yl = _y1;
  step.x3 = _x3;
  step.y3 = _y3;
}

void setDragOil(int _drag,int _oil){
  drag = _drag;
  oil = _oil;
}

void setFeedrate(int _feedrate){
  feedrate = _feedrate;
}

static void getdir(){
static int rad,radrad,f,a,b,d;
int binrep = 0;
   step.xo = step.yo = 0;
  if(d)binrep = binrep + 8;
  if(f)binrep = binrep + 4;
  if(a)binrep = binrep + 2;
  if(b)binrep = binrep + 1;

  switch(binrep){
      case 0:  step.yo = -1; break;
      case 1:  step.xo = -1; break;
      case 2:  step.xo = 1;break;
      case 3:  step.yo = 1; break;
      case 4:  step.xo = 1;break;
      case 5:  step.yo = -1;break;
      case 6:  step.yo = 1; break;
      case 7:  step.xo = -1; break;
      case 8:  step.xo = -1; break;
      case 9:  step.yo = 1;  break;
      case 10: step.yo = -1;break;
      case 11: step.xo = 1;break;
      case 12: step.yo = 1;break;
      case 13: step.xo = 1; break;
      case 14: step.xo = -1; break;
      case 15: step.yo = -1; break;
  }
}

static void setdirection(){
  step.dy = step.y3 - step.yl;
  if(step.dy < 0) step.yo = -1;
  else step.yo = 1;
  step.dy = abs(step.dy);

  step.dx = step.x3 - step.xl;
  if(step.dx < 0) step.xo = -1;
  else step.xo = 1;
  step.dx = abs(step.dx);

  if(step.dx>step.dy){step.fxy = step.dx - step.dy;step.fm=0;}
  else {step.fxy = step.dy - step.dx; step.fm=1;}
}


void doline(){
  step.stepnum = step.x2 = step.y2 = step.fxy = 0;
  setdirection();
  while(DMA_IsOn(1));
  dma_printf("%s","\nStep\tFXY\tX2\tY2\t\tXO\tYO\toutput\tacc_val\tdrag\toil\n");
  RestartTmr8();
  while ( (step.dx > step.x2) && (step.dy > step.y2)){ // at endpoint?
     out = 0;
     while(!step.move){
      ;//_delay_();
     }
     step.move = false;
     if(!step.fm){
         ++step.x2; step.fxy -= step.dy;
         bit_true(out,bit(0));
        if(step.fxy < 0){
         ++step.y2; step.fxy += step.dx;
         bit_true(out,bit(1));
        }
     }else{
         ++step.y2; step.fxy -= step.dx;
         bit_true(out,bit(1));
       if(step.fxy < 0){
         ++step.x2;step.fxy += step.dy;
         bit_true(out,bit(0));
       }
     }
     while(DMA_IsOn(1));
     dma_printf("\n%d\t%d\t%d\t%d\t\t%d\t%d\t%d\t%d\t%d\t%d"
               ,step.stepnum++,step.fxy,step.x2,
               step.y2,step.xo,step.yo,out,acc_val,drag,oil);
  }
  StopTmr8();
}

/*
void _delay_(){
static long ii;
static int i = 0;

 acc_val = abs((feedrate + drag) << 2 ); // divide by 4
 acc_val <<= 8;
 ii = setUsec(0);
 while (getUsec() <= (long)acc_val)continue;
*/
/* falls off exponentially */
// if(i < 10){
 /* drag increases the delay at the beginning */
 /* to allow for inertia in machine startup */
 /*  drag = drag - (oil * oil);
   --oil;
 }else if(i > 40){
   ++oil;
   drag = drag + (oil * oil);
 }
 i++;
 if (drag < 0) drag = 0;
 if(i == 40)oil = 0;
 drag = (drag > 100)? 100:drag;

}
*/