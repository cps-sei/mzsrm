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

#include <time.h>
#include <netinet/in.h>

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

#define TIMER_ZS  1
#define TIMER_ENF 2
#define TIMER_ZS_ENF 3
#define TIMER_PERIOD 4

#define MAX_RESERVES 64

#define REMOTE_SERVER_PORT 1500

#define ZS_TIMER_SIGNAL (SIGRTMIN+1)
#define ZS_BUDGET_TIMER_SIGNAL (SIGRTMIN+2)

#define ZS_BUDGET_ENFORCEMENT_MASK 0x1
#define ZS_PERIOD_DEGRADATION_MAKS 0x2
#define ZS_CRITICALITY_ENFORCEMENT_MASK 0x4

#define MAX_DEGRADED_MODES 4

#define GET_EFFECTIVE_UTILITY(i) (reserve_table[i].current_degraded_mode == -1 ? reserve_table[i].params.overloaded_marginal_utility: reserve_table[i].params.degraded_marginal_utility[reserve_table[i].current_degraded_mode][1])

struct zs_timer{
	timer_t tid;
	int timer_type;
	int reserve_desc;
	struct timespec expiration; // from zero if it is the first one
	struct zs_timer *next;
};

struct zs_reserve_params{
	struct timespec period;
	struct timespec execution_time;
	struct timespec zs_instant;
	struct timespec overload_instant;
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
};

struct zs_reserve {
	int pid;
	struct zs_timer period_timer;
	struct zs_timer zs_timer;
	int in_critical_mode;
	int critical_utility_mode_enforced;
	int current_degraded_mode;
	struct zs_reserve_params params;
	struct timespec start_of_period;
};

struct attach_api{
	int reserveid;
	int pid;
};

struct api_call{
	int api_id;
	union {
		int reserveid;
		struct attach_api attach_params;
		struct zs_reserve_params reserve_parameters;
	}args;
};

struct zs_sched_descriptor {
	struct sockaddr_in cliAddr, remoteServAddr;
	int sd;
	int enforcement_mask;
	timer_t wcet_timerid;
	struct timespec wcet;
	int rid;
};

// library calls signatures
struct zs_sched_descriptor *zs_open_sched(void);
int zs_close_sched(struct zs_sched_descriptor *s);
int zs_create_reserve(struct zs_sched_descriptor *s, struct zs_reserve_params *p);
int zs_attach_reserve(struct zs_sched_descriptor *s, int rid, int pid);
int zs_detach_reserve(struct zs_sched_descriptor *s, int rid);
int zs_delete_reserve(struct zs_sched_descriptor *s, int rid);
int zs_wait_next_period(struct zs_sched_descriptor *s, int rid);
#endif
