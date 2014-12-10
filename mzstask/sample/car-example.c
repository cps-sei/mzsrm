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
#include <unistd.h>

#define MAX_TIMESTAMPS 1000000

#define HIGH_CRITICALITY 10
#define MEDIUM_CRITICALITY 8
#define LOW_CRITICALITY 6

struct modal_task taskSuspension;
struct modal_task taskPedestrian;
struct modal_task taskCruise;

struct modal_system_t modal_system;

struct triple_timestamps_t{
  long vectsize;
  long bufidx1;
  long bufidx2;
  long bufidx3;
  unsigned long long timestamps_ns1[MAX_TIMESTAMPS];
  unsigned long long timestamps_ns2[MAX_TIMESTAMPS];
  unsigned long long timestamps_ns3[MAX_TIMESTAMPS];
}*triple_tsp;


void suspension_mode_one_job(){
  printf("suspension: mode one.\n");
  busy_timestamped(3000, &(triple_tsp->timestamps_ns1[0]), MAX_TIMESTAMPS, &(triple_tsp->bufidx1));
}

void suspension_mode_two_job(){
  printf("suspension: mode two.\n");
  busy_timestamped(7000, &(triple_tsp->timestamps_ns1[0]), MAX_TIMESTAMPS, &(triple_tsp->bufidx1));
}

void pedestrian_mode_one_job(){
  printf("pedestrian: mode one.\n");
  busy_timestamped(2000, &(triple_tsp->timestamps_ns2[0]), MAX_TIMESTAMPS, &(triple_tsp->bufidx2)); 
}

void pedestrian_mode_two_job(){
  printf("pedestrian: mode two.\n");
  busy_timestamped(2000, &(triple_tsp->timestamps_ns2[0]), MAX_TIMESTAMPS, &(triple_tsp->bufidx2));   
}


void cruise_mode_one_job(){
  printf("cruise: mode one.\n");
  busy_timestamped(1000, &(triple_tsp->timestamps_ns3[0]), MAX_TIMESTAMPS, &(triple_tsp->bufidx3));   
}

void cruise_mode_two_job(){
  printf("cruise: mode two.\n");
  busy_timestamped(1000, &(triple_tsp->timestamps_ns3[0]), MAX_TIMESTAMPS, &(triple_tsp->bufidx3));   
}

void suspension_init(int argc, void *argv[]){
  printf("suspension init\n");
}

void pedestrian_init(int argc, void *argv[]){
  printf("pedestrian init\n");
}

void cruise_init(int argc, void *argv[]){
  printf("cruise init\n");
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

  if ((shmid = shmget(IPC_PRIVATE, sizeof(struct triple_timestamps_t), IPC_CREAT | 0777))<0){
    printf("Error creating shared mem segment");
    return -1;
  }

  if ((triple_tsp = (struct triple_timestamps_t*) shmat(shmid, NULL,0)) == NULL){
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

  if ((sched = zs_open_sched()) == -1){
    printf("error opening scheduler\n");
    return -1;
  }

  modal_system.num_sys_modes = 1;
  modal_system.sys_modes = (struct system_mode_t*) malloc(sizeof(struct system_mode_t));

  taskSuspension.init = suspension_init;
  taskSuspension.num_modes =2;
  taskSuspension.current_mode =0;
  taskSuspension.sync_start_semid = sync_start_semid;
  taskSuspension.modes = (struct mode *) malloc(sizeof(struct mode)*2);
  taskSuspension.modes[0].mode_job_function=suspension_mode_one_job;
  //taskSuspension.modes[0].mode_job_function=suspension_mode_two_job; // testing
  taskSuspension.modes[0].suspend = 0;
  taskSuspension.modes[1].mode_job_function=suspension_mode_two_job;
  //taskSuspension.modes[1].mode_job_function=suspension_mode_one_job;
  taskSuspension.modes[1].suspend = 0;
  taskSuspension.in_transition = 0;
  CPU_ZERO(&(taskSuspension.affinity_mask));
  CPU_SET(0,&(taskSuspension.affinity_mask));

  taskPedestrian.init = pedestrian_init;
  taskPedestrian.num_modes=2;
  taskPedestrian.current_mode=0;
  taskPedestrian.sync_start_semid = sync_start_semid;
  taskPedestrian.modes = (struct mode *) malloc(sizeof (struct mode)*2);
  taskPedestrian.modes[0].mode_job_function = pedestrian_mode_one_job;
  taskPedestrian.modes[0].suspend = 0;
  taskPedestrian.modes[1].mode_job_function = pedestrian_mode_two_job;
  taskPedestrian.modes[1].suspend = 0;
  taskPedestrian.in_transition=0;
  CPU_ZERO(&(taskPedestrian.affinity_mask));
  CPU_SET(0,&(taskPedestrian.affinity_mask));

  taskCruise.init = cruise_init;
  taskCruise.num_modes=2;
  taskCruise.current_mode=0;
  taskCruise.sync_start_semid = sync_start_semid;
  taskCruise.modes = (struct mode *) malloc(sizeof (struct mode)*2);
  taskCruise.modes[0].mode_job_function = cruise_mode_one_job;
  taskCruise.modes[0].suspend = 0;
  taskCruise.modes[1].mode_job_function = cruise_mode_two_job;
  taskCruise.modes[1].suspend = 0;
  taskCruise.in_transition=0;
  CPU_ZERO(&(taskCruise.affinity_mask));
  CPU_SET(0,&(taskCruise.affinity_mask));

  struct zs_reserve_params cpuattrSuspension;
  struct zs_reserve_params cpuattrPedestrian;
  struct zs_reserve_params cpuattrCruise;

  cpuattrSuspension.period.tv_sec = 16;
  cpuattrSuspension.period.tv_nsec=0;
  cpuattrSuspension.reserve_type = CRITICALITY_RESERVE;
  cpuattrSuspension.criticality =7; 
  cpuattrSuspension.priority = 5;
  cpuattrSuspension.zs_instant.tv_sec = 32; // disable for now
  cpuattrSuspension.zs_instant.tv_nsec=0;
  cpuattrSuspension.response_time_instant.tv_sec =16; // try enforcement
  cpuattrSuspension.response_time_instant.tv_nsec=0;
  cpuattrSuspension.enforcement_mask = 0;

  cpuattrPedestrian.period.tv_sec = 4;
  cpuattrPedestrian.period.tv_nsec=0;
  cpuattrPedestrian.reserve_type  = CRITICALITY_RESERVE;
  cpuattrPedestrian.criticality =8; 
  cpuattrPedestrian.priority = 10;
  cpuattrPedestrian.zs_instant.tv_sec = 8; // disable for now
  cpuattrPedestrian.zs_instant.tv_nsec=0;
  cpuattrPedestrian.response_time_instant.tv_sec =4; // try enforcement
  cpuattrPedestrian.response_time_instant.tv_nsec=0;
  cpuattrPedestrian.enforcement_mask = 0;

  cpuattrCruise.period.tv_sec = 8;
  cpuattrCruise.period.tv_nsec=0;
  cpuattrCruise.reserve_type = CRITICALITY_RESERVE;
  cpuattrCruise.criticality =8; // set marginal_utlility instead
  cpuattrCruise.priority = 8;
  cpuattrCruise.zs_instant.tv_sec = 16; // disable for now
  cpuattrCruise.zs_instant.tv_nsec=0;
  cpuattrCruise.response_time_instant.tv_sec =8; // try enforcement
  cpuattrCruise.response_time_instant.tv_nsec=0;
  cpuattrCruise.critical_util_degraded_mode=-1;
  cpuattrCruise.enforcement_mask = 0;

  cpuattrSuspension.criticality = LOW_CRITICALITY;
  int ridSuspensionStreetMode = zs_create_reserve(sched, &cpuattrSuspension);

  cpuattrSuspension.criticality = HIGH_CRITICALITY;
  cpuattrSuspension.zs_instant.tv_sec = 13;
  cpuattrSuspension.zs_instant.tv_nsec =0;
  int ridSuspensionHighwayMode = zs_create_reserve(sched, &cpuattrSuspension);

  cpuattrPedestrian.criticality = HIGH_CRITICALITY;
  int ridPedestrianStreetMode = zs_create_reserve(sched, &cpuattrPedestrian);

  cpuattrPedestrian.criticality = LOW_CRITICALITY;
  int ridPedestrianHighwayMode = zs_create_reserve(sched, &cpuattrPedestrian);

  cpuattrCruise.criticality = MEDIUM_CRITICALITY;
  int ridCruiseHighwayMode = zs_create_reserve(sched, &cpuattrCruise);

  int mridSuspension = zs_create_modal_reserve(sched,2);
  zs_add_reserve_to_mode(sched,mridSuspension,0,ridSuspensionStreetMode);
  zs_add_reserve_to_mode(sched,mridSuspension,1,ridSuspensionHighwayMode);
  //zs_add_reserve_to_mode(sched,mridSuspension,1,ridSuspensionStreetMode);
  //zs_add_reserve_to_mode(sched,mridSuspension,0,ridSuspensionHighwayMode);

  int mridPedestrian = zs_create_modal_reserve(sched,2);
  zs_add_reserve_to_mode(sched,mridPedestrian,0,ridPedestrianStreetMode);
  zs_add_reserve_to_mode(sched,mridPedestrian,1,ridPedestrianHighwayMode);
  //zs_add_reserve_to_mode(sched,mridPedestrian,1,ridPedestrianStreetMode);
  //zs_add_reserve_to_mode(sched,mridPedestrian,0,ridPedestrianHighwayMode);

  int mridCruise = zs_create_modal_reserve(sched,2);
  zs_add_reserve_to_mode(sched,mridCruise,0,ridCruiseHighwayMode);
  zs_add_reserve_to_mode(sched,mridCruise,1,ridCruiseHighwayMode); // same reserve
  //zs_add_reserve_to_mode(sched,mridCruise,1,ridCruiseHighwayMode);
  //zs_add_reserve_to_mode(sched,mridCruise,0,ridCruiseHighwayMode); // same reserve

  struct zs_mode_transition transitionSuspension;
  transitionSuspension.from_mode=0;
  transitionSuspension.to_mode = 1;
  transitionSuspension.zs_instant.tv_sec = 13;
  transitionSuspension.zs_instant.tv_nsec = 0;

  zs_add_mode_transition(sched, mridSuspension, 0, &transitionSuspension);

  struct zs_mode_transition transitionPedestrian;
  transitionPedestrian.from_mode=0;
  transitionPedestrian.to_mode = 1;
  transitionPedestrian.zs_instant.tv_sec = 4;
  transitionPedestrian.zs_instant.tv_nsec = 0;

  zs_add_mode_transition(sched, mridPedestrian, 0, &transitionPedestrian);

  struct zs_mode_transition transitionCruise;
  transitionCruise.from_mode=0;
  transitionCruise.to_mode = 1;
  transitionCruise.zs_instant.tv_sec = 8;
  transitionCruise.zs_instant.tv_nsec = 0;

  zs_add_mode_transition(sched, mridCruise, 0, &transitionCruise);

  int sys_transition = zs_create_sys_transition(sched);

  zs_add_transition_to_sys_transition(sched, 
				      sys_transition, 
				      mridSuspension,
				      0);

  zs_add_transition_to_sys_transition(sched, 
				      sys_transition, 
				      mridPedestrian,
				      0);

  zs_add_transition_to_sys_transition(sched, 
				      sys_transition, 
				      mridCruise,
				      0);

  struct modal_system_t modal_system;
  modal_system.num_sys_modes = 2;
  modal_system.sys_modes = (struct system_mode_t*) malloc(sizeof(struct system_mode_t)*2);
  modal_system.sys_transitions = (struct system_transition_t *) malloc(sizeof(struct system_transition_t));
  modal_system.num_sys_transitions=1;
  modal_system.sys_transitions[0].from_mode=0;
  modal_system.sys_transitions[0].to_mode=1;
  modal_system.sys_transitions[0].transition_id =0;
 
  struct sched_param p;

  p.sched_priority = 30;

  if (sched_setscheduler(getpid(), SCHED_FIFO, &p)<0){
    printf("error setting priority\n");
  }

  // set start timestamp

  taskSuspension.start_timestamp_ns = now_ns();
  taskPedestrian.start_timestamp_ns = taskSuspension.start_timestamp_ns;
  taskCruise.start_timestamp_ns = taskSuspension.start_timestamp_ns;

  printf("starting taskSuspension & taskPedestrian\n");
  taskSuspension.mrid = mridSuspension;
  start_modal_task(sched, &taskSuspension, argc, (void **)argv);

  taskPedestrian.mrid = mridPedestrian;
  start_modal_task(sched, &taskPedestrian, argc, (void **)argv);

  taskCruise.mrid = mridCruise;
  start_modal_task(sched, &taskCruise, argc, (void **)argv);

  modal_system.sys_modes[0].num_active_task_modes=3;
  modal_system.sys_modes[0].active_task_modes = (struct task_mode_t*) malloc(3*sizeof(struct task_mode_t));
  modal_system.sys_modes[0].active_task_modes[0].pid = taskSuspension.pid;
  modal_system.sys_modes[0].active_task_modes[0].mode = 0;
  modal_system.sys_modes[0].active_task_modes[1].pid = taskPedestrian.pid;
  modal_system.sys_modes[0].active_task_modes[1].mode = 0;
  modal_system.sys_modes[0].active_task_modes[2].pid = taskCruise.pid;
  modal_system.sys_modes[0].active_task_modes[2].mode = 0;

  modal_system.sys_modes[1].num_active_task_modes=3;
  modal_system.sys_modes[1].active_task_modes = (struct task_mode_t*) malloc(3*sizeof(struct task_mode_t));
  modal_system.sys_modes[1].active_task_modes[0].pid = taskSuspension.pid;
  modal_system.sys_modes[1].active_task_modes[0].mode = 0;
  modal_system.sys_modes[1].active_task_modes[1].pid = taskPedestrian.pid;
  modal_system.sys_modes[1].active_task_modes[1].mode = 0;
  modal_system.sys_modes[1].active_task_modes[2].pid = taskCruise.pid;
  modal_system.sys_modes[1].active_task_modes[2].mode = 0;

  

  // up semaphore to sync start tasks

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = 3; // three ups one for each task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  printf("all tasks started\n");

  // wait until 1 sec before largest period expires to do mode change
  // and make the effective mode change happen at 16
  usleep(15000000);

  printf("taking transition\n");
  sys_mode_change(sched,&modal_system,sys_transition);

  usleep(20000000);

  request_wait_exit(&taskSuspension);
  request_wait_exit(&taskPedestrian);
  request_wait_exit(&taskCruise);

  zs_delete_modal_reserve(sched, mridSuspension);
  zs_delete_modal_reserve(sched, mridPedestrian);
  zs_delete_modal_reserve(sched, mridCruise);

  zs_close_sched(sched);
  
  cleanup(sync_start_semid,shmid);

  // dump timestamps

  FILE* fid1 = fopen("ts-taskSuspension.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-taskSuspension.txt\n");
    return -1;
  }

  FILE* fid2 = fopen("ts-taskPedestrian.txt","w+");
  if (fid2 ==NULL){
    printf("error opening ts-taskPedestrian.txt\n");
    return -1;
  }

  FILE* fid3 = fopen("ts-taskCruise.txt","w+");
  if (fid2 ==NULL){
    printf("error opening ts-taskCruise.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < triple_tsp->bufidx1 ; idx++){
    fprintf(fid1,"%llu 1\n",(triple_tsp->timestamps_ns1[idx]-taskSuspension.start_timestamp_ns));
  }

  for (idx = 0 ; idx < triple_tsp->bufidx2 ; idx++){
    fprintf(fid2,"%llu 2\n",(triple_tsp->timestamps_ns2[idx] - taskSuspension.start_timestamp_ns));
  }

  for (idx = 0 ; idx < triple_tsp->bufidx3 ; idx++){
    fprintf(fid3,"%llu 3\n",(triple_tsp->timestamps_ns3[idx] - taskSuspension.start_timestamp_ns));
  }

  fclose(fid1);
  fclose(fid2);
  fclose(fid3);
}
