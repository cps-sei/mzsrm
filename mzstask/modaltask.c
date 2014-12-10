
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


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "modaltask.h"
#include "modaltrigger.h"
#include <zsrm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>


unsigned long long now_ns(){
  struct timespec n;
  clock_gettime(CLOCK_MONOTONIC,&n);

  unsigned long long retnow = (((unsigned long long) n.tv_sec) *1000L) + (unsigned long long) (n.tv_nsec/1000000);
return retnow;
}

struct modal_task *modal_task_self;

static void signal_handler (int sig, siginfo_t *siginfo, void *context){
  int mode = (int) siginfo->si_value.sival_int;
  // otherwise wait until next period to swith mode
  modal_task_self->in_transition=1;
  modal_task_self->next_mode = mode;
  printf("pid(%d): mode_switch(%d)\n",getpid(),mode);
}

static void exit_request_signal_handler (int sig, siginfo_t *siginfo, void *context){
  modal_task_self->exit_requested = 1;
}


int install_signal_handlers(){
  struct sigaction act, act1;
  memset(&act, '\0',sizeof(act));

  act.sa_sigaction = signal_handler;
  act.sa_flags = SA_SIGINFO;
  if (sigaction(MODE_CHANGE_SIGNAL, &act, NULL)<0){
    perror("sigaction mode change");
    return -1;
  }
  memset(&act1, '\0',sizeof(act1));

  act1.sa_sigaction = exit_request_signal_handler;
  act1.sa_flags = SA_SIGINFO;
  if (sigaction(EXIT_REQUEST_SIGNAL, &act1, NULL)<0){
    perror("sigaction exit request");
    return -1;
  }
  return 0;
}

void start_modal_task(int sched, struct modal_task *task, int argc, void *argv[]){
  int pid;

  if (!(pid=fork())){
    modal_task_self = task;
    modal_task_self->exit_requested = 0;
    modal_task_self->init(argc,argv);
    install_signal_handlers();

    
    if (sched_setaffinity(getpid(), sizeof(task->affinity_mask), &(task->affinity_mask)) <0){
      printf("error setting affinity\n");
    }
    

    if (modal_task_self->sync_start_semid != -1){
      struct sembuf sops;
      sops.sem_num=0;
      sops.sem_op = -1; // down
      sops.sem_flg = 0;
      if (semop(modal_task_self->sync_start_semid,&sops,1)<0){
	printf("error on sync star sem down\n");
      }
      zs_attach_modal_reserve(sched, task->mrid,getpid());
    } else {
      zs_attach_modal_reserve(sched, task->mrid,getpid());
      zs_wait_next_period(sched, modal_task_self->mrid);
    }
    while(!modal_task_self->exit_requested){
      if (modal_task_self->current_mode != DISABLED_MODE)
	modal_task_self->modes[modal_task_self->current_mode].mode_job_function();
      if (zs_wait_next_period(sched, modal_task_self->mrid)<0){
	printf("wait next period returned negative\n");
	if (errno == EINTR){
	  printf("signal interruption!!\n");
	}
      }
      // if mode change pending then 
      if (modal_task_self->in_transition){
	// here we need to call a cancel timers syscall
	modal_task_self->current_mode = task->next_mode;
	modal_task_self->in_transition=0;
      }
    } 
    exit(0);
  } else {
    task->pid = pid;
  }
}
