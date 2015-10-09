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
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>
#include <string.h>


#include "../busy.h"
//#include "generic-stage-params.h"
#include "parse_pipeline_tasks_c.h"

#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#else
#error "SYS_gettid unavailable in this system"
#endif

#define MAX_TIMESTAMPS 1000000

#define BUFSIZE 100



int sync_start_semid;

int *fds;

#include "generic-stage-subsystem.h"

int main(int argc, char *argv[]){
  int i;
  int sched;
  pthread_t nodetid;

  unsigned long long start_timestamp_ns;
  long idx;
  struct sched_param p;
  key_t semkey;
  FILE* fid1;
  char filename[100];


  int numreserves=0;
  pthread_t *tids;
  struct reserve_list *rsvlist,*nextrsv;


  union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
  } arg;


  if (argc != 2){
    printf("usage: %s <task description file>\n",argv[0]);
    return -1;
  }

  rsvlist = get_reserve_list(argv[1],&numreserves);

  if (rsvlist == NULL){
    printf("could not retrieve reserve list\n");
    return -1;
  }

  tids = malloc(sizeof(pthread_t)*numreserves);
  fds = malloc(sizeof(int)*numreserves);
  
  setlocale(LC_NUMERIC,"");

  p.sched_priority = 30;
  if (sched_setscheduler(getpid(), SCHED_FIFO,&p)<0){
    printf("error setting fixed priority\n");
    return -1;
  }

  calibrate_ticks();

  if ((semkey = ftok("/tmp", 11766)) == (key_t) -1) {
    perror("IPC error: ftok"); exit(1);
  }

  // create sync start semaphore
  if ((sync_start_semid = semget(semkey, 1, IPC_CREAT|0777)) <0){
    perror("creating the semaphone\n");
    return -1;
  }

  arg.val = 0;
  if (semctl(sync_start_semid, 0, SETVAL, arg) < 0){
    printf("Error setting sem to zero\n");
    return -1;
  }

  int numfds = 0;
  nextrsv = rsvlist;
  for (i=0;i<numreserves;i++){
    if (pthread_create(&(tids[i]),NULL, generic_stage_task,(void *)&(nextrsv->taskparams)) != 0){
      printf("Error creating thread %d\n",i);
      return -1;
    }
    if (nextrsv->taskparams.fd_index >=0){
      numfds++;
    }
    nextrsv = nextrsv->next;
  }


  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }
  

  // let the threads prepare
  sleep(5);

  printf("about to start node with %d fds\n",numfds);
  nodetid = zs_start_node(sched,fds,numfds);// 12
  
  if (nodetid <0 ){
    printf("error starting node server\n");
    return -1;
  }

  start_timestamp_ns = now_ns();//ticks2nanos(rdtsc());

  // Go!
  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = numreserves; // 16 ups one for each task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  printf("test tasks started\n");
  usleep(1000);

  for (i=0;i<numreserves;i++){
    pthread_join(tids[i],NULL);
    printf("task %d finished\n",i);
  }

  nextrsv = rsvlist;
  for (i=0;i<numreserves;i++){
    sprintf(filename,"ts-%s.txt",nextrsv->taskparams.cpuattr.name);
    fid1 = fopen(filename,"w+");
    if (fid1==NULL){
      printf("error opening %s\n",filename);
      return -1;
    }
    
    printf("writing bufidx(%lu) samples for %s\n",nextrsv->taskparams.bufidx,
	   nextrsv->taskparams.cpuattr.name);
    for (idx = 0 ; idx < nextrsv->taskparams.bufidx ; idx++){
      fprintf(fid1,"%llu 1\n",nextrsv->taskparams.timestamp_ns[idx]-start_timestamp_ns);
    }
    
    fclose(fid1);
    nextrsv = nextrsv->next;
  }



  if (semctl(sync_start_semid, 0, IPC_RMID)<0){
    printf("error removing semaphore\n");
  }

  printf("shutting down arrival server\n");
  zs_stop_node(nodetid);

}
