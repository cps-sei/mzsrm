#ifndef __ZSMUTEX_H__
#define __ZSMUTEX_H__

#include <pthread.h>

typedef struct zs_pccp_mutex_struct {
  int criticality_ceiling;
  int priority_ceiling;
  pthread_mutex_t pmutex;
} zs_pccp_mutex_t;
 
int zs_pccp_mutex_init(zs_pccp_mutex_t *mutex, int priority_ceiling, int criticality_ceiling, pthread_mutexattr_t *attr);
int zs_pccp_mutex_lock(int sched_id, zs_pccp_mutex_t *mutex);
int zs_pccp_mutex_unlock(int sched_id, zs_pccp_mutex_t *mutex);
int zs_pccp_mutex_destroy(zs_pccp_mutex_t *mutex);


#endif
