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
#include <stdlib.h>
#include <time.h>
#define __USE_GNU
#include <sched.h>
#include "modaltask.h"
#include "modaltrigger.h"
#include <zsrm.h>

static unsigned long prevtime=0;

unsigned long long timestamp_ns(){
  struct timespec now_ts;
  clock_gettime(CLOCK_MONOTONIC, &now_ts);
  return ((unsigned long long)now_ts.tv_nsec) + ((unsigned long long)now_ts.tv_sec) * 1000000000ll;
}

static unsigned long long prev_ns=0;

unsigned long long elapsed(){
  unsigned long long now_ns= timestamp_ns();
  unsigned long long elapsed_ns;
  if (prev_ns == 0)
    elapsed_ns =0;
  else
    elapsed_ns = now_ns - prev_ns;
  prev_ns = now_ns;
  return elapsed_ns;
}

void task1_mode_one_job(){
  unsigned long long elapsed_ns = elapsed();
  printf("task1: mode one. elapsed(%llu) ns from previous arrival \n",elapsed_ns);
}

void task1_mode_two_job(){
  unsigned long long elapsed_ns = elapsed();
  printf("task1: mode two. elapsed(%llu) ns from previous arrival \n",elapsed_ns);
}

void task1_init(int argc, void * argv[]){
  printf("task 1 init\n");
}

struct modal_task task1;

struct modal_system_t modal_system;

int main(int argc, char * argv){
  int sched;
  struct zs_reserve_params cpuattr1;
  struct zs_reserve_params cpuattr2;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }

  modal_system.num_sys_modes = 2;
  modal_system.sys_modes= (struct system_mode_t*) malloc(sizeof(struct system_mode_t)*2);

  task1.init = task1_init;
  task1.num_modes=2;
  task1.current_mode=DISABLED_MODE;
  task1.modes = (struct mode *) malloc(2*sizeof(struct mode));
  task1.modes[0].mode_job_function=task1_mode_one_job;
  task1.modes[0].period_ms=1000;
  task1.modes[0].suspend=0;
  task1.in_transition = 0;
  task1.modes[1].mode_job_function=task1_mode_two_job;
  task1.modes[1].period_ms=2000;
  task1.modes[1].suspend=1;
  CPU_ZERO(&(task1.affinity_mask));
  CPU_SET(0,&(task1.affinity_mask));

  // setup modal reserve mode

  cpuattr1.period.tv_sec = 0;//2;
  cpuattr1.period.tv_nsec=200000000;//0;
  cpuattr1.criticality = 7;
  cpuattr1.reserve_type = CRITICALITY_RESERVE;
  cpuattr1.priority = 10;
  cpuattr1.zs_instant.tv_sec=5;
  cpuattr1.zs_instant.tv_nsec=0;
  cpuattr1.response_time_instant.tv_sec = 4;
  cpuattr1.response_time_instant.tv_nsec =0;
  cpuattr1.enforcement_mask=0;
    
  cpuattr2.period.tv_sec = 0;//4;
  cpuattr2.period.tv_nsec=400000000;//0;
  cpuattr2.criticality = 7;
  cpuattr2.reserve_type = CRITICALITY_RESERVE;
  cpuattr2.priority = 9;
  cpuattr2.zs_instant.tv_sec=9;
  cpuattr2.zs_instant.tv_nsec=0;
  cpuattr2.response_time_instant.tv_sec = 8;
  cpuattr2.response_time_instant.tv_nsec =0;
  cpuattr2.enforcement_mask=0;

  int rid1 = zs_create_reserve(sched, &cpuattr1);
  int rid2 = zs_create_reserve(sched, &cpuattr2);

  int mrid = zs_create_modal_reserve(sched, 2);

  zs_add_reserve_to_mode(sched,mrid,0,rid1);
  zs_add_reserve_to_mode(sched,mrid,1,rid2);

  // setup modal reserve transitions
  struct zs_mode_transition transition1;
  transition1.from_mode=DISABLED_MODE;
  transition1.to_mode=1;
  transition1.zs_instant.tv_sec = 10;
  transition1.zs_instant.tv_nsec =0;

  zs_add_mode_transition(sched, mrid, 0, &transition1);

  struct zs_mode_transition transition2;
  transition2.from_mode=1;
  transition2.to_mode=DISABLED_MODE;
  transition2.zs_instant.tv_sec=10;
  transition2.zs_instant.tv_nsec=0;

  zs_add_mode_transition(sched, mrid, 1, &transition2);

  int sys_trans1 = zs_create_sys_transition(sched);
  int sys_trans2 = zs_create_sys_transition(sched);

  printf("sys_trans1 :%d \n",sys_trans1);
  printf("sys_trans2 :%d \n",sys_trans2);

  zs_add_transition_to_sys_transition(sched, sys_trans1, mrid, 0);
  zs_add_transition_to_sys_transition(sched, sys_trans2, mrid, 1);

  // setup sys mode transitions
  modal_system.sys_transitions = (struct system_transition_t *) malloc(2*sizeof(struct system_transition_t));
  modal_system.num_sys_transitions=2;
  modal_system.sys_transitions[0].from_mode=0;
  modal_system.sys_transitions[0].to_mode=1;
  modal_system.sys_transitions[0].transition_id = 0;

  modal_system.sys_transitions[1].from_mode=1;
  modal_system.sys_transitions[1].to_mode=0;
  modal_system.sys_transitions[1].transition_id= 1;

  task1.mrid = mrid;
  start_modal_task(sched, &task1,argc,(void **)argv);

  // setup mode 0
  modal_system.sys_modes[0].num_active_task_modes=0;
  modal_system.sys_modes[0].active_task_modes = (struct task_mode_t *) malloc(1*sizeof(struct task_mode_t));
  modal_system.sys_modes[0].active_task_modes[0].pid = task1.pid;
  modal_system.sys_modes[0].active_task_modes[0].mode=DISABLED_MODE;
  

  // setup mode 1
  modal_system.sys_modes[1].num_active_task_modes=1;
  modal_system.sys_modes[1].active_task_modes = (struct task_mode_t *) malloc(1*sizeof(struct task_mode_t));
  modal_system.sys_modes[1].active_task_modes[0].pid = task1.pid;
  modal_system.sys_modes[1].active_task_modes[0].mode=1;


  struct sched_param p;

  p.sched_priority = 30;

  if (sched_setscheduler(getpid(), SCHED_FIFO, &p)<0){
    printf("error setting priority\n");
  }
  
  // now do some mode triggers

  int transition=sys_trans1,j;
  for (j=0;j<1001;j++){
    sleep(2);
    printf("\nTaking transition %d\n",transition);
    sys_mode_change(sched, &modal_system,transition);
    transition = (transition == sys_trans1 ? sys_trans2 : sys_trans1);
  }

  request_wait_exit(&task1);

  zs_delete_modal_reserve(sched, mrid);
  zs_delete_sys_transition(sched, sys_trans1);
  zs_delete_sys_transition(sched, sys_trans2);
  zs_close_sched(sched);
}
