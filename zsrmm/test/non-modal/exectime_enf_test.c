
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
#include <locale.h>
#include <time.h>
#include "../../zsrm.h"
#include "../busy.h"

#define MAX_TIMESTAMPS 1000000

long bufidx1;
unsigned long long timestamps_ns1[MAX_TIMESTAMPS];

int main(){
  int first_time=1;
  int rid,mid;
  int sched;
  int i,j,k;
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long start_ns;
  FILE *fid;
  long l;

  setlocale(LC_NUMERIC,"");

  cpuattr.period.tv_sec = 1;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.criticality = 1;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.priority = 10;
  cpuattr.zs_instant.tv_sec=1;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 100000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 100000000;
  cpuattr.response_time_instant.tv_sec = 1;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.critical_util_degraded_mode=-1;
  cpuattr.normal_marginal_utility=7;
  cpuattr.overloaded_marginal_utility=7;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu=0;
  cpuattr.enforcement_mask = ZS_ENFORCE_OVERLOAD_BUDGET_MASK ;//| ZS_ENFORCEMENT_HARD_MASK;
  
  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }

  rid = zs_create_reserve(sched,&cpuattr);
  zs_attach_reserve(sched,rid,getpid());
  
  start_ns = now_ns();
  for (i=0;i<10;i++){
    if (first_time){
      first_time=0;
      busy_timestamped(500,timestamps_ns1,MAX_TIMESTAMPS,&bufidx1);
    } else {
      busy_timestamped(50,timestamps_ns1,MAX_TIMESTAMPS,&bufidx1);
    }
    zs_wait_next_period(sched,rid);
  }

  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  fid = fopen("ts-enforcement-test.txt","w+");
  if (fid ==NULL){
    printf("error opening file\n");
    return -1;
  }

  for (l=0;l<bufidx1;l++){
    fprintf(fid,"%llu 1\n",timestamps_ns1[l]-start_ns);
  }

  fclose(fid);
}
