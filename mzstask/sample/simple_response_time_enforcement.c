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

#define MAX_TIMESTAMPS 1000000

struct modal_task task1;
struct modal_task task2;

struct modal_system_t modal_system;

struct dual_timestamps_t{
  long vectsize;
  long bufidx1;
  long bufidx2;
  unsigned long long timestamps_ns1[MAX_TIMESTAMPS];
  unsigned long long timestamps_ns2[MAX_TIMESTAMPS];
}*dual_tsp;


void task1_mode_one_job(){
  printf("task1: mode one start.... \n");
  //busy(1500);
  busy_timestamped(1500, &(dual_tsp->timestamps_ns1[0]), MAX_TIMESTAMPS, &(dual_tsp->bufidx1));
  printf("task 1 done\n");
}

void task2_mode_one_job(){
  printf("task2: mode one. \n");
  busy_timestamped(1100, &(dual_tsp->timestamps_ns2[0]), MAX_TIMESTAMPS, &(dual_tsp->bufidx2));
  //busy(1100);
}

void task1_init(int argc, void *argv[]){
  printf("task 1 init\n");
}

void task2_init(int argc, void *argv[]){
  printf("task 2 init\n");
}

int cleanup(int semid, int shmid){
  if (semid != -1){
    if (semctl(semid, 0, IPC_RMID)<0){
      printf("error removing semaphore\n");
    }
  }

  if (shmid != -1){
    if (shmctl(shmid, IPC_RMID, NULL)<0){
      printf("error removing shmem\n");
    }
  }
}

int main(int argc, char *argv[]){
  long idx;
  int sched;
  int sync_start_semid;
  int shmid;

  if ((shmid = shmget(IPC_PRIVATE, sizeof(struct dual_timestamps_t), IPC_CREAT | 0777))<0){
    printf("Error creating shared mem segment");
    return -1;
  }

  if ((dual_tsp = (struct dual_timestamps_t*) shmat(shmid, NULL,0)) == NULL){
    printf("error attaching to shared mem segment");
    cleanup(-1,shmid);
    return -1;
  }

  // create sync start semaphore
  if ((sync_start_semid = semget(IPC_PRIVATE, 1, IPC_CREAT|0777)) <0){
    perror("creating the semaphone\n");
    cleanup(-1,shmid);
    return -1;
  }

  struct zs_reserve_params cpuattr1;
  struct zs_reserve_params cpuattr2;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening scheduler\n");
    return -1;
  }

  modal_system.num_sys_modes = 1;
  modal_system.sys_modes = (struct system_mode_t*) malloc(sizeof(struct system_mode_t));

  task1.init = task1_init;
  task1.num_modes =1;
  task1.current_mode =0;
  task1.sync_start_semid = sync_start_semid;
  task1.modes = (struct mode *) malloc(sizeof(struct mode));
  task1.modes[0].mode_job_function=task1_mode_one_job;
  task1.modes[0].period_ms = 1000;
  task1.modes[0].suspend = 0;
  task1.in_transition = 0;
  CPU_ZERO(&(task1.affinity_mask));
  CPU_SET(0,&(task1.affinity_mask));

  task2.init = task2_init;
  task2.num_modes=1;
  task2.current_mode=0;
  task2.sync_start_semid = sync_start_semid;
  task2.modes = (struct mode *) malloc(sizeof (struct mode));
  task2.modes[0].mode_job_function = task2_mode_one_job;
  task2.modes[0].period_ms = 1000;
  task2.modes[0].suspend = 0;
  task2.in_transition=0;
  CPU_ZERO(&(task2.affinity_mask));
  CPU_SET(0,&(task2.affinity_mask));

  cpuattr1.period.tv_sec = 2;
  cpuattr1.period.tv_nsec=0;
  cpuattr1.criticality =7;
  cpuattr1.reserve_type = CRITICALITY_RESERVE;
  cpuattr1.priority = 10;
  cpuattr1.zs_instant.tv_sec = 3; // disable for now
  cpuattr1.zs_instant.tv_nsec=0;
  cpuattr1.response_time_instant.tv_sec =5; // try enforcement
  cpuattr1.response_time_instant.tv_nsec=0;
  cpuattr1.enforcement_mask = 0;//ZS_RESPONSE_TIME_ENFORCEMENT_MASK;

  cpuattr2.period.tv_sec = 4;
  cpuattr2.period.tv_nsec=0;
  cpuattr2.criticality =8;
  cpuattr2.reserve_type = CRITICALITY_RESERVE;
  cpuattr2.priority = 9;
  cpuattr2.zs_instant.tv_sec = 1; // disable for now
  cpuattr2.zs_instant.tv_nsec=0;
  cpuattr2.response_time_instant.tv_sec =5; // try enforcement
  cpuattr2.response_time_instant.tv_nsec=0;
  cpuattr2.enforcement_mask = 0;

  int rid1 = zs_create_reserve(sched, &cpuattr1);
  int rid2 = zs_create_reserve(sched, &cpuattr2);

  int mrid1 = zs_create_modal_reserve(sched,1);

  zs_add_reserve_to_mode(sched,mrid1,0,rid1);

  int mrid2 = zs_create_modal_reserve(sched,1);

  zs_add_reserve_to_mode(sched,mrid2,0,rid2);

  struct sched_param p;

  p.sched_priority = 30;

  if (sched_setscheduler(getpid(), SCHED_FIFO, &p)<0){
    printf("error setting priority\n");
  }

  // set start timestamp

  task1.start_timestamp_ns = now_ns();
  task2.start_timestamp_ns = task1.start_timestamp_ns;

  printf("starting task1 & task2\n");
  task1.mrid = mrid1;
  start_modal_task(sched, &task1, argc, (void **)argv);

  task2.mrid = mrid2;
  start_modal_task(sched, &task2, argc, (void **)argv);

  // up semaphore to sync start tasks

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = 2; // two ups one for each task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  printf("all tasks started\n");

  // wait for 20 secs
  sleep(20);

  request_wait_exit(&task1);
  request_wait_exit(&task2);

  zs_delete_modal_reserve(sched, mrid1);
  zs_delete_modal_reserve(sched, mrid2);

  zs_close_sched(sched);
  
  cleanup(sync_start_semid,shmid);

  // dump timestamps

  FILE* fid1 = fopen("ts-task1.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-task1.txt\n");
    return -1;
  }

  FILE* fid2 = fopen("ts-task2.txt","w+");
  if (fid2 ==NULL){
    printf("error opening ts-task2.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < dual_tsp->bufidx1 ; idx++){
    fprintf(fid1,"%llu 1\n",(dual_tsp->timestamps_ns1[idx]-task1.start_timestamp_ns));
  }

  for (idx = 0 ; idx < dual_tsp->bufidx2 ; idx++){
    fprintf(fid2,"%llu 2\n",(dual_tsp->timestamps_ns2[idx] - task1.start_timestamp_ns));
  }

  fclose(fid1);
  fclose(fid2);

}
