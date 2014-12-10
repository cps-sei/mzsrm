
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


#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "modaltrigger.h"

void signal_mode_change(int pid, int mode){
  printf("signaling pid(%d) mode(%d)\n",pid,mode);
  sigqueue(pid, MODE_CHANGE_SIGNAL, (const union sigval)mode);
}

void sys_mode_change(int sched, struct modal_system_t *msys, int sys_transition){
  int i,j;

  int to_mode = msys->sys_transitions[sys_transition].to_mode;
  int transition_id = msys->sys_transitions[sys_transition].transition_id;

  for (j=0;j<msys->sys_modes[to_mode].num_active_task_modes;j++){
    signal_mode_change(msys->sys_modes[to_mode].active_task_modes[j].pid,
		       msys->sys_modes[to_mode].active_task_modes[j].mode);
  }


  zs_mode_switch(sched, transition_id);
}

void request_exit(struct modal_task *taskp){
  sigqueue(taskp->pid, EXIT_REQUEST_SIGNAL, (const union sigval)0);
}

void request_wait_exit(struct modal_task *taskp){
  int retval;

  request_exit(taskp);
  waitpid(taskp->pid,&retval,0);
}
