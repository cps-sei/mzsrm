#ifndef __GENERIC_STAGE_PARAMS_H__
#define __GENERIC_STAGE_PARAMS_H__
struct task_stage_params{
  int myport;
  char *destination_address;
  int destination_port;
  int fd_index;
  struct zs_reserve_params cpuattr;
  long usec_delay;
  long busytime_ms;
  unsigned long long timestamp_ns[MAX_TIMESTAMPS];
  long bufidx;
};

#endif
