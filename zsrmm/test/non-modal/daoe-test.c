/*
 * This test is calibrated to run on Lenovo Yoga 13 i7 at 1.5 GHz CPU frequency
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
long bufidx2;
long bufidx3;
unsigned long long timestamps_ns1[MAX_TIMESTAMPS];
unsigned long long timestamps_ns2[MAX_TIMESTAMPS];
unsigned long long timestamps_ns3[MAX_TIMESTAMPS];

int rid1;
int rid2;

int sync_start_semid;
int idle_start_semid;

void *task1(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int sched;
  int i;
 
  /*
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 1\n");
  }
  */
  
  cpuattr.period.tv_sec = 0;
  cpuattr.period.tv_nsec= 400000000;
  cpuattr.priority = 10;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 200000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 200000000;
  cpuattr.zs_instant.tv_sec=0;
  cpuattr.zs_instant.tv_nsec=400000000;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.criticality = 1;
  cpuattr.normal_marginal_utility = 1;
  cpuattr.overloaded_marginal_utility = 1;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.enforcement_mask=0;
  cpuattr.bound_to_cpu = 0;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  if (rid < 0){
    printf("could not create reservation\n");
    return NULL;
  }

  rid1=rid;
  
  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  if (zs_attach_reserve(sched,rid,gettid())<0){
    printf("could not attached reserve\n");
    return NULL;
  }

  printf("task 1 getting into loop\n");

  for (i=0;i<10;i++){
    busy_timestamped(200,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
    printf("task1 before wait next period\n");
    zs_wait_next_period(sched,rid);
    printf("task1 after wait next period\n");
  }

  printf("task 1 done with loop. About to finish\n");

  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

}

void *task2(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int sched;
  int i;

  /*
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 2\n");
  }
  */

  cpuattr.period.tv_sec = 0;
  cpuattr.period.tv_nsec= 800000000;
  cpuattr.priority = 9;
  cpuattr.execution_time.tv_sec =0;
  cpuattr.execution_time.tv_nsec = 300000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 500000000;
  cpuattr.zs_instant.tv_sec=0;
  cpuattr.zs_instant.tv_nsec=500000000;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.criticality = 2;
  cpuattr.normal_marginal_utility = 2;
  cpuattr.overloaded_marginal_utility = 2;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.enforcement_mask=0;
  cpuattr.bound_to_cpu = 0;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  if (rid < 0){
    printf("could not create reserve\n");
    return NULL;
  }

  rid2=rid;

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }


  if (zs_attach_reserve(sched,rid,gettid())<0){
    printf("could not attach reserve\n");
    return NULL;
  }

  printf("task 2 about to start\n");
  for (i=0;i<10;i++){
    if (i == 0)
      busy_timestamped(350,timestamps_ns2, MAX_TIMESTAMPS,&bufidx2);
    else
      busy_timestamped(350,timestamps_ns2, MAX_TIMESTAMPS,&bufidx2);
    printf("task2 before wait next period\n");
    zs_wait_next_period(sched,rid);
    printf("task2 after wait next period\n");
  }

  printf("task 2 about to finish\n");
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);
}

int idletaskrunning = 1;

void *idletask(void *argp){
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 2\n");
  }

  struct sched_param p;
  p.sched_priority=1;

  if (pthread_setschedparam(pthread_self(),SCHED_FIFO,&p) != 0){
    printf("could not set fixed priority to idletask\n");
  }

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(idle_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  if (idletaskrunning)
    busy(2000);
  //  while(idletaskrunning)
  //  ;
}

int main(int argc, char *argv[]){
  int sched;
  struct zsrm_debug_trace_record drec;
  pthread_t tid1,tid2,tid3;
  unsigned long long start_timestamp_ns;
  long idx;
  struct sched_param p;

  union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
  } arg;


  if (argc != 2){
    printf("usage: %s <0=no idle task | 1 = idle task>",argv[0]);
    return -1;
  }

  idletaskrunning = atoi(argv[1]);

  setlocale(LC_NUMERIC,"");

  p.sched_priority = 30;
  if (sched_setscheduler(getpid(), SCHED_FIFO,&p)<0){
    printf("error setting fixed priority\n");
    return -1;
  }

  calibrate_ticks();

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

  // create idle start semaphore
  if ((idle_start_semid = semget(IPC_PRIVATE, 1, IPC_CREAT|0777)) <0){
    perror("creating the semaphone\n");
    return -1;
  }

  arg.val = 0;
  if (semctl(idle_start_semid, 0, SETVAL, arg) < 0){
    printf("Error setting sem to zero\n");
    return -1;
  }

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }

  zs_reset_debug_trace_write_index(sched);
  zs_reset_debug_trace_read_index(sched);

  if (pthread_create(&tid1,NULL, task1, NULL) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  if (pthread_create(&tid2,NULL, task2, NULL) != 0){
    printf("Error creating thread 2\n");
    return -1;
  }

  if (pthread_create(&tid3,NULL, idletask, NULL) != 0){
    printf("Error creating thread 2\n");
    return -1;
  }

  // let the threads prepare -- In your marks, get set...
  sleep(3);

  start_timestamp_ns = now_ns();//ticks2nanos(rdtsc());

  // Go!
  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = 3; // three ups one for each task including idle task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  printf("test tasks started\n");
  usleep(1000);

  // release idletask!
  sops.sem_num=0;
  sops.sem_op = 1; // up
  sops.sem_flg = 0;
  if (semop(idle_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  printf("idle task released\n");

  pthread_join(tid1,NULL);
  printf("task 1 finished\n");

  pthread_join(tid2,NULL);
  printf("task 2 finished\n");

  // stop idletask
  idletaskrunning = 0;
  pthread_join(tid3,NULL);

  printf("idle task finished\n");

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

  for (idx = 0 ; idx < bufidx1 ; idx++){
    fprintf(fid1,"%llu %d\n",timestamps_ns1[idx]-start_timestamp_ns, rid1);
  }

  for (idx = 0 ; idx < bufidx2 ; idx++){
    fprintf(fid2,"%llu %d\n",timestamps_ns2[idx] -start_timestamp_ns,rid2);
  }

  fclose(fid1);
  fclose(fid2);

  FILE* fid3 = fopen("ts-kernel.txt","w+");
  if (fid3 ==NULL){
    printf("error opening ts-kernel.txt\n");
    return -1;
  }

  while (zs_next_debug_trace_record(sched,&drec)>0){
    // transform into ms
    drec.timestamp = drec.timestamp / 1000000;
    fprintf(fid3,"%llu %u %d\n",(drec.timestamp-start_timestamp_ns),drec.event_type, drec.event_param);
  }

  fclose(fid3);

  if (semctl(sync_start_semid, 0, IPC_RMID)<0){
    printf("error removing semaphore\n");
  }

  if (semctl(idle_start_semid, 0, IPC_RMID)<0){
    printf("error removing semaphore\n");
  }

}
