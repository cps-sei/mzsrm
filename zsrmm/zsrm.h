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

#ifndef __ZSRM_H__
#define __ZSRM_H__


#ifdef __KERNEL__
#include <linux/time.h>
#else
#include <sys/time.h>
#endif


#define DAEMON_PRIORITY 50;
// calls
#define CREATE_RESERVE 1
#define ATTACH_RESERVE 2
#define DETACH_RESERVE 3
#define DELETE_RESERVE 4
#define MODAL_WAIT_NEXT_PERIOD 5
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

// execution status
// a reserve can be in the following combined status:
// 
// EXEC_WAITING_NEXT_PERIOD & EXEC_ENFORCED_PAUSED
// EXEC_WAITING_NEXT_PERIOD & EXEC_ENFORCED_DROPPED
// EXEC_RUNNING & EXEC_ENFORCED_PAUSED
//
// EXEC_RUNNING & EXEC_ENFORCED_DROPPED does not exist because
// a dropped job resumes as EXEC_WAITING_NEXT_PERIOD
//
#define EXEC_BEFORE_START 0
#define EXEC_WAIT_NEXT_PERIOD 1
#define EXEC_RUNNING 2
#define EXEC_ENFORCED_PAUSED 4
#define EXEC_ENFORCED_DROPPED 8

#define WAIT_NEXT_PERIOD 20

#define RAISE_PRIORITY_CRITICALITY 21
#define RESTORE_BASE_PRIORITY_CRITICALITY 22

#define WAIT_NEXT_ARRIVAL 23

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


/* RESERVE TYPE DEFINITIONS
 * Types of reserves. Can be Utility / Criticality with
 * periodic or aperiodic arrival
 * The reserve type combines these types
 */
// Types of reserves
#define CRITICALITY_RESERVE 0
#define UTILITY_RESERVE 2
// Arrival type
#define APERIODIC_ARRIVAL 4 // bit == 0 => periodic

/* END OF RESERVE TYPE DEFINITIONS
 */

#define TIMESPEC2NS(ts) (((unsigned long long) (ts)->tv_sec) * 1000000000ll + (unsigned long long) (ts)->tv_nsec)

#define GET_EFFECTIVE_UTILITY(i) (reserve_table[i].current_degraded_mode == -1 ? reserve_table[i].params.overloaded_marginal_utility: reserve_table[i].params.degraded_marginal_utility[reserve_table[i].current_degraded_mode][1])

struct zs_reserve_params{
  struct timespec period;
  struct timespec execution_time;
  struct timespec overload_execution_time;
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
  int rid;
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
  // Vars for PCCP -- not compatible with
  // modal reserves!!
  int base_priority;
  int base_criticality;
  int execution_status;
  unsigned long long nominal_exectime_nanos;
  unsigned long long overload_exectime_nanos;
  unsigned long long job_executing_nanos;
  unsigned long long job_resumed_timestamp_nanos;
  struct zs_reserve *next_lower_priority;
  struct zs_reserve *next_paused_lower_criticality;
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

struct task_struct *gettask(int pid);
enum hrtimer_restart zs_instant_handler(struct hrtimer *timer);
enum hrtimer_restart period_handler(struct hrtimer *timer);
int attach_reserve(int rid, int pid);
int find_empty_entry(void);
int wait_for_next_period(int rid);
int modal_wait_for_next_period(int mrid);
int delete_reserve(int rid);
int detach_reserve(int rid);
void init(void);
int create_reserve(int rid);
void period_degradation(int rid);
void try_resume_normal_period(int rid);
int init_sys_mode_transitions(void);
int create_sys_mode_transition(void);
int delete_sys_mode_transition(int stid);
int add_transition_to_sys_transition(int stid, int mrid, int mtid);
int sys_mode_transition(int stid);
int mode_transition(int mrid, int tid);
int create_modal_reserve(int);
int delete_modal_reserve(int mrid);
int detach_modal_reserve(int mrid);
int attach_reserve_transitional(int rid, int pid, struct timespec *trans_zs_instant);
int print_stats(void);
unsigned long long ticks2nanos(unsigned long long ticks);
void test_ticks(void);
unsigned long long timestamp_ns(void);
void calibrate_ticks(void);
static inline unsigned long long rdtsc(void);
int raise_priority_criticality(int rid, int priority_ceiling, int criticality_ceiling);
int restore_base_priority_criticality(int rid);
static void zsrm_cleanup_module(void);

#define UPDATE_HIGHEST_PRIORITY_TASK 1
#define DONT_UPDATE_HIGHEST_PRIORITY_TASK 0

void stop_accounting(struct zs_reserve *rsv, int update_highest_priority);
void start_accounting(struct zs_reserve *rsv);

void test_accounting(void);

void add_paused_reserve(struct zs_reserve *rsv);
void del_paused_reserve(struct zs_reserve *rsv);
void kill_paused_reserves(int criticality);
void resume_paused_reserves(int criticality);
void kill_reserve(struct zs_reserve *rsv);
void pause_reserve(struct zs_reserve *rsv);
void resume_reserve(struct zs_reserve *rsv);
int push_to_reschedule(int i);
int pop_to_reschedule(void);

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

struct raise_priority_criticality_api{
  int priority_ceiling;
  int criticality_ceiling;
};

struct wait_next_arrival_api{
  int reserveid;
  int wfd;
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
    struct raise_priority_criticality_api raise_priority_criticality_params;
    struct wait_next_arrival_api wait_next_arrival_params;
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
int zs_modal_wait_next_period(int fd, int mrid);
int zs_wait_next_period(int fd, int rid);
int zs_wait_next_arrival(int fd, int rid, int wfd);
int zs_get_jiffies_ms(int fd);
int zs_add_reserve_to_mode(int fid, int mrid, int mode, int rid);
int zs_print_stats(int fd);

int zs_raise_priority_criticality(int fid, int priority_ceiling,int criticality_ceiling);
int zs_restore_base_priority_criticality(int fid);
#endif
