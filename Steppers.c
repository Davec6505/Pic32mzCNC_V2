#include "Steppers.h"

<<<<<<< HEAD

=======
void (*Dly)();
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345

struct stepper{
int xl,yl; /* starting point */
int x2,y2; /* relative position */
int x3,y3; /* endpoint */
int xo,yo; /* direction of output: +1, -I, or 0 */
int dx,dy; /* differentials of x and y */
int stepnum;
int fxy;
short fm; /* value of function / fm = master [1=y%0=x]*/
};

volatile static struct stepper step;
/* vars dealing with feedrate and delay func. */
volatile static int feedrate,drag,oil,acc_val;
unsigned int out;

<<<<<<< HEAD
void Init_Steppers(){
   InitTimer8(&delay);
}

=======
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
/* delay must remain in this position for local scope association 
 * Timer8 provides a master freq, feedate is supplied from gcode
 * drag is acc constant and oil provides a form of s curve.
 */
<<<<<<< HEAD
void delay(){
  LED2 = !LED2;
}


void _delay_(){
=======
static void delay(){
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
static long ii;
static int i = 0;
static int last_drag;
 acc_val = abs((feedrate + drag) / 10);
 ii = setUsec(0);
 while (getUsec() <= (long)acc_val)continue;
/* falls off exponentially */
 if(i < 10){
   //if(drag > 0 || i > 489) {
      /* drag increases the delay at the beginning */
      /* to allow for inertia in machine startup */
   drag = drag - (oil * oil);
   oil--;
 }else if(i > 40){
   ++oil;
   drag = drag + (oil * oil);
 }
   i++;
   if (drag < 0) drag = 0;
}

void setStepXY(int _x1,int _y1,int _x3,int _y3){
  step.xl = _x1;
  step.yl = _y1;
  step.x3 = _x3;
  step.y3 = _y3;
}

void setDragOil(int _feedrate,int _drag,int _oil){
<<<<<<< HEAD
=======
  Dly = delay;
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
  feedrate = MAXFEED - _feedrate;
  drag = _drag;
  oil = _oil;
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
                                \
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
  while ( (step.dx > step.x2) && (step.dy > step.y2)){ // at endpoint?
<<<<<<< HEAD
     _delay_();
=======
     delay();
>>>>>>> 5fccbb493b943575cfd5e09931f584d18a7d5345
     //if(!T8IE_bit){T8IE_bit = true;TMR8 =
     out = 0;
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
}