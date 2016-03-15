
/***********************
 * STALL WARNING SYSTEM*
 ***********************/
void *airspeed_task(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int err;
  int sched;
  int i;
  unsigned char *buf;
  struct sockaddr_in myaddr,outaddr;
  int fd;
  struct task_stage_params *stage_paramsp;
  int io_flag;
  struct sched_param p;
  
  stage_paramsp = (struct task_stage_params *)argp;

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <0){
    perror("could not create socket\n");
    return NULL;
  }

  printf("airspeed: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);

  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(stage_paramsp->myport);
  
  memset((char *)&outaddr, 0, sizeof(outaddr));
  outaddr.sin_family = AF_INET;
  outaddr.sin_port = htons(stage_paramsp->destination_port);
  if (inet_aton(stage_paramsp->destination_address, &outaddr.sin_addr)==0){
    printf("could not parse destination ip address");
    return NULL;
  }



  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(0,&cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 1\n");
  }

  strcpy(cpuattr.name,"airspeed");
  cpuattr.period.tv_sec = 14;
  cpuattr.period.tv_nsec= 534000000;
  cpuattr.priority = 1;
  /* cpuattr.execution_time.tv_sec = 0; */
  /* cpuattr.execution_time.tv_nsec = 100000000; */
  /* cpuattr.overload_execution_time.tv_sec = 0; */
  /* cpuattr.overload_execution_time.tv_nsec = 100000000; */
  cpuattr.zs_instant.tv_sec=8;//11 ;
  cpuattr.zs_instant.tv_nsec=0;//278000000; 
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_ROOT;
  cpuattr.enforcement_mask = DONT_ENFORCE_ZERO_SLACK_MASK; //0;//ZS_ENFORCEMENT_HARD_MASK;//DONT_ENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 4;
  cpuattr.e2e_execution_time.tv_nsec = 360000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 5;
  cpuattr.e2e_overload_execution_time.tv_nsec = 448000000;
  cpuattr.criticality = 4;
  cpuattr.normal_marginal_utility = 4;
  cpuattr.overloaded_marginal_utility = 4;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu=0;
  cpuattr.outsockfd = fd;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  buf = zs_alloc_stage_msg_packet(sched, BUFSIZE);
  if (buf == NULL){
    printf("failed to allocate msg packet\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("failed to create reserve\n");
    return NULL;
  }


  p.sched_priority = 30;
  if (pthread_setschedparam(pthread_self(), SCHED_FIFO,&p)<0){
    printf("error setting fixed priority\n");
    return NULL;
  }

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  // wait to change phasing
  //usleep(500000);

  zs_attach_reserve(sched,rid,gettid());

  printf("airspeed reserved attached\n");
  int firsttime =1;

  //for (i=0;i<ITERATIONS;i++){
  for (i=0;i<2;i++){
    printf("airspeed: before busy period\n");
    if (firsttime){
      firsttime=0;
      //busy_timestamped(1362,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
    } else {
      //busy_timestamped(1362,timestamps_ns1, MAX_TIMESTAMPS,&bufidx1);
    }
    sprintf(buf,"msg[%d]",i);
    printf("airspeed: before wait_next_root_period\n");
    if ((err = zs_wait_next_root_period(sched,rid,fd, 
					buf ,strlen(buf)+1, 
					0, (struct sockaddr *)&outaddr, 
					sizeof (outaddr)))<0){
      perror("error in wait_next_root");
      break;
    }
    printf("airspeed sent(%s)\n",buf);
  }

  sprintf(buf,"bye");
  if (err >=0){
    zs_wait_next_root_period(sched,rid,
			     fd, buf, strlen(buf)+1, 0, 
			     (struct sockaddr *)&outaddr, 
			     sizeof (outaddr));
  }

  zs_free_msg_packet(sched, buf);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  close(fd);

}


void *lift_task(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int err;
  int sched;
  int i;
  unsigned char *buf;
  struct sockaddr_in myaddr,outaddr,prevaddr;
  int fd;
  struct task_stage_params *stage_paramsp;
  int io_flag;
  int remaddr_len,prevaddr_len;
  
  stage_paramsp = (struct task_stage_params *)argp;

  printf("lift: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <0){
    perror("could not create socket\n");
    return NULL;
  }

  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(stage_paramsp->myport);

  if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    perror("bind failed");
    return NULL;
  }

  

  memset((char *)&outaddr, 0, sizeof(outaddr));
  outaddr.sin_family = AF_INET;
  outaddr.sin_port = htons(stage_paramsp->destination_port);
  if (inet_aton(stage_paramsp->destination_address, &outaddr.sin_addr)==0){
    printf("could not parse destination ip address");
    return NULL;
  }


  // BOUND TO PROCESSOR 1
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(1,&cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 1\n");
  }

  strcpy(cpuattr.name,"lift");
  cpuattr.period.tv_sec = 14;
  cpuattr.period.tv_nsec= 534000000;
  cpuattr.priority = 1;
  /* cpuattr.execution_time.tv_sec = 0; */
  /* cpuattr.execution_time.tv_nsec = 100000000; */
  /* cpuattr.overload_execution_time.tv_sec = 0; */
  /* cpuattr.overload_execution_time.tv_nsec = 100000000; */
  cpuattr.zs_instant.tv_sec=8;//11; 
  cpuattr.zs_instant.tv_nsec=0; //278000000; 
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_MIDDLE | APERIODIC_ARRIVAL;
  cpuattr.enforcement_mask = DONT_ENFORCE_ZERO_SLACK_MASK; //ZS_ENFORCEMENT_HARD_MASK;//DONT_ENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 4;
  cpuattr.e2e_execution_time.tv_nsec = 360000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 5;
  cpuattr.e2e_overload_execution_time.tv_nsec = 448000000;
  cpuattr.criticality = 4;
  cpuattr.normal_marginal_utility = 4;
  cpuattr.overloaded_marginal_utility = 4;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu=1;
  //cpuattr.insockfd = fd;
  cpuattr.outsockfd = fd;

  fds[stage_paramsp->fd_index] = fd;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  buf = zs_alloc_stage_msg_packet(sched, BUFSIZE);
  if (buf == NULL){
    printf("failed to allocate msg packet\n");
    return NULL;
  }


  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("failed to create reserve\n");
    return NULL;
  }



  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  printf("lift: after attach reserve\n");
  io_flag = MIDDLE_STAGE_DONT_SEND_OUTPUT;
  i=0;
  buf[0] = '\0';
  while (strstr(buf,"bye") == NULL){
    remaddr_len = sizeof (outaddr);
    prevaddr_len = sizeof(outaddr);
    printf("lift: before wait_next_stage\n");
    if ((err = zs_wait_next_stage_arrival(sched,rid,
					  fd, buf ,
					  BUFSIZE, 0, 
					  (struct sockaddr *)&outaddr, 
					  remaddr_len,
					  (struct sockaddr *)&prevaddr,
					  &prevaddr_len,
					  io_flag
					  ))<0){
      perror("error in wait_next_root");
      break;
    }
    printf("lift received(%s)\n",buf);
    if (strstr(buf,"bye") == NULL){
      sprintf(buf,"msg[%d]",i++);
    }
    io_flag = 0;
    //busy_timestamped(1362,timestamps_ns2, MAX_TIMESTAMPS,&bufidx2);
  }

  sprintf(buf,"bye");

  io_flag = MIDDLE_STAGE_DONT_WAIT_INPUT;
  remaddr_len = sizeof (outaddr);
  prevaddr_len = sizeof(outaddr);
  if ((err = zs_wait_next_stage_arrival(sched, rid, 
					fd, buf, 
					BUFSIZE,0, 
					(struct sockaddr *)&outaddr,
					remaddr_len,
					(struct sockaddr *)&prevaddr,
					&prevaddr_len,
					io_flag
					))<0){
    printf("Error(%d) in wait_next_arrival\n",err);
  }

  zs_free_msg_packet(sched, buf);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  close(fd);

}

void *stall_task(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int err;
  int sched;
  int i;
  unsigned char *buf;
  struct sockaddr_in myaddr,outaddr,prevaddr;
  int fd;
  struct task_stage_params *stage_paramsp;
  int io_flag;
  int remaddr_len,prevaddr_len;
  
  stage_paramsp = (struct task_stage_params *)argp;

  printf("stall: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);


  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <0){
    perror("could not create socket\n");
    return NULL;
  }

  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(stage_paramsp->myport);

  if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    perror("bind failed");
    return NULL;
  }  

  memset((char *)&outaddr, 0, sizeof(outaddr));
  outaddr.sin_family = AF_INET;
  outaddr.sin_port = htons(stage_paramsp->destination_port);
  if (inet_aton(stage_paramsp->destination_address, &outaddr.sin_addr)==0){
    printf("could not parse destination ip address");
    return NULL;
  }


  // BOUND TO PROCESSOR 2
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(2,&cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 1\n");
  }
  
  strcpy(cpuattr.name,"stall");
  cpuattr.period.tv_sec = 14;
  cpuattr.period.tv_nsec= 534000000;
  cpuattr.priority = 1;
  /* cpuattr.execution_time.tv_sec = 0; */
  /* cpuattr.execution_time.tv_nsec = 100000000; */
  /* cpuattr.overload_execution_time.tv_sec = 0; */
  /* cpuattr.overload_execution_time.tv_nsec = 100000000; */
  cpuattr.zs_instant.tv_sec=8;//11; 
  cpuattr.zs_instant.tv_nsec=0; //278000000;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_MIDDLE | APERIODIC_ARRIVAL;
  cpuattr.enforcement_mask = DONT_ENFORCE_ZERO_SLACK_MASK;//ZS_ENFORCEMENT_HARD_MASK;//DONT_ENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 4;
  cpuattr.e2e_execution_time.tv_nsec = 360000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 5;
  cpuattr.e2e_overload_execution_time.tv_nsec = 448000000;
  cpuattr.criticality = 4;
  cpuattr.normal_marginal_utility = 4;
  cpuattr.overloaded_marginal_utility = 4;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu = 2;
  //cpuattr.insockfd = fd;
  cpuattr.outsockfd = fd;

  fds[stage_paramsp->fd_index] = fd;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  buf = zs_alloc_stage_msg_packet(sched, BUFSIZE);
  if (buf == NULL){
    printf("failed to allocate msg packet\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("failed to create reserve\n");
    return NULL;
  }



  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  printf("stall: after attach reserve\n");
  io_flag = MIDDLE_STAGE_DONT_SEND_OUTPUT;
  i=0;
  int firsttime=1;
  buf[0] = '\0';
  while (strstr(buf,"bye") == NULL){
    remaddr_len = sizeof (outaddr);
    prevaddr_len = sizeof(outaddr);
    printf("stall: before wait_next stage\n");
    if ((err = zs_wait_next_stage_arrival(sched,rid,
					  fd, buf ,
					  BUFSIZE, 0, 
					  (struct sockaddr *)&outaddr, 
					  remaddr_len,
					  (struct sockaddr *)&prevaddr,
					  &prevaddr_len,
					  io_flag
					  ))<0){
      perror("error in wait_next_stage_arrival");
      break;
    }
    printf("stall received(%s)\n",buf);
    if (strstr(buf,"bye") == NULL){
      sprintf(buf,"msg[%d]",i++);
    }
    io_flag = 0;
    if (firsttime){
      firsttime=0;
      //busy_timestamped(1362,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
    } else {
      //busy_timestamped(1362,timestamps_ns3, MAX_TIMESTAMPS,&bufidx3);
    }
  }

  sprintf(buf,"bye");

  io_flag = MIDDLE_STAGE_DONT_WAIT_INPUT;
  remaddr_len = sizeof (outaddr);
  prevaddr_len = sizeof(outaddr);
  printf("stall: before last next stage arrival\n");
  if ((err = zs_wait_next_stage_arrival(sched, rid, 
					fd, buf, 
					BUFSIZE,0, 
					(struct sockaddr *)&outaddr,
					remaddr_len,
					(struct sockaddr *)&prevaddr,
					&prevaddr_len,
					io_flag
					))<0){
    printf("Error(%d) in wait_next_stage_arrival\n",err);
  }

  zs_free_msg_packet(sched, buf);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  close(fd);

}


void *angle_task(void *argp){
  struct zs_reserve_params cpuattr;
  struct timespec now;
  unsigned long long now_ns;
  int rid;
  int err;
  int sched;
  int i;
  unsigned char *buf;
  struct sockaddr_in myaddr, remaddr;
  int fd;
  struct task_stage_params *stage_paramsp;
  int io_flag;
  int remaddr_len;
  
  stage_paramsp = (struct task_stage_params *)argp;

  printf("angle: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <0){
    perror("could not create socket\n");
    return NULL;
  }

  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(stage_paramsp->myport);
  
  if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
    perror("bind failed");
    return NULL;
  }

  memset((char *)&remaddr, 0, sizeof(remaddr));
  remaddr.sin_family = AF_INET;
  remaddr.sin_port = htons(stage_paramsp->destination_port);
  if (inet_aton(stage_paramsp->destination_address, &remaddr.sin_addr)==0){
    printf("could not parse destination ip address");
    return NULL;
  }


  // BOUND TO PROCESSOR 3

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(3,&cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 1\n");
  }

  strcpy(cpuattr.name,"angle");
  cpuattr.period.tv_sec =  14;
  cpuattr.period.tv_nsec= 534000000;
  cpuattr.priority = 1;
  /* cpuattr.execution_time.tv_sec = 0; */
  /* cpuattr.execution_time.tv_nsec = 100000000; */
  /* cpuattr.overload_execution_time.tv_sec = 0; */
  /* cpuattr.overload_execution_time.tv_nsec = 500000000; */
  cpuattr.zs_instant.tv_sec=8;//11; 
  cpuattr.zs_instant.tv_nsec=0; //278000000; 
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_LEAF | APERIODIC_ARRIVAL;
  cpuattr.enforcement_mask = DONT_ENFORCE_ZERO_SLACK_MASK;//ZS_ENFORCEMENT_HARD_MASK;//DONTENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 4;
  cpuattr.e2e_execution_time.tv_nsec = 360000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 5;
  cpuattr.e2e_overload_execution_time.tv_nsec = 448000000;
  cpuattr.criticality = 4;
  cpuattr.normal_marginal_utility = 4;
  cpuattr.overloaded_marginal_utility = 4;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu=3;
  cpuattr.insockfd = fd;

  fds[stage_paramsp->fd_index] = fd;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  buf = zs_alloc_stage_msg_packet(sched, BUFSIZE);
  if (buf == NULL){
    printf("failed to allocate msg packet\n");
    return NULL;
  }


  rid = zs_create_reserve(sched,&cpuattr);

  if (rid <0){
    printf("failed to create reserve\n");
    return NULL;
  }



  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  printf("angle: after attached\n");
  i=0;
  int firsttime=1;
  buf[0] = '\0';
  while (strstr(buf,"bye") == NULL){
    remaddr_len = sizeof(remaddr);
    printf("angle: before wait_next_leaft\n");
    if ((err = zs_wait_next_leaf_stage_arrival(sched, rid, 
					       fd, buf, 
					       BUFSIZE,0, 
					       (struct sockaddr *)&remaddr,
					       &remaddr_len))<0){
      printf("Error(%d) in wait_next_arrival\n",err);
      break;
    }
    printf("angle received[%s] from addr(%s)\n",buf,inet_ntoa(remaddr.sin_addr));
    if (firsttime){
      firsttime=0;
      //busy_timestamped(1362,timestamps_ns4, MAX_TIMESTAMPS,&bufidx4);
    } else {
      //busy_timestamped(1362,timestamps_ns4, MAX_TIMESTAMPS,&bufidx4);
    }
  }

  zs_free_msg_packet(sched, buf);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  close(fd);

}

/******************************************
 * END    OF    STALL WARNING SYSTEM      *
 ******************************************/
