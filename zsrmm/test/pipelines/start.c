#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/ipc.h>

int main(){
  key_t semkey;
  int sync_start_semid;
  struct sembuf sops;

  if ((semkey = ftok("/tmp", 11766)) == (key_t) -1) {
    perror("IPC error: ftok"); 
    return -1;
  }

  // create sync start semaphore
  if ((sync_start_semid = semget(semkey, 1, 0)) <0){
    perror("obtaining the semaphone\n");
    return -1;
  }

  // up semaphore to sync start tasks

  sops.sem_num=0;
  sops.sem_op = 3; // three ups one for each task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

}
