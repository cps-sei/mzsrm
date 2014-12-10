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
#define __USE_GNU
#include <sched.h>
#undef __USE_GNU
#include <time.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include "zsrm.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h> 
#include <errno.h>


#define LOCAL_SERVER_PORT 1500

void timer_handler(int i, siginfo_t *sinfo, void *p);
void add_timer(struct zs_timer *timer);
struct timespec *get_current_effective_period(int rid);

struct zs_timer *root_timer;
timer_t root_timerid;
struct timespec root_timer_start_timestamp;

int captured_timer = 0;

long system_utility = 0;

struct zs_reserve reserve_table[MAX_RESERVES];

void start_of_period(int rid){
	struct timespec *p = get_current_effective_period(rid);

	reserve_table[rid].period_timer.expiration.tv_sec = p->tv_sec;
	reserve_table[rid].period_timer.expiration.tv_nsec = p->tv_nsec;
	// start timers
	add_timer(&(reserve_table[rid].period_timer));
	add_timer(&(reserve_table[rid].zs_timer));
	clock_gettime(CLOCK_REALTIME,&reserve_table[rid].start_of_period);
	kill(reserve_table[rid].pid,SIGCONT);
}

void extend_period_timer(int rid, struct timespec *p){
	struct itimerspec value;
	struct sigevent evp;
	unsigned long long zs_elapsed_ns;
	unsigned long long newperiod_ns;
	struct timespec now;
	unsigned long long now_ns;

	clock_gettime(CLOCK_REALTIME,&now);
	now_ns = ((unsigned long long) now.tv_sec) * 1000000000ll + (unsigned long long) now.tv_nsec;
	zs_elapsed_ns = ((unsigned long long)reserve_table[rid].start_of_period.tv_sec) * 1000000000ll + (unsigned long long) reserve_table[rid].start_of_period.tv_nsec;
	zs_elapsed_ns = now_ns - zs_elapsed_ns;
	newperiod_ns = ((unsigned long long) p->tv_sec) * 1000000000ll + (unsigned long long) p->tv_nsec;

	newperiod_ns = newperiod_ns - zs_elapsed_ns;

	timer_delete(reserve_table[rid].period_timer.tid);

	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = ZS_TIMER_SIGNAL; 
	evp.sigev_value.sival_ptr = &(reserve_table[rid].period_timer);

	value.it_value.tv_sec = newperiod_ns / 1000000000;
	value.it_value.tv_nsec = newperiod_ns % 1000000000;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_nsec = 0;
	timer_create(CLOCK_REALTIME,&evp, &(reserve_table[rid].period_timer.tid));
	timer_settime(reserve_table[rid].period_timer.tid, 0 , &value, NULL);
}

void period_degradation(int rid){
	long current_marginal_utility;
	long tmp_marginal_utility;
	struct sched_param p;
	int i;

	current_marginal_utility = GET_EFFECTIVE_UTILITY(rid);
	
	reserve_table[rid].in_critical_mode = 1;

	if (system_utility < current_marginal_utility){
		system_utility = current_marginal_utility;
		if (reserve_table[rid].params.critical_util_degraded_mode != -1){
			if (reserve_table[rid].current_degraded_mode == -1 || reserve_table[rid].current_degraded_mode < reserve_table[rid].params.critical_util_degraded_mode){
				reserve_table[rid].current_degraded_mode = reserve_table[rid].params.critical_util_degraded_mode;
				extend_period_timer(rid, &reserve_table[rid].params.degraded_period[reserve_table[rid].params.critical_util_degraded_mode]);
				p.sched_priority = reserve_table[rid].params.degraded_priority[reserve_table[rid].current_degraded_mode];
				sched_setscheduler(reserve_table[rid].pid, SCHED_FIFO, &p);
			}
		}

		// now degrade all others

		for (i=0; i< MAX_RESERVES; i++){
			// skip empty reserves`
			if (reserve_table[i].params.priority == -1)
				continue;

			tmp_marginal_utility = GET_EFFECTIVE_UTILITY(i);

			if (tmp_marginal_utility < system_utility){
				int selected_mode=0;
				while(selected_mode < (reserve_table[i].params.num_degraded_modes) &&
					reserve_table[i].params.degraded_marginal_utility[selected_mode][1] < system_utility)
					selected_mode++;
				if (selected_mode < reserve_table[i].params.num_degraded_modes){
					reserve_table[i].current_degraded_mode = selected_mode;
					extend_period_timer(i, &reserve_table[i].params.degraded_period[selected_mode]);
					p.sched_priority = reserve_table[i].params.degraded_priority[selected_mode];
					sched_setscheduler(reserve_table[i].pid, SCHED_FIFO, &p);
				} else {
					// stop the task
					reserve_table[i].critical_utility_mode_enforced = 1;
					kill(reserve_table[i].pid,SIGSTOP);
				}
			}
		}
	} 
}

/* Returns the current period to be used in wait for next period*/
struct timespec *get_current_effective_period(int rid){
	if (reserve_table[rid].current_degraded_mode == -1){
		return &(reserve_table[rid].params.period);
	} else {
		return &(reserve_table[rid].params.degraded_period[reserve_table[rid].current_degraded_mode]);
	}
}

void finish_period_degradation(int rid){
	long max_marginal_utility=0;
	long tmp_marginal_utility=0;
	int i;
	struct sched_param p;

	reserve_table[rid].in_critical_mode = 0;
	
	// find the max marginal utility of reserves
	// in critical mode
	for (i=0;i<MAX_RESERVES; i++){
		if (!reserve_table[i].in_critical_mode)
			continue;

		tmp_marginal_utility = 	GET_EFFECTIVE_UTILITY(i);
		if (tmp_marginal_utility > max_marginal_utility)
			max_marginal_utility = tmp_marginal_utility;
	}

	system_utility = max_marginal_utility;

	for (i=0; i< MAX_RESERVES; i++){
		if (reserve_table[i].params.overloaded_marginal_utility >= system_utility){
			if (reserve_table[i].critical_utility_mode_enforced){
				reserve_table[i].critical_utility_mode_enforced = 0;
				kill(reserve_table[i].pid, SIGCONT);
			}
			p.sched_priority = reserve_table[i].params.priority;
			sched_setscheduler(reserve_table[i].pid, SCHED_FIFO, &p);
			reserve_table[i].current_degraded_mode = -1;
		} else {
			int selected_mode=0;
			while(selected_mode < reserve_table[i].params.num_degraded_modes &&
				reserve_table[i].params.degraded_marginal_utility[selected_mode][1] < system_utility)
				selected_mode++;
			if (selected_mode < reserve_table[i].params.num_degraded_modes){
				reserve_table[i].current_degraded_mode = selected_mode;
				if (reserve_table[i].critical_utility_mode_enforced){
					reserve_table[i].critical_utility_mode_enforced = 0;
					kill(reserve_table[i].pid, SIGCONT);
				}
				p.sched_priority = reserve_table[i].params.degraded_priority[selected_mode];
				sched_setscheduler(reserve_table[i].pid, SCHED_FIFO, &p);
			}
		}
	}
}

void timer_handler(int i, siginfo_t *sinfo, void *p){
	struct zs_timer *timer = (struct zs_timer *) sinfo->si_value.sival_ptr;
	// process the timer
	switch(timer->timer_type){
		case TIMER_ZS:
			break;
		case TIMER_ENF:
			break;
		case TIMER_ZS_ENF:
			period_degradation(timer->reserve_desc);
			break;
		case TIMER_PERIOD:
			start_of_period(timer->reserve_desc);
			break;
		otherwise:
			printf("unknown\n");
			break;
	}
}

cancel_timer(struct zs_timer *timer){
	timer_delete(timer->tid);
}

void add_timer(struct zs_timer *timer){
	struct itimerspec value;
	struct sigevent evp;

	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = ZS_TIMER_SIGNAL; //SIGRTMIN+1;
	evp.sigev_value.sival_ptr = timer;

	value.it_value.tv_sec = timer->expiration.tv_sec;
	value.it_value.tv_nsec = timer->expiration.tv_nsec;
	value.it_interval.tv_sec = 0;
	value.it_interval.tv_nsec = 0;
	timer_create(CLOCK_REALTIME,&evp, &(timer->tid));
	timer_settime(timer->tid, 0 , &value, NULL);
}

int wait_for_next_period(int rid){
	cancel_timer(&(reserve_table[rid].zs_timer));
	if (reserve_table[rid].in_critical_mode){
		finish_period_degradation(rid);
	}
	kill(reserve_table[rid].pid,SIGSTOP);
	return 0;
}

int budget_overrun(int rid){
	kill(reserve_table[rid].pid, SIGSTOP);
	// period enforcement if beyond zs instant
	return 0;
}

int find_empty_entry(){
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

int attach_reserve(int rid, int pid){
	struct sched_param p;
	reserve_table[rid].pid = pid;
	reserve_table[rid].in_critical_mode = 0;
	reserve_table[rid].current_degraded_mode = -1;
	reserve_table[rid].critical_utility_mode_enforced = 0;
	p.sched_priority = reserve_table[rid].params.priority;
	sched_setscheduler(pid,SCHED_FIFO, &p);
	add_timer(&(reserve_table[rid].period_timer));
	add_timer(&(reserve_table[rid].zs_timer));
	clock_gettime(CLOCK_REALTIME,&reserve_table[rid].start_of_period);
	return 0;
}

int delete_reserve(int rid){
	if (reserve_table[rid].pid != -1){
		detach_reserve(rid);
	}
	reserve_table[rid].params.priority = -1;	
	return 0;
}

int detach_reserve(int rid){
	struct sched_param p;
	p.sched_priority = 0;
	sched_setscheduler(reserve_table[rid].pid, SCHED_OTHER,&p);
	cancel_timer(&(reserve_table[rid].zs_timer));
	cancel_timer(&(reserve_table[rid].period_timer));
	// mark as detached
	reserve_table[rid].pid = -1;
	return 0;
}

int create_reserve(int rid){
	reserve_table[rid].period_timer.timer_type = TIMER_PERIOD;
	reserve_table[rid].period_timer.reserve_desc = rid;
	reserve_table[rid].period_timer.expiration.tv_sec = reserve_table[rid].params.period.tv_sec;
	reserve_table[rid].period_timer.expiration.tv_nsec = reserve_table[rid].params.period.tv_nsec;
	reserve_table[rid].zs_timer.timer_type = TIMER_ZS_ENF;
	reserve_table[rid].zs_timer.reserve_desc = rid;
	reserve_table[rid].zs_timer.expiration.tv_sec = reserve_table[rid].params.zs_instant.tv_sec;
	reserve_table[rid].zs_timer.expiration.tv_nsec = reserve_table[rid].params.zs_instant.tv_nsec;
	return 0;
}

void init(){
	int i;

	root_timer = NULL;
	for (i=0;i<MAX_RESERVES;i++){
		reserve_table[i].params.priority = -1; // mark as empty
		reserve_table[i].period_timer.next = NULL;
		reserve_table[i].zs_timer.next = NULL;
	}
}

int get_command(int sd, struct api_call *call, struct sockaddr_in *cliAddr){
	int cliLen, n, flags =0;

	cliLen = sizeof(struct sockaddr_in);
	n = recvfrom(sd, call, sizeof(struct api_call), flags, (struct sockaddr *) cliAddr, &cliLen);
	return n;
}

int send_response(int sd, int ret, struct sockaddr_in *cliAddr){
	int flags = 0;
	if (sendto(sd, &ret, sizeof(int), flags, (struct sockaddr *) cliAddr, sizeof(struct sockaddr_in))<0){
		printf("error sending error response\n");
	}
}

int main(){
	int i,j;
	struct api_call call;
	struct sched_param p;
	int sd, rc, n, cliLen, flags;
	struct sockaddr_in cliAddr, servAddr;
	cpu_set_t mask; 
	int ret; 
	sigset_t sigmask;
	sigset_t sigmask1;
	struct sigaction act;

	sigemptyset(&sigmask);
	sigaddset(&sigmask, ZS_TIMER_SIGNAL ); //SIGRTMIN+1);

	CPU_ZERO(&mask); 
	CPU_SET(0, &mask); 
	ret = sched_setaffinity(getpid(), sizeof mask, &mask); 

	p.sched_priority = DAEMON_PRIORITY;
	sched_setscheduler(getpid(),SCHED_FIFO, &p);

	sigemptyset(&sigmask1);
	act.sa_sigaction = timer_handler;
	act.sa_mask = sigmask1 ;
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	act.sa_restorer = NULL;
	//if (sigaction(SIGRTMIN+1, &act, NULL) < 0){
	if (sigaction(ZS_TIMER_SIGNAL, &act, NULL) < 0){
		perror("in sigaction\n");
	}

	init();

	flags = 0;
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd <0){
		printf("could not open socket\n");
		return -1;
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(LOCAL_SERVER_PORT);
	rc = bind(sd, (struct sockaddr *)&servAddr, sizeof(servAddr));
	if (rc <0){
		printf("could not bind\n");
		return -1;
	}

	do{
		sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
		n = get_command(sd, &call, &cliAddr);
		sigprocmask(SIG_BLOCK, &sigmask, NULL);
		if (n>0){
			switch (call.api_id){
				case CREATE_RESERVE:
					i = find_empty_entry();
					if (i == -1){
						// full, return -1
						send_response(sd,i,&cliAddr);
					} else {
						memcpy(&reserve_table[i].params,&(call.args.reserve_parameters),sizeof(struct zs_reserve_params));
						create_reserve(i);
						send_response(sd,i,&cliAddr);
					}	
					break;
				case ATTACH_RESERVE:
					i = attach_reserve(call.args.attach_params.reserveid, call.args.attach_params.pid);
					send_response(sd,i,&cliAddr);
					break;
				case DETACH_RESERVE:
					i= detach_reserve(call.args.reserveid);
					send_response(sd,i,&cliAddr);
					break;
				case DELETE_RESERVE:
					i = delete_reserve(call.args.reserveid);
					send_response(sd,i,&cliAddr);
					break;
				case WAIT_NEXT_PERIOD:
					i = wait_for_next_period(call.args.reserveid);
					send_response(sd,i,&cliAddr);
					break;
				case BUDGET_OVERRUN:
					i = budget_overrun(call.args.reserveid);
					send_response(sd,i,&cliAddr);
					break;
			}
		} else {
			printf("Read garbage\n");
		}
	}while(1);
}
