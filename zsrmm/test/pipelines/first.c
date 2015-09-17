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

#include "../../zsrm.h"


#define BUFSIZE 100

int main(int argc, char * argv[]){
  int rid;
  int err;
  int i;
  int sched;
  int fd;
  struct sockaddr_in myaddr;

  unsigned char *buf;
  struct zs_reserve_params cpuattr;
  int myport;
  int dest_port;
  char *dest_address;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);
  if (sched_setaffinity(getpid(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 2\n");
  }

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

  memset((char *)&cpuattr.outaddr, 0, sizeof(cpuattr.outaddr));
  cpuattr.outaddr.sin_family = AF_INET;
  cpuattr.outaddr.sin_port = htons(dest_port);
  if (inet_aton(dest_address, &cpuattr.outaddr.sin_addr)==0){
    printf("could not parse destination ip address");
    return -1;
  }
  
  // create reserve
  cpuattr.period.tv_sec = 5;
  cpuattr.period.tv_nsec=0;
  cpuattr.criticality = 0;
  cpuattr.priority = 10;
  cpuattr.zs_instant.tv_sec=5;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.critical_util_degraded_mode=-1;
  cpuattr.normal_marginal_utility=7;
  cpuattr.overloaded_marginal_utility=7;
  cpuattr.num_degraded_modes=0;
  cpuattr.enforcement_mask= DONT_ENFORCE_ZERO_SLACK;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_ROOT;
  cpuattr.e2e_execution_time.tv_sec = 3;
  cpuattr.e2e_execution_time.tv_nsec = 0;
  cpuattr.e2e_overload_execution_time.tv_sec = 4;
  cpuattr.e2e_overload_execution_time.tv_nsec = 0;
  cpuattr.outsockfd = fd;
  

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
  zs_attach_reserve(sched,rid,getpid());

  for (i=0;i<1;i++){
    sprintf(buf,"msg[%d]",i);
    printf("root sending msg[%s]\n",buf);
    if ((err = zs_wait_next_root_period(sched,rid,fd, 
					buf ,strlen(buf)+1, 
					0, (struct sockaddr *)&cpuattr.outaddr, 
					sizeof (cpuattr.outaddr)))<0){
      perror("error in wait_next_root");
      break;
    }
  }
  sprintf(buf,"bye");
  if (err >=0){
    zs_wait_next_root_period(sched,rid,
			     fd, buf, strlen(buf)+1, 0, 
			     (struct sockaddr *)&cpuattr.outaddr, 
			     sizeof (cpuattr.outaddr));
  }

  zs_free_msg_packet(sched, buf);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  close(fd);
}
