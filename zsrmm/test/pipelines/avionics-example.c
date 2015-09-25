#define _GNU_SOURCE
#include <stdio.h>
#include <locale.h>
#include <time.h>
#include "../../zsrm.h"
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/sem.h>
#include <sched.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <poll.h>


#include "../busy.h"

#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#else
#error "SYS_gettid unavailable in this system"
#endif

#define MAX_TIMESTAMPS 1000000

#define BUFSIZE 100

struct task_stage_params{
  int myport;
  char *destination_address;
  int destination_port;
  int fd_index;
};

long bufidx1;
long bufidx2;
long bufidx3;
long bufidx4;
unsigned long long timestamps_ns1[MAX_TIMESTAMPS];
unsigned long long timestamps_ns2[MAX_TIMESTAMPS];
unsigned long long timestamps_ns3[MAX_TIMESTAMPS];
unsigned long long timestamps_ns4[MAX_TIMESTAMPS];

long bufidx5;
long bufidx6;
long bufidx7;
long bufidx8;
unsigned long long timestamps_ns5[MAX_TIMESTAMPS];
unsigned long long timestamps_ns6[MAX_TIMESTAMPS];
unsigned long long timestamps_ns7[MAX_TIMESTAMPS];
unsigned long long timestamps_ns8[MAX_TIMESTAMPS];

long bufidx9;
long bufidxa;
long bufidxb;
long bufidxc;
unsigned long long timestamps_ns9[MAX_TIMESTAMPS];
unsigned long long timestamps_nsa[MAX_TIMESTAMPS];
unsigned long long timestamps_nsb[MAX_TIMESTAMPS];
unsigned long long timestamps_nsc[MAX_TIMESTAMPS];

long bufidxd;
long bufidxe;
long bufidxf;
long bufidxg;
unsigned long long timestamps_nsd[MAX_TIMESTAMPS];
unsigned long long timestamps_nse[MAX_TIMESTAMPS];
unsigned long long timestamps_nsf[MAX_TIMESTAMPS];
unsigned long long timestamps_nsg[MAX_TIMESTAMPS];


int sync_start_semid;

int fds[16];

#include "stall-warning-subsystem.h"
#include "runway-overrun-subsystem.h"
#include "traffic-awareness-subsystem.h"
#include "terrain-awareness-subsystem.h"


int main(int argc, char *argv[]){
  int sched;
  pthread_t tid_airspeed,tid_lift,tid_stall,tid_angle;
  pthread_t tid_gps_position, tid_stop_distance, tid_stop_location, tid_virtual_runway;
  pthread_t tid_air_radar, tid_object_identification, tid_track_building, tid_traffic_warning;
  pthread_t tid_ground_radar,tid_terrain_distance, tid_time_to_terrain, tid_terrain_warning;
  pthread_t nodetid;

  unsigned long long start_timestamp_ns;
  long idx;
  struct sched_param p;
  key_t semkey;
  struct task_stage_params airspeed_task_params;
  struct task_stage_params lift_task_params;
  struct task_stage_params stall_task_params;
  struct task_stage_params angle_task_params;

  struct task_stage_params gps_position_task_params;
  struct task_stage_params stop_distance_task_params;
  struct task_stage_params stop_location_task_params;
  struct task_stage_params virtual_runway_task_params;

  struct task_stage_params air_radar_task_params;
  struct task_stage_params object_identification_task_params;
  struct task_stage_params track_building_task_params;
  struct task_stage_params traffic_warning_task_params;

  struct task_stage_params ground_radar_task_params;
  struct task_stage_params terrain_distance_task_params;
  struct task_stage_params time_to_terrain_task_params;
  struct task_stage_params terrain_warning_task_params;


  union semun  {
    int val;
    struct semid_ds *buf;
    ushort *array;
  } arg;

  setlocale(LC_NUMERIC,"");

  p.sched_priority = 30;
  if (sched_setscheduler(getpid(), SCHED_FIFO,&p)<0){
    printf("error setting fixed priority\n");
    return -1;
  }

  calibrate_ticks();

  if ((semkey = ftok("/tmp", 11766)) == (key_t) -1) {
    perror("IPC error: ftok"); exit(1);
  }

  // create sync start semaphore
  if ((sync_start_semid = semget(semkey, 1, IPC_CREAT|0777)) <0){
    perror("creating the semaphone\n");
    return -1;
  }

  arg.val = 0;
  if (semctl(sync_start_semid, 0, SETVAL, arg) < 0){
    printf("Error setting sem to zero\n");
    return -1;
  }

  /**
   * STALL WARNING SETUP
   */
  airspeed_task_params.myport = 5550;
  airspeed_task_params.destination_address = "127.0.0.1";
  airspeed_task_params.destination_port = 5551;
  if (pthread_create(&tid_airspeed,NULL, airspeed_task,(void *)&airspeed_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  lift_task_params.myport = 5551;
  lift_task_params.destination_address = "127.0.0.1";
  lift_task_params.destination_port = 5552;
  lift_task_params.fd_index = 0;
  if (pthread_create(&tid_lift,NULL, lift_task,(void *)&lift_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  stall_task_params.myport = 5552;
  stall_task_params.destination_address = "127.0.0.1";
  stall_task_params.destination_port = 5553;
  stall_task_params.fd_index = 1;
  if (pthread_create(&tid_stall,NULL, stall_task,(void *)&stall_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  angle_task_params.myport = 5553;
  angle_task_params.destination_address = "127.0.0.1";
  angle_task_params.destination_port = 5554;
  angle_task_params.fd_index = 2;
  if (pthread_create(&tid_angle,NULL, angle_task,(void *)&angle_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }


  /**
   * RUNWAY OVERRUN SETUP
   */
  gps_position_task_params.myport = 6660;
  gps_position_task_params.destination_address = "127.0.0.1";
  gps_position_task_params.destination_port = 6661;
  if (pthread_create(&tid_gps_position,NULL, gps_position_task,(void *)&gps_position_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  stop_distance_task_params.myport = 6661;
  stop_distance_task_params.destination_address = "127.0.0.1";
  stop_distance_task_params.destination_port = 6662;
  stop_distance_task_params.fd_index=3;
  if (pthread_create(&tid_stop_distance,NULL, stop_distance_task,(void *)&stop_distance_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  stop_location_task_params.myport = 6662;
  stop_location_task_params.destination_address = "127.0.0.1";
  stop_location_task_params.destination_port = 6663;
  stop_location_task_params.fd_index = 4;
  if (pthread_create(&tid_stop_location,NULL, stop_location_task,(void *)&stop_location_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  virtual_runway_task_params.myport = 6663;
  virtual_runway_task_params.destination_address = "127.0.0.1";
  virtual_runway_task_params.destination_port = 6664;
  virtual_runway_task_params.fd_index = 5;
  if (pthread_create(&tid_virtual_runway,NULL, virtual_runway_task,(void *)&virtual_runway_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }


  /**
   * TRAFFIC AWARENESS SETUP
   */
  air_radar_task_params.myport = 7770;
  air_radar_task_params.destination_address = "127.0.0.1";
  air_radar_task_params.destination_port = 7771;
  if (pthread_create(&tid_air_radar,NULL, air_radar_task,(void *)&air_radar_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  object_identification_task_params.myport = 7771;
  object_identification_task_params.destination_address = "127.0.0.1";
  object_identification_task_params.destination_port = 7772;
  object_identification_task_params.fd_index = 6;
  if (pthread_create(&tid_object_identification,NULL, object_identification_task,(void *)&object_identification_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  track_building_task_params.myport = 7772;
  track_building_task_params.destination_address = "127.0.0.1";
  track_building_task_params.destination_port = 7773;
  track_building_task_params.fd_index = 7;
  if (pthread_create(&tid_track_building,NULL, track_building_task,(void *)&track_building_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  traffic_warning_task_params.myport = 7773;
  traffic_warning_task_params.destination_address = "127.0.0.1";
  traffic_warning_task_params.destination_port = 7774;
  traffic_warning_task_params.fd_index = 8;
  if (pthread_create(&tid_traffic_warning,NULL, traffic_warning_task,(void *)&traffic_warning_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }


  /**
   * TERRAIN AWARENESS SETUP
   */
  ground_radar_task_params.myport = 9990;
  ground_radar_task_params.destination_address = "127.0.0.1";
  ground_radar_task_params.destination_port = 9991;
  if (pthread_create(&tid_ground_radar,NULL, ground_radar_task,(void *)&ground_radar_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  terrain_distance_task_params.myport = 9991;
  terrain_distance_task_params.destination_address = "127.0.0.1";
  terrain_distance_task_params.destination_port = 9992;
  terrain_distance_task_params.fd_index = 9;
  if (pthread_create(&tid_terrain_distance,NULL, terrain_distance_task,(void *)&terrain_distance_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  time_to_terrain_task_params.myport = 9992;
  time_to_terrain_task_params.destination_address = "127.0.0.1";
  time_to_terrain_task_params.destination_port = 9993;
  time_to_terrain_task_params.fd_index = 10;
  if (pthread_create(&tid_time_to_terrain,NULL, time_to_terrain_task,(void *)&time_to_terrain_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  terrain_warning_task_params.myport = 9993;
  terrain_warning_task_params.destination_address = "127.0.0.1";
  terrain_warning_task_params.destination_port = 9994;
  terrain_warning_task_params.fd_index = 11;
  if (pthread_create(&tid_terrain_warning,NULL, terrain_warning_task,(void *)&terrain_warning_task_params ) != 0){
    printf("Error creating thread 1\n");
    return -1;
  }

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return -1;
  }
  

  // let the threads prepare
  sleep(5);

  printf("about to start node\n");
  nodetid = zs_start_node(sched,fds,12);
  
  if (nodetid <0 ){
    printf("error starting node server\n");
    return -1;
  }

  start_timestamp_ns = now_ns();//ticks2nanos(rdtsc());

  // Go!
  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = 16; // 16 ups one for each task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  printf("test tasks started\n");
  usleep(1000);


  pthread_join(tid_airspeed,NULL);
  printf("airspeed task finished\n");
  pthread_join(tid_lift, NULL);
  printf("lift task finished\n");
  pthread_join(tid_stall,NULL);
  printf("stall task finished\n");
  pthread_join(tid_angle,NULL);
  printf("angle task finished\n");

  pthread_join(tid_gps_position,NULL);
  printf("gps position finished\n");
  pthread_join(tid_stop_distance, NULL);
  printf("stop distance finished\n");
  pthread_join(tid_stop_location,NULL);
  printf("stop location finished\n");
  pthread_join(tid_virtual_runway, NULL);
  printf("virtual_runway finished\n");

  pthread_join(tid_air_radar, NULL);
  printf("air radar finished\n");
  pthread_join(tid_object_identification, NULL);
  printf("object identification finished\n");
  pthread_join(tid_track_building,NULL);
  printf("track building finished\n");
  pthread_join(tid_traffic_warning, NULL);
  printf("traffic warninig\n");

  pthread_join(tid_ground_radar,NULL);
  printf("ground radar finished\n");
  pthread_join(tid_terrain_distance,NULL);
  printf("terrain distance finished\n");
  pthread_join(tid_time_to_terrain,NULL);
  printf("time to terrain finished\n");
  pthread_join(tid_terrain_warning,NULL);
  printf("terrain warning finished \n");

  zs_stop_node(nodetid);

  FILE* fid1 = fopen("ts-airspeed.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-airspeed.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < bufidx1 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns1[idx]-start_timestamp_ns);
  }

  fclose(fid1);

  FILE* fid2 = fopen("ts-lift.txt","w+");
  if (fid2==NULL){
    printf("erorr opening ts-lift.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < bufidx2 ; idx++){
    fprintf(fid2,"%llu 1\n",timestamps_ns2[idx]-start_timestamp_ns);
  }

  fclose(fid2);

  FILE* fid3 = fopen("ts-stall.txt","w+");
  if (fid3==NULL){
    printf("erorr opening ts-stall.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < bufidx3 ; idx++){
    fprintf(fid3,"%llu 1\n",timestamps_ns3[idx]-start_timestamp_ns);
  }

  fclose(fid3);

  FILE* fid4 = fopen("ts-angle.txt","w+");
  if (fid4==NULL){
    printf("erorr opening ts-angle.txt\n");
    return -1;
  }

  for (idx = 0 ; idx < bufidx4 ; idx++){
    fprintf(fid4,"%llu 1\n",timestamps_ns4[idx]-start_timestamp_ns);
  }

  fclose(fid4);



  fid1 = fopen("ts-gps-position.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-gps-position.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidx5 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns5[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-stop-distance.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-stop-distance.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidx6 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns6[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-stop-location.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-stop-location.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidx7 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns7[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-virtual-runway.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-virtual-runway.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidx8 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns8[idx]-start_timestamp_ns);
  }
  fclose(fid1);


  fid1 = fopen("ts-air-radar.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-air-radar.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidx9 ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_ns9[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-object-identification.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-object-identification.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidxa ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_nsa[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-track-building.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-track-building.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidxb ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_nsb[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-traffic-warning.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-traffic-warning.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidxc ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_nsc[idx]-start_timestamp_ns);
  }
  fclose(fid1);


  fid1 = fopen("ts-ground-radar.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-ground-radar.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidxd ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_nsd[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-terrain-distance.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-terrain-distance.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidxe ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_nse[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-time-to-terrain.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-time-to-terrain.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidxf ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_nsf[idx]-start_timestamp_ns);
  }
  fclose(fid1);
  fid1 = fopen("ts-terrain-warning.txt","w+");
  if (fid1==NULL){
    printf("erorr opening ts-terrain-warning.txt\n");
    return -1;
  }
  for (idx = 0 ; idx < bufidxg ; idx++){
    fprintf(fid1,"%llu 1\n",timestamps_nsg[idx]-start_timestamp_ns);
  }
  fclose(fid1);


  if (semctl(sync_start_semid, 0, IPC_RMID)<0){
    printf("error removing semaphore\n");
  }

}
