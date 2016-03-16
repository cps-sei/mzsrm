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

// change for test

#include <linux/unistd.h>
#include <linux/math64.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/fdtable.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/nsproxy.h>
#include <linux/sched.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <asm/siginfo.h>
#include <linux/semaphore.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "zsrm.h"


//int (*do_sys_pollp)(struct pollfd __user *ufds, unsigned int nfds, struct timespec *end_time);
//int (*sys_recvfromp)(int fd, void __user *ubuf, size_t size, unsigned int flags, struct sockaddr __user *addr, int __user *addr_len);
//int (*sys_sendtop)(int fd, void __user *ubuf, size_t len, unsigned int flags, struct sockaddr __user *addr, int addr_len);

// for kernel 3.19
int (*do_sys_pollp)(struct pollfd __user *ufds, unsigned int nfds, struct timespec *end_time);
long (*sys_recvfromp)(int fd, void __user *ubuf, size_t size, unsigned flags, struct sockaddr __user *addr, int __user *addr_len);
long (*sys_sendtop)(int fd, void __user *ubuf, size_t len, unsigned flags, struct sockaddr __user *addr, int addr_len);

MODULE_AUTHOR("Dionisio de Niz");
MODULE_LICENSE("GPL");

// some configuration options -- transform into CONFIG_*
#define __ZSRM_TSC_CLOCK__

#define DEVICE_NAME "zsrmm"

/*******************************
 ********** DEBUGGING **********
 *******************************
 */

#define ZSRM_DEBUG_TRACE_SIZE 1000

unsigned int zsrm_debug_trace_read_index=0;
unsigned int zsrm_debug_trace_write_index=0;
struct zsrm_debug_trace_record zsrm_debug_trace[ZSRM_DEBUG_TRACE_SIZE];

void zsrm_add_debug_event(unsigned long long ts, unsigned int event, int param){
  if (zsrm_debug_trace_write_index < ZSRM_DEBUG_TRACE_SIZE){
    zsrm_debug_trace[zsrm_debug_trace_write_index].timestamp=ts;
    zsrm_debug_trace[zsrm_debug_trace_write_index].event_type=event;
    zsrm_debug_trace[zsrm_debug_trace_write_index].event_param=param;
    zsrm_debug_trace_write_index++;
  }
}

struct zsrm_debug_trace_record * zsrm_next_debug_event(){
  if (zsrm_debug_trace_read_index < zsrm_debug_trace_write_index){
    return &(zsrm_debug_trace[zsrm_debug_trace_read_index++]);
  } else {
    return NULL;
  }
}

/*******************************
 ********** END DEBUGGING ******
 *******************************
 */

#ifdef __ARMCORE__

#define PMCNTNSET_C_BIT		0x80000000
#define PMCR_D_BIT		0x00000008
#define PMCR_C_BIT		0x00000004
#define PMCR_P_BIT		0x00000002
#define PMCR_E_BIT		0x00000001

#endif


int scheduler_priority = 50;

/* device number. */
static dev_t dev_id;
static unsigned int dev_major;
static struct class *dev_class = NULL;

/* char device structure. */
static struct cdev c_dev;

static struct hrtimer ht;
int timer_started = 0;

static DEFINE_SPINLOCK(zsrmlock);
static DEFINE_SPINLOCK(reschedule_lock);
struct semaphore zsrmsem;

/*******************
 * Variable to track if system has modal reserves
 * If it does. It is assume that all the reserves
 * should be modal. 
 * Otherwise the scheduler will not work.
 *
 * We will detect what kind of reserve was attach to a process
 * The first attachment determines the 'session' modal/non-modal 
 * character.
 * A session start at initialization (loading) of the module 
 * and ends at unloading of the module
 *******************/

int modal_scheduling  = 0; // NO by default

struct zs_reserve *active_reserve_ptr[MAX_CPUS];
struct zs_reserve *head_paused_reserve_ptr[MAX_CPUS];

struct hrtimer exectime_enforcer_timers[MAX_CPUS];

struct task_struct *sched_task;

//timestamps
unsigned long long arrival_ts_ns=0LL;
unsigned long long mode_change_ts_ns=0LL;
unsigned long long no_mode_change_ts_ns=0LL;
unsigned long long request_mode_change_ts_ns=0LL;
unsigned long long mode_change_signals_sent_ts_ns=0LL;
unsigned long long trans_into_susp_ts_ns = 0LL;
unsigned long long trans_from_susp_ts_ns = 0LL;
unsigned long long start_of_enforcement_ts_ns =0LL;
unsigned long long end_of_enforcement_ts_ns = 0LL;
unsigned long long start_of_defer_ts_ns = 0LL;
unsigned long long end_of_defer_ts_ns = 0LL;
unsigned long long start_of_resume_ts_ns=0LL;
unsigned long long end_of_resume_ts_ns=0LL;
unsigned long long start_of_net_arrival_ts_ns=0LL;
unsigned long long end_of_net_arrival_ts_ns=0LL;

// measurements
unsigned long long mode_change_latency_ns_avg=0LL;
unsigned long long mode_change_latency_ns_worst=0LL;
unsigned long long arrival_no_mode_ns_avg=0LL;
unsigned long long arrival_no_mode_ns_worst=0LL;
unsigned long long arrival_mode_ns_avg=0LL;
unsigned long long arrival_mode_ns_worst=0LL;
unsigned long long transition_into_susp_ns_avg=0LL;
unsigned long long transition_into_susp_ns_worst=0LL;
unsigned long long transition_from_susp_ns_avg=0LL;
unsigned long long transition_from_susp_ns_worst=0LL;
unsigned long long enforcement_latency_ns_avg=0LL;
unsigned long long enforcement_latency_ns_worst=0LL;
unsigned long long defer_latency_ns_avg=0LL;
unsigned long long defer_latency_ns_worst=0LL;
unsigned long long resume_latency_ns_avg =0LL;
unsigned long long resume_latency_ns_worst=0LL;
unsigned long long net_arrival_latency_ns_avg = 0LL;
unsigned long long net_arrival_latency_ns_worst = 0LL;

struct zs_reserve reserve_table[MAX_RESERVES];
struct zs_modal_reserve modal_reserve_table[MAX_MODAL_RESERVES];
struct zs_modal_transition_entry sys_mode_transition_table[MAX_SYS_MODE_TRANSITIONS][MAX_TRANSITIONS_IN_SYS_TRANSITIONS];

#define now2timespec(ts) getnstimeofday(ts)
/* #ifdef __ZSRM_TSC_CLOCK__ */
/* #define now2timespec(ts) nanos2timespec(ticks2nanos(rdtsc()),ts) */
/* #else */
/* #define now2timespec(ts) jiffies_to_timespec(jiffies,ts) */
/* #endif */

#ifdef __ARMCORE__

static inline void start_tsc(void){
	unsigned long tmp;

	tmp = PMCNTNSET_C_BIT;
	asm volatile ("mcr p15, 0, %0, c9, c12, 1" : : "r" (tmp));


	asm volatile ("mrc p15, 0, %0, c9, c12, 0" : "=r" (tmp));
	tmp |= PMCR_C_BIT | PMCR_E_BIT;
	asm volatile ("mcr p15, 0, %0, c9, c12, 0" : : "r" (tmp));
}

static inline unsigned long long rdtsc(void){
	unsigned long result;
	asm volatile ("mrc p15, 0, %0, c9, c13, 0" : "=r" (result));
	return (unsigned long long) result;
}

#else

static inline unsigned long long rdtsc(){
  unsigned int hi, lo;
  __asm__ volatile ("rdtsc" : "=a" (lo), "=d" (hi) : :);
  return ((unsigned long long) hi << 32) | lo;
}

#endif

unsigned long nanosPerTenTicks;

unsigned long long timestamp_ns(){
  struct timespec ts;

  getnstimeofday(&ts);

  return (((unsigned long long)ts.tv_sec) * 1000000000L) + (unsigned long long) ts.tv_nsec;
}

void calibrate_ticks(){
  unsigned long long begin=0,end=0;
#ifdef __ARMCORE__
  unsigned long elapsed10s;
#endif
  unsigned long waituntil;
  struct timespec begints, endts;
  unsigned long long diffts;

  getnstimeofday(&begints);
  begin = rdtsc();
  waituntil = jiffies + (HZ/4);
  while (time_before(jiffies, waituntil))
    ;
  end = rdtsc();
  getnstimeofday(&endts);
  printk("zsrm.init: begin[%llu] end[%llu] begints[%lu] endts[%lu]\n",begin, end, begints.tv_nsec, endts.tv_nsec);
  diffts = ((endts.tv_sec * 1000000000)+ endts.tv_nsec) -
    ((begints.tv_sec * 1000000000) + begints.tv_nsec);
  end = end-begin;
  printk("zsrm.init: elapse ns (%llu)\n",diffts);
  printk("zsrm.init: elapse tk (%llu)\n",end);
#ifdef __ARMCORE__
  do_div(end,10);
  elapsed10s = (unsigned long) end;
  do_div(diffts,elapsed10s);
  nanosPerTenTicks = diffts;
#else
  end /=10;
  nanosPerTenTicks = diffts / end;
#endif

}

// this will automatically truncate the result
unsigned long long ticks2nanos(unsigned long long ticks){
  unsigned long long tmp =(unsigned long long) (ticks * nanosPerTenTicks);
#ifdef __ARMCORE__
  do_div(tmp,10);
#else
  tmp = tmp / 10;
#endif
  return tmp;
}

void nanos2timespec(unsigned long long ns, struct timespec *ts){
#ifdef __ARMCORE__
  unsigned long long tmp = ns;
  do_div(tmp,1000000000);
  ts->tv_sec = tmp;
  tmp = ns;
  do_mod(tmp,1000000000);
  ts->tv_nsec = tmp;
#else
  ts->tv_sec = ns / 1000000000L;
  ts->tv_nsec = ns % 1000000000L;
#endif
}


void test_ticks(){
  unsigned long long begin=0,end=0;
  unsigned long waituntil;
  struct timespec begints, endts;
  unsigned long long diffts;

  getnstimeofday(&begints);
  begin = rdtsc();
  waituntil = jiffies + (HZ/4);
  while (time_before(jiffies, waituntil))
    ;
  end = rdtsc();
  getnstimeofday(&endts);
  diffts = ((endts.tv_sec * 1000000000)+ endts.tv_nsec) -
    ((begints.tv_sec * 1000000000) + begints.tv_nsec);
  end = end-begin;

  end = ticks2nanos(end);

  printk("MZSRMM: tfd   nanos : %llu\n",diffts);
  printk("MZSRMM: ticks nanos : %llu\n",end);
}

int print_stats(){
  printk("Nanos per Ten ticks: %lu\n", nanosPerTenTicks);

  // comment out the mode change
  /* printk("avg mode change latency: %llu (ns) \n",mode_change_latency_ns_avg ); */
  /* printk("worst mode change latency: %llu (ns) \n",mode_change_latency_ns_worst); */
  printk("avg arrival : %llu (ns) \n",arrival_no_mode_ns_avg);
  printk("worst arrival: %llu (ns) \n", arrival_no_mode_ns_worst);
  /* printk("avg arrival mode change %llu (ns) \n",arrival_mode_ns_avg); */
  /* printk("worst arrival mode change %llu (ns) \n",arrival_mode_ns_worst); */
  /* printk("avg transition into suspension %llu (ns) \n",transition_into_susp_ns_avg); */
  /* printk("worst transition into suspension %llu (ns) \n",transition_into_susp_ns_worst); */
  /* printk("avg transition from suspension %llu (ns)\n",transition_from_susp_ns_avg); */
  /* printk("worst transition_from_susp_ns_worst %llu (ns) \n",transition_from_susp_ns_worst); */
  printk("avg enforcement latency: %llu (ns) \n",enforcement_latency_ns_avg);
  printk("worst enforcement latency: %llu (ns) \n",enforcement_latency_ns_worst);
  printk("avg defer latency: %llu (ns) \n",defer_latency_ns_avg);
  printk("worst defer latency: %llu (ns)\n",defer_latency_ns_worst);
  printk("avg resume latency: %llu (ns)\n",resume_latency_ns_avg);
  printk("worst resume latency: %llu (ns)\n",resume_latency_ns_worst);
  printk("avg net arrival latency: %llu (ns)\n",net_arrival_latency_ns_avg);
  printk("worst net arrival latency: %llu (ns)\n",net_arrival_latency_ns_worst);
  return 0;
}

void add_paused_reserve(struct zs_reserve *rsv){
  struct zs_reserve *tmp;
  if (head_paused_reserve_ptr[rsv->params.bound_to_cpu] == NULL){
    rsv->next_paused_lower_criticality = NULL;
    head_paused_reserve_ptr[rsv->params.bound_to_cpu] = rsv;
  } else if (head_paused_reserve_ptr[rsv->params.bound_to_cpu]->params.overloaded_marginal_utility < rsv->params.overloaded_marginal_utility) {
    rsv->next_paused_lower_criticality = head_paused_reserve_ptr[rsv->params.bound_to_cpu];
    head_paused_reserve_ptr[rsv->params.bound_to_cpu] = rsv;
  } else {
    tmp = head_paused_reserve_ptr[rsv->params.bound_to_cpu];
    while (tmp->next_paused_lower_criticality != NULL && 
	   tmp->next_paused_lower_criticality->params.overloaded_marginal_utility > rsv->params.overloaded_marginal_utility){
      tmp = tmp->next_paused_lower_criticality;
    }
    rsv->next_paused_lower_criticality = tmp->next_paused_lower_criticality;
    tmp->next_paused_lower_criticality = rsv;
  }
}

void del_paused_reserve(struct zs_reserve *rsv){
  struct zs_reserve *tmp;

  if (head_paused_reserve_ptr[rsv->params.bound_to_cpu] == rsv){
    head_paused_reserve_ptr[rsv->params.bound_to_cpu] = head_paused_reserve_ptr[rsv->params.bound_to_cpu]->next_paused_lower_criticality;
    rsv->next_paused_lower_criticality = NULL;
  } else {
    tmp = head_paused_reserve_ptr[rsv->params.bound_to_cpu];
    while(tmp != NULL && 
	  tmp->next_paused_lower_criticality != rsv){
      tmp = tmp->next_paused_lower_criticality;
    }
    tmp =rsv->next_paused_lower_criticality;
    rsv->next_paused_lower_criticality = NULL;
  }
}

void kill_paused_reserves(int criticality, int cpuid){
  struct zs_reserve *tmp = head_paused_reserve_ptr[cpuid];
  struct zs_reserve *prev = NULL;

  while(tmp != NULL && tmp->params.overloaded_marginal_utility >= criticality){
    tmp = tmp->next_paused_lower_criticality;
  }

  while (tmp != NULL){
    kill_reserve(tmp,0);
    prev = tmp;
    if (tmp == head_paused_reserve_ptr[cpuid])
      head_paused_reserve_ptr[cpuid] = tmp->next_paused_lower_criticality;
    tmp=tmp->next_paused_lower_criticality;
    prev->next_paused_lower_criticality = NULL;
  }
}

void resume_paused_reserves(int criticality, int cpuid){
  struct zs_reserve *tmp = head_paused_reserve_ptr[cpuid];
  struct zs_reserve *prev = NULL;

  while(tmp != NULL && tmp->params.overloaded_marginal_utility >= criticality){
    resume_reserve(tmp);
    prev = tmp;
    if (head_paused_reserve_ptr[cpuid] == tmp)
      head_paused_reserve_ptr[cpuid] = tmp->next_paused_lower_criticality;
    tmp = tmp->next_paused_lower_criticality;
    prev->next_paused_lower_criticality = NULL;
  }
}

// kill always happens on a paused reserve once the overloading
// job finishes, so the next job arrival revives the reserve
void kill_reserve(struct zs_reserve *rsv, int in_interrupt_context){
  struct task_struct *task;
  struct sched_param p;
  //char *type = (rsv->params.reserve_type & APERIODIC_ARRIVAL?"APERIODIC":"PERIODIC");
  task = gettask(rsv->pid);
  if (task != NULL){
    // restore priority
    if (!in_interrupt_context){
      if (rsv->params.enforcement_mask & ZS_ENFORCEMENT_HARD_MASK){
	rsv->effective_priority = 0;
	p.sched_priority = rsv->effective_priority;
	sched_setscheduler(task, SCHED_FIFO, &p);    
	// set task state not reliable -- HARD enforcement broken for now
	//set_task_state(task, TASK_INTERRUPTIBLE);
      } else { // ZS_ENFORCEMENT_SOFT_MASK -- DEFAULT
	rsv->effective_priority = 0;
	p.sched_priority = 0;
	sched_setscheduler(task, SCHED_NORMAL, &p);    
      }
      zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_ZS_DEFER,rsv->rid);
      // debug -- perhaps we don't need this resched
      //set_tsk_need_resched(task);
      stop_accounting(rsv,UPDATE_HIGHEST_PRIORITY_TASK,0,0);
    } else {
      rsv->request_stop = REQUEST_STOP_DEFER; 
      push_to_reschedule(rsv->rid);
      wake_up_process(sched_task);
    }
    //debug -- perhaps we don't need this resched
    // set_tsk_need_resched(task);

    if (start_of_defer_ts_ns != 0){
      end_of_defer_ts_ns = timestamp_ns();
      end_of_defer_ts_ns -= start_of_defer_ts_ns;
      if (defer_latency_ns_avg == 0)
	defer_latency_ns_avg = end_of_defer_ts_ns;
      else{
#ifdef __ARMCORE__
	defer_latency_ns_avg = defer_latency_ns_avg + end_of_defer_ts_ns;
	do_div(defer_latency_ns_avg,2);
#else
	defer_latency_ns_avg = (defer_latency_ns_avg + end_of_defer_ts_ns) / 2;
#endif
	if (end_of_defer_ts_ns > defer_latency_ns_worst){
	  defer_latency_ns_worst = end_of_defer_ts_ns;
	}
	start_of_defer_ts_ns = 0L;
      }
    }
    // jumps directly to wait for next period 
    // CHECK if we can change this to DROPPED WITHOUT LOCKS
    rsv->execution_status = EXEC_WAIT_NEXT_PERIOD | EXEC_ENFORCED_DEFERRED;
    rsv->critical_utility_mode_enforced=0;
    if (!in_interrupt_context){
      // try limiting this to not from interrupt.
      /* --- cancel all timers --- */
      if (rsv->params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
	hrtimer_cancel(&(rsv->response_time_timer));
      }
      // this is already cancelled within the interrupt
      hrtimer_cancel(&(rsv->zs_timer));
    }
  }
}

void pause_reserve(struct zs_reserve *rsv){
  add_paused_reserve(rsv);
  // pipelines : kernel-dis DIFF: if commented out in kernel-dis -- uncommenting
  if (rsv->execution_status == EXEC_RUNNING){
    // stop the task
    rsv->critical_utility_mode_enforced = 1;
    rsv->just_returned_from_degradation=0;
    rsv->request_stop = REQUEST_STOP_PAUSE;
    push_to_reschedule(rsv->rid);
  } else {
    start_of_enforcement_ts_ns = 0L;
  }
  rsv->execution_status |= EXEC_ENFORCED_PAUSED;
}

void resume_reserve(struct zs_reserve *rsv){
  struct task_struct *task;
  
  if (rsv->execution_status & EXEC_RUNNING){
    rsv->execution_status = EXEC_RUNNING;
    task = gettask(rsv->pid);
    
    if (task != NULL && (task->state & TASK_INTERRUPTIBLE ||
			 task->state & TASK_UNINTERRUPTIBLE)){
      wake_up_process(task);
      
    }
    
    // reschedule again
    rsv->effective_priority = rsv->params.priority;
    push_to_reschedule(rsv->rid);
    rsv->current_degraded_mode = -1;
    rsv->just_returned_from_degradation=1;
    // this is taken care of at the scheduler_task level
    //rsv->critical_utility_mode_enforced = 0;
    zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_RESUME_NOW,rsv->rid);
  } else if (rsv->execution_status & EXEC_WAIT_NEXT_PERIOD){
    rsv->execution_status = EXEC_WAIT_NEXT_PERIOD;
    zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_RESUME_NEXT_PERIOD,rsv->rid);
  } 
}

void start_accounting(struct zs_reserve *rsv){
  if (active_reserve_ptr[rsv->params.bound_to_cpu] == rsv){
    // only start accounting
    rsv->job_resumed_timestamp_nanos = timestamp_ns();
    zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_START_ACCT, rsv->rid);
    if ((rsv->params.enforcement_mask & ZS_ENFORCE_OVERLOAD_BUDGET_MASK) &&
	!(rsv->execution_status & EXEC_ENFORCED_DEFERRED)){
      exectime_enforcer_timer_start(rsv);
    }
  } else if (active_reserve_ptr[rsv->params.bound_to_cpu] == NULL){
    active_reserve_ptr[rsv->params.bound_to_cpu] = rsv;
    rsv->next_lower_priority = NULL;
    rsv->job_resumed_timestamp_nanos = timestamp_ns();
    zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_START_ACCT, rsv->rid);
    if ((rsv->params.enforcement_mask & ZS_ENFORCE_OVERLOAD_BUDGET_MASK) &&
	!(rsv->execution_status & EXEC_ENFORCED_DEFERRED)){
      exectime_enforcer_timer_start(rsv);
    }
  } else if (active_reserve_ptr[rsv->params.bound_to_cpu]->effective_priority < rsv->effective_priority){
    stop_accounting(active_reserve_ptr[rsv->params.bound_to_cpu], 
		    DONT_UPDATE_HIGHEST_PRIORITY_TASK,0,0);
    rsv->next_lower_priority = active_reserve_ptr[rsv->params.bound_to_cpu];
    active_reserve_ptr[rsv->params.bound_to_cpu] = rsv;
    rsv->job_resumed_timestamp_nanos = timestamp_ns();
    zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_START_ACCT, rsv->rid);
    if ((rsv->params.enforcement_mask & ZS_ENFORCE_OVERLOAD_BUDGET_MASK) &&
	!(rsv->execution_status & EXEC_ENFORCED_DEFERRED)){    
      exectime_enforcer_timer_start(rsv);
    }
  } else{
    struct zs_reserve *tmp = active_reserve_ptr[rsv->params.bound_to_cpu];
    while (tmp->next_lower_priority != NULL && 
	   tmp->next_lower_priority->effective_priority >= rsv->effective_priority)
      tmp = tmp->next_lower_priority;
    rsv->next_lower_priority = tmp->next_lower_priority;
    tmp->next_lower_priority = rsv;
  }
}

void stop_accounting(struct zs_reserve *rsv, int update_active_highest_priority,int in_enforcer_handler, int swap_task){
  unsigned long long job_stop_timestamp_nanos,old_job_executing_nanos;
  zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_STOP_ACCT, rsv->rid);

  if (active_reserve_ptr[rsv->params.bound_to_cpu] == rsv){
    job_stop_timestamp_nanos = timestamp_ns();//ticks2nanos(rdtsc());
    old_job_executing_nanos = rsv->job_executing_nanos;
    rsv->job_executing_nanos += (job_stop_timestamp_nanos - rsv->job_resumed_timestamp_nanos);
    if (!in_enforcer_handler){
      if (rsv->params.enforcement_mask & ZS_ENFORCE_OVERLOAD_BUDGET_MASK){
	exectime_enforcer_timer_stop(rsv);
      }
    }
    if (update_active_highest_priority){
      active_reserve_ptr[rsv->params.bound_to_cpu] = rsv->next_lower_priority;
      if (active_reserve_ptr[rsv->params.bound_to_cpu] != NULL){
	start_accounting(active_reserve_ptr[rsv->params.bound_to_cpu]);
      } 
      // when active == NULL it will restart the same task -- calling start_accounting reselects this
      // or puts it in the right priority order if the priority was demoted.
      // should be the last thing this function should do
      if (swap_task){
	start_accounting(rsv);
      }
    }
  } else {
    if (active_reserve_ptr[rsv->params.bound_to_cpu] != NULL){
      struct zs_reserve *tmp = active_reserve_ptr[rsv->params.bound_to_cpu];
      // just remove it from queue
      while(tmp->next_lower_priority != NULL && tmp->next_lower_priority != rsv){
	tmp = tmp->next_lower_priority;
      }
      if (tmp->next_lower_priority == rsv){
	tmp->next_lower_priority = rsv->next_lower_priority;
	rsv->next_lower_priority = NULL;
      }
    } else {
      printk("zsrm.stop_acct:  ERROR top == NULL expecting at least rsv(%d)\n",rsv->rid);
    }
  }
}

int init_sys_mode_transitions(){
  int i,j;
  for (i=0;i<MAX_SYS_MODE_TRANSITIONS;i++){
    for (j=0;j<MAX_TRANSITIONS_IN_SYS_TRANSITIONS;j++){
      sys_mode_transition_table[i][j].mrid=-1;
      sys_mode_transition_table[i][j].modal_transition_id=-1;
      sys_mode_transition_table[i][j].in_use=0;
    }
  }
  return 0;
}

int create_sys_mode_transition(){
  int i,j;
  for (i=0;i<MAX_SYS_MODE_TRANSITIONS;i++){
    if (sys_mode_transition_table[i][0].in_use==0){
      for (j=0;j<MAX_TRANSITIONS_IN_SYS_TRANSITIONS;j++){
	sys_mode_transition_table[i][j].mrid=-1;
	sys_mode_transition_table[i][j].modal_transition_id = -1;
	sys_mode_transition_table[i][j].in_use=1;
      }
      return i;
    }
  }
  return -1;
}

int delete_sys_mode_transition(int stid){
  int i;
  sys_mode_transition_table[stid][0].in_use=0;
  for (i=0;i<MAX_TRANSITIONS_IN_SYS_TRANSITIONS;i++){
    sys_mode_transition_table[stid][i].in_use=0;
    sys_mode_transition_table[stid][i].modal_transition_id=-1;
    sys_mode_transition_table[stid][i].mrid=-1;
  }
  return 0;
}

int add_transition_to_sys_transition(int stid, int mrid, int mtid){
  int i;
  for (i=0;i<MAX_TRANSITIONS_IN_SYS_TRANSITIONS;i++){
    if (sys_mode_transition_table[stid][i].mrid == -1){
      sys_mode_transition_table[stid][i].mrid = mrid;
      sys_mode_transition_table[stid][i].modal_transition_id = mtid;
      return 0;
    }
  }
  return -1;
}

int send_mode_change_signal(struct task_struct *task, int newmode){
  struct siginfo info;
  
  info.si_signo = MODE_CHANGE_SIGNAL;
  info.si_value.sival_int = newmode;
  info.si_errno = 0; // no recovery?
  info.si_code = SI_QUEUE;

  if (send_sig_info(MODE_CHANGE_SIGNAL, &info, task)<0){
    printk("error sending mode change signal\n");
    return -1;
  }

  return 0;
}

unsigned long long latest_deadline_ns=0ll;
unsigned long long end_of_transition_interval_ns=0ll;

int sys_mode_transition(int stid){
  int i;

  // record the latest deadline among the running jobs as the end of transition.
  end_of_transition_interval_ns = latest_deadline_ns;

  for (i=0;i<MAX_TRANSITIONS_IN_SYS_TRANSITIONS;i++){
    if (sys_mode_transition_table[stid][i].mrid == -1){
      break;
    }
    mode_transition(sys_mode_transition_table[stid][i].mrid,
		    sys_mode_transition_table[stid][i].modal_transition_id);
  }
  return 0;
}

long system_utility=0;

int reschedule_stack[MAX_RESERVES];
int top=-1;

struct task_struct *arrival_task=NULL;

int fd2rid(int fd){
  int i;

  for (i=0;i<MAX_RESERVES;i++){
    if (reserve_table[i].params.priority == -1)
      continue;
    if (reserve_table[i].params.insockfd == fd){
      return i;
    }
  }

  return -1;
}

static void arrival_server_task(void *a){
  int i;
  struct pollfd *fds ;
  struct arrival_server_task_params *args;
  

  args = (struct arrival_server_task_params *) a;

  fds = kmalloc(sizeof(struct pollfd)*args->nfds,GFP_KERNEL);

  while (!kthread_should_stop()) {
      do_sys_pollp(args->fds, args->nfds, NULL);
      if (!copy_from_user(&fds, args->fds,sizeof(struct pollfd)*args->nfds)){
	for (i=0;i<args->nfds;i++){
	  if (fds[i].revents & POLLRDNORM){
	    // wakeup the process. For now just print
	    printk("zsrmm: fd(%d) rid(%d) event\n",fds[i].fd,fd2rid(fds[i].fd));
	  }
	}
      } else {
	printk("zsrmm:arrival_server: could not copy fds from user space\n");
      }
  }

  kfree(fds);
}

struct task_struct *gettask(int pid){
  struct task_struct *tsk;
  struct pid *pid_struct;
  pid_struct = find_get_pid(pid);
  tsk = pid_task(pid_struct,PIDTYPE_PID);
  return tsk;
}

int push_to_reschedule(int i){
  int ret;
  unsigned long flags;
  spin_lock_irqsave(&reschedule_lock,flags);
  if ((top+1) < MAX_RESERVES){
    top++;
    reschedule_stack[top]=i;
    ret =0;
  } else	
    ret=-1;
  
  spin_unlock_irqrestore(&reschedule_lock,flags);
  return ret;
}

int pop_to_reschedule(void){
  int ret;
  unsigned long flags;
  spin_lock_irqsave(&reschedule_lock,flags);
  if (top >=0){
    ret = reschedule_stack[top];
    top--;
  } else
    ret = -1;
  spin_unlock_irqrestore(&reschedule_lock,flags);
  return ret;
}

int rsv_to_stop[MAX_RESERVES];
int tostopidx=-1;

int push_to_stop(int i){
  if ((tostopidx+1) < MAX_RESERVES){
    tostopidx++;
    rsv_to_stop[tostopidx]=i;
  } else {
    return -1;
  }
  return 0;
}

int pop_to_stop(void){
  int ret;
  if (tostopidx >=0){
    ret = rsv_to_stop[tostopidx];
    tostopidx--;
  } else {
    ret = -1;
  }
  return ret;
}

static void scheduler_task(void *a){
  int rid;
  struct sched_param p;
  struct task_struct *task;
  unsigned long flags;
  int swap_task=0;
  
  while (!kthread_should_stop()) {

    // prevent concurrent execution with interrupts
    spin_lock_irqsave(&zsrmlock,flags);

    while ((rid = pop_to_reschedule()) != -1) {
      task = gettask(reserve_table[rid].pid);
      if (task == NULL){
	printk("scheduler_task : wrong pid(%d) for rid(%d)\n",reserve_table[rid].pid,rid);
	continue;
      }
      if (reserve_table[rid].request_stop){
	if (reserve_table[rid].params.enforcement_mask & ZS_ENFORCEMENT_HARD_MASK){
	  // setting task state is unreliable
	  //set_task_state(task, TASK_INTERRUPTIBLE);
	  //push_to_stop(rid);
	  //swap_task=0;
	  p.sched_priority = 0;
	  sched_setscheduler(task, SCHED_NORMAL, &p);
	  swap_task =1;
	  zsrm_add_debug_event(timestamp_ns(),
			       ZSRM_DEBUG_EVENT_JOB_INDIRECT_HARD_STOP,
			       rid);
	} else { // ZS_ENFORCEMENT_SOFT_MASK -- DEFAULT
	  p.sched_priority = 0;
	  sched_setscheduler(task, SCHED_NORMAL, &p);
	  swap_task =1;
	  zsrm_add_debug_event(timestamp_ns(),
			       ZSRM_DEBUG_EVENT_JOB_INDIRECT_SOFT_STOP,
			       rid);
	}
	// debug -- perhaps we don't need this resched
	// set_tsk_need_resched(task);

	reserve_table[rid].effective_priority = 0;
	stop_accounting(&reserve_table[rid],
			UPDATE_HIGHEST_PRIORITY_TASK,0,swap_task);
	if (reserve_table[rid].request_stop == REQUEST_STOP_DEFER){
	  // the job ended its execution
	  reserve_table[rid].job_executing_nanos = 0L;
	}
	reserve_table[rid].request_stop = 0;
	// measurement of enforcement
	if (start_of_enforcement_ts_ns != 0){
	  end_of_enforcement_ts_ns =  timestamp_ns();//ticks2nanos(rdtsc());
	  end_of_enforcement_ts_ns -= start_of_enforcement_ts_ns;
	  start_of_enforcement_ts_ns = 0L;
	  if (enforcement_latency_ns_avg == 0)
	    enforcement_latency_ns_avg = end_of_enforcement_ts_ns;
	  else{
#ifdef __ARMCORE__
	    enforcement_latency_ns_avg = enforcement_latency_ns_avg + end_of_enforcement_ts_ns;
	    do_div(enforcement_latency_ns_avg,2);
#else
	    enforcement_latency_ns_avg = (enforcement_latency_ns_avg + end_of_enforcement_ts_ns) / 2;
#endif			      
	  }
	  
	  if (end_of_enforcement_ts_ns > enforcement_latency_ns_worst)
	    enforcement_latency_ns_worst = end_of_enforcement_ts_ns;
	}
	// end of measurement code
      } else {
	if (reserve_table[rid].critical_utility_mode_enforced){
	  reserve_table[rid].critical_utility_mode_enforced = 0;
	  //wake_up_process(task);
	}

	p.sched_priority = reserve_table[rid].effective_priority;
	sched_setscheduler(task, SCHED_FIFO, &p);
	reserve_table[rid].execution_status = EXEC_RUNNING;
	start_accounting(&reserve_table[rid]);
	if (start_of_resume_ts_ns != 0){
	  end_of_resume_ts_ns =  timestamp_ns();
	  end_of_resume_ts_ns -= start_of_resume_ts_ns;
	  start_of_resume_ts_ns = 0L;
	  if (resume_latency_ns_avg == 0){
	    resume_latency_ns_avg = end_of_resume_ts_ns;
	  }else{
#ifdef __ARMCORE__
	    resume_latency_ns_avg = resume_latency_ns_avg + end_of_resume_ts_ns;
	    do_div(resume_latency_ns_avg,2);
#else
	    resume_latency_ns_avg = (resume_latency_ns_avg + end_of_resume_ts_ns) / 2;
#endif			      
	  }
	  
	  if (end_of_resume_ts_ns > resume_latency_ns_worst){
	    resume_latency_ns_worst = end_of_resume_ts_ns;
	  }
	}
      }
    }

    /* while ((rid = pop_to_stop()) != -1){ */
    /*   task = gettask(reserve_table[rid].pid); */
    /*   if (task != NULL){ */
    /* 	set_task_state(task, TASK_INTERRUPTIBLE); */
    /* 	schedule(); */
    /*   } */
    /* } */

    spin_unlock_irqrestore(&zsrmlock,flags);
    
    set_current_state(TASK_INTERRUPTIBLE);
    schedule();
  }
}


int pid2reserve(int pid){
  int i=0;
  for (i=0;i<MAX_RESERVES;i++){
    if (pid == reserve_table[i].pid)
      return i;
  }
  return -1;
}

int timer2reserve(struct hrtimer *tmr){
	int i=0;
	for (i=0;i<MAX_RESERVES;i++){
		if (tmr == &(reserve_table[i].period_timer))
			return i;
		if (tmr == &(reserve_table[i].zs_timer))
			return i;
		if (tmr == &(reserve_table[i].response_time_timer))
			return i;
	}

	return -1;
}

enum hrtimer_restart exectime_enforcer_handler(struct hrtimer *timer){
  unsigned long flags;
  int i;
  int cpuidx=-1;
  struct zs_reserve *rsv;

  spin_lock_irqsave(&zsrmlock,flags);
  for (i=0;i<MAX_CPUS;i++){
    if(&(exectime_enforcer_timers[i]) == timer)
      cpuidx=i;
  }

  if (cpuidx >=0){
    // enforce reserve running in cpuidx
    rsv = active_reserve_ptr[cpuidx];
    if (rsv != NULL){
      // make sure the zs_timer is cancelled
      hrtimer_cancel(&(rsv->zs_timer));
      kill_reserve(rsv,1); 
      set_tsk_need_resched(current);
    }
  }
  spin_unlock_irqrestore(&zsrmlock,flags);
  return HRTIMER_NORESTART;
}

void exectime_enforcer_timer_stop(struct zs_reserve *rsv){
  hrtimer_cancel(&exectime_enforcer_timers[rsv->params.bound_to_cpu]);
}

void exectime_enforcer_timer_start(struct zs_reserve *rsv){
  ktime_t kperiod;
  unsigned long long remaining_ns;
  unsigned long sec,nsec;

  remaining_ns = rsv->overload_exectime_nanos - rsv->job_executing_nanos;
  if (remaining_ns >0){
    sec = (unsigned long)(remaining_ns / 1000000000);
    nsec = (unsigned long) (remaining_ns % 1000000000);
    kperiod = ktime_set(sec,nsec);
    // The initialization of the timers happens once at the loading of the module. I am assuming that I do not 
    // need to reinitialize it again. We'll see.
    //hrtimer_init(&exectime_enforcer_timers[rsv->params.bound_to_cpu], CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    exectime_enforcer_timers[rsv->params.bound_to_cpu].function = exectime_enforcer_handler;
    hrtimer_start(&exectime_enforcer_timers[rsv->params.bound_to_cpu], kperiod, HRTIMER_MODE_REL);
  } else {
    printk("zsrmm: did not start exectime_enforcer_timer: remaining_ns negative\n");
  }
}

enum hrtimer_restart zs_instant_handler(struct hrtimer *timer){
	int rid;
	unsigned long flags;

	spin_lock_irqsave(&zsrmlock,flags);
	rid = timer2reserve(timer);
	if (rid == -1){
		printk("zs_timer without reserve\n");
		spin_unlock_irqrestore(&zsrmlock,flags);
		return HRTIMER_NORESTART;
	}

	start_of_enforcement_ts_ns =  timestamp_ns();//ticks2nanos(rdtsc());

	// special debugging mask
	if (!(reserve_table[rid].params.enforcement_mask & 
	      DONT_ENFORCE_ZERO_SLACK_MASK)){
	  // currently running not paused or dropped
	  if (reserve_table[rid].execution_status == EXEC_RUNNING)
	    period_degradation(rid);
	}
	spin_unlock_irqrestore(&zsrmlock,flags);
	return HRTIMER_NORESTART;
}

/* Returns the current period to be used in wait for next period*/
struct timespec *get_current_effective_period(int rid){
	if (reserve_table[rid].current_degraded_mode == -1){
		return &(reserve_table[rid].params.period);
	} else {
		return &(reserve_table[rid].params.degraded_period[reserve_table[rid].current_degraded_mode]);
	}
}


void record_latest_deadline(struct timespec *relative_deadline){
  struct timespec now;
  unsigned long long now_ns;
  unsigned long long rdeadline_ns;

  //jiffies_to_timespec(jiffies, &now);
  now2timespec(&now);
  now_ns = ((unsigned long long)now.tv_sec) * 1000000000ll +
    ((unsigned long long) now.tv_nsec);
	
  rdeadline_ns = ((unsigned long long)relative_deadline->tv_sec) * 1000000000ll+
		  ((unsigned long long) relative_deadline->tv_nsec);
  rdeadline_ns = now_ns+rdeadline_ns;

  if (latest_deadline_ns < rdeadline_ns){
    latest_deadline_ns = rdeadline_ns;
  }
}

enum hrtimer_restart response_time_handler(struct hrtimer *timer){
	unsigned long flags;
	int rid;
	struct task_struct *task;

	start_of_enforcement_ts_ns =  timestamp_ns();//ticks2nanos(rdtsc());

	spin_lock_irqsave(&zsrmlock,flags);
	rid = timer2reserve(timer);
	if (rid == -1){
		printk("timer without reserve!");
		spin_unlock_irqrestore(&zsrmlock,flags);
		return HRTIMER_NORESTART;
	}

	//task = find_task_by_pid_ns(reserve_table[rid].pid, current->nsproxy->pid_ns);
	task = gettask(reserve_table[rid].pid);
	if (task == NULL){
		printk("reserve without task\n");
		spin_unlock_irqrestore(&zsrmlock,flags);
		return HRTIMER_NORESTART;
	}

	reserve_table[rid].request_stop =REQUEST_STOP_DEFER;
	push_to_reschedule(rid);
	wake_up_process(sched_task);
	set_tsk_need_resched(current);
	
	spin_unlock_irqrestore(&zsrmlock,flags);
	return HRTIMER_NORESTART;
}

unsigned long long prev_ns=0;

enum hrtimer_restart period_handler(struct hrtimer *timer){
	struct task_struct *task;
	int rid;
	int mrid=0;
	ktime_t kperiod, kzs;
	ktime_t kresptime;
	struct timespec *p;
	unsigned long flags;
	struct timespec now;
	unsigned long long now_ns;
	int newmode=0;
	int oldmode=0;
	int mode_change=0;

	spin_lock_irqsave(&zsrmlock,flags);
	//jiffies_to_timespec(jiffies, &now);
	now2timespec(&now);
	now_ns = TIMESPEC2NS(&now);

	arrival_ts_ns =  timestamp_ns();//ticks2nanos(rdtsc());

	
	if (prev_ns == 0){
	  prev_ns = now_ns;
	} else {
	  //unsigned long long dur_ns = (now_ns - prev_ns);
	  prev_ns = now_ns;
	}

	rid = timer2reserve(timer);
	if (rid == -1){
		printk("timer without reserve!");
		spin_unlock_irqrestore(&zsrmlock,flags);
		return HRTIMER_NORESTART;
	}

	zsrm_add_debug_event(arrival_ts_ns,ZSRM_DEBUG_EVENT_JOB_ARRIVAL,rid);

	/* --- cancel all timers --- */
	if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
	  hrtimer_cancel(&(reserve_table[rid].response_time_timer));
	}
	hrtimer_cancel(&(reserve_table[rid].zs_timer));
	/* --------------------------*/

	task = gettask(reserve_table[rid].pid);
	if (task == NULL){
	  printk("reserve without task\n");
	  spin_unlock_irqrestore(&zsrmlock,flags);
	  return HRTIMER_NORESTART;
	}

	if (modal_scheduling){
	  mrid = reserve_table[rid].parent_modal_reserve_id;

	  if (modal_reserve_table[mrid].in_transition){
	    if (modal_reserve_table[mrid].mode_change_pending){
	      oldmode = modal_reserve_table[mrid].transitions[modal_reserve_table[mrid].active_transition].from_mode;
	      newmode = modal_reserve_table[mrid].transitions[modal_reserve_table[mrid].active_transition].to_mode;
	      modal_reserve_table[mrid].mode_change_pending=0;	    
	      if (newmode != DISABLED_MODE){
		// transition into non-suspension mode		
		rid = modal_reserve_table[mrid].reserve_for_mode[newmode];
		modal_reserve_table[mrid].mode = newmode;
	      } else {
		// DO NOT WAKE THE PROCESS
		// transition into suspension
		modal_reserve_table[mrid].mode = newmode;
	      }
	      mode_change=1;
	    } else {
	      mode_change=0;
	    }

	    if (end_of_transition_interval_ns <= now_ns){
	      // end of transition
	      modal_reserve_table[mrid].in_transition=0;
	      modal_reserve_table[mrid].active_transition=-1;	    
	    }
	  }
	}

	// if not in modal_scheduling the newmode == 0 => != DISABLED_MODE
	if (newmode != DISABLED_MODE){
	  p = get_current_effective_period(rid);
	  kperiod = ktime_set(p->tv_sec,p->tv_nsec);
	  hrtimer_init(&(reserve_table[rid].period_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	  reserve_table[rid].period_timer.function = period_handler;
	  hrtimer_start(&(reserve_table[rid].period_timer), kperiod, HRTIMER_MODE_REL);

	  if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
	    kresptime = ktime_set(reserve_table[rid].params.response_time_instant.tv_sec,
				  reserve_table[rid].params.response_time_instant.tv_nsec);
	    hrtimer_init(&(reserve_table[rid].response_time_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	    reserve_table[rid].response_time_timer.function = response_time_handler;
	    hrtimer_start(&(reserve_table[rid].response_time_timer),kresptime, HRTIMER_MODE_REL);
	  }

	  if (modal_scheduling && modal_reserve_table[mrid].in_transition){
	    struct timespec *trans_zsp = &(modal_reserve_table[mrid].transitions[modal_reserve_table[mrid].active_transition].zs_instant);
	    kzs = ktime_set(trans_zsp->tv_sec,trans_zsp->tv_nsec);
	  } else {
	    kzs = ktime_set(reserve_table[rid].params.zs_instant.tv_sec, 
			    reserve_table[rid].params.zs_instant.tv_nsec);
	  }
	  hrtimer_init(&(reserve_table[rid].zs_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	  reserve_table[rid].zs_timer.function = zs_instant_handler;
	  hrtimer_start(&(reserve_table[rid].zs_timer), kzs, HRTIMER_MODE_REL);
	  now2timespec(&reserve_table[rid].start_of_period);
	  reserve_table[rid].local_e2e_start_of_period_nanos =  timestamp_ns();
	  record_latest_deadline(&(reserve_table[rid].params.period));
	  reserve_table[rid].effective_priority = (reserve_table[rid].current_degraded_mode == -1) ? 
	    reserve_table[rid].params.priority :
	    reserve_table[rid].params.degraded_priority[reserve_table[rid].current_degraded_mode];
	  // if deferred to next arrival clear flag
	  if ((reserve_table[rid].execution_status & EXEC_ENFORCED_DEFERRED)){
	    reserve_table[rid].execution_status &= ~((unsigned int)EXEC_ENFORCED_DEFERRED);
	  }
	  // only wake it up if it was not enforced -- paused really
	  if (reserve_table[rid].critical_utility_mode_enforced == 0 &&
	      !(reserve_table[rid].execution_status & EXEC_ENFORCED_PAUSED) ){
	    // moved the wakeup call here to ensure only waking up when not enforced
	    reserve_table[rid].execution_status = EXEC_RUNNING;
	    
	    wake_up_process(task);
	    push_to_reschedule(rid);
	    wake_up_process(sched_task);
	    set_tsk_need_resched(current);
	  }
	}

	// no modal_scheduling => !mode_change
	if (!mode_change){
	  no_mode_change_ts_ns = timestamp_ns();
	  no_mode_change_ts_ns -= arrival_ts_ns;
#ifdef __ARMCORE__
	  arrival_no_mode_ns_avg = arrival_no_mode_ns_avg + no_mode_change_ts_ns;
	  do_div(arrival_no_mode_ns_avg,2);
#else
	  arrival_no_mode_ns_avg = (arrival_no_mode_ns_avg + no_mode_change_ts_ns) /2;
#endif
	  if (arrival_no_mode_ns_worst < no_mode_change_ts_ns){
	    arrival_no_mode_ns_worst = no_mode_change_ts_ns;
	  }
	} else {
	  if (newmode == DISABLED_MODE){
	    // transition into suspension
	    trans_into_susp_ts_ns = ticks2nanos(rdtsc());
	    trans_into_susp_ts_ns -= arrival_ts_ns;
#ifdef __ARMCORE__
	    transition_into_susp_ns_avg = transition_into_susp_ns_avg + trans_into_susp_ts_ns;
	    do_div(transition_into_susp_ns_avg,2);
#else
	    transition_into_susp_ns_avg = (transition_into_susp_ns_avg + trans_into_susp_ts_ns) / 2;
#endif
	    if (transition_into_susp_ns_worst < trans_into_susp_ts_ns){
	      transition_into_susp_ns_worst = trans_into_susp_ts_ns;
	    }
	  } else if (oldmode == DISABLED_MODE){
	    // transition into suspension
	    trans_from_susp_ts_ns = ticks2nanos(rdtsc());
	    trans_from_susp_ts_ns -= arrival_ts_ns;
#ifdef __ARMCORE__
	    transition_from_susp_ns_avg = transition_from_susp_ns_avg + trans_from_susp_ts_ns;
	    do_div(transition_from_susp_ns_avg,2);
#else
	    transition_from_susp_ns_avg = (transition_from_susp_ns_avg + trans_from_susp_ts_ns) / 2;
#endif
	    if (transition_from_susp_ns_worst < trans_from_susp_ts_ns){
	      transition_from_susp_ns_worst = trans_from_susp_ts_ns;
	    }
	  } else {
	    mode_change_ts_ns= ticks2nanos(rdtsc());
	    mode_change_ts_ns -= arrival_ts_ns;
#ifdef __ARMCORE__
	    arrival_mode_ns_avg = arrival_mode_ns_avg + mode_change_ts_ns;
	    do_div(arrival_mode_ns_avg,2);
#else
	    arrival_mode_ns_avg = (arrival_mode_ns_avg + mode_change_ts_ns) / 2;
#endif
	    if (arrival_mode_ns_worst < mode_change_ts_ns){
	      arrival_mode_ns_worst = mode_change_ts_ns;
	    }
	  }
	}
	spin_unlock_irqrestore(&zsrmlock,flags);
	return HRTIMER_NORESTART;
}


int get_jiffies_ms(void){
	return (int) (jiffies * 1000 / HZ);
}

void delete_all_modal_reserves(void){
  int i;

  for (i=0;i<MAX_MODAL_RESERVES;i++){
    modal_reserve_table[i].in_use=0;
  }
}

int set_initial_mode_modal_reserve(int mrid,int initmode){
  modal_reserve_table[mrid].mode = initmode;
  return 0;
}

int create_modal_reserve(int num_modes){
  int i;

  for (i=0;i<MAX_MODAL_RESERVES;i++){
    if (!modal_reserve_table[i].in_use){
      modal_reserve_table[i].in_use=1;
      modal_reserve_table[i].num_modes = num_modes;
      modal_reserve_table[i].mode=0;
      return i;
    }
  }

  return -1;
}

int add_reserve_to_mode(int mrsvid, int mode, int rsvid){
  modal_reserve_table[mrsvid].reserve_for_mode[mode] = rsvid;
  reserve_table[rsvid].parent_modal_reserve_id = mrsvid;
  return 0;
}

int add_mode_transition(int mrid, int transition_id, struct zs_mode_transition *ptransition){
  memcpy(&(modal_reserve_table[mrid].transitions[transition_id]),
	 ptransition,sizeof(struct zs_mode_transition));
  return 0;
}


/*
  The system mode transition is composed of a series of 
  individual reserve mode transitions execute executed in sequence.
  This individual reserve transitions will be grouped in a system mode
  transition to avoid multiple system calls
 */
int mode_transition(int mrid, int tid){
  ktime_t kzs;
  struct timespec now;
  unsigned long long now_ns;
  unsigned long long abs_zsi;
#ifdef __ARMCORE__
  u32 zsi_rem;
#endif
  unsigned long long start_of_period_ns;
  struct timespec time_to_zsi;
  struct task_struct *task;

  int mode = modal_reserve_table[mrid].mode;
  int rid=0;
  int debug_to_mode,debug_from_mode;

  task = gettask(modal_reserve_table[mrid].pid);

  if (task == NULL){
    printk("mode_transition: modal reserve(%d) without task\n",mrid);
    return -1;
  }
  // send the signal -- this is now back in the user-mode mode-change manager
  //send_mode_change_signal(task, modal_reserve_table[mrid].transitions[tid].to_mode);

  debug_to_mode = modal_reserve_table[mrid].transitions[tid].to_mode;
  debug_from_mode = modal_reserve_table[mrid].transitions[tid].from_mode;

  if (mode == DISABLED_MODE){
    // wake up the task immediately
    wake_up_process(task);
    
    // and install the reservation immediately with the target reservation
    rid = modal_reserve_table[mrid].reserve_for_mode[modal_reserve_table[mrid].transitions[tid].to_mode];
    modal_reserve_table[mrid].mode = modal_reserve_table[mrid].transitions[tid].to_mode;
    attach_reserve_transitional(rid, modal_reserve_table[mrid].pid, 
				&(modal_reserve_table[mrid].transitions[tid].zs_instant));

  } else {
    struct timespec *zsts = &(modal_reserve_table[mrid].transitions[tid].zs_instant);

    rid = modal_reserve_table[mrid].reserve_for_mode[mode];

    start_of_period_ns = ((unsigned long long)reserve_table[rid].start_of_period.tv_sec) * 1000000000ll+
      (unsigned long long) reserve_table[rid].start_of_period.tv_nsec;
    abs_zsi = ((unsigned long long)zsts->tv_sec)* 1000000000ll + (unsigned long long) zsts->tv_nsec;
    abs_zsi = start_of_period_ns + abs_zsi;
    //jiffies_to_timespec(jiffies, &now);
    now2timespec(&now);
    now_ns = ((unsigned long long) now.tv_sec) * 1000000000ll + (unsigned long long) now.tv_nsec;

    // moved to sys_mode_transition
    // record the latest deadline among the running jobs as the end of transition.
    //end_of_transition_interval_ns = latest_deadline_ns;

    // mark transition as active
    modal_reserve_table[mrid].in_transition=1;
    modal_reserve_table[mrid].active_transition=tid;
    modal_reserve_table[mrid].mode_change_pending=1;

    // we will point to target reserve in next period arrival
    if (abs_zsi <= now_ns ){
      // transitional zs instant already elapsed.
      // Immediate enforcement
      period_degradation(rid);
    } else { // abs_zsi >= now_ns
      // we need to setup the timer in the source reserve zs timer
      // this timer should remain active for all the jobs of this
      // task that arrive before the end_of_transition_interval_ns
      // TODO: handle end of the interval

      abs_zsi = abs_zsi - now_ns;
    
#ifdef __ARMCORE__
      time_to_zsi.tv_sec = div_u64_rem(abs_zsi, 1000000000, &zsi_rem);
      time_to_zsi.tv_nsec=zsi_rem;
#else
      time_to_zsi.tv_sec=abs_zsi / 1000000000ll;
      time_to_zsi.tv_nsec=abs_zsi % 1000000000ll;
#endif

      hrtimer_cancel(&(reserve_table[rid].zs_timer));
      kzs = ktime_set(time_to_zsi.tv_sec, 
		      time_to_zsi.tv_nsec);
      hrtimer_init(&(reserve_table[rid].zs_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
      reserve_table[rid].zs_timer.function = zs_instant_handler;
      hrtimer_start(&(reserve_table[rid].zs_timer), kzs, HRTIMER_MODE_REL);    
    }
  }

  return 0;
}

int attach_modal_reserve(int mrid, int pid){
  int i;

  modal_reserve_table[mrid].pid = pid;
  for (i=0;i<modal_reserve_table[mrid].num_modes;i++){
    reserve_table[modal_reserve_table[mrid].reserve_for_mode[i]].pid = pid;
  }

  if (modal_reserve_table[mrid].mode != DISABLED_MODE){
    attach_reserve(modal_reserve_table[mrid].reserve_for_mode[modal_reserve_table[mrid].mode],pid);
  }

  return 0;
}


int create_reserve(int rid){
	return 0;
}

int attach_reserve(int rid, int pid){
  return attach_reserve_transitional(rid, pid, NULL);
}

int attach_reserve_transitional(int rid, int pid, struct timespec *trans_zs_instant){
  struct sched_param p;
  ktime_t kperiod, kzs,kresptime;
  cpumask_t cpumask;
  
  struct task_struct *task;
  //task = find_task_by_pid_ns(pid, current->nsproxy->pid_ns);
  task = gettask(pid);
  if (task == NULL)
    return -1;
  
  reserve_table[rid].pid = pid;
  reserve_table[rid].in_critical_mode = 0;
  reserve_table[rid].current_degraded_mode = -1;
  reserve_table[rid].critical_utility_mode_enforced = 0;
  reserve_table[rid].execution_status = EXEC_RUNNING;
  reserve_table[rid].next_lower_priority = NULL;
  reserve_table[rid].next_paused_lower_criticality=NULL;
  
  // init pipeline
  reserve_table[rid].e2e_job_executing_nanos=0;
  
  // fix cpu affinity to its own
  cpus_clear(cpumask);
  cpu_set(reserve_table[rid].params.bound_to_cpu,cpumask);

  /* Note: this call works in the kernel 3.5.0-51-generic
   *       but I have seen that it creates a 'BUG: scheduling while atomic'
   *       error with kernel: 3.10.39-rk
   */
  set_cpus_allowed_ptr(task,&cpumask);

  p.sched_priority = reserve_table[rid].params.priority;
  sched_setscheduler(task,SCHED_FIFO, &p);  
  
  // if NOT APERIODIC  arrival then it is periodic. Start periodic timer
  if (!(reserve_table[rid].params.reserve_type & APERIODIC_ARRIVAL)){
    kperiod = ktime_set(reserve_table[rid].params.period.tv_sec, 
			reserve_table[rid].params.period.tv_nsec);
    hrtimer_init(&reserve_table[rid].period_timer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    reserve_table[rid].period_timer.function = period_handler;
    hrtimer_start(&reserve_table[rid].period_timer, kperiod, HRTIMER_MODE_REL);
  }
  
  if (trans_zs_instant != NULL){
    kzs = ktime_set(trans_zs_instant->tv_sec,
		    trans_zs_instant->tv_nsec);
  } else {
    kzs = ktime_set(reserve_table[rid].params.zs_instant.tv_sec, 
		    reserve_table[rid].params.zs_instant.tv_nsec);
  }
  hrtimer_init(&reserve_table[rid].zs_timer,CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  reserve_table[rid].zs_timer.function = zs_instant_handler;
  hrtimer_start(&reserve_table[rid].zs_timer, kzs, HRTIMER_MODE_REL);
  
  if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
    kresptime = ktime_set(reserve_table[rid].params.response_time_instant.tv_sec,
			  reserve_table[rid].params.response_time_instant.tv_nsec);
    hrtimer_init(&(reserve_table[rid].response_time_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    reserve_table[rid].response_time_timer.function = response_time_handler;
    hrtimer_start(&(reserve_table[rid].response_time_timer),kresptime, HRTIMER_MODE_REL);
  }
  
  //jiffies_to_timespec(jiffies, &reserve_table[rid].start_of_period);
  now2timespec(&reserve_table[rid].start_of_period);
  if (reserve_table[rid].params.reserve_type & PIPELINE_STAGE_ROOT){
    reserve_table[rid].local_e2e_start_of_period_nanos = TIMESPEC2NS(&reserve_table[rid].start_of_period);
  }

  zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_JOB_ARRIVAL,rid);
  start_accounting(&reserve_table[rid]);
  
  // debug -- perhaps we don't ned this resched
  // set_tsk_need_resched(task);
  set_tsk_need_resched(current);
  return 0;
}

int find_empty_entry(void){
  int i=0;
  
  while (i < MAX_RESERVES && reserve_table[i].params.priority != -1  )
    i++;
  if (i< MAX_RESERVES){
    reserve_table[i].params.priority = 0;
    return i;
  } else {
    return -1;
  }
}

void finish_period_degradation(int rid, int is_pipeline_stage){
  long max_marginal_utility=0;
  long tmp_marginal_utility=0;
  int i;
  
  reserve_table[rid].in_critical_mode = 0;

  // find the max marginal utility of reserves
  // in critical mode
  for (i=0;i<MAX_RESERVES; i++){
    // empty reserves
    if (reserve_table[i].params.priority == -1)
      continue; 
    if (!reserve_table[i].in_critical_mode)
      continue;
    
    tmp_marginal_utility = GET_EFFECTIVE_UTILITY(i);
    if (tmp_marginal_utility > max_marginal_utility)
      max_marginal_utility = tmp_marginal_utility;
  }
  
  system_utility = max_marginal_utility;
  
  // check if the reserve overloaded
  if ( (is_pipeline_stage &&
  	(reserve_table[rid].e2e_job_executing_nanos >
  	 reserve_table[rid].e2e_nominal_exectime_nanos)) ||
       (!is_pipeline_stage &&
  	(reserve_table[rid].job_executing_nanos >
  	 reserve_table[rid].nominal_exectime_nanos))
       ){
    kill_paused_reserves(reserve_table[rid].params.overloaded_marginal_utility,reserve_table[rid].params.bound_to_cpu);
  } else {
    // did not overload. Resume jobs with new system_utility
    resume_paused_reserves(system_utility,reserve_table[rid].params.bound_to_cpu);
  }


  for (i=0; i< MAX_RESERVES; i++){
    // empty reserves
    if (reserve_table[i].params.priority == -1)
      continue; 
    // only for ZS-QRAM
    if (reserve_table[i].params.num_degraded_modes >0){
      if (reserve_table[i].params.overloaded_marginal_utility < system_utility){
	int selected_mode=0;
	while(selected_mode < reserve_table[i].params.num_degraded_modes &&
	      reserve_table[i].params.degraded_marginal_utility[selected_mode][1] < system_utility)
	  selected_mode++;
	if (selected_mode < reserve_table[i].params.num_degraded_modes){
	  reserve_table[i].current_degraded_mode = selected_mode;
	  reserve_table[i].effective_priority = reserve_table[i].params.degraded_priority[selected_mode];
	  push_to_reschedule(i);
	}
      }
    }
  }
  wake_up_process(sched_task);
  set_tsk_need_resched(current);
}

void try_resume_normal_period(int rid){
	unsigned long long zs_elapsed_ns;
	unsigned long long newperiod_ns;
	struct timespec now;
	unsigned long long now_ns, secs;
	unsigned long long jiffy_ns;
	u32 ns_reminder;
	ktime_t kperiod;

	//jiffies_to_timespec(jiffies, &now);
	now2timespec(&now);
	now_ns = ((unsigned long long) now.tv_sec) * 1000000000ll + (unsigned long long) now.tv_nsec;
	zs_elapsed_ns = ((unsigned long long)reserve_table[rid].start_of_period.tv_sec) * 1000000000ll + (unsigned long long) reserve_table[rid].start_of_period.tv_nsec;
	zs_elapsed_ns = now_ns - zs_elapsed_ns;
	newperiod_ns = ((unsigned long long) reserve_table[rid].params.period.tv_sec) * 1000000000ll + (unsigned long long) reserve_table[rid].params.period.tv_nsec;

	// do we still have time to return to previous period?
	if (newperiod_ns > zs_elapsed_ns){
		newperiod_ns = newperiod_ns - zs_elapsed_ns;
		jiffy_ns = div_u64(1000000000ll,HZ);
		// greater than a jiffy?
		if (newperiod_ns > jiffy_ns){
		  if (!(reserve_table[rid].params.reserve_type & APERIODIC_ARRIVAL)){
			hrtimer_cancel(&(reserve_table[rid].period_timer));
			secs = div_u64_rem(newperiod_ns, 1000000000L, &ns_reminder);
			kperiod = ktime_set(secs, ns_reminder);
			hrtimer_init(&(reserve_table[rid].period_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			reserve_table[rid].period_timer.function = period_handler;
			hrtimer_start(&(reserve_table[rid].period_timer), kperiod, HRTIMER_MODE_REL);
		  }
		}
	}
}

void extend_period_timer(int rid, struct timespec *p){
	unsigned long long zs_elapsed_ns;
	unsigned long long newperiod_ns;
	struct timespec now;
	unsigned long long now_ns, secs;
	u32 ns_reminder;
	ktime_t kperiod;

	//jiffies_to_timespec(jiffies, &now);
	now2timespec(&now);
	now_ns = ((unsigned long long) now.tv_sec) * 1000000000ll + (unsigned long long) now.tv_nsec;
	zs_elapsed_ns = ((unsigned long long)reserve_table[rid].start_of_period.tv_sec) * 1000000000ll + (unsigned long long) reserve_table[rid].start_of_period.tv_nsec;
	zs_elapsed_ns = now_ns - zs_elapsed_ns;
	newperiod_ns = ((unsigned long long) p->tv_sec) * 1000000000ll + (unsigned long long) p->tv_nsec;

	// check whether the current elapsed time is already larger than the new period
	// this could happen because the global variable jiffies is incremented every 10ms.
	if (newperiod_ns > zs_elapsed_ns){
		newperiod_ns = newperiod_ns - zs_elapsed_ns;

		if (!(reserve_table[rid].params.reserve_type & APERIODIC_ARRIVAL)){
		  hrtimer_cancel(&(reserve_table[rid].period_timer));

		  secs = div_u64_rem(newperiod_ns, 1000000000L, &ns_reminder);
		  kperiod = ktime_set(secs, ns_reminder);
		  hrtimer_init(&(reserve_table[rid].period_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		  reserve_table[rid].period_timer.function = period_handler;
		  hrtimer_start(&(reserve_table[rid].period_timer), kperiod, HRTIMER_MODE_REL);
		}
	}
}

void period_degradation(int rid){
  long current_marginal_utility;
  long tmp_marginal_utility;
  int i,j;
  int maxreserves = (modal_scheduling == 0 ? MAX_RESERVES : MAX_MODAL_RESERVES);
  long long elapsedns;
    
  elapsedns =  timestamp_ns() - reserve_table[rid].local_e2e_start_of_period_nanos;
  current_marginal_utility = GET_EFFECTIVE_UTILITY(rid);
  reserve_table[rid].in_critical_mode = 1;
  
  if (system_utility < current_marginal_utility){
    system_utility = current_marginal_utility;
    if (reserve_table[rid].params.critical_util_degraded_mode != -1){
      if (reserve_table[rid].current_degraded_mode == -1 || reserve_table[rid].current_degraded_mode < reserve_table[rid].params.critical_util_degraded_mode){
	reserve_table[rid].current_degraded_mode = reserve_table[rid].params.critical_util_degraded_mode;
	extend_period_timer(rid, &reserve_table[rid].params.degraded_period[reserve_table[rid].params.critical_util_degraded_mode]);
	reserve_table[rid].effective_priority = reserve_table[rid].params.degraded_priority[reserve_table[rid].current_degraded_mode];
	push_to_reschedule(rid);
      }
    } 
  } 

  // now degrade all others  
  for (j=0;j < maxreserves; j++){	  

    if (modal_scheduling){
      if (!modal_reserve_table[j].in_use)
	continue;
      // get the normal reseve
      i = modal_reserve_table[j].reserve_for_mode[modal_reserve_table[j].mode];
    } else {
      i=j;
    }
        
    // skip empty reserves
    if (reserve_table[i].params.priority == -1)
      continue;

    // skip myself
    if (rid == i)
      continue;
    
    // only the ones on the same processor
    if (reserve_table[rid].params.bound_to_cpu != reserve_table[i].params.bound_to_cpu)
      continue;
    
    tmp_marginal_utility = GET_EFFECTIVE_UTILITY(i);
    
    if (tmp_marginal_utility < system_utility){
      int selected_mode=0;
      while(selected_mode < (reserve_table[i].params.num_degraded_modes) &&
	    reserve_table[i].params.degraded_marginal_utility[selected_mode][1] < system_utility)
	selected_mode++;
      if (selected_mode < reserve_table[i].params.num_degraded_modes){
	reserve_table[i].current_degraded_mode = selected_mode;
	reserve_table[i].just_returned_from_degradation=0;
	extend_period_timer(i, &reserve_table[i].params.degraded_period[selected_mode]);
	reserve_table[i].effective_priority = reserve_table[i].params.degraded_priority[selected_mode];
	push_to_reschedule(i);
      } else {
	// stop the task
	reserve_table[i].critical_utility_mode_enforced = 1;
	reserve_table[i].just_returned_from_degradation=0;	
	zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_ZS_SUSPENSION,i);
	pause_reserve(&reserve_table[i]);
      }
    }
  }
  wake_up_process(sched_task);
  set_tsk_need_resched(current);
}

int modal_wait_for_next_period(int mrid){
  int mode = modal_reserve_table[mrid].mode;
  if (mode == DISABLED_MODE){
    // need to cancel timers from previous period!
    // we need to know whether the mode change req happen
    // during the waitfornextperiod or during the execution, i.e.,
    // if occurs after calling wfnp means that the timers are
    // already cancelled, if not we need to cancel them explicitly.
    //
    // if the wfnp is called in a disabled mode it means that 
    // this is the last call to block the task until another mode
    // change happens. This means that we need to clean up the
    // previous mode timers.
    set_current_state(TASK_UNINTERRUPTIBLE);
  } else {
    int rid = modal_reserve_table[mrid].reserve_for_mode[mode];
    wait_for_next_period(rid);
  }
  return 0;
}


// TODO: perhaps encode clock diff
/*
long long get_clock_diff(struct sockaddr_in remaddr){
  int i;

  for (i=0;i<PIPELINE_NODES_TABLE_SIZE;i++){
    if (pipeline_nodes_table[i].addr.sin_addr.s_addr == remaddr.sin_addr.s_addr){
      return pipeline_nodes_table[i].clock_diff;
    }
  }

  return 0L;
}
*/

unsigned long long get_adjusted_root_start_of_period(unsigned long long receiving_timestamp_nanos,
						     unsigned long long sending_timestamp_nanos,
						     unsigned long long root_start_period_nanos,
						     struct sockaddr_in remaddr){
  return root_start_period_nanos;
  /* disable for now
      if (receiving_timestamp_nanos < sending_timestamp_nanos){
	diff_timestamp_nanos = sending_timestamp_nanos - receiving_timestamp_nanos;
	  return (root_start_period_nanos - diff_timestamp_nanos);
      } else {
	diff_timestamp_nanos = receiving_timestamp_nanos - sending_timestamp_nanos;
	  return (root_start_period_nanos + diff_timestamp_nanos);
      }
  */
}

/* Expected to return a negative number that should be added to the zs timer to 
 * reduce the waiting time towards the zs instant.
 *
 * It assumes that the start_of_period is already adjusted to the local clock, i.e.,
 * any clock difference with the other pipeline stages is already taken into 
 * account for the start of the period
 */

long long calculate_zero_slack_adjustment_ns(unsigned long long start_period_ns){
  unsigned long long now_ns;

  now_ns = timestamp_ns();

  return (start_period_ns - now_ns);
}

//TODO: in stages we need to copy udp in/out to/from user buffers
// compensating of the pipeline header
// -- read the pipeline header cummulative execution and calculate new
// total consumption. Apply it to the after-overload enforcement mechanism

int wait_for_next_leaf_stage_arrival(int rid, unsigned long *flags, 
				     int fd, void __user *ubuf, size_t size, 
				     unsigned int sockflags, struct sockaddr __user *addr, 
				     int __user *addr_len){
  int len;
  struct pipeline_header pipeline_hdr;
  unsigned long long receiving_timestamp_nanos;
  struct sockaddr_in remaddr;
  long long zero_slack_adjustment;

  end_of_job_processing(rid,1);

  // release zsrm locks
  spin_unlock_irqrestore(&zsrmlock, *flags);
  up(&zsrmsem);

  len = sys_recvfromp(fd,ubuf-sizeof(struct pipeline_header), 
		      size+sizeof(struct pipeline_header), 
		      sockflags, addr, addr_len);
  receiving_timestamp_nanos = timestamp_ns(); 

  if (len <0){
    printk("zsrmm: wait next leaf: sock_recvmsg failed. err:%d\n",len);
  }

  // reacquire zsrm locks
  down(&zsrmsem);
  spin_lock_irqsave(&zsrmlock, *flags);

  reserve_table[rid].e2e_job_executing_nanos = pipeline_hdr.cumm_exectime_nanos;

  // read the pipeline header
  if (copy_from_user(&pipeline_hdr, ubuf-sizeof(struct pipeline_header), sizeof(struct pipeline_header))){
    return -EFAULT;
  }

  if(copy_from_user(&remaddr, addr, sizeof(struct sockaddr_in))){
    return -EFAULT;
  }
  reserve_table[rid].local_e2e_start_of_period_nanos = get_adjusted_root_start_of_period(receiving_timestamp_nanos,
											 pipeline_hdr.rem_sending_timestamp_nanos,
											 pipeline_hdr.rem_start_of_period_nanos,
											 remaddr);

  // TODO: reprogram the e2e_zs_timer discounting the currently elapsed e2e response time  
  // we only call the start of job if the notify arrival has not done it for us before
  if (reserve_table[rid].execution_status & EXEC_WAIT_NEXT_PERIOD){
    zero_slack_adjustment = calculate_zero_slack_adjustment_ns(reserve_table[rid].local_e2e_start_of_period_nanos);
    if (start_of_job_processing(rid, zero_slack_adjustment)){
      printk("zsrmm: wait_next_leaf_arrival: START OF JOB FAILED zero_slack_adjustment(%lld)\n",zero_slack_adjustment);
    }
  } 
  
  return len;
}

int wait_for_next_stage_arrival(int rid, unsigned long *flags,
				int fd, void __user *buf, size_t size, 
				unsigned int sockflags, 
				struct sockaddr __user *outaddr, 
				int outaddrlen,
				struct sockaddr __user *inaddr, 
				int __user *inaddrlen,
				int io_mask
				){
  struct pipeline_header_with_signature pipeline_hdrs; 
  struct pipeline_header pipeline_hdr;
  int len=0;
  unsigned long long receiving_timestamp_nanos;
  struct sockaddr_in remaddr;
  long long zero_slack_adjustment;
  
  if (copy_from_user(&pipeline_hdrs, (buf-sizeof(struct pipeline_header_with_signature)), 
		     sizeof(struct pipeline_header_with_signature))){
    return -EFAULT;
  }
  if (pipeline_hdrs.signature != MODULE_SIGNATURE){
    printk("zsrmm: wait_next_stage_arrival: wrong packet signature\n");
    len = -1;
  } else {
    
    end_of_job_processing(rid, 1);
    
    pipeline_hdrs.header.cumm_exectime_nanos = reserve_table[rid].e2e_job_executing_nanos;
    pipeline_hdrs.header.rem_start_of_period_nanos = reserve_table[rid].local_e2e_start_of_period_nanos;
    pipeline_hdrs.header.rem_sending_timestamp_nanos = timestamp_ns();//ticks2nanos(rdtsc());
    if (copy_to_user(buf - sizeof(struct pipeline_header_with_signature), 
		     &pipeline_hdrs, sizeof(struct pipeline_header_with_signature))){
      return -EFAULT;
    }

    // release zsrm locks
    spin_unlock_irqrestore(&zsrmlock, *flags);
    up(&zsrmsem);

    len = 0;

    if (!(io_mask & MIDDLE_STAGE_DONT_SEND_OUTPUT)){
      len = sys_sendtop(fd, buf-sizeof(struct pipeline_header), 
			size+sizeof(struct pipeline_header), 
			sockflags, outaddr, outaddrlen);
    }
    if (len<0){
      printk("zsrm.wait_for_next_stage_arrival: error. sys_sendto returned %d\n",len);
      // reacquire zsrm locks 
      down(&zsrmsem);
      spin_lock_irqsave(&zsrmlock, *flags);
    } else {
      len=0;     
      if (!(io_mask & MIDDLE_STAGE_DONT_WAIT_INPUT)){
	len = sys_recvfromp(fd,buf-sizeof(struct pipeline_header), 
			    size+sizeof(struct pipeline_header), 
			    sockflags, inaddr, inaddrlen);
      }      
      receiving_timestamp_nanos = timestamp_ns();

      // reacquire zsrm locks
      down(&zsrmsem);
      spin_lock_irqsave(&zsrmlock, *flags);

      // read the pipeline header
      if (copy_from_user(&pipeline_hdr, buf-sizeof(struct pipeline_header), sizeof(struct pipeline_header))){
	return -EFAULT;
      }
      
      reserve_table[rid].e2e_job_executing_nanos = pipeline_hdr.cumm_exectime_nanos;

      if(copy_from_user(&remaddr, inaddr, sizeof(struct sockaddr_in))){
	return -EFAULT;
      }
      reserve_table[rid].local_e2e_start_of_period_nanos = get_adjusted_root_start_of_period(receiving_timestamp_nanos,
											     pipeline_hdr.rem_sending_timestamp_nanos,
											     pipeline_hdr.rem_start_of_period_nanos,
											     remaddr);
      // TODO: reprogram the e2e_zs_timer discounting the currently elapsed e2e response time
      
      if (reserve_table[rid].execution_status & EXEC_WAIT_NEXT_PERIOD){
	zero_slack_adjustment = calculate_zero_slack_adjustment_ns(reserve_table[rid].local_e2e_start_of_period_nanos);
	
	if (start_of_job_processing(rid,zero_slack_adjustment)){
	  printk("zsrmm: wait_next_stage_arrival: start job returned error adjustment(%lld)!!\n",zero_slack_adjustment);
	}
      }
      
    }
  }

  return len;
}

int wait_for_next_root_period(int rid, unsigned long *irqflags, int fd, void __user *buf, size_t buflen, unsigned int flags, struct sockaddr __user *addr, int addrlen){
  int len=0;
  struct pipeline_header_with_signature pipeline_hdrs; 

  end_of_job_processing(rid, 1);
  
  if (copy_from_user(&pipeline_hdrs, (buf-sizeof(struct pipeline_header_with_signature)), 
		     sizeof(struct pipeline_header_with_signature))){
    return -EFAULT;
  }
  if (pipeline_hdrs.signature != MODULE_SIGNATURE){
    printk("zsrmm: wait_next_root_period: wrong packet signature\n");
    len = -1;
  } else {
    pipeline_hdrs.header.cumm_exectime_nanos = reserve_table[rid].e2e_job_executing_nanos;
    pipeline_hdrs.header.rem_start_of_period_nanos = reserve_table[rid].local_e2e_start_of_period_nanos;
    pipeline_hdrs.header.rem_sending_timestamp_nanos = timestamp_ns();//ticks2nanos(rdtsc());

    copy_to_user(buf - sizeof(struct pipeline_header_with_signature), 
		 &pipeline_hdrs, sizeof(struct pipeline_header_with_signature));
     // release zsrm locks
    spin_unlock_irqrestore(&zsrmlock, *irqflags);
    up(&zsrmsem);

    len = sys_sendtop(fd, buf-sizeof(struct pipeline_header), 
		      buflen+sizeof(struct pipeline_header), 
		      flags, addr, addrlen);    


    if (len  <0){
      printk("zsrm.wait_for_next_root_period: kernel_sendmsg returned %d\n",len);
      if (len == -EINVAL){
	printk("zsrmm: kernel_sendmsg return EINVAL\n");
      }
    }
    
    set_current_state(TASK_UNINTERRUPTIBLE);

    // reacquire zsrm locks
    down(&zsrmsem);
    spin_lock_irqsave(&zsrmlock, *irqflags);

  }
  
  return len;
}

void end_of_job_processing(int rid, int is_pipeline_stage){

  start_of_resume_ts_ns = start_of_defer_ts_ns = timestamp_ns();

  hrtimer_cancel(&(reserve_table[rid].zs_timer));
  if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
    hrtimer_cancel(&(reserve_table[rid].response_time_timer));
  }

  stop_accounting(&reserve_table[rid],UPDATE_HIGHEST_PRIORITY_TASK,0,0);

  if (is_pipeline_stage){
    reserve_table[rid].e2e_job_executing_nanos += reserve_table[rid].job_executing_nanos;
  }
  
  if (reserve_table[rid].in_critical_mode){
    finish_period_degradation(rid, is_pipeline_stage);
  } else {
    // reset the defer timer
    start_of_resume_ts_ns = start_of_defer_ts_ns = 0L;
  }
  if (reserve_table[rid].just_returned_from_degradation){
    reserve_table[rid].just_returned_from_degradation = 0;
  }

  // reset the job_executing_nanos
  reserve_table[rid].job_executing_nanos=0L;

  reserve_table[rid].execution_status = EXEC_WAIT_NEXT_PERIOD;
}

int start_of_job_processing(int rid, long long zero_slack_adjustment_ns){
  ktime_t kzs;
  struct sched_param p;
  struct timespec adjusted_zs_instant;
  unsigned long long zs_instant_ns;
  struct task_struct *task;

  adjusted_zs_instant.tv_sec = reserve_table[rid].params.zs_instant.tv_sec;
  adjusted_zs_instant.tv_nsec = reserve_table[rid].params.zs_instant.tv_nsec;
  if (zero_slack_adjustment_ns != 0){
    zs_instant_ns = TIMESPEC2NS(&adjusted_zs_instant);
    zs_instant_ns = zs_instant_ns + zero_slack_adjustment_ns;
    if (zs_instant_ns <0){
      // too late to setup timer!! zs already elapsed
      // return error and let the caller deal with it
      return -1;
    }
    nanos2timespec(zs_instant_ns, &adjusted_zs_instant);
  }

  kzs = ktime_set(adjusted_zs_instant.tv_sec, 
		  adjusted_zs_instant.tv_nsec);

  hrtimer_init(&(reserve_table[rid].zs_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  reserve_table[rid].zs_timer.function = zs_instant_handler;
  hrtimer_start(&(reserve_table[rid].zs_timer), kzs, HRTIMER_MODE_REL);

  now2timespec(&reserve_table[rid].start_of_period);
  record_latest_deadline(&(reserve_table[rid].params.period));

  if ((reserve_table[rid].critical_utility_mode_enforced == 0) &&
      !(reserve_table[rid].execution_status & EXEC_ENFORCED_DEFERRED) &&    
      !(reserve_table[rid].execution_status & EXEC_ENFORCED_PAUSED)){
    
    reserve_table[rid].execution_status = EXEC_RUNNING;    
    start_accounting(&reserve_table[rid]);

    task = gettask(reserve_table[rid].pid);
    if (task != NULL){
      p.sched_priority = reserve_table[rid].effective_priority; 
      sched_setscheduler(task, SCHED_FIFO, &p);
    } else {
      printk("zsrmm.start_of_job_processing: task == NULL");
    }
  } 

  return 0;
}

int wait_for_next_period(int rid){
  end_of_job_processing(rid, 0);
  // create loop to check if the task was interrupted 
  // by a signal that was sent to it and if so go back
  // to sleep.
  set_current_state(TASK_UNINTERRUPTIBLE);

  return 0;
}

/**
 * In some system calls we may need to change the memory upper limit 
 * to access user memory with
 *
 * mm_segment_t fs;
 * fs = get_fs();
 * set_fs(get_ds());
 *  // syscall
 * set_fs(fs)
 *
 * However in this one the sys_poll already takes a user pointer.
 */
// called with zsrm_lock locked and will release it inside in order to block
int wait_for_next_arrival(int rid, struct pollfd __user *fds, unsigned int nfds, unsigned long *flags){
  int retval = 0;
  struct sched_param p;
  unsigned long long now_ns;

  end_of_job_processing(rid,0);

  p.sched_priority = scheduler_priority;
  sched_setscheduler(current, SCHED_FIFO, &p);


  // release zsrm locks
  spin_unlock_irqrestore(&zsrmlock, *flags);
  up(&zsrmsem);
  
  do_sys_pollp(fds, (unsigned int) 1, NULL);

  now_ns = timestamp_ns();

  // reacquire zsrm locks
  down(&zsrmsem);
  spin_lock_irqsave(&zsrmlock, *flags);

  // no zero_slack adjustment for now
  start_of_job_processing(rid, 0L);

  return retval;
}

int delete_reserve(int rid){
  if (reserve_table[rid].pid != -1){
    detach_reserve(rid);
  }
  reserve_table[rid].params.priority = -1;
  reserve_table[rid].execution_status = EXEC_BEFORE_START;
  return 0;
}

int delete_modal_reserve(int mrid){
  if (modal_reserve_table[mrid].pid !=-1){
    detach_modal_reserve(mrid);
  }
  modal_reserve_table[mrid].in_use=0;
  modal_reserve_table[mrid].mode=0;
  return 0;
}

int detach_modal_reserve(int mrid){
  int i;
  // ToDo: Do we need to do something special when in transition?
  // we can assume/enforce that no detach call will be received during a 
  // transition
  if (modal_reserve_table[mrid].in_transition){
    // don't allow detachment in the middle of a transition
    return -1;
  }

  detach_reserve(modal_reserve_table[mrid].reserve_for_mode[modal_reserve_table[mrid].mode]);

  modal_reserve_table[mrid].pid = -1;
  for (i=0;i<modal_reserve_table[mrid].num_modes;i++){
    reserve_table[modal_reserve_table[mrid].reserve_for_mode[i]].pid = -1;
  }

  return 0;
}

int detach_reserve(int rid){
  struct sched_param p;
  struct task_struct *task;
  task = gettask(reserve_table[rid].pid);
  
  if (task != NULL){
    p.sched_priority = 0;
    sched_setscheduler(task, SCHED_NORMAL,&p);
    if (task != current)
      set_tsk_need_resched(current);
  }

  hrtimer_cancel(&(reserve_table[rid].zs_timer));
  if (!(reserve_table[rid].params.reserve_type & APERIODIC_ARRIVAL))
    hrtimer_cancel(&(reserve_table[rid].period_timer));
  if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
    hrtimer_cancel(&(reserve_table[rid].response_time_timer));
  }
  
  if (reserve_table[rid].in_critical_mode){
    finish_period_degradation(rid,0);
  } 

  stop_accounting(&reserve_table[rid], UPDATE_HIGHEST_PRIORITY_TASK,0,0);

  // mark as detached
  reserve_table[rid].pid = -1;
  
  return 0;
}

int raise_priority_criticality(int rid, int priority_ceiling, int criticality_ceiling){
  struct task_struct *task;
  struct sched_param p;

  task = gettask(reserve_table[rid].pid);

  if (task == NULL){
    printk("reserve(%d) without task\n",rid);
    return -1;
  }

  // save base priority + criticality
  reserve_table[rid].base_criticality = reserve_table[rid].params.criticality;
  reserve_table[rid].base_priority = reserve_table[rid].params.priority;

  // raise the priority + criticality
  reserve_table[rid].params.priority = priority_ceiling;
  reserve_table[rid].params.criticality = criticality_ceiling;
  // to make it work with the ZS-QRAM core -- but single degradation mode
  reserve_table[rid].params.normal_marginal_utility = criticality_ceiling;
  reserve_table[rid].params.overloaded_marginal_utility = criticality_ceiling;

  // one possibility is to check if the task has already being enforced looking at: if (task->policy == SCHED_FIFO){
  // check if system criticality is lower than this criticality
  if (reserve_table[rid].params.overloaded_marginal_utility >= system_utility){
    p.sched_priority = priority_ceiling;
    sched_setscheduler(task,SCHED_FIFO,&p);
  }
  // else do nothing, the saved priority and criticality ceilings will be used later  
  return 0;
}

int restore_base_priority_criticality(int rid){
  struct task_struct *task;
  struct sched_param p;

  task = gettask(reserve_table[rid].pid);

  if (task == NULL){
    printk("reserve(%d) without task\n",rid);
    return -1;
  }

  // restore base priority + criticality
  reserve_table[rid].params.criticality = reserve_table[rid].base_criticality;
  reserve_table[rid].params.priority = reserve_table[rid].base_priority;

  // to make it work with the ZS-QRAM core -- but single degradation mode
  reserve_table[rid].params.normal_marginal_utility = reserve_table[rid].params.criticality;
  reserve_table[rid].params.overloaded_marginal_utility = reserve_table[rid].params.criticality;

  // check if system criticality is lower than this criticality
  
  if (reserve_table[rid].params.overloaded_marginal_utility >= system_utility){
    p.sched_priority = reserve_table[rid].params.priority;
    sched_setscheduler(task,SCHED_FIFO,&p);
  } else {
    // it should be enforced
    pause_reserve(&reserve_table[rid]);
    wake_up_process(sched_task);
    set_tsk_need_resched(current);
  }
  return 0;
}

int kfds[MAX_RESERVES];

int notify_arrival(int __user *fds, int nfds){
  struct task_struct *task;
  int i,j;
  long long zero_slack_adjustment;

  start_of_net_arrival_ts_ns = timestamp_ns();

  if (copy_from_user(&kfds, fds, sizeof(int) * nfds)){
    return -EFAULT;
  }

  printk("zsrm.notify_arrival: checking waking socks\n");
  for (i=0;i<nfds;i++){
    for (j=0;j<MAX_RESERVES;j++){
      if (reserve_table[j].params.priority == -1)
	continue;
      if (reserve_table[j].params.insockfd == kfds[i]){
	// This processing should only occur in the case that the task was deferred
	// because then we need to "reschedule" it with its 'active' parameters
	// Otherwise the task will wakeup by itself with its proper parameters.
	if (reserve_table[j].execution_status & EXEC_ENFORCED_DEFERRED){
	  reserve_table[j].execution_status &= ~((unsigned int)EXEC_ENFORCED_DEFERRED);
      	  task = gettask(reserve_table[j].pid);
	  if (task != NULL){
	    if ((task->state & TASK_INTERRUPTIBLE) || 
		(task->state & TASK_UNINTERRUPTIBLE)){
	      wake_up_process(task);
	    }
	  }
	  zero_slack_adjustment = 
	    calculate_zero_slack_adjustment_ns(reserve_table[j].local_e2e_start_of_period_nanos);	
	  if (start_of_job_processing(j,zero_slack_adjustment)){
	    printk("zsrmm: notify_arrival: start job returned error adjustment(%lld)!!\n",zero_slack_adjustment);
	  }
	  if (start_of_net_arrival_ts_ns != 0){
	    end_of_net_arrival_ts_ns = timestamp_ns();
	    end_of_net_arrival_ts_ns -= start_of_net_arrival_ts_ns;
	    if (net_arrival_latency_ns_avg == 0)
	      net_arrival_latency_ns_avg = end_of_net_arrival_ts_ns;
	    else{
#ifdef __ARMCORE__
	      net_arrival_latency_ns_avg = net_arrival_latency_ns_avg + end_of_net_arrival_ts_ns;
	      do_div(net_arrival_latency_ns_avg,2);
#else
	      net_arrival_latency_ns_avg = (net_arrival_latency_ns_avg + end_of_net_arrival_ts_ns) / 2;
#endif		    
	    }
	    if (end_of_net_arrival_ts_ns > net_arrival_latency_ns_worst){
	      net_arrival_latency_ns_worst = end_of_net_arrival_ts_ns;
	    }
	    
	    start_of_net_arrival_ts_ns=0L;
	  }
	} 
	break;
      }
    }
  }
  return 0;
}

void init(void){
  int i,j;
  
  for (i=0;i<MAX_RESERVES;i++){
    reserve_table[i].params.name[0]='\0';
    reserve_table[i].rid=i;
    reserve_table[i].params.priority = -1; // mark as empty
    reserve_table[i].params.enforcement_mask = 0;
    reserve_table[i].current_degraded_mode = -1;
    reserve_table[i].just_returned_from_degradation=0;
    reserve_table[i].in_critical_mode = 0;
    reserve_table[i].pid = -1;
    reserve_table[i].request_stop = 0;
    reserve_table[i].parent_modal_reserve_id=-1;
    reserve_table[i].execution_status = EXEC_BEFORE_START;
    reserve_table[i].next_lower_priority = NULL;
    reserve_table[i].next_paused_lower_criticality = NULL;
    reserve_table[i].e2e_job_executing_nanos=0L;
    reserve_table[i].local_e2e_start_of_period_nanos=0L;
    init_waitqueue_head(&(reserve_table[i].arrival_waitq));
  }
  
  for (i=0;i<MAX_MODAL_RESERVES;i++){
    modal_reserve_table[i].in_use=0;
    modal_reserve_table[i].pid=-1;
    modal_reserve_table[i].mode=0;
    modal_reserve_table[i].in_transition=0;
    modal_reserve_table[i].active_transition=-1;
    modal_reserve_table[i].mode_change_pending=0;
    modal_reserve_table[i].num_modes=0;
    modal_reserve_table[i].num_transitions=0;
    for (j=0;j<MAX_RESERVES;j++){
      modal_reserve_table[i].reserve_for_mode[j]=-1;
    }
  }
  
  init_sys_mode_transitions();
}

void delete_all_reserves(void){
  int i;
  
  for (i=0;i<MAX_RESERVES;i++){
    if (reserve_table[i].params.priority != -1){
      delete_reserve(i);
    }
  }
}


/* dummy function. */
static int zsrm_open(struct inode *inode, struct file *filp)
{
  return 0;
}

/* dummy function. */
static int zsrm_release(struct inode *inode, struct file *filp)
{
  return 0;
}


static ssize_t zsrm_write
(struct file *file, const char *buf, size_t count, loff_t *offset)
{
  //int err;
  int need_reschedule=0;
  int res = 0, i;
  struct api_call call;
  unsigned long flags;
  
  /* copy data to kernel buffer. */
  if (copy_from_user(&call, buf, count)) {
    printk(KERN_WARNING "ZSRMM: failed to copy data.\n");
    return -EFAULT;
  }

  // try to lock semaphore to prevent concurrent syscalls
  // before disabling interrupts  
  if ((res = down_interruptible(&zsrmsem)) < 0){
    return res;
  } 

  // disable interrupts to avoid concurrent interrupts
  spin_lock_irqsave(&zsrmlock,flags);
  
  switch (call.api_id) {
  case CREATE_RESERVE:
    i = find_empty_entry();
    if (i==-1)
      res = -1;
    else {	
      res = 0;
      // check parameters
      if (call.args.reserve_parameters.bound_to_cpu > (num_online_cpus()-1) ||
	  (call.args.reserve_parameters.bound_to_cpu <0)){
	res = -1;
	break;
      }
      memcpy(&reserve_table[i].params,
	     &(call.args.reserve_parameters), 
	     sizeof(struct zs_reserve_params));
      reserve_table[i].effective_priority = reserve_table[i].params.priority;
      reserve_table[i].params.name[RESERVE_NAME_LEN-1]='\0';
      reserve_table[i].nominal_exectime_nanos = 
	reserve_table[i].params.execution_time.tv_sec * 1000000000L +
	reserve_table[i].params.execution_time.tv_nsec;
      reserve_table[i].overload_exectime_nanos = 
	reserve_table[i].params.overload_execution_time.tv_sec * 1000000000L +
	reserve_table[i].params.overload_execution_time.tv_nsec;
      if (reserve_table[i].params.reserve_type & CRITICALITY_RESERVE){
	reserve_table[i].params.normal_marginal_utility = reserve_table[i].params.criticality;
	reserve_table[i].params.overloaded_marginal_utility = reserve_table[i].params.criticality;
	reserve_table[i].params.critical_util_degraded_mode = -1;
	reserve_table[i].params.num_degraded_modes=0;
      } 
      if (reserve_table[i].params.reserve_type & PIPELINE_STAGE_TYPE_MASK){
	reserve_table[i].e2e_nominal_exectime_nanos = 
	  reserve_table[i].params.e2e_execution_time.tv_sec * 1000000000L +
	  reserve_table[i].params.e2e_execution_time.tv_nsec;
	reserve_table[i].e2e_overload_exectime_nanos = 
	  reserve_table[i].params.e2e_overload_execution_time.tv_sec * 1000000000L +
	  reserve_table[i].params.e2e_overload_execution_time.tv_nsec;
      }
      if (res >=0){
	create_reserve(i);
	res = i;
      }
    }
    break;
  case CREATE_MODAL_RESERVE:
    i = create_modal_reserve(call.args.num_modes);
    res = i;
    break;
  case SET_INITIAL_MODE_MODAL_RESERVE:
    if (VALID_MRID(call.args.set_initial_mode_params.modal_reserve_id) &&
	VALID_MID(call.args.set_initial_mode_params.initial_mode_id)){
      res = set_initial_mode_modal_reserve(call.args.set_initial_mode_params.modal_reserve_id,
					   call.args.set_initial_mode_params.initial_mode_id);
    } else {
      res = -1;
    }
    break;
  case ADD_RESERVE_TO_MODE:
    if (VALID_RID(call.args.rsv_to_mode_params.reserve_id) &&
	VALID_MRID(call.args.rsv_to_mode_params.modal_reserve_id) &&
	VALID_MID(call.args.rsv_to_mode_params.mode_id)){
      res= add_reserve_to_mode(call.args.rsv_to_mode_params.modal_reserve_id, 
			       call.args.rsv_to_mode_params.mode_id,
			       call.args.rsv_to_mode_params.reserve_id);
    } else {
      res = -1;
    }
    break;
  case ADD_MODE_TRANSITION:
    if (VALID_MRID(call.args.add_mode_transition_params.modal_reserve_id) &&
	VALID_TRID(call.args.add_mode_transition_params.transition_id)){
      res = add_mode_transition(call.args.add_mode_transition_params.modal_reserve_id, 
			      call.args.add_mode_transition_params.transition_id,
			      &call.args.add_mode_transition_params.transition);
    } else {
      res = -1;
    }  
    break;
  case ATTACH_MODAL_RESERVE:
    if (VALID_MRID(call.args.attach_params.reserveid)){    
      res = attach_modal_reserve(call.args.attach_params.reserveid,
				 call.args.attach_params.pid);
      modal_scheduling = 1;
    } else {
      res = -1;
    }
    break;
  case DETACH_MODAL_RESERVE:
    if (VALID_MRID(call.args.reserveid)){
      res = detach_modal_reserve(call.args.reserveid);
    } else {
      res = -1;
    }
    break;
  case DELETE_MODAL_RESERVE:
    if (VALID_MRID(call.args.reserveid)){
      res = delete_modal_reserve(call.args.reserveid);
    } else {
      res = -1;
    }
    break;
  case CREATE_SYS_TRANSITION:
    res = create_sys_mode_transition();
    break;
  case ADD_TRANSITION_TO_SYS_TRANSITION:
    if (VALID_SYS_TRANSITION_ID(call.args.trans_to_sys_trans_params.sys_transition_id)&&
	VALID_MRID(call.args.trans_to_sys_trans_params.mrid) && 
	VALID_TRID(call.args.trans_to_sys_trans_params.transition_id)){
      res = add_transition_to_sys_transition(call.args.trans_to_sys_trans_params.sys_transition_id,
					     call.args.trans_to_sys_trans_params.mrid,
					     call.args.trans_to_sys_trans_params.transition_id);
    } else {
      res = -1;
    }
    break;
  case DELETE_SYS_TRANSITION:
    if (VALID_SYS_TRANSITION_ID(call.args.reserveid)){
      res = delete_sys_mode_transition(call.args.reserveid);
    } else {
      res = -1;
    }
    break;
  case MODE_SWITCH:
    if (VALID_SYS_TRANSITION_ID(call.args.mode_switch_params.transition_id)){
      request_mode_change_ts_ns = ticks2nanos(rdtsc());
      sys_mode_transition(call.args.mode_switch_params.transition_id);
      mode_change_signals_sent_ts_ns=ticks2nanos(rdtsc());
      mode_change_signals_sent_ts_ns -= request_mode_change_ts_ns;
#ifdef __ARMCORE__
      mode_change_latency_ns_avg = mode_change_latency_ns_avg + mode_change_signals_sent_ts_ns;
      do_div(mode_change_latency_ns_avg,2);
#else
      mode_change_latency_ns_avg = (mode_change_latency_ns_avg + 
				    mode_change_signals_sent_ts_ns) / 2;
#endif
      if (mode_change_signals_sent_ts_ns > mode_change_latency_ns_worst)
	mode_change_latency_ns_worst = mode_change_signals_sent_ts_ns;
      
      need_reschedule = 1;
    } else {
      res = -1;
    }
    break;
  case ATTACH_RESERVE:
    if (VALID_RID(call.args.attach_params.reserveid)){
      res = attach_reserve(call.args.attach_params.reserveid, 
			   call.args.attach_params.pid);
    } else {
      res = -1;
    }
    break;
  case DETACH_RESERVE:
    if (VALID_RID(call.args.reserveid)){
      res = detach_reserve(call.args.reserveid);
    } else {
      res = -1;
    }
    break;
  case DELETE_RESERVE:
    if (VALID_RID(call.args.reserveid)){
      res = delete_reserve(call.args.reserveid);
    } else {
      res = -1;
    }
    break;
  case MODAL_WAIT_NEXT_PERIOD:
    if (VALID_MRID(call.args.reserveid)){
      res = modal_wait_for_next_period(call.args.reserveid);
      need_reschedule = 1;
    } else {
      res = -1;
    }
    break;
  case WAIT_NEXT_PERIOD:
    if (VALID_RID(call.args.reserveid)){
      res = wait_for_next_period(call.args.reserveid);
      need_reschedule = 1;
    } else {
      res = -1;
    }
    break;
  case INITIALIZE_NODE:
    res = start_arrival_task(call.args.initialize_node_params.fds,
			     call.args.initialize_node_params.nfds);
    break;
  case WAIT_NEXT_ARRIVAL:
    if (VALID_RID(call.args.wait_next_arrival_params.reserveid)){
      res = wait_for_next_arrival(call.args.wait_next_arrival_params.reserveid, 
				  call.args.wait_next_arrival_params.fds, 
				  call.args.wait_next_arrival_params.nfds,
				  &flags);
      need_reschedule = 1;
    } else {
      res = -1;
    }
    break;
  case WAIT_NEXT_LEAF_STAGE_ARRIVAL:
    if (VALID_RID(call.args.wait_next_leaf_stage_arrival_params.reserveid)){
      if (!(reserve_table[call.args.wait_next_leaf_stage_arrival_params.reserveid].params.reserve_type & PIPELINE_STAGE_LEAF)){
	printk("zsrmm: wait_next_leaf_stage_arrival call on wrong reserve (rid:%d) type(%X)\n",call.args.wait_next_leaf_stage_arrival_params.reserveid, reserve_table[call.args.wait_next_leaf_stage_arrival_params.reserveid].params.reserve_type);
	res = -1;
      } else {
	res  = wait_for_next_leaf_stage_arrival(call.args.wait_next_leaf_stage_arrival_params.reserveid, &flags,
						call.args.wait_next_leaf_stage_arrival_params.fd,
						call.args.wait_next_leaf_stage_arrival_params.usrindata,
						call.args.wait_next_leaf_stage_arrival_params.usrindatalen,
						call.args.wait_next_leaf_stage_arrival_params.flags,
						call.args.wait_next_leaf_stage_arrival_params.addr,
						call.args.wait_next_leaf_stage_arrival_params.addr_len);
	if (res >= 0){
	  need_reschedule = 1;
	}
      }
    } else {
      res = -1;
    }
    break;
  case WAIT_NEXT_STAGE_ARRIVAL:
    if (VALID_RID(call.args.wait_next_stage_arrival_params.reserveid)){
      if (!(reserve_table[call.args.wait_next_stage_arrival_params.reserveid].params.reserve_type & PIPELINE_STAGE_MIDDLE)){
	res = -1;
      } else {
	res = wait_for_next_stage_arrival(call.args.wait_next_stage_arrival_params.reserveid,
					  &flags,
					  call.args.wait_next_stage_arrival_params.fd,
					  call.args.wait_next_stage_arrival_params.usrdata,
					  call.args.wait_next_stage_arrival_params.usrdatalen,
					  call.args.wait_next_stage_arrival_params.flags,
					  call.args.wait_next_stage_arrival_params.outaddr,
					  call.args.wait_next_stage_arrival_params.outaddrlen,
					  call.args.wait_next_stage_arrival_params.inaddr,
					  call.args.wait_next_stage_arrival_params.inaddrlen,
					  call.args.wait_next_stage_arrival_params.io_flag);
	if (res >=0){
	  need_reschedule = 1;
	}
      }
    } else {
      res = -1;
    }
    break;
  case WAIT_NEXT_ROOT_PERIOD:
    if (VALID_RID(call.args.wait_next_root_stage_period_params.reserveid)){
      if (!(reserve_table[call.args.wait_next_root_stage_period_params.reserveid].params.reserve_type & PIPELINE_STAGE_ROOT)){
	printk("zsrmm: eror in wait_next_root_period rid(%d) wrong type(0x%X)\n",
	       call.args.wait_next_root_stage_period_params.reserveid,
	       reserve_table[call.args.wait_next_root_stage_period_params.reserveid].params.reserve_type);
	res = -1;
      } else {
	res = wait_for_next_root_period(call.args.wait_next_root_stage_period_params.reserveid, 
					&flags,
					call.args.wait_next_root_stage_period_params.fd,
					call.args.wait_next_root_stage_period_params.usroutdata,
					call.args.wait_next_root_stage_period_params.usroutdatalen,
					call.args.wait_next_root_stage_period_params.flags,
					call.args.wait_next_root_stage_period_params.addr,
					call.args.wait_next_root_stage_period_params.addr_len);
	if (res >=0){
	  need_reschedule = 1;
	} else {
	  printk("zsrmm: error in wait_next_root_period rid(%d) executing wait_for_next_root_period() internal call\n",
		 call.args.wait_next_root_stage_period_params.reserveid);
	}
      }
    } else {
      res = -1;
    }
    break;
  case NOTIFY_ARRIVAL:
    res = notify_arrival(call.args.notify_arrival_params.fds,
			 call.args.notify_arrival_params.nfds);
    need_reschedule = 1;
    break;
  case GET_SCHEDULER_PRIORITY:
    res = scheduler_priority;
    break;
  case RAISE_PRIORITY_CRITICALITY:
    {
      int rsvid = pid2reserve(current->pid);
      if (rsvid != -1){
	res = raise_priority_criticality(rsvid,
					 call.args.raise_priority_criticality_params.priority_ceiling,
					 call.args.raise_priority_criticality_params.criticality_ceiling);
	need_reschedule = 1;
      } else {
	res = -1;
      }
      break;
    }
  case RESTORE_BASE_PRIORITY_CRITICALITY:
    {
      int rsvid = pid2reserve(current->pid);
      if (rsvid != -1){
	res = restore_base_priority_criticality(rsvid);
	need_reschedule = 1;
      } else {
	res = -1;
      }
      break;
    }
  case GET_JIFFIES_MS:
    res = get_jiffies_ms();
    break;
  case PRINT_STATS:
    res=print_stats();
    break;
  case GET_PIPELINE_HEADER_SIZE:
    res = sizeof (struct pipeline_header_with_signature);
    break;
  case GET_PIPELINE_HEADER_SIGNATURE:
    res = MODULE_SIGNATURE;
    break;
  case GET_NEXT_DEBUG_EVENT:
    {
      struct zsrm_debug_trace_record *rec;
      rec = zsrm_next_debug_event();
      if (rec != NULL){
	call.args.next_debug_event_params.timestamp= rec->timestamp;
	call.args.next_debug_event_params.event_type = rec->event_type;
	call.args.next_debug_event_params.event_param = rec->event_param;
	if (copy_to_user((void *)buf, (void *)&call, count)) {
	  printk(KERN_WARNING "ZSRMM: failed to copy data to user.\n");
	  res= -EFAULT;
	}	
	res = 1;
      } else {
	res = 0;
      }
    }
    break;
  case RESET_DEBUG_TRACE_WRITE_INDEX:
    zsrm_debug_trace_write_index = 0;
    res = 0;
    break;
  case RESET_DEBUG_TRACE_READ_INDEX:
    zsrm_debug_trace_read_index = 0;
    res = 0;
    break;
  default: /* illegal api identifier. */
    res = -1;
    printk(KERN_WARNING "MZSRMM: illegal API identifier.\n");
    break;
  }

  // enable interrupts
  spin_unlock_irqrestore(&zsrmlock,flags);

  // allow other syscalls
  up(&zsrmsem);

  if (need_reschedule){
    schedule();
  }
  return res;
}

// dummy
//#if LINUX_KERNEL_MINOR_VERSION >= 37 /* we don't believe LINUX_VERSION_CODE */
static long zsrm_ioctl(struct file *file,
		       unsigned int cmd, 
		       unsigned long arg)
/*#else
static int zsrm_ioctl(struct inode *inode,
struct file *file,
unsigned int cmd, 
unsigned long arg)
#endif
*/
{
	return 0;
}

static struct file_operations zsrm_fops;
/*
 = {
	.owner = THIS_MODULE,
	.open = zsrm_open, / do nothing but must exist. /
	.release = zsrm_release, / do nothing but must exist. /
	.read = NULL,
	.write = zsrm_write,
#if LINUX_KERNEL_MINOR_VERSION >= 37
	.unlocked_ioctl = zsrm_ioctl
#else
	.ioctl = zsrm_ioctl
#endif
};
*/

static void
zsrm_cleanup_module(){
  int i;
  unsigned long flags;

  if (dev_class){
    device_destroy(dev_class, MKDEV(dev_major, 0));
  }
  /* delete the char device. */
  cdev_del(&c_dev);

  if (dev_class)
    class_destroy(dev_class);
  /* return back the device number. */
  unregister_chrdev_region(dev_id, 1);	

  printk("zsrm.cleanup(): about to cancel enforcer timers\n");
  //lock 
  spin_lock_irqsave(&zsrmlock,flags);
  for (i=0;i<MAX_CPUS;i++){
    hrtimer_cancel(&exectime_enforcer_timers[i]);
  }
  spin_unlock_irqrestore(&zsrmlock,flags);
  
}

static int __init zsrm_init(void)
{
  int ret;
  int i;
  int err = 0;
  dev_t devno;
  struct device *device = NULL;
  struct sched_param p;
  //struct timespec ts1;
  //struct timespec ts2;
  

  // initialize debugging pointers
  zsrm_debug_trace_read_index=0;
  zsrm_debug_trace_write_index=0;
  
  modal_scheduling = 0 ;
  
  // initialize scheduling top
  top = -1;

  for (i=0;i<MAX_CPUS;i++){
    active_reserve_ptr[i] = NULL;
    head_paused_reserve_ptr[i] = NULL;
    hrtimer_init(&exectime_enforcer_timers[i], CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    exectime_enforcer_timers[i].function = exectime_enforcer_handler;
  }

  // get do_sys_poll pointer

  do_sys_pollp = (int (*)(struct pollfd __user *ufds, unsigned int nfds,
			   struct timespec *end_time))
    kallsyms_lookup_name("do_sys_poll");

  if (((unsigned long)do_sys_pollp) == 0){
    printk("zsrmm: Failed to obtain do_sys_poll pointer\n");
    return -1;
  }

  sys_recvfromp = (long(*)(int fd, void __user *ubuf, size_t size, unsigned flags, struct sockaddr __user *addr, int __user *addr_len))
    kallsyms_lookup_name("sys_recvfrom");

  if ( ( (unsigned long)sys_recvfromp) == 0) {
    printk("zsrmm: Failed to obtained sys_recvfrom pointer\n");
    return -1;
  }

  sys_sendtop = (long (*)(int fd, void __user *ubuf, size_t size, unsigned flags, struct sockaddr __user *addr, int addr_len)) 
    kallsyms_lookup_name("sys_sendto");

  if (((unsigned long)sys_sendtop) == 0){
    printk("zsrmm: Failed to obtained sys_sendto pointer\n");
    return -1;
  }


  /* For time precision testing
  //getrawmonotonic(&ts1);
  //getrawmonotonic(&ts2);
  getnstimeofday(&ts1);
  getnstimeofday(&ts2);
  printk("zsrmm:init: ts1(%llu)\n",((unsigned long long)ts1.tv_sec) * 1000000000L + 
	 ((unsigned long long)ts1.tv_nsec));
  printk("zsrmm:init: ts2(%llu)\n",((unsigned long long)ts2.tv_sec) * 1000000000L + 
  ((unsigned long long) ts2.tv_nsec));
  */

  // initialize semaphore
  sema_init(&zsrmsem,1); // binary - initially unlocked

#ifdef __ARMCORE__
  start_tsc();
#endif
  
  init();
  printk(KERN_INFO "MZSRMM: HELLO!\n");
  
  calibrate_ticks();
  
  test_ticks();
  
  /* get the device number of a char device. */
  ret = alloc_chrdev_region(&dev_id, 0, 1, DEVICE_NAME);
  if (ret < 0) {
    printk(KERN_WARNING "MZSRMM: failed to allocate device.\n");
    return ret;
  }
  
  dev_major = MAJOR(dev_id);
  
  dev_class = class_create(THIS_MODULE,DEVICE_NAME);
  if (IS_ERR(dev_class)){
    printk(KERN_WARNING "ZSRMM: failed to create device class.\n");
    err = PTR_ERR(dev_class);
    dev_class = NULL;
    zsrm_cleanup_module();
    return err;
  }
  
  devno = MKDEV(dev_major, 0);
  
  // initialize the fops 
  zsrm_fops.owner = THIS_MODULE;
  zsrm_fops.open = zsrm_open;
  zsrm_fops.release = zsrm_release;
  zsrm_fops.read = NULL;
  zsrm_fops.write = zsrm_write;
  
  //#if LINUX_KERNEL_MINOR_VERSION < 37
  //zsrm_fops.ioctl = zsrm_ioctl;
  //#else
  zsrm_fops.unlocked_ioctl = zsrm_ioctl;
  //#endif
  
  /* initialize the char device. */
  cdev_init(&c_dev, &zsrm_fops);
  
  /* register the char device. */
  ret = cdev_add(&c_dev, dev_id, 1);
  if (ret < 0) {
    printk(KERN_WARNING "ZSRMM: failed to register device.\n");
    return ret;
  }
  
  device = device_create(dev_class, NULL, devno, NULL, DEVICE_NAME "%d", 0);
  
  if (IS_ERR(device)){
    err = PTR_ERR(device);
    printk(KERN_WARNING "Error %d while trying to create dev %s%d", err, DEVICE_NAME,0);
    cdev_del(&c_dev);
    return err;
  }
  
  sched_task = kthread_create((void *)scheduler_task, NULL, "ZSRMM scheduler thread");  
  p.sched_priority = scheduler_priority;
  sched_setscheduler(sched_task, SCHED_FIFO, &p);
  kthread_bind(sched_task, 0);
  // try to pint sched task in cpu where the enforcement happens
  //kthread_bind(sched_task, 3);

  printk(KERN_WARNING "MZSRMM: nanos per ten ticks: %lu \n",nanosPerTenTicks);
  printk(KERN_WARNING "MZSRMM: ready!\n");

  zsrm_add_debug_event(timestamp_ns(),ZSRM_DEBUG_EVENT_MODULE_STARTED,0);
    
  return 0;
}

int start_arrival_task(struct pollfd __user *fds, unsigned int nfds){
  struct arrival_server_task_params params;
  struct sched_param p;

  params.fds = fds;
  params.nfds = nfds;

  arrival_task = kthread_create((void *) arrival_server_task, (void *)&params, "ASRM Arrival Server Task");
  p.sched_priority = scheduler_priority;
  sched_setscheduler(arrival_task, SCHED_FIFO,&p);
  kthread_bind(arrival_task,0);
  return 0;
}

void test_accounting(void){

  int rsvid1 = 0;
  int rsvid2 = 1;
  int rsvid3 = 2;

  // simulate tasks 
  // T1=100 ms, C1= 50
  // T2=200 ms, C2 = 50
  // T3=400 ms, C3 = 50

  unsigned long waituntil;
  unsigned long jiffies10ms = HZ /100;
  unsigned long jiffies50ms = (HZ *5) / 100;
  unsigned long jiffies100ms = HZ / 10;
  unsigned long jiffies200ms = (HZ * 2) / 10;
  unsigned long jiffies400ms = (HZ * 4) /10;
  unsigned long t1nextarrival;
  unsigned long t2nextarrival;
  unsigned long t3nextarrival;

  reserve_table[rsvid1].job_executing_nanos = 0L;
  reserve_table[rsvid2].job_executing_nanos = 0L;
  reserve_table[rsvid3].job_executing_nanos = 0L;

  reserve_table[rsvid1].params.priority=10;
  reserve_table[rsvid2].params.priority=9;
  reserve_table[rsvid3].params.priority=8;
  
  printk("zsrm.test_acct: HZ(%d)\n",HZ);
  printk("zsrm.test_acct: jiffiesx1(%lu)\n",jiffies10ms);
  printk("zsrm.test_acct: jiffiesx5(%lu)\n",jiffies50ms);
  printk("zsrm.test_acct: jiffiesx10(%lu)\n",jiffies100ms);
  printk("zsrm.test_acct: jiffiesx20(%lu)\n",jiffies200ms);
  printk("zsrm.test_acct: jiffiesx40(%lu)\n",jiffies400ms);

  // arrival of t3
  start_accounting(&reserve_table[rsvid3]);
  waituntil = jiffies + jiffies10ms;
  t3nextarrival = jiffies+jiffies400ms;
  while (time_before(jiffies, waituntil))
    ;

  //arrival of t2
  start_accounting(&reserve_table[rsvid2]);
  printk("zsrm: t3 c: %lld\n",reserve_table[rsvid3].job_executing_nanos);
  waituntil = jiffies + jiffies10ms;
  t2nextarrival = jiffies+jiffies200ms;
  while (time_before(jiffies, waituntil))
    ;

  //arrival of t1
  start_accounting(&reserve_table[rsvid1]);
  printk("zsrm: t2 c: %lld\n",reserve_table[rsvid2].job_executing_nanos);
  waituntil = jiffies + jiffies50ms;
  t1nextarrival = jiffies+jiffies100ms;
  while (time_before(jiffies, waituntil))
    ;

  stop_accounting(&reserve_table[rsvid1],UPDATE_HIGHEST_PRIORITY_TASK,0,0);
  printk("zsrm: t1 c: %lld\n",reserve_table[rsvid1].job_executing_nanos);
  reserve_table[rsvid1].job_executing_nanos = 0L;

  waituntil = jiffies + (jiffies10ms *4);
  while(time_before(jiffies,waituntil))
    ;

  stop_accounting(&reserve_table[rsvid2],1,0,0);
  printk("zsrm: t2 c: %lld\n",reserve_table[rsvid2].job_executing_nanos);
  printk("zsrm: t3 c: %lld\n",reserve_table[rsvid3].job_executing_nanos);
  reserve_table[rsvid2].job_executing_nanos = 0L;
  waituntil = jiffies+(jiffies10ms * 1);
  while(time_before(jiffies,waituntil))
    ;
  
  // next t1 arrival
  start_accounting(&reserve_table[rsvid1]);
  printk("zsrm: t3 c: %lld\n",reserve_table[rsvid3].job_executing_nanos);
  waituntil = jiffies+ jiffies50ms;
  while(time_before(jiffies,waituntil))
    ;

  stop_accounting(&reserve_table[rsvid1],1,0,0);
  printk("zsrm: t1 c: %lld\n",reserve_table[rsvid1].job_executing_nanos);
  printk("zsrm: t2 c: %lld\n",reserve_table[rsvid2].job_executing_nanos);
  printk("zsrm: t3 c: %lld\n",reserve_table[rsvid3].job_executing_nanos);

}

static void __exit zsrm_exit(void)
{
  if (timer_started)
    hrtimer_cancel(&ht);
  
  // empty rescheduling stack
  top = -1;
  kthread_stop(sched_task);
  if (arrival_task != NULL){
    kthread_stop(arrival_task);
  }

  delete_all_reserves();
  
  delete_all_modal_reserves();
  
  printk(KERN_INFO "MZSRMM: GOODBYE!\n");
  
  zsrm_cleanup_module();  
}

module_init(zsrm_init);
module_exit(zsrm_exit);
