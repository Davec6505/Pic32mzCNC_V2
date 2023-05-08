#include "Nuts_Bolts.h"


#define MAX_INT_DIGITS 8 // Maximum number of digits in int32 (and float)


// Convert a floating point to a unsigned long for flash write
unsigned long flt2ulong(float f_){
unsigned long ul_ = 0;
  memcpy(&ul_,&f_,sizeof(float));
  return ul_;
}

//Convert a unsigned long back to a floating point from flash memory
float ulong2flt(unsigned long ul_){
float f_ = 0.0;
 memcpy(&f_,&ul_,sizeof(unsigned long ));
 
return f_;
}

//returns the specific decimal value (rounded value) of the given float
float fround(float val){
float value = (long)(val * 100.00 + 0.5);
  return (float)(value / 100.00);
}

//return the int val rounfed off to the nearest int
int round(float val){
float temp = 0.00,tempC = 0.00,tempF = 0.00,dec = 0.00;
  tempC = ceil(val);
  tempF = floor(val);
  dec = val - tempF;
  temp = (dec > 0.5)? tempC : tempF;
  return (int)temp;
}

//return the int val rounfed off to the nearest int
long lround(float val){
float temp = 0.00,tempC = 0.00,tempF = 0.00,dec = 0.00;
  tempC = ceil(val);
  tempF = floor(val);
  dec = val - tempF;
  temp = (dec > 0.5)? tempC : tempF;
  return (long)temp;
}