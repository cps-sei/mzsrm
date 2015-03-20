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

#include <zsmutex.h>

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

int sync_start_semid;

zs_pccp_mutex_t mutex;

void *task1(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int sched;
  int i;
  
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 1\n");
  }
  
  cpuattr.period.tv_sec = 1;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.priority = 10;
  cpuattr.zs_instant.tv_sec=10;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.criticality = 1;
  cpuattr.enforcement_mask=0;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());


  for (i=0;i<10;i++){
    if (i==0){
      busy_timestamped(500,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
      //busy_timestamped(200,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
      //zs_pccp_mutex_lock(sched,&mutex);
      //busy_timestamped(100,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
      //zs_pccp_mutex_unlock(sched,&mutex);
      //busy_timestamped(200,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
    } else {
      busy_timestamped(500,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
    }
    zs_wait_next_period(sched,rid);
  }

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
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 2\n");
  }

  cpuattr.period.tv_sec = 2;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.priority = 9;
  cpuattr.zs_instant.tv_sec=1;
  cpuattr.zs_instant.tv_nsec=500000000;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.criticality = 2;
  cpuattr.enforcement_mask=0;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }


  zs_attach_reserve(sched,rid,gettid());

  for (i=0;i<10;i++){
    /*
    if (i == 0)
      busy_timestamped(1000,timestamps_ns2, MAX_TIMESTAMPS,&bufidx2);
    else
    */
    zs_pccp_mutex_lock(sched,&mutex);    
    busy_timestamped(200,timestamps_ns2, MAX_TIMESTAMPS,&bufidx2);
    zs_pccp_mutex_unlock(sched,&mutex);    
    busy_timestamped(300,timestamps_ns2, MAX_TIMESTAMPS,&bufidx2);
    //busy_timestamped(500,timestamps_ns2, MAX_TIMESTAMPS,&bufidx2);
    zs_wait_next_period(sched,rid);
  }

  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);
}

void *task3(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int sched;
  int i;
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 3\n");
  }

  cpuattr.period.tv_sec = 4;//0;
  cpuattr.period.tv_nsec=0;//100000000;
  cpuattr.priority = 8;
  cpuattr.zs_instant.tv_sec=2;
  cpuattr.zs_instant.tv_nsec=500000000;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE;
  cpuattr.criticality = 3;
  cpuattr.enforcement_mask=0;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }


  zs_attach_reserve(sched,rid,gettid());


  for (i=0;i<10;i++){
    if (i==0){
      //busy_timestamped(1500,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
      //busy_timestamped(500,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
      zs_pccp_mutex_lock(sched, &mutex);
      //busy_timestamped(100,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
      //busy_timestamped(600,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
      busy_timestamped(1500,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
      zs_pccp_mutex_unlock(sched,&mutex);
      //busy_timestamped(900,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
    } else {
      busy_timestamped(1000,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
    }
    zs_wait_next_period(sched,rid);
  }

  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);
}

int main(){
  pthread_t tid1,tid2,tid3;
  unsigned long long start_timestamp_ns;
  long idx;
  struct sched_param p;

  union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
  } arg;


  setlocale(LC_NUMERIC,"");

  p.sched_priority = 30;
  if (sched_setscheduler(getpid(), SCHED_FIFO,&p)<0){
    printf("error setting fixed priority\n");
    return -1;
  }

  calibrate_ticks();

  
  zs_pccp_mutex_init(&mutex, 
		     10, // priority ceiling
		     3,  // criticality ceiling
		     NULL);
  

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

  // let the threads prepare -- In your marks, get set...
  sleep(3);

  start_timestamp_ns = now_ns();//ticks2nanos(rdtsc());

  // Go!
  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = 3; // three ups one for each task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }


  pthread_join(tid1,NULL);
  pthread_join(tid2,NULL);
  pthread_join(tid3,NULL);


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

  FILE* fid3 = fopen("ts-task3.txt","w+");
  if (fid2 ==NULL){
    printf("error opening ts-task3.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < bufidx1 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns1[idx]-start_timestamp_ns);
  }

  for (idx = 0 ; idx < bufidx2 ; idx++){
    fprintf(fid2,"%llu 2\n",timestamps_ns2[idx] -start_timestamp_ns);
  }

  for (idx = 0 ; idx < bufidx3 ; idx++){
    fprintf(fid3,"%llu 3\n",timestamps_ns3[idx] - start_timestamp_ns);
  }

  fclose(fid1);
  fclose(fid2);
  fclose(fid3);

  if (semctl(sync_start_semid, 0, IPC_RMID)<0){
    printf("error removing semaphore\n");
  }

  zs_pccp_mutex_destroy(&mutex);

}
