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

#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <sys/time.h>
#endif

#ifndef __ZSRM_H__
#define __ZSRM_H__

#define DAEMON_PRIORITY 50;
// calls
#define CREATE_RESERVE 1
#define ATTACH_RESERVE 2
#define DETACH_RESERVE 3
#define DELETE_RESERVE 4
#define WAIT_NEXT_PERIOD 5
#define BUDGET_OVERRUN 6
#define GET_JIFFIES_MS 7

#define CREATE_MODAL_RESERVE 8
#define ADD_RESERVE_TO_MODE 9
#define ATTACH_MODAL_RESERVE 10
#define DETACH_MODAL_RESERVE 11
#define MODE_SWITCH 12
#define ADD_MODE_TRANSITION 13
#define DELETE_MODAL_RESERVE 14
#define CREATE_SYS_TRANSITION 15
#define ADD_TRANSITION_TO_SYS_TRANSITION 16
#define DELETE_SYS_TRANSITION 17
#define SET_INITIAL_MODE_MODAL_RESERVE 18
#define PRINT_STATS 19

#define TIMER_ZS  1
#define TIMER_ENF 2
#define TIMER_ZS_ENF 3
#define TIMER_PERIOD 4

#define MAX_RESERVES 64
#define MAX_MODAL_RESERVES 64

#define MAX_SYS_MODES 64

#define MAX_SYS_MODE_TRANSITIONS 64
#define MAX_TRANSITIONS_IN_SYS_TRANSITIONS 64

#define REMOTE_SERVER_PORT 1500

#define ZS_TIMER_SIGNAL (SIGRTMIN+1)
#define ZS_BUDGET_TIMER_SIGNAL (SIGRTMIN+2)

#define MODE_CHANGE_SIGNAL (SIGRTMIN+3)
#define EXIT_REQUEST_SIGNAL (SIGRTMIN+4)


#define ZS_RESPONSE_TIME_ENFORCEMENT_MASK 0x1
#define ZS_PERIOD_DEGRADATION_MAKS 0x2
#define ZS_CRITICALITY_ENFORCEMENT_MASK 0x4

#define MAX_DEGRADED_MODES 4
#define MAX_TRANSITIONS 64

#define DISABLED_MODE -1

// Types of reserves
#define CRITICALITY_RESERVE 0
#define UTILITY_RESERVE 1

#define TIMESPEC2NS(ts) (((unsigned long long) (ts)->tv_sec) * 1000000000ll + (unsigned long long) (ts)->tv_nsec)

#define GET_EFFECTIVE_UTILITY(i) (reserve_table[i].current_degraded_mode == -1 ? reserve_table[i].params.overloaded_marginal_utility: reserve_table[i].params.degraded_marginal_utility[reserve_table[i].current_degraded_mode][1])

struct zs_reserve_params{
	struct timespec period;
	struct timespec execution_time;
	struct timespec zs_instant;
	struct timespec response_time_instant;
	long normal_marginal_utility;
	long overloaded_marginal_utility;
	int num_degraded_modes;
	int critical_util_degraded_mode;
	int enforcement_mask;
	int criticality;
	int priority;
	long degraded_marginal_utility[MAX_DEGRADED_MODES][2];
	struct timespec degraded_period[MAX_DEGRADED_MODES];
	int degraded_priority[MAX_DEGRADED_MODES];
        int reserve_type;
};

struct zs_mode_transition{
  int from_mode;
  int to_mode;  
  struct timespec zs_instant;
};

struct zs_modal_transition_entry{
  int mrid;
  int modal_transition_id;
  int in_use;
};

#ifdef __KERNEL__

struct zs_reserve {
	int pid;
        int parent_modal_reserve_id;
	int effective_priority;
	int request_stop;
	struct hrtimer period_timer;
	struct hrtimer zs_timer;
	struct hrtimer response_time_timer;
	int in_critical_mode;
	int critical_utility_mode_enforced;
	int current_degraded_mode;
	int just_returned_from_degradation;
	struct zs_reserve_params params;
	struct timespec start_of_period;
};

struct zs_modal_reserve{
  int pid;
  int in_use;
  int in_transition;
  // in transition will use the timer of the source mode
  int active_transition;
  int mode_change_pending;
  int mode;
  int num_modes;
  int reserve_for_mode[MAX_RESERVES];
  int num_transitions;
  struct zs_mode_transition transitions[MAX_TRANSITIONS];
};

#endif

struct attach_api{
	int reserveid;
	int pid;
};

struct add_reserve_to_mode_api{
  int modal_reserve_id;
  int reserve_id;
  int mode_id;
};

struct add_mode_transition_api{
  int modal_reserve_id;
  int transition_id;
  struct zs_mode_transition transition;
};

struct set_initial_mode_modal_reserve_api {
  int modal_reserve_id;
  int initial_mode_id;
};

struct add_transition_to_sys_transition_api{
  int sys_transition_id;
  int mrid;
  int transition_id;
};

struct mode_switch_api{
  int transition_id;
};

struct api_call{
  int api_id;
  union {
    int reserveid;
    int num_modes;
    struct attach_api attach_params;
    struct zs_reserve_params reserve_parameters;
    struct add_reserve_to_mode_api rsv_to_mode_params;
    struct add_mode_transition_api add_mode_transition_params;
    struct mode_switch_api mode_switch_params;
    struct add_transition_to_sys_transition_api trans_to_sys_trans_params;
    struct set_initial_mode_modal_reserve_api set_initial_mode_params;
  }args;
};

// library calls signatures
int zs_open_sched(void);
int zs_close_sched(int fd);
int zs_create_reserve(int fd, struct zs_reserve_params *p);
int zs_create_modal_reserve(int fid, int num_modes);
int zs_add_reserve_to_mode(int fid, int modal_reserve_id, int mode_id, int rid);
int zs_add_mode_transition(int fid, int modal_reserve_id, int transition_id, struct zs_mode_transition *p);
int zs_attach_modal_reserve(int fid, int mrid, int pid);
int zs_detach_modal_reserve(int fid, int mrid);
int zs_delete_modal_reserve(int fid, int mrid);
int zs_create_sys_transition(int fid);
int zs_add_transition_to_sys_transition(int fid, int sys_transition_id, int mrid, int transitionid);
int zs_mode_switch(int fid, int sys_transition_id);
int zs_attach_reserve(int fd, int rid, int pid);
int zs_detach_reserve(int fd, int rid);
int zs_delete_reserve(int fd, int rid);
int zs_wait_next_period(int fd, int rid);
int zs_get_jiffies_ms(int fd);
int zs_add_reserve_to_mode(int fid, int mrid, int mode, int rid);
int zs_print_stats(int fd);

#endif
