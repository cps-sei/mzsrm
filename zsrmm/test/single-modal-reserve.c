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
#include <time.h>
#include "../zsrm.h"

int job_mode1(){
  printf("mode 1\n");
}

int job_mode2(){
  printf("mode 2\n");
}

int main(){
  int rid1,rid2,mid;
  int sched;
  int i,j,k;
  struct zs_reserve_params cpuattr1;
  struct zs_reserve_params cpuattr2;
  struct timespec now;
  unsigned long long now_ns;
  unsigned long long start_ns;

  cpuattr1.period.tv_sec = 4;
  cpuattr1.period.tv_nsec=0;
  cpuattr1.criticality = 0;
  cpuattr1.priority = 10;
  cpuattr1.zs_instant.tv_sec=5;
  cpuattr1.zs_instant.tv_nsec=0;
  cpuattr1.response_time_instant.tv_sec = 4;
  cpuattr1.response_time_instant.tv_nsec =0;
  cpuattr1.critical_util_degraded_mode=-1;
  cpuattr1.normal_marginal_utility=7;
  cpuattr1.overloaded_marginal_utility=7;
  cpuattr1.num_degraded_modes=0;
  cpuattr1.enforcement_mask=0;

  cpuattr2.period.tv_sec = 8;
  cpuattr2.period.tv_nsec=0;
  cpuattr2.criticality = 0;
  cpuattr2.priority = 10;
  cpuattr2.zs_instant.tv_sec=9;
  cpuattr2.zs_instant.tv_nsec=0;
  cpuattr2.response_time_instant.tv_sec = 8;
  cpuattr2.response_time_instant.tv_nsec =0;
  cpuattr2.critical_util_degraded_mode=-1;
  cpuattr2.normal_marginal_utility=7;
  cpuattr2.overloaded_marginal_utility=7;
  cpuattr2.num_degraded_modes=0;
  cpuattr2.enforcement_mask=0;
  
  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }

  rid1 = zs_create_reserve(sched,&cpuattr1);
  rid2 = zs_create_reserve(sched,&cpuattr2);
  mid = zs_create_modal_reserve(sched,2);
  zs_add_reserve_to_mode(sched,mid,0,rid1);
  zs_add_reserve_to_mode(sched,mid,1,rid2);
  zs_attach_modal_reserve(sched,mid,getpid());

}
