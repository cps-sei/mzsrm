/*) 2014 Carnegie Mellon University.

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
#include "../zsrm.h"
#include "zsmutex.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/unistd.h>
#include <errno.h>
#include <sys/epoll.h>

int zs_open_sched(){
	int fd;
	fd = open("/dev/zsrmm0", O_RDWR);
	if (fd < 0) {
		printf("Error: failed to access the module!\n");
		return -1;
	}

	return fd;
}

int zs_close_sched(int fd){
	close(fd);
}

int zs_get_jiffies_ms(int fd){
	struct api_call call;
	int rid;
	
	call.api_id = GET_JIFFIES_MS;
	rid = write(fd,&call, sizeof(call));
	return rid;
}

int zs_print_stats(int fd){
  struct api_call call;
  int rid;

  call.api_id= PRINT_STATS;
  rid = write(fd, &call, sizeof(call));
  return rid;
}

int zs_create_reserve(int fd, struct zs_reserve_params *p){
	struct api_call call;
	int rid;

	call.api_id = CREATE_RESERVE;
	memcpy(&(call.args.reserve_parameters),p, sizeof(struct zs_reserve_params)); 
	rid = write(fd,&call, sizeof(call));
	return rid;
}

int zs_attach_reserve(int fd , int rid, int pid){
	struct api_call call;
	int ret;
	sigset_t sigmask1;
	struct sigaction act;

	call.api_id = ATTACH_RESERVE;
	call.args.attach_params.reserveid = rid;
	call.args.attach_params.pid = pid;
	ret = write(fd,&call,sizeof(call));
	return ret;
}

int zs_detach_reserve(int fd, int rid){
	struct api_call call;
	int ret;

	call.api_id = DETACH_RESERVE;
	call.args.reserveid = rid;
	ret = write(fd, &call, sizeof(call));
	return ret;
}

int zs_delete_reserve(int fd, int rid){
	struct api_call call;
	int ret;

	call.api_id = DELETE_RESERVE;
	call.args.reserveid = rid;
	ret = write(fd, &call, sizeof(call));
	return ret;
}

int zs_modal_wait_next_period(int fd, int mrid){
	struct api_call call;
	int ret;

	call.api_id = MODAL_WAIT_NEXT_PERIOD;
	call.args.reserveid = mrid;
	while ((ret = write(fd, &call, sizeof(call))) == -1 && errno == EINTR)
	  ;
	return ret;
}

int zs_wait_next_period(int fd, int rid){
	struct api_call call;
	int ret;

	call.api_id = WAIT_NEXT_PERIOD;
	call.args.reserveid = rid;
	while ((ret = write(fd, &call, sizeof(call))) == -1 && errno == EINTR)
	  ;
	return ret;
}

int zs_wait_next_arrival(int fd, int rid, struct pollfd *fds, unsigned int nfds){
  struct api_call call;
  int ret;

  call.api_id = WAIT_NEXT_ARRIVAL;
  call.args.wait_next_arrival_params.reserveid = rid;
  call.args.wait_next_arrival_params.fds = fds;
  call.args.wait_next_arrival_params.nfds = nfds;
  while ((ret = write(fd, &call, sizeof(call))) == -1 && errno == EINTR)
    ;

  return ret;
}

int zs_set_initial_mode_modal_reserve(int fid, int modal_reserve_id, int mode_id){
  struct api_call call;
  int ret;

  call.api_id = SET_INITIAL_MODE_MODAL_RESERVE;
  call.args.set_initial_mode_params.modal_reserve_id = modal_reserve_id;
  call.args.set_initial_mode_params.initial_mode_id = mode_id;
  ret = write(fid,&call,sizeof(call));
  return ret;
}

int zs_create_modal_reserve(int fid,int num_modes){
  struct api_call call;
  int ret;

  call.api_id = CREATE_MODAL_RESERVE;
  call.args.num_modes = num_modes;
  ret = write(fid,&call, sizeof(call));
  return ret;
}

int zs_add_reserve_to_mode(int fid, int modal_reserve_id, int mode_id, int rid){
  struct api_call call;
  int ret;
  
  call.api_id = ADD_RESERVE_TO_MODE;
  call.args.rsv_to_mode_params.modal_reserve_id = modal_reserve_id;
  call.args.rsv_to_mode_params.reserve_id = rid;
  call.args.rsv_to_mode_params.mode_id = mode_id;

  ret = write(fid, &call, sizeof(call));
  return ret;
}

int zs_add_mode_transition(int fid, int modal_reserve_id, int transition_id, struct zs_mode_transition *p){
  struct api_call call;
  int ret;

  call.api_id = ADD_MODE_TRANSITION;
  call.args.add_mode_transition_params.modal_reserve_id = modal_reserve_id;
  call.args.add_mode_transition_params.transition_id = transition_id;
  memcpy(&(call.args.add_mode_transition_params.transition),p, sizeof(struct zs_mode_transition)); 

  ret = write(fid,&call, sizeof(call));
  return ret;  
}

int zs_attach_modal_reserve(int fid, int modal_reserve_id, int pid){
  struct api_call call;
  int ret;

  call.api_id = ATTACH_MODAL_RESERVE;
  call.args.attach_params.reserveid = modal_reserve_id;
  call.args.attach_params.pid = pid;

  ret = write(fid,&call, sizeof(call));
  return ret;
}

int zs_detach_modal_reserve(int fid, int modal_reserve_id){
  struct api_call call;
  int ret;

  call.api_id = DETACH_MODAL_RESERVE;
  call.args.reserveid = modal_reserve_id;

  ret = write(fid,&call,sizeof(call));
  return ret;
}

int zs_delete_modal_reserve(int fid, int modal_reserve_id){
  struct api_call call;
  int ret;

  call.api_id = DELETE_MODAL_RESERVE;
  call.args.reserveid = modal_reserve_id;

  ret = write(fid,&call,sizeof(call));
  return ret;
}

int zs_delete_sys_transition(int fid, int stid){
  struct api_call call;
  int ret;

  call.api_id=DELETE_SYS_TRANSITION;
  call.args.reserveid = stid;

  ret = write(fid,&call, sizeof(call));
  return ret;
}

int zs_create_sys_transition(int fid){
  struct api_call call;
  int ret;
  
  call.api_id = CREATE_SYS_TRANSITION;
  
  ret = write(fid, &call, sizeof(call));
  return ret;
}

int zs_add_transition_to_sys_transition(int fid, int sys_transition_id,  int mrid, int transition_id){
  struct api_call call;
  int ret;

  call.api_id = ADD_TRANSITION_TO_SYS_TRANSITION;
  call.args.trans_to_sys_trans_params.sys_transition_id = sys_transition_id;
  call.args.trans_to_sys_trans_params.mrid = mrid;
  call.args.trans_to_sys_trans_params.transition_id = transition_id;

  ret = write(fid,&call,sizeof(call));
  return ret;
}

int zs_mode_switch(int fid, int sys_transition_id){
  struct api_call call;
  int ret;

  call.api_id = MODE_SWITCH;
  call.args.mode_switch_params.transition_id = sys_transition_id;

  ret = write(fid, &call, sizeof(call));
  return ret;
}

int zs_raise_priority_criticality(int fid, int priority_ceiling,int criticality_ceiling){
  struct api_call call;
  int ret;
  
  call.api_id = RAISE_PRIORITY_CRITICALITY;
  call.args.raise_priority_criticality_params.priority_ceiling = priority_ceiling;
  call.args.raise_priority_criticality_params.criticality_ceiling = criticality_ceiling;

  ret = write(fid,&call, sizeof(call));
  return ret;
}

int zs_restore_base_priority_criticality(int fid){
  struct api_call call;
  int ret;

  call.api_id = RESTORE_BASE_PRIORITY_CRITICALITY;

  ret = write(fid,&call, sizeof(call));
  return ret;
}


int zs_pccp_mutex_init(zs_pccp_mutex_t *mutex, int priority_ceiling, int criticality_ceiling, pthread_mutexattr_t *attr){

  mutex->criticality_ceiling = criticality_ceiling;
  mutex->priority_ceiling = priority_ceiling;
  return pthread_mutex_init(&(mutex->pmutex),attr);
}

int zs_pccp_mutex_lock(int sched_id, zs_pccp_mutex_t *mutex){
  zs_raise_priority_criticality(sched_id, mutex->priority_ceiling,mutex->criticality_ceiling);
  return pthread_mutex_lock(&(mutex->pmutex));
}

int zs_pccp_mutex_unlock(int sched_id, zs_pccp_mutex_t *mutex){
  int ret;

  ret = pthread_mutex_unlock(&(mutex->pmutex));
  zs_restore_base_priority_criticality(sched_id);
  return ret;
}

int zs_pccp_mutex_destroy(zs_pccp_mutex_t *mutex){
  return pthread_mutex_destroy(&(mutex->pmutex));
}

int zs_wait_next_stage_arrival(int sched_fid, int rid, int fd, void *data, int datalen, int flags, struct sockaddr *outaddr, int outaddrlen, struct sockaddr *inaddr, int *inaddrlen, int io_flag){
  struct api_call call;
  int ret;

  call.api_id = WAIT_NEXT_STAGE_ARRIVAL;
  call.args.wait_next_stage_arrival_params.reserveid = rid;
  call.args.wait_next_stage_arrival_params.fd = fd;
  call.args.wait_next_stage_arrival_params.usrdata = data;
  call.args.wait_next_stage_arrival_params.usrdatalen = datalen;
  call.args.wait_next_stage_arrival_params.flags = flags;
  call.args.wait_next_stage_arrival_params.outaddr = outaddr;
  call.args.wait_next_stage_arrival_params.outaddrlen = outaddrlen;
  call.args.wait_next_stage_arrival_params.inaddr = inaddr;
  call.args.wait_next_stage_arrival_params.inaddrlen = inaddrlen;
  call.args.wait_next_stage_arrival_params.io_flag = io_flag;

  ret = write(sched_fid,&call, sizeof(call));
  return ret;
}

int zs_wait_next_leaf_stage_arrival(int sched_fid, int rid, int fd, void *indata, int indatalen, unsigned int flags, struct sockaddr *addr, int *addr_len){
  struct api_call call;
  int ret;

  call.api_id = WAIT_NEXT_LEAF_STAGE_ARRIVAL;
  call.args.wait_next_leaf_stage_arrival_params.reserveid = rid;
  call.args.wait_next_leaf_stage_arrival_params.usrindata = indata;
  call.args.wait_next_leaf_stage_arrival_params.usrindatalen = indatalen;
  call.args.wait_next_leaf_stage_arrival_params.fd = fd;
  call.args.wait_next_leaf_stage_arrival_params.flags = flags;
  call.args.wait_next_leaf_stage_arrival_params.addr = addr;
  call.args.wait_next_leaf_stage_arrival_params.addr_len =addr_len;

  ret = write(sched_fid,&call, sizeof(call));
  return ret;
}

//int zs_wait_next_root_period(int sched_fid, int rid, void *outdata, int outdatalen){
int zs_wait_next_root_period(int sched_fid, int rid, int fd, void *buf, size_t buflen, unsigned int flags, struct sockaddr *addr, int addrlen){

  struct api_call call;
  int ret;

  call.api_id = WAIT_NEXT_ROOT_PERIOD;
  call.args.wait_next_root_stage_period_params.reserveid = rid;
  call.args.wait_next_root_stage_period_params.fd = fd;
  call.args.wait_next_root_stage_period_params.usroutdata = buf;
  call.args.wait_next_root_stage_period_params.usroutdatalen = buflen;
  call.args.wait_next_root_stage_period_params.flags = flags;
  call.args.wait_next_root_stage_period_params.addr = addr;
  call.args.wait_next_root_stage_period_params.addr_len = addrlen;

  ret = write(sched_fid,&call, sizeof(call));
  return ret;
}

int zs_get_pipeline_header_size(int sched_fd){
  struct api_call call;
  int ret;

  call.api_id = GET_PIPELINE_HEADER_SIZE;

  ret = write(sched_fd, &call, sizeof(call));
  return ret;
}

int zs_get_pipeline_header_signature(int sched_fd){
  struct api_call call;
  int ret;

  call.api_id = GET_PIPELINE_HEADER_SIGNATURE;

  ret = write(sched_fd, &call, sizeof(call));
  return ret;
}

void *zs_alloc_stage_msg_packet(int sched_fd, size_t size){
  int header_size;
  unsigned int signature;
  void *p;
  unsigned int *pheader;

  signature = zs_get_pipeline_header_signature(sched_fd);
  header_size = zs_get_pipeline_header_size(sched_fd);

  p = malloc(size+header_size);

  if (p!= NULL){
    pheader = (unsigned int *) p;
    *pheader = signature;
    
    return (p+header_size);
  } else {
    return NULL;
  }
}

void zs_free_msg_packet(int sched_fd, void *buf){
  int header_size;

  header_size = zs_get_pipeline_header_size(sched_fd);

  buf = buf - header_size;

  free(buf);
}

int zs_notify_arrival(int sched, int *fds, int nfds){
  struct api_call call;
  int ret;

  call.api_id = NOTIFY_ARRIVAL;
  call.args.notify_arrival_params.fds = fds;
  call.args.notify_arrival_params.nfds = nfds;

  ret = write(sched, &call, sizeof(call));
  return ret;
  
}

int request_poll_server_shutdown = 0;

void *poll_server_task(void *argp){
  int i;
  int ret;
  int sched;
  int epollfd;
  int fds[MAX_RESERVES];
  int nfds=0;
  struct sched_param schedp;
  struct epoll_event ev, *events;
  struct poll_server_params *p = (struct poll_server_params *)argp;

  sched = p->sched;

  events = calloc(p->nfds,sizeof(struct epoll_event));
  if (events == NULL){
    printf("could not allocation events memory\n");
    return NULL;
  }

  if ((epollfd = epoll_create(p->nfds))<0){
    printf("error creating epoll\n");
    return NULL;
  }

  for (i=0;i<p->nfds;i++){
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = p->fds[i];
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, p->fds[i],&ev)<0){
      printf("error adding event\n");
      return NULL;
    }
  }

  schedp.sched_priority = zs_get_scheduler_priority(sched);

  printf("setting polling server priority to: %d\n",schedp.sched_priority);
  if (pthread_setschedparam(pthread_self(), SCHED_FIFO,&schedp)<0){
    printf("poll server. error setting fixed priority\n");
    return NULL;
  }
  
  while (!request_poll_server_shutdown){
    ret = epoll_wait(epollfd, events, p->nfds, -1);
    if (ret <0){
      printf("epoll failed\n");
    }
    if (ret >0){
      if (ret <= MAX_RESERVES){
	for (i=0;i<ret;i++){
	  //printf("data available in fd(%d)\n",events[i].data.fd);
	  fds[i] = events[i].data.fd;
	}
	zs_notify_arrival(sched, fds, ret);
      } else {
	printf("arrival server: too many arrival events: %d\n",ret);
      }
    }
  }
  free(events);
}

struct poll_server_params p;

pthread_t zs_start_node(int sched_fid, int *fds, unsigned int nfds){
  pthread_t server_tid;

  p.sched = sched_fid;
  p.fds = fds;
  p.nfds = nfds;
  p.sched = sched_fid;

  request_poll_server_shutdown = 0;
  if (pthread_create(&server_tid, NULL, poll_server_task, (void*) &p) != 0){
    printf("error creating poll server thread\n");
    return -1;
  }
  return server_tid;
}

void zs_stop_node(pthread_t tid){
  request_poll_server_shutdown = 1;
  pthread_kill(tid,SIGQUIT);
}

int zs_get_scheduler_priority(int sched){
  struct api_call call;
  int ret;

  call.api_id = GET_SCHEDULER_PRIORITY;
  ret = write(sched,&call, sizeof(call));
  return ret;
}
