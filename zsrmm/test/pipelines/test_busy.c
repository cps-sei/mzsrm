#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>

#include "../busy.h"

#define MAX_TIMESTAMPS 1000000

long bufidx1;
unsigned long long timestamps_ns1[MAX_TIMESTAMPS];

int main(){
  int i=0;
  long idx;
  unsigned long long start_timestamp_ns;

  calibrate_ticks();

  start_timestamp_ns = now_ns();//ticks2nanos(rdtsc());

  for (i=0;i<5;i++){
    busy_timestamped(500,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
    sleep (3);
  }

  FILE* fid1 = fopen("test-busy.txt","w+");
  if (fid1==NULL){
    printf("erorr opening test-busy.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < bufidx1 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns1[idx]-start_timestamp_ns);
  }

  fclose(fid1);
}
