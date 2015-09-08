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
//#include "accounting.h"
#include "zsrm.h"


int (*do_sys_pollp)(struct pollfd __user *ufds, unsigned int nfds,
				struct timespec *end_time);

MODULE_AUTHOR("Dionisio de Niz");
MODULE_LICENSE("GPL");

// some configuration options -- transform into CONFIG_*
#define __ZSRM_TSC_CLOCK__

#define DEVICE_NAME "zsrmm"

#ifdef __ARMCORE__

#define PMCNTNSET_C_BIT		0x80000000
#define PMCR_D_BIT		0x00000008
#define PMCR_C_BIT		0x00000004
#define PMCR_P_BIT		0x00000002
#define PMCR_E_BIT		0x00000001

#endif



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

struct zs_reserve *active_reserve_ptr = NULL;
struct zs_reserve *head_paused_reserve_ptr = NULL;

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

struct zs_reserve reserve_table[MAX_RESERVES];
struct zs_modal_reserve modal_reserve_table[MAX_MODAL_RESERVES];
struct zs_modal_transition_entry sys_mode_transition_table[MAX_SYS_MODE_TRANSITIONS][MAX_TRANSITIONS_IN_SYS_TRANSITIONS];

#ifdef __ZSRM_TSC_CLOCK__
#define now2timespec(ts) nanos2timespec(ticks2nanos(rdtsc()),ts)
#else
#define now2timespec(ts) jiffies_to_timespec(jiffies,ts)
#endif

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
  printk("avg mode change latency: %llu (ns) \n",mode_change_latency_ns_avg );
  printk("worst mode change latency: %llu (ns) \n",mode_change_latency_ns_worst);
  printk("avg arrival no mode change %llu (ns) \n",arrival_no_mode_ns_avg);
  printk("worst arrival no mode change %llu (ns) \n", arrival_no_mode_ns_worst);
  printk("avg arrival mode change %llu (ns) \n",arrival_mode_ns_avg);
  printk("worst arrival mode change %llu (ns) \n",arrival_mode_ns_worst);
  printk("avg transition into suspension %llu (ns) \n",transition_into_susp_ns_avg);
  printk("worst transition into suspension %llu (ns) \n",transition_into_susp_ns_worst);
  printk("avg transition from suspension %llu (ns)\n",transition_from_susp_ns_avg);
  printk("worst transition_from_susp_ns_worst %llu (ns) \n",transition_from_susp_ns_worst);
  printk("avg enforcement latency: %llu (ns) \n",enforcement_latency_ns_avg);
  printk("worst enforcement latency: %llu (ns) \n",enforcement_latency_ns_worst);
  return 0;
}

void add_paused_reserve(struct zs_reserve *rsv){
  struct zs_reserve *tmp;
  if (head_paused_reserve_ptr == NULL){
    rsv->next_paused_lower_criticality = NULL;
    head_paused_reserve_ptr = rsv;
  } else if (head_paused_reserve_ptr->params.overloaded_marginal_utility < rsv->params.overloaded_marginal_utility) {
    rsv->next_paused_lower_criticality = head_paused_reserve_ptr;
    head_paused_reserve_ptr = rsv;
  } else {
    tmp = head_paused_reserve_ptr;
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

  if (head_paused_reserve_ptr == rsv){
    head_paused_reserve_ptr = head_paused_reserve_ptr->next_paused_lower_criticality;
    rsv->next_paused_lower_criticality = NULL;
  } else {
    tmp = head_paused_reserve_ptr;
    while(tmp != NULL && 
	  tmp->next_paused_lower_criticality != rsv){
      tmp = tmp->next_paused_lower_criticality;
    }
    tmp =rsv->next_paused_lower_criticality;
    rsv->next_paused_lower_criticality = NULL;
  }
}

void kill_paused_reserves(int criticality){
  struct zs_reserve *tmp = head_paused_reserve_ptr;
  struct zs_reserve *prev = NULL;

  while(tmp != NULL && tmp->params.overloaded_marginal_utility >= criticality){
    tmp = tmp->next_paused_lower_criticality;
  }

  while (tmp != NULL){
    kill_reserve(tmp);
    prev = tmp;
    if (tmp == head_paused_reserve_ptr)
      head_paused_reserve_ptr = tmp->next_paused_lower_criticality;
    tmp=tmp->next_paused_lower_criticality;
    prev->next_paused_lower_criticality = NULL;
  }
}

void resume_paused_reserves(int criticality){
  struct zs_reserve *tmp = head_paused_reserve_ptr;
  struct zs_reserve *prev = NULL;

  while(tmp != NULL && tmp->params.overloaded_marginal_utility >= criticality){
    resume_reserve(tmp);
    prev = tmp;
    if (head_paused_reserve_ptr == tmp)
      head_paused_reserve_ptr = tmp->next_paused_lower_criticality;
    tmp = tmp->next_paused_lower_criticality;
    prev->next_paused_lower_criticality = NULL;
  }
}

// kill always happens on a paused reserve once the overloading
// job finishes, so the next job arrival revives the reserve
void kill_reserve(struct zs_reserve *rsv){
  struct task_struct *task;
  struct sched_param p;
  task = gettask(rsv->pid);
  if (task != NULL){
    // restore priority
    p.sched_priority = rsv->effective_priority;
    sched_setscheduler(task, SCHED_FIFO, &p);
    printk("zsrmm.kill_reserve: setting rid(%d) TASK_INTERRUPTIBLE\n",rsv->rid);
    set_task_state(task, TASK_INTERRUPTIBLE);
    set_tsk_need_resched(task);
  }

  // jumps directly to wait for next period
  rsv->execution_status = EXEC_WAIT_NEXT_PERIOD;
  rsv->critical_utility_mode_enforced=0;
  /* --- cancel all timers --- */
  if (rsv->params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
    hrtimer_cancel(&(rsv->response_time_timer));
  }
  hrtimer_cancel(&(rsv->zs_timer));
}

void pause_reserve(struct zs_reserve *rsv){
  add_paused_reserve(rsv);
  if (rsv->execution_status == EXEC_RUNNING){
    // stop the task
    rsv->critical_utility_mode_enforced = 1;
    rsv->just_returned_from_degradation=0;
    rsv->request_stop = 1;
    push_to_reschedule(rsv->rid);
  } else {
    printk("zsrmm.pause_reserve: reserve != EXEC_RUNNING status(%X)\n",rsv->execution_status);
  }
  rsv->execution_status |= EXEC_ENFORCED_PAUSED;
}

void resume_reserve(struct zs_reserve *rsv){
  struct task_struct *task;
  // double check that it was paused
  //if (rsv->execution_status & EXEC_ENFORCED_PAUSED){
    if (rsv->execution_status & EXEC_RUNNING){
      rsv->execution_status = EXEC_RUNNING;
      task = gettask(rsv->pid);
      if (task == NULL && (task->state & TASK_INTERRUPTIBLE ||
			   task->state & TASK_UNINTERRUPTIBLE)){
	wake_up_process(task);
      }
      // reschedule again
      // rsv->effective_priority = rsv->params.priority;
      push_to_reschedule(rsv->rid);
      rsv->current_degraded_mode = -1;
      rsv->just_returned_from_degradation=1;
    } else if (rsv->execution_status & EXEC_WAIT_NEXT_PERIOD){
      rsv->execution_status = EXEC_WAIT_NEXT_PERIOD;
    } else {
      printk("zsrmm.resume_reserve: rid(%d) not running or waiting status(%X)\n",rsv->rid, rsv->execution_status);
    }
    //} else {
    //printk("zsrm. erroneous execution_status found while resuming running reserve");
    //}
}

void start_accounting(struct zs_reserve *rsv){
  if (active_reserve_ptr == rsv){
    //printk("zsrm.start_acct: ptr(%d) == rsv(%d)\n",active_reserve_ptr->rid,rsv->rid);
    // only start accounting
    rsv->job_resumed_timestamp_nanos = ticks2nanos(rdtsc());
  } else if (active_reserve_ptr == NULL){
    //printk("zsrm.start_acct: ptr == NULL adding rsv(%d)\n",rsv->rid);
    active_reserve_ptr = rsv;
    rsv->next_lower_priority = NULL;
    rsv->job_resumed_timestamp_nanos = ticks2nanos(rdtsc());
  } else if (active_reserve_ptr->params.priority < rsv->params.priority){
    //printk("zsrm.start_acct: ptr(%d) preempted by rsv(%d)\n",active_reserve_ptr->rid,rsv->rid);
    stop_accounting(active_reserve_ptr, 
		    DONT_UPDATE_HIGHEST_PRIORITY_TASK);
    rsv->next_lower_priority = active_reserve_ptr;
    active_reserve_ptr = rsv;
    rsv->job_resumed_timestamp_nanos = ticks2nanos(rdtsc());
  } else{
    struct zs_reserve *tmp = active_reserve_ptr;
    //printk("zsrm.start_acct: ptr(%d) top prio. rsv(%d) to queue\n",active_reserve_ptr->rid,rsv->rid);
    while (tmp->next_lower_priority != NULL && 
	   tmp->next_lower_priority->params.priority >= rsv->params.priority)
      tmp = tmp->next_lower_priority;
    rsv->next_lower_priority = tmp->next_lower_priority;
    tmp->next_lower_priority = rsv;
  }
}

void stop_accounting(struct zs_reserve *rsv, int update_active_highest_priority){
  unsigned long long job_stop_timestamp_nanos;
  if (active_reserve_ptr == rsv){
    //printk("zsrm.stop_acct: ptr(%d) == rsv(%d) -- stopping\n",active_reserve_ptr->rid,rsv->rid);
    job_stop_timestamp_nanos = ticks2nanos(rdtsc());
    rsv->job_executing_nanos += job_stop_timestamp_nanos - 
      rsv->job_resumed_timestamp_nanos;
    if (update_active_highest_priority){
      active_reserve_ptr = rsv->next_lower_priority;
      if (active_reserve_ptr != NULL)
	start_accounting(active_reserve_ptr);
    }
  } else {
    //printk("zsrm.stop_acct: ptr(%d) != rsv(%d) not top priority\n",active_reserve_ptr->rid,rsv->rid);
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
  printk(KERN_INFO "add_trans_to_sys_trans stid: %d mird: %d mtid: %d\n",
	 stid, mrid, mtid);
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

  printk("mode(%d) change signal sent\n",newmode);
  return 0;
}

unsigned long long latest_deadline_ns=0ll;
unsigned long long end_of_transition_interval_ns=0ll;

int sys_mode_transition(int stid){
  int i;

  // record the latest deadline among the running jobs as the end of transition.
  end_of_transition_interval_ns = latest_deadline_ns;

  printk(KERN_INFO "sys_mode_transition stid: %d end of transition: %llu ns\n",stid,end_of_transition_interval_ns);

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

struct task_struct *sched_task;

static void scheduler_task(void *a){
	int rid;
	struct sched_param p;
	struct task_struct *task;

	while (!kthread_should_stop()) {
		while ((rid = pop_to_reschedule()) != -1) {
		  task = gettask(reserve_table[rid].pid);
			if (task == NULL){
				printk("scheduler_task : wrong pid(%d) for rid(%d)\n",reserve_table[rid].pid,rid);
				continue;
			}
			if (reserve_table[rid].request_stop){
				reserve_table[rid].request_stop = 0;
				
				//set_task_state(task, TASK_INTERRUPTIBLE);
				p.sched_priority = 0;
				sched_setscheduler(task, SCHED_NORMAL, &p);
				stop_accounting(&reserve_table[rid],
						UPDATE_HIGHEST_PRIORITY_TASK);

				// measurement of enforcement
				end_of_enforcement_ts_ns = ticks2nanos(rdtsc());
				end_of_enforcement_ts_ns -= start_of_enforcement_ts_ns;
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
			}
		}
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

enum hrtimer_restart zs_instant_handler(struct hrtimer *timer){
	int rid;
	unsigned long flags;

	spin_lock_irqsave(&zsrmlock,flags);
	rid = timer2reserve(timer);
	printk("zs_instant of reserveid:%d\n",rid);
	if (rid == -1){
		printk("zs_timer without reserve\n");
		spin_unlock_irqrestore(&zsrmlock,flags);
		return HRTIMER_NORESTART;
	}
	// currently running not paused or dropped
	if (reserve_table[rid].execution_status == EXEC_RUNNING)
	  period_degradation(rid);
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

	start_of_enforcement_ts_ns = ticks2nanos(rdtsc());

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

	//set_task_state(task,TASK_INTERRUPTIBLE);
	//set_tsk_need_resched(task);
	reserve_table[rid].request_stop =1;
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

	arrival_ts_ns = ticks2nanos(rdtsc());

	if (prev_ns == 0){
	  prev_ns = now_ns;
	} else {
	  //unsigned long long dur_ns = (now_ns - prev_ns);
	  prev_ns = now_ns;
	  //printk(KERN_INFO "period_handler. elapse ns from previous: %llu\n",dur_ns);
	}

	rid = timer2reserve(timer);
	if (rid == -1){
		printk("timer without reserve!");
		spin_unlock_irqrestore(&zsrmlock,flags);
		return HRTIMER_NORESTART;
	}

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
		rid = modal_reserve_table[mrid].reserve_for_mode[newmode];
		modal_reserve_table[mrid].mode = newmode;
		//printk(KERN_INFO "start of period: activating mode %d rid %d\n",newmode, rid);
		// transition into non-suspension mode
		
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
	      //printk(KERN_INFO "End of transition interval\n");
	    }
	  }
	}

	// if not in modal_scheduling the newmode == 0 => != DISABLED_MODE
	if (newmode != DISABLED_MODE){
	  p = get_current_effective_period(rid);

	  //wake_up_process(task);
	  //printk(KERN_INFO "Wakeup(task) rid(%d) 1\n",rid);

	  printk(KERN_INFO "period timer rid(%d) : %ld secs: %ld ns \n",rid,p->tv_sec, p->tv_nsec);
	  kperiod = ktime_set(p->tv_sec,p->tv_nsec);
	  hrtimer_init(&(reserve_table[rid].period_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	  reserve_table[rid].period_timer.function = period_handler;
	  hrtimer_start(&(reserve_table[rid].period_timer), kperiod, HRTIMER_MODE_REL);

	  if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
	    // cancelled at begining of function
	    //hrtimer_cancel(&(reserve_table[rid].response_time_timer));
	    kresptime = ktime_set(reserve_table[rid].params.response_time_instant.tv_sec,
				  reserve_table[rid].params.response_time_instant.tv_nsec);
	    hrtimer_init(&(reserve_table[rid].response_time_timer), CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	    reserve_table[rid].response_time_timer.function = response_time_handler;
	    hrtimer_start(&(reserve_table[rid].response_time_timer),kresptime, HRTIMER_MODE_REL);
	  }
	  //cancelled at start of function
	  //hrtimer_cancel(&(reserve_table[rid].zs_timer));

	  if (modal_scheduling && modal_reserve_table[mrid].in_transition){
	    struct timespec *trans_zsp = &(modal_reserve_table[mrid].transitions[modal_reserve_table[mrid].active_transition].zs_instant);
	    kzs = ktime_set(trans_zsp->tv_sec,trans_zsp->tv_nsec);
	  } else {
	    kzs = ktime_set(reserve_table[rid].params.zs_instant.tv_sec, 
			    reserve_table[rid].params.zs_instant.tv_nsec);
	    //printk("period_handler: zs_instant sec:%lu nsec:%lu\n ",reserve_table[rid].params.zs_instant.tv_sec,
	    //   reserve_table[rid].params.zs_instant.tv_nsec);
	  }
	  hrtimer_init(&(reserve_table[rid].zs_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	  reserve_table[rid].zs_timer.function = zs_instant_handler;
	  hrtimer_start(&(reserve_table[rid].zs_timer), kzs, HRTIMER_MODE_REL);
	  //jiffies_to_timespec(jiffies, &reserve_table[rid].start_of_period);
	  now2timespec(&reserve_table[rid].start_of_period);
	  record_latest_deadline(&(reserve_table[rid].params.period));
	  //printk("waking up process: %d at %ld:%ld\n",reserve_table[rid].pid,now.tv_sec, now.tv_nsec);
	  reserve_table[rid].effective_priority = (reserve_table[rid].current_degraded_mode == -1) ? reserve_table[rid].params.priority :
	    reserve_table[rid].params.degraded_priority[reserve_table[rid].current_degraded_mode];
	  // only wake it up if it was not enforced
	  if (reserve_table[rid].critical_utility_mode_enforced == 0 &&
	      !(reserve_table[rid].execution_status & EXEC_ENFORCED_DROPPED) &&
	      !(reserve_table[rid].execution_status & EXEC_ENFORCED_PAUSED) ){

	    // moved the wakeup call here to ensure only waking up when not enforced
	    reserve_table[rid].execution_status = EXEC_RUNNING;
	    wake_up_process(task);
	    printk(KERN_INFO "Wakeup(task) rid(%d) 1\n",rid);
	    push_to_reschedule(rid);
	    wake_up_process(sched_task);
	    set_tsk_need_resched(task);
	    set_tsk_need_resched(current);
	  }
	  //set_tsk_need_resched(sched_task);
	}

	// no modal_scheduling => !mode_change
	if (!mode_change){
	  no_mode_change_ts_ns = ticks2nanos(rdtsc());
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

  printk("MZSRMM.mode_transition: pid(%d) from mode(%d) to mode(%d) current mode(%d)\n",modal_reserve_table[mrid].pid, debug_from_mode,debug_to_mode,mode);


  if (mode == DISABLED_MODE){
    // wake up the task immediately
    printk(KERN_INFO "mode_transition from DISABLED_RESERVE\n");
    wake_up_process(task);
    //set_task_state(task,); // do we need this?
    printk(KERN_INFO "Wakeup(task) 2\n");
    
    set_tsk_need_resched(task);
    
    // and install the reservation immediately with the target reservation
    rid = modal_reserve_table[mrid].reserve_for_mode[modal_reserve_table[mrid].transitions[tid].to_mode];
    modal_reserve_table[mrid].mode = modal_reserve_table[mrid].transitions[tid].to_mode;
    attach_reserve_transitional(rid, modal_reserve_table[mrid].pid, 
				&(modal_reserve_table[mrid].transitions[tid].zs_instant));

  } else {
    struct timespec *zsts = &(modal_reserve_table[mrid].transitions[tid].zs_instant);

    rid = modal_reserve_table[mrid].reserve_for_mode[mode];
    printk(KERN_INFO "mode_transition() rid: %d\n",rid );


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

    printk(KERN_INFO "mode_transition. tid: %d\n",tid);

    // we will point to target reserve in next period arrival

    if (abs_zsi <= now_ns ){
      // transitional zs instant already elapsed.
      // Immediate enforcement
      printk(KERN_INFO "mode_transition: immediate period_degradation\n");
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

      printk("mode_transition: delayed transitional zsi to expire in %lu:%lu \n",time_to_zsi.tv_sec, time_to_zsi.tv_nsec);

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
	p.sched_priority = reserve_table[rid].params.priority;
	sched_setscheduler(task,SCHED_FIFO, &p);

	// if NOT APERIODIC  arrival then it is periodic. Start periodic timer
	if (!(reserve_table[rid].params.reserve_type & APERIODIC_ARRIVAL)){
	  printk("zsrmm: attach_reserve. Programming periodic timer reserve_type(%x)\n",reserve_table[rid].params.reserve_type);
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
	start_accounting(&reserve_table[rid]);

	set_tsk_need_resched(task);
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

void finish_period_degradation(int rid){
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
  if (reserve_table[rid].job_executing_nanos > reserve_table[rid].nominal_exectime_nanos){
    // overloaded. Kill paused jobs
    printk("rid(%d) overloaded killing paused reserves\n",rid);
    kill_paused_reserves(reserve_table[rid].params.overloaded_marginal_utility);
  } else {
    // did not overload. Resume jobs with new system_utility
    printk("rid(%d) DID NOT overload resuming paused reserves at criticality(%ld)\n",rid,system_utility);
    resume_paused_reserves(system_utility);
  }


  for (i=0; i< MAX_RESERVES; i++){
    // empty reserves
    if (reserve_table[i].params.priority == -1)
      continue; 
    
    /* if (reserve_table[i].params.overloaded_marginal_utility >= system_utility){ */
    /*   reserve_table[i].effective_priority = reserve_table[i].params.priority; */
    /*   reserve_table[i].current_degraded_mode = -1; */
    /*   reserve_table[i].just_returned_from_degradation=1; */
    /*   if ((reserve_table[i].execution_status & EXEC_ENFORCED_DROPPED) || */
    /* 	  (reserve_table[i].execution_status & EXEC_ENFORCED_PAUSED)){ */
    /* 	resume_reserve(&reserve_table[i]); */
    /*   } else { */
    /* 	push_to_reschedule(i); */
    /*   } */
    /* } else  */

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
  
  
  printk(KERN_INFO "period degradation:start\n");
  current_marginal_utility = GET_EFFECTIVE_UTILITY(rid);
  
  reserve_table[rid].in_critical_mode = 1;
  
  printk(KERN_INFO "period degradation rid(%d) curr utility: %ld, system_utility: %ld\n",rid,current_marginal_utility, system_utility);
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
    
    
    tmp_marginal_utility = GET_EFFECTIVE_UTILITY(i);
    
    printk(KERN_INFO "period degradation checking rid(%d) util:%ld sysutil:%ld\n",i,tmp_marginal_utility, system_utility);
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
	
	printk(KERN_INFO "period degrad: stopping task with rid(%d)\n",i);

	pause_reserve(&reserve_table[i]);

	/* reserve_table[i].request_stop = 1; */
	/* push_to_reschedule(i); */
	/* wake_up_process(sched_task); */
	/* set_tsk_need_resched(current); */
	
	
	//task = gettask(reserve_table[i].pid);
	/*
	  if (task != NULL){
	  task->state = TASK_INTERRUPTIBLE;
	  set_tsk_need_resched(task);
	  }
	*/
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

int wait_for_next_period(int rid){
  hrtimer_cancel(&(reserve_table[rid].zs_timer));
  if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
    hrtimer_cancel(&(reserve_table[rid].response_time_timer));
  }

  stop_accounting(&reserve_table[rid],UPDATE_HIGHEST_PRIORITY_TASK);

  if (reserve_table[rid].in_critical_mode){
    finish_period_degradation(rid);
  }
  if (reserve_table[rid].just_returned_from_degradation){
    reserve_table[rid].just_returned_from_degradation = 0;
    // Dio: it seems that this is unstable.
    //try_resume_normal_period(rid);
  }


  // reset the job_executing_nanos
  reserve_table[rid].job_executing_nanos=0L;

  reserve_table[rid].execution_status |= EXEC_WAIT_NEXT_PERIOD;
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
 * However in this one the sys_poll already takes user pointer.
 */
// called with zsrm_lock locked and will release it inside in order to block
int wait_for_next_arrival(int rid, struct pollfd __user *fds, unsigned int nfds, unsigned long *flags){
  int retval = 0;
  struct sched_param p;
  unsigned long long now_ns;
  ktime_t kzs;



  hrtimer_cancel(&(reserve_table[rid].zs_timer));
  if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
    hrtimer_cancel(&(reserve_table[rid].response_time_timer));
  }

  stop_accounting(&reserve_table[rid],UPDATE_HIGHEST_PRIORITY_TASK);

  if (reserve_table[rid].in_critical_mode){
    finish_period_degradation(rid);
  }
  if (reserve_table[rid].just_returned_from_degradation){
    reserve_table[rid].just_returned_from_degradation = 0;
    // Dio: it seems that this is unstable.
    //try_resume_normal_period(rid);
  }


  // reset the job_executing_nanos
  reserve_table[rid].job_executing_nanos=0L;

  reserve_table[rid].execution_status |= EXEC_WAIT_NEXT_PERIOD;

  p.sched_priority = 50;
  sched_setscheduler(current, SCHED_FIFO, &p);


  // release zsrm locks
  spin_unlock_irqrestore(&zsrmlock, *flags);
  up(&zsrmsem);
  
  do_sys_pollp(fds, (unsigned int) 1, NULL);

  now_ns = ticks2nanos(rdtsc());

  // reacquire zsrm locks
  down(&zsrmsem);
  spin_lock_irqsave(&zsrmlock, *flags);

  kzs = ktime_set(reserve_table[rid].params.zs_instant.tv_sec, 
		  reserve_table[rid].params.zs_instant.tv_nsec);

  hrtimer_init(&(reserve_table[rid].zs_timer),CLOCK_MONOTONIC, HRTIMER_MODE_REL);
  reserve_table[rid].zs_timer.function = zs_instant_handler;
  hrtimer_start(&(reserve_table[rid].zs_timer), kzs, HRTIMER_MODE_REL);
	  
  now2timespec(&reserve_table[rid].start_of_period);
  record_latest_deadline(&(reserve_table[rid].params.period));

  start_accounting(&reserve_table[rid]);
  // get ready to return
  p.sched_priority = reserve_table[rid].params.priority;
  sched_setscheduler(current, SCHED_FIFO, &p);

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
		set_tsk_need_resched(task);
		if (task != current)
			set_tsk_need_resched(current);
	}

	hrtimer_cancel(&(reserve_table[rid].zs_timer));
	if (!(reserve_table[rid].params.reserve_type & APERIODIC_ARRIVAL))
	  hrtimer_cancel(&(reserve_table[rid].period_timer));
	if (reserve_table[rid].params.enforcement_mask & ZS_RESPONSE_TIME_ENFORCEMENT_MASK){
		hrtimer_cancel(&(reserve_table[rid].response_time_timer));
	}
	stop_accounting(&reserve_table[rid], UPDATE_HIGHEST_PRIORITY_TASK);

	if (reserve_table[rid].in_critical_mode){
	  finish_period_degradation(rid);
	}

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
    /* reserve_table[rid].request_stop = 1; */
    /* push_to_reschedule(rid); */
    wake_up_process(sched_task);
    set_tsk_need_resched(current);
  }
  return 0;
}


void init(void){
  int i,j;
  
  for (i=0;i<MAX_RESERVES;i++){
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
      memcpy(&reserve_table[i].params,
	     &(call.args.reserve_parameters), 
	     sizeof(struct zs_reserve_params));
      reserve_table[i].nominal_exectime_nanos = 
	reserve_table[i].params.execution_time.tv_sec * 1000000000L +
	reserve_table[i].params.execution_time.tv_nsec;
      reserve_table[i].overload_exectime_nanos = 
	reserve_table[i].params.overload_execution_time.tv_sec * 1000000000L +
	reserve_table[i].params.overload_execution_time.tv_nsec;
      if ((reserve_table[i].params.reserve_type & RESERVE_TYPE_MASK) == CRITICALITY_RESERVE){
	reserve_table[i].params.normal_marginal_utility = reserve_table[i].params.criticality;
	reserve_table[i].params.overloaded_marginal_utility = reserve_table[i].params.criticality;
	reserve_table[i].params.critical_util_degraded_mode = -1;
	reserve_table[i].params.num_degraded_modes=0;
	printk("create reserve id(%d) CRITICALITY RESERVE\n",i);
      } else {
	printk("create reserve id(%d) OTHER RESERVE\n",i);
      }
      create_reserve(i);
      res = i;
    }
    break;
  case CREATE_MODAL_RESERVE:
    i = create_modal_reserve(call.args.num_modes);
    res = i;
    break;
  case SET_INITIAL_MODE_MODAL_RESERVE:
    res = set_initial_mode_modal_reserve(call.args.set_initial_mode_params.modal_reserve_id,
					 call.args.set_initial_mode_params.initial_mode_id);
    break;
  case ADD_RESERVE_TO_MODE:
    res= add_reserve_to_mode(call.args.rsv_to_mode_params.modal_reserve_id, 
			     call.args.rsv_to_mode_params.mode_id,
			     call.args.rsv_to_mode_params.reserve_id);
    break;
  case ADD_MODE_TRANSITION:
    res = add_mode_transition(call.args.add_mode_transition_params.modal_reserve_id, 
			      call.args.add_mode_transition_params.transition_id,
			      &call.args.add_mode_transition_params.transition);  
    break;
  case ATTACH_MODAL_RESERVE:
    res = attach_modal_reserve(call.args.attach_params.reserveid,
			       call.args.attach_params.pid);
    modal_scheduling = 1;
    break;
  case DETACH_MODAL_RESERVE:
    res = detach_modal_reserve(call.args.reserveid);
    break;
  case DELETE_MODAL_RESERVE:
    res = delete_modal_reserve(call.args.reserveid);
    break;
  case CREATE_SYS_TRANSITION:
    res = create_sys_mode_transition();
    break;
  case ADD_TRANSITION_TO_SYS_TRANSITION:
    res = add_transition_to_sys_transition(call.args.trans_to_sys_trans_params.sys_transition_id,
					   call.args.trans_to_sys_trans_params.mrid,
					   call.args.trans_to_sys_trans_params.transition_id);
    break;
  case DELETE_SYS_TRANSITION:
    res = delete_sys_mode_transition(call.args.reserveid);
    break;
  case MODE_SWITCH:
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
    break;
  case ATTACH_RESERVE:
    res = attach_reserve(call.args.attach_params.reserveid, 
			 call.args.attach_params.pid);
    break;
  case DETACH_RESERVE:
    res = detach_reserve(call.args.reserveid);
    break;
  case DELETE_RESERVE:
    res = delete_reserve(call.args.reserveid);
    break;
  case MODAL_WAIT_NEXT_PERIOD:
    res = modal_wait_for_next_period(call.args.reserveid);
    need_reschedule = 1;
    break;
  case WAIT_NEXT_PERIOD:
    res = wait_for_next_period(call.args.reserveid);
    need_reschedule = 1;
    break;
  case WAIT_NEXT_ARRIVAL:
    res = wait_for_next_arrival(call.args.wait_next_arrival_params.reserveid, 
				call.args.wait_next_arrival_params.fds, 
				call.args.wait_next_arrival_params.nfds,
				&flags);
    // this call will block internally triggering the reschedule -- hopefully
    need_reschedule = 0;
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

  if (dev_class){
    device_destroy(dev_class, MKDEV(dev_major, 0));
  }
  /* delete the char device. */
  cdev_del(&c_dev);

  if (dev_class)
    class_destroy(dev_class);
  /* return back the device number. */
  unregister_chrdev_region(dev_id, 1);	
}

static int __init zsrm_init(void)
{
  int ret;
  int err = 0;
  dev_t devno;
  struct device *device = NULL;
  struct sched_param p;
  
  modal_scheduling = 0 ;
  
  // initialize scheduling top
  top = -1;


  // get do_sys_poll pointer

  do_sys_pollp = (int (*)(struct pollfd __user *ufds, unsigned int nfds,
			  struct timespec *end_time))
    kallsyms_lookup_name("do_sys_poll");

  if (((unsigned long)do_sys_pollp) == 0){
    printk("zsrmm: Failed to obtain do_sys_poll pointer\n");
    return -1;
  }

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
  
  p.sched_priority = 50;
  sched_setscheduler(sched_task, SCHED_FIFO, &p);
  kthread_bind(sched_task, 0);
  printk(KERN_WARNING "MZSRMM: nanos per ten ticks: %lu \n",nanosPerTenTicks);
  printk(KERN_WARNING "MZSRMM: ready!\n");
    
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

  stop_accounting(&reserve_table[rsvid1],1);
  printk("zsrm: t1 c: %lld\n",reserve_table[rsvid1].job_executing_nanos);
  reserve_table[rsvid1].job_executing_nanos = 0L;

  waituntil = jiffies + (jiffies10ms *4);
  while(time_before(jiffies,waituntil))
    ;

  stop_accounting(&reserve_table[rsvid2],1);
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

  stop_accounting(&reserve_table[rsvid1],1);
  printk("zsrm: t1 c: %lld\n",reserve_table[rsvid1].job_executing_nanos);
  printk("zsrm: t2 c: %lld\n",reserve_table[rsvid2].job_executing_nanos);
  printk("zsrm: t3 c: %lld\n",reserve_table[rsvid3].job_executing_nanos);

}

static void __exit zsrm_exit(void)
{
  if (timer_started)
    hrtimer_cancel(&ht);
  
  delete_all_reserves();
  
  delete_all_modal_reserves();
  
  printk(KERN_INFO "MZSRMM: GOODBYE!\n");
  
  zsrm_cleanup_module();
  
  kthread_stop(sched_task);
}

module_init(zsrm_init);
module_exit(zsrm_exit);
