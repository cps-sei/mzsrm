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


#include <stdio.h>
#define __USE_GNU
#include <sched.h>
#include <time.h>
#include <errno.h>

int main(){
  struct timespec start,end;
  unsigned long long diff_ns = 1000000000L;
  unsigned long long elapsed_ns = 0L;
  unsigned long long top=1000000L;
  double loops_per_nanos=0.0f;
  unsigned long long cnt;
  cpu_set_t cpuset;
  struct sched_param prio;
  
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);

  if (sched_setaffinity(getpid(),sizeof(cpuset),&cpuset)<0){
    perror("Error setting affinity mask. Aborting");
    return 1;
  }

  prio.sched_priority = 10;

  if (sched_setscheduler(getpid(), SCHED_FIFO,&prio)<0){
    perror("Error setting priority. Aborting");
    return 2;
  }


  while (diff_ns > 100){
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (cnt=0; cnt < top;cnt++){
    }
    clock_gettime(CLOCK_MONOTONIC,&end);
    elapsed_ns = (end.tv_sec * 1000000000L + end.tv_nsec);
    elapsed_ns = elapsed_ns - (start.tv_sec * 1000000000L + start.tv_nsec);
    diff_ns = 1000000 - elapsed_ns;

    loops_per_nanos = (double)top / (double)elapsed_ns;    
    printf("elapsed(%llu) ns, diff(%llu) ns top(%llu) loops per nanos (%f)\n",elapsed_ns,diff_ns,top, loops_per_nanos);
    top = 1000000 * loops_per_nanos;
  }

  printf("loops per ms: %llu\n",top);
}
