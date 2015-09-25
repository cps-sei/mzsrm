
/*
Copyright (c) 2014 Carnegie Mellon University.

All Rights Reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the 
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following 
acknowledgments and disclaimers.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
acknowledgments and disclaimers in the documentation and/or other materials provided with the distribution.
3. Products derived from this software may not include “Carnegie Mellon University,” "SEI” and/or “Software 
Engineering Institute" in the name of such derived product, nor shall “Carnegie Mellon University,” "SEI” and/or 
“Software Engineering Institute" be used to endorse or promote products derived from this software without prior 
written permission. For written permission, please contact permission@sei.cmu.edu.

ACKNOWLEDMENTS AND DISCLAIMERS:
Copyright 2014 Carnegie Mellon University
This material is based upon work funded and supported by the Department of Defense under Contract No. FA8721-
05-C-0003 with Carnegie Mellon University for the operation of the Software Engineering Institute, a federally 
funded research and development center.

Any opinions, findings and conclusions or recommendations expressed in this material are those of the author(s) and 
do not necessarily reflect the views of the United States Department of Defense.

NO WARRANTY. 
THIS CARNEGIE MELLON UNIVERSITY AND SOFTWARE ENGINEERING INSTITUTE 
MATERIAL IS FURNISHED ON AN “AS-IS” BASIS. CARNEGIE MELLON UNIVERSITY MAKES NO 
WARRANTIES OF ANY KIND, EITHER EXPRESSED OR IMPLIED, AS TO ANY MATTER INCLUDING, 
BUT NOT LIMITED TO, WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, 
EXCLUSIVITY, OR RESULTS OBTAINED FROM USE OF THE MATERIAL. CARNEGIE MELLON 
UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT TO FREEDOM FROM 
PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.

This material has been approved for public release and unlimited distribution.
Carnegie Mellon® is registered in the U.S. Patent and Trademark Office by Carnegie Mellon University.

DM-0000891
*/

//#include <modaltask.h>


#ifdef __ARMCORE__

static inline void start_tsc(void){
	unsigned long tmp;

	tmp = PMCNTNSET_C_BIT;
	asm volatile ("mcr p15, 0, %0, c9, c12, 1" : : "r" (tmp));


	asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (tmp));
	tmp |= PMCR_C_BIT | PMCR_E_BIT;
	asm volatile ("mcr p15, 0, %0, c9, c12, 0" : : "r" (tmp));
}

static inline unsigned long long rdtsc(void){
	unsigned long result;
	asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (result));
	return (unsigned long long) result;
}

#else

static inline unsigned long long rdtsc(){
  unsigned int hi, lo;
  __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi) : :);
  return ((unsigned long long) hi << 32) | lo;
}

#endif



#define NUM_SILENT_ITERATIONS 10000


unsigned long long now_ns(){
  struct timespec n;
  clock_gettime(CLOCK_MONOTONIC,&n);

  unsigned long long retnow = (((unsigned long long) n.tv_sec) *1000L) + (unsigned long long) (n.tv_nsec/1000000);
return retnow;
}


// at frequency: 1500000  old:247500
//#define IN_LOOP_ONE_MS 213788
// at frequency 3.5 GHz
#define IN_LOOP_ONE_MS 487130
#define NUM_IN_LOOPS_ONE_MS 1

void  busy(long millis){
  long i,j,k;
  
  for (k=0;k<millis;k++)
    for (i=0;i<NUM_IN_LOOPS_ONE_MS;i++)
      for (j=0;j<IN_LOOP_ONE_MS;j++)
	;
}


unsigned long nanosPerTenTicks;

void calibrate_ticks(){
  unsigned long long begin=0,end=0;
#ifdef __ARMCORE__
  unsigned long elapsed10s;
#endif
  unsigned long waituntil;
  struct timespec begints, endts;
  unsigned long long diffts;
  
  clock_gettime(CLOCK_MONOTONIC,&begints);
  begin = rdtsc();
  
  // busy wait for one second
  busy(1000);
  
  end = rdtsc();
  clock_gettime(CLOCK_MONOTONIC,&endts);
  printf("zsrm.init: begin[%llu] end[%llu] begints[%lu] endts[%lu]\n",begin, end, begints.tv_nsec, endts.tv_nsec);
  diffts = ((endts.tv_sec * 1000000000)+ endts.tv_nsec) -
    ((begints.tv_sec * 1000000000) + begints.tv_nsec);
  end = end-begin;
#ifdef __ARMCORE__
  do_div(end,10);
  elapsed10s = (unsigned long) end;
  do_div(diffts,elapsed10s);
  nanosPerTenTicks = diffts;
#else
  end /=10;
  nanosPerTenTicks = diffts / end;
#endif
}
  
  unsigned long long ticks2nanos(unsigned long long ticks){
  unsigned long long tmp =(unsigned long long) (ticks * nanosPerTenTicks);
#ifdef __ARMCORE__
  do_div(tmp,10);
#else
  tmp = tmp / 10;
#endif
  return tmp;
}
  

 void busy_timestamped(long millis, unsigned long long tsbuffer[], long bufsize, long *bufidx){
  
  long i,j,k,s;
  
  s=0;
  for (k=0;k<millis;k++)
    for (i=0;i<NUM_IN_LOOPS_ONE_MS;i++)
      for (j=0;j<IN_LOOP_ONE_MS;j++){
        s++;
	if (s >= NUM_SILENT_ITERATIONS){
         s=0;
	 if (*bufidx < bufsize){
           tsbuffer[(*bufidx)++] = now_ns(); //ticks2nanos(rdtsc()); 
         }
}
}
}
