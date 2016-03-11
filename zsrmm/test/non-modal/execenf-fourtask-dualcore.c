
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
#include <locale.h>
#include <time.h>
#include "../../zsrm.h"
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/sem.h>
#include <sched.h>

#include "busy.h"

#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#else
#error "SYS_gettid unavailable in this system"
#endif


#define MAX_TIMESTAMPS 1000000

long bufidx1;
unsigned long long timestamps_ns1[MAX_TIMESTAMPS];
long bufidx2;
unsigned long long timestamps_ns2[MAX_TIMESTAMPS];
long bufidx3;
unsigned long long timestamps_ns3[MAX_TIMESTAMPS];
long bufidx4;
unsigned long long timestamps_ns4[MAX_TIMESTAMPS];
int sync_start_semid;

unsigned long long start_ns;

void *task1(void *argp){
  struct zsrm_debug_trace_record drec;
  int first_time=1;
  int rid,mid;
  int sched;
  int i,j,k;
  struct zs_reserve_params cpuattr;
  struct timespec now;
  FILE *fid;
  long l;

  setlocale(LC_NUMERIC,"");

  cpuattr.period.tv_sec = 1;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.criticality = 1;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.priority = 11;
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
  cpuattr.enforcement_mask = ZS_ENFORCE_OVERLOAD_BUDGET_MASK;// | ZS_ENFORCEMENT_HARD_MASK;
  
  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }


  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("task 1: error creating reserve\n");
  }

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  for (i=0;i<10;i++){
    if (first_time){
      first_time=0;
      busy_timestamped(500,timestamps_ns1,MAX_TIMESTAMPS,&bufidx1);
    } else {
      busy_timestamped(50,timestamps_ns1,MAX_TIMESTAMPS,&bufidx1);
    }
    zs_wait_next_period(sched,rid);
  }

  printf("Test finished...saving %ld task datapoins...\n",bufidx1);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);

  fid = fopen("ts-enf-task1.txt","w+");
  if (fid ==NULL){
    printf("error opening file\n");
    return NULL;
  }

  for (l=0;l<bufidx1;l++){
    fprintf(fid,"%llu %d\n",timestamps_ns1[l]-start_ns,rid);
  }

  fclose(fid);

  zs_close_sched(sched);
}

void *task2(void *argp){
  struct zsrm_debug_trace_record drec;
  int first_time=1;
  int rid,mid;
  int sched;
  int i,j,k;
  struct zs_reserve_params cpuattr;
  struct timespec now;
  FILE *fid;
  long l;

  setlocale(LC_NUMERIC,"");

  cpuattr.period.tv_sec = 2;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.criticality = 2;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.priority = 10;
  cpuattr.zs_instant.tv_sec=2;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 200000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 200000000;
  cpuattr.response_time_instant.tv_sec = 1;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.critical_util_degraded_mode=-1;
  cpuattr.normal_marginal_utility=7;
  cpuattr.overloaded_marginal_utility=7;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu=0;
  cpuattr.enforcement_mask = ZS_ENFORCE_OVERLOAD_BUDGET_MASK;// | ZS_ENFORCEMENT_HARD_MASK;
  
  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }


  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("task 2: error creating reserve\n");
  }


  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  for (i=0;i<6;i++){
    if (first_time){
      first_time=0;
      busy_timestamped(190,timestamps_ns2,MAX_TIMESTAMPS,&bufidx2);
    } else {
      busy_timestamped(190,timestamps_ns2,MAX_TIMESTAMPS,&bufidx2);
    }
    zs_wait_next_period(sched,rid);
  }

  printf("Test finished...saving %ld task datapoins...\n",bufidx1);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);

  fid = fopen("ts-enf-task2.txt","w+");
  if (fid ==NULL){
    printf("error opening file\n");
    return NULL;
  }

  for (l=0;l<bufidx2;l++){
    fprintf(fid,"%llu %d\n",timestamps_ns2[l]-start_ns,rid);
  }

  fclose(fid);

  zs_close_sched(sched);
}

void *task3(void *argp){
  struct zsrm_debug_trace_record drec;
  int first_time=1;
  int rid,mid;
  int sched;
  int i,j,k;
  struct zs_reserve_params cpuattr;
  struct timespec now;
  FILE *fid;
  long l;

  setlocale(LC_NUMERIC,"");

  cpuattr.period.tv_sec = 1;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.criticality = 1;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.priority = 11;
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
  cpuattr.bound_to_cpu=1;
  cpuattr.enforcement_mask = ZS_ENFORCE_OVERLOAD_BUDGET_MASK;// | ZS_ENFORCEMENT_HARD_MASK;
  
  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }


  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("task 3: error creating reserve\n");
  }


  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  for (i=0;i<10;i++){
    if (first_time){
      first_time=0;
      busy_timestamped(500,timestamps_ns3,MAX_TIMESTAMPS,&bufidx3);
    } else {
      busy_timestamped(50,timestamps_ns3,MAX_TIMESTAMPS,&bufidx3);
    }
    zs_wait_next_period(sched,rid);
  }

  printf("Test finished...saving %ld task datapoins...\n",bufidx3);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);

  fid = fopen("ts-enf-task3.txt","w+");
  if (fid ==NULL){
    printf("error opening file\n");
    return NULL;
  }

  for (l=0;l<bufidx3;l++){
    fprintf(fid,"%llu %d\n",timestamps_ns3[l]-start_ns,rid);
  }

  fclose(fid);

  zs_close_sched(sched);
}

void *task4(void *argp){
  struct zsrm_debug_trace_record drec;
  int first_time=1;
  int rid,mid;
  int sched;
  int i,j,k;
  struct zs_reserve_params cpuattr;
  struct timespec now;
  FILE *fid;
  long l;

  setlocale(LC_NUMERIC,"");

  cpuattr.period.tv_sec = 2;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.criticality = 2;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.priority = 10;
  cpuattr.zs_instant.tv_sec=2;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 200000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 200000000;
  cpuattr.response_time_instant.tv_sec = 1;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.critical_util_degraded_mode=-1;
  cpuattr.normal_marginal_utility=7;
  cpuattr.overloaded_marginal_utility=7;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu=1;
  cpuattr.enforcement_mask = ZS_ENFORCE_OVERLOAD_BUDGET_MASK;// | ZS_ENFORCEMENT_HARD_MASK;
  
  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }


  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("task 4: error creating reserve\n");
  }


  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  for (i=0;i<6;i++){
    if (first_time){
      first_time=0;
      busy_timestamped(190,timestamps_ns4,MAX_TIMESTAMPS,&bufidx4);
    } else {
      busy_timestamped(190,timestamps_ns4,MAX_TIMESTAMPS,&bufidx4);
    }
    zs_wait_next_period(sched,rid);
  }

  printf("Test finished...saving %ld task datapoins...\n",bufidx4);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);

  fid = fopen("ts-enf-task4.txt","w+");
  if (fid ==NULL){
    printf("error opening file\n");
    return NULL;
  }

  for (l=0;l<bufidx4;l++){
    fprintf(fid,"%llu %d\n",timestamps_ns4[l]-start_ns,rid);
  }

  fclose(fid);

  zs_close_sched(sched);
}


int main(){
  int sched;
  struct zsrm_debug_trace_record drec;
  pthread_t tid1,tid2,tid3,tid4;
  struct sched_param p;

  union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
  } arg;

   p.sched_priority = 30;
  if (sched_setscheduler(getpid(), SCHED_FIFO,&p)<0){
    printf("error setting fixed priority\n");
    return -1;
  }

 // create sync start semaphore
  if ((sync_start_semid = semget(IPC_PRIVATE, 1, IPC_CREAT|0777)) <0){
    perror("creating the semaphone\n");
    return -1;
  }

  arg.val = 0;
  if (semctl(sync_start_semid, 0, SETVAL, arg) < 0){
    printf("Error setting sem to zero\n");
    return -1;
  }

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }

  if (pthread_create(&tid1,NULL, task1, NULL) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  if (pthread_create(&tid2,NULL, task2, NULL) != 0){
    printf("Error creating thread 2\n");
    return -1;
  }

  if (pthread_create(&tid3,NULL, task3, NULL) != 0){
    printf("Error creating thread 3\n");
    return -1;
  }

  if (pthread_create(&tid4,NULL, task4, NULL) != 0){
    printf("Error creating thread 4\n");
    return -1;
  }

  zs_reset_debug_trace_write_index(sched);
  zs_reset_debug_trace_read_index(sched);

  sleep(3);

  start_ns = now_ns();

  // Go!
  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = 4; // four ups one for each task 
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  pthread_join(tid1,NULL);
  printf("task 1 finished\n");

  pthread_join(tid2,NULL);
  printf("task 2 finished\n");

  pthread_join(tid3,NULL);
  printf("task 3 finished\n");

  pthread_join(tid4,NULL);
  printf("task 4 finished\n");

  FILE* fid3 = fopen("ts-kernel.txt","w+");
  if (fid3 ==NULL){
    printf("error opening ts-kernel.txt\n");
    return -1;
  }

  printf("saving kernel events...\n");

  long cnt=0;
  while (zs_next_debug_trace_record(sched,&drec)>0){
    // transform into ms
    printf("saving kernel record %ld\r",cnt++);
    drec.timestamp = drec.timestamp / 1000000;
    fprintf(fid3,"%llu %u %d\n",(drec.timestamp-start_ns),drec.event_type, drec.event_param);
  }

  fclose(fid3);
  zs_close_sched(sched);

  if (semctl(sync_start_semid, 0, IPC_RMID)<0){
    printf("error removing semaphore\n");
  }

}
