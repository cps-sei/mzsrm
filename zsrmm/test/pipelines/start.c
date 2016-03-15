#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/sem.h>
#include <sys/ipc.h>

int main(int argc, char *argv[]){
  key_t semkey;
  int sync_start_semid;
  struct sembuf sops;
  int numprocs;

  if (argc != 2){
    printf("usage: %s <num processes>\n",argv[0]);
    return -1;
  }

  numprocs = atoi(argv[1]);

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
  sops.sem_op = numprocs; // # ups one for each task
  sops.sem_flg = 0;
  if (semop(sync_start_semid, &sops,1)<0){
    printf("error in semop up\n");
  }

  if (semctl(sync_start_semid, 0, IPC_RMID)<0){
    printf("error removing semaphore\n");
  }

}
