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


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "modaltask.h"
#include "modaltrigger.h"
#include <zsrm.h>
#include "busy.h"
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sched.h>

#define LOOPS_PER_MS 441840

struct modal_task task1;

struct modal_system_t modal_system;


void busy(long millis){
  unsigned long long cnt;
  unsigned long long top;

  top = millis * LOOPS_PER_MS;

  for (cnt =0;cnt<top;cnt++){
  }
}

void task1_mode_one_job(){
  // 40ms
  busy(40);
}

void task1_init(int argc, void *argv[]){
  printf("task 1 init\n");
}

int main(int argc, char *argv[]){
  long idx;
  int sched;

  struct zs_reserve_params cpuattr1;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening scheduler\n");
    return -1;
  }

  modal_system.num_sys_modes = 1;
  modal_system.sys_modes = (struct system_mode_t*) malloc(sizeof(struct system_mode_t));

  task1.init = task1_init;
  task1.num_modes =1;
  task1.current_mode =0;
  task1.modes = (struct mode *) malloc(sizeof(struct mode));
  task1.modes[0].mode_job_function=task1_mode_one_job;
  task1.modes[0].period_ms = 200;
  task1.modes[0].suspend = 0;
  task1.in_transition = 0;
  CPU_ZERO(&(task1.affinity_mask));
  CPU_SET(0,&(task1.affinity_mask));

  cpuattr1.reserve_type = CRITICALITY_RESERVE;
  cpuattr1.period.tv_sec = 0;
  cpuattr1.period.tv_nsec=200000000;
  cpuattr1.criticality =7; 
  cpuattr1.priority = 10;
  cpuattr1.zs_instant.tv_sec = 3; // disable for now
  cpuattr1.zs_instant.tv_nsec=0;
  cpuattr1.response_time_instant.tv_sec =0; // try enforcement
  cpuattr1.response_time_instant.tv_nsec=20000000;
  cpuattr1.enforcement_mask = ZS_RESPONSE_TIME_ENFORCEMENT_MASK;

  int rid1 = zs_create_reserve(sched, &cpuattr1);

  int mrid1 = zs_create_modal_reserve(sched,1);

  zs_add_reserve_to_mode(sched,mrid1,0,rid1);

  struct sched_param p;

  p.sched_priority = 30;

  if (sched_setscheduler(getpid(), SCHED_FIFO, &p)<0){
    printf("error setting priority\n");
  }

  // set start timestamp

  task1.start_timestamp_ns = now_ns();
  printf("starting task1\n");
  task1.mrid = mrid1;
  start_modal_task(sched, &task1, argc, (void **)argv);

  printf("all tasks started\n");

  // wait for 20 secs
  sleep(20);

  request_wait_exit(&task1);

  zs_delete_modal_reserve(sched, mrid1);

  zs_close_sched(sched);
}
