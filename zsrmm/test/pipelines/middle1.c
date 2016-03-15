#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/ipc.h>


#include "../../zsrm.h"


#define BUFSIZE 100

int wait_start_signal(int sync_start_semid){
  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
    return -1;
  }
  return 0;
}


int main(int argc, char * argv[]){
  int rid;
  int err;
  int i=0;
  int sched;
  int fd;
  int remaddrlen, prevaddrlen;
  struct sockaddr_in myaddr, remaddr, prevaddr;
  unsigned char *buf;
  struct zs_reserve_params cpuattr;
  int myport;
  int dest_port;
  char *dest_address;
  cpu_set_t cpuset;
  int io_flag=0;
  key_t semkey;
  int sync_start_semid;

  /* CPU_ZERO(&cpuset); */
  /* CPU_SET(0,&cpuset); */
  /* if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset) != 0){ */
  /*   printf("Error setting CPU affinity of task 2\n"); */
  /* } */

  if (argc != 4){
    printf("usage: %s <my port> <destination ip> <destination port>\n",argv[0]);
    return -1;
  }

  myport = atoi(argv[1]);
  dest_address = argv[2];
  dest_port = atoi(argv[3]);
  
  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <0){
    perror("could not create socket\n");
    return -1;
  }

  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(myport);

  if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    perror("bind failed");
    return 0;
  }

  memset((char *) &remaddr, 0, sizeof (remaddr));
  remaddr.sin_family = AF_INET;
  remaddr.sin_port = htons(dest_port);
  if (inet_aton(dest_address, &remaddr.sin_addr)==0){
    printf("could not parse destination ip address\n");
    return -1;
  }

  memset((char *)&cpuattr.outaddr, 0, sizeof(cpuattr.outaddr));
  cpuattr.outaddr.sin_family = AF_INET;
  cpuattr.outaddr.sin_port = htons(dest_port);
  if (inet_aton(dest_address, &cpuattr.outaddr.sin_addr)==0){
    printf("could not parse destination ip address");
    return -1;
  }
  

  if ((semkey = ftok("/tmp", 11766)) == (key_t) -1) {
    perror("IPC error: ftok"); exit(1);
  }

  // create sync start semaphore
  if ((sync_start_semid = semget(semkey, 1, 0)) <0){
    perror("obtaining the semaphone\n");
    return -1;
  }


  // create reserve
  cpuattr.period.tv_sec = 4;
  cpuattr.period.tv_nsec=0;
  cpuattr.criticality = 0;
  cpuattr.priority = 10;
  cpuattr.zs_instant.tv_sec=4;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.critical_util_degraded_mode=-1;
  cpuattr.normal_marginal_utility=7;
  cpuattr.overloaded_marginal_utility=7;
  cpuattr.num_degraded_modes=0;
  cpuattr.enforcement_mask= DONT_ENFORCE_ZERO_SLACK_MASK;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_MIDDLE | APERIODIC_ARRIVAL;
  cpuattr.e2e_execution_time.tv_sec = 3;
  cpuattr.e2e_execution_time.tv_nsec = 0;
  cpuattr.e2e_overload_execution_time.tv_sec = 4;
  cpuattr.e2e_overload_execution_time.tv_nsec = 0;
  cpuattr.outsockfd = fd;
  cpuattr.bound_to_cpu=1;
  

  printf("Reserve type: %x\n",cpuattr.reserve_type);

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }

  buf = zs_alloc_stage_msg_packet(sched, BUFSIZE);
  if (buf == NULL){
    printf("failed to allocate msg packet\n");
    return -1;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  // wait for up on start sync sem
  wait_start_signal(sync_start_semid);

  zs_attach_reserve(sched,rid,getpid());

  buf[0] = '\0';

  // while not receiving a 'bye' message
  io_flag = MIDDLE_STAGE_DONT_SEND_OUTPUT;
  while (strstr(buf,"bye") == NULL){
    sprintf(buf,"msg[%d]",i++);
    remaddrlen = sizeof (remaddr);
    prevaddrlen = sizeof(remaddr);
    if ((err = zs_wait_next_stage_arrival(sched, rid, 
					  fd, buf, 
					  BUFSIZE,0, 
					  (struct sockaddr *)&remaddr,
					  remaddrlen,
					  (struct sockaddr *)&prevaddr,
					  &prevaddrlen,
					  io_flag
					  ))<0){
      printf("Error(%d) in wait_next_arrival\n",err);
      break;
    } else {
      io_flag = 0;
      printf("middle1: received[%s] from addr(%s)\n",buf,inet_ntoa(prevaddr.sin_addr));
    }
  }

  io_flag = MIDDLE_STAGE_DONT_WAIT_INPUT;
  printf("out of loop sending bye\n");
  sprintf(buf,"bye");
  remaddrlen = sizeof (remaddr);
  prevaddrlen = sizeof(remaddr);
  if ((err = zs_wait_next_stage_arrival(sched, rid, 
					fd, buf, 
					BUFSIZE,0, 
					(struct sockaddr *)&remaddr,
					remaddrlen,
					(struct sockaddr *)&prevaddr,
					&prevaddrlen,
					io_flag
					))<0){
    printf("Error(%d) in wait_next_arrival\n",err);
  }

  zs_free_msg_packet(sched, buf);  
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  printf("middle1 done\n");
  close(fd);
}
