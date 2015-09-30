/***********************
 * TRAFFIC AWARENESS SYSTEM*
 ***********************/
void *ground_radar_task(void *argp){
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
  
  stage_paramsp = (struct task_stage_params *)argp;

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <0){
    perror("could not create socket\n");
    return NULL;
  }

  printf("gps-position: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);

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

  strcpy(cpuattr.name,"groundradar");
  cpuattr.period.tv_sec = 2;
  cpuattr.period.tv_nsec= 0;
  cpuattr.priority = 2;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 100000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 100000000;
  cpuattr.zs_instant.tv_sec=2;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_ROOT;
  cpuattr.enforcement_mask = 0;//DONT_ENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 0;
  cpuattr.e2e_execution_time.tv_nsec = 500000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 0;
  cpuattr.e2e_overload_execution_time.tv_nsec = 600000000;
  cpuattr.criticality = 3;
  cpuattr.normal_marginal_utility = 3;
  cpuattr.overloaded_marginal_utility = 3;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu = 0;

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

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());


  for (i=0;i<10;i++){
    busy_timestamped(100,timestamps_nsd, MAX_TIMESTAMPS,&bufidxd);
    sprintf(buf,"msg[%d]",i);
    if ((err = zs_wait_next_root_period(sched,rid,fd, 
					buf ,strlen(buf)+1, 
					0, (struct sockaddr *)&outaddr, 
					sizeof (outaddr)))<0){
      perror("error in wait_next_root");
      break;
    }
    printf("gps-position sent(%s)\n",buf);
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


void *terrain_distance_task(void *argp){
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

  printf("stop-distance: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);

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

  strcpy(cpuattr.name,"distance");
  cpuattr.period.tv_sec = 2;
  cpuattr.period.tv_nsec= 0;
  cpuattr.priority = 2;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 100000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 100000000;
  cpuattr.zs_instant.tv_sec=2;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_MIDDLE | APERIODIC_ARRIVAL;
  cpuattr.enforcement_mask = 0;//DONT_ENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 0;
  cpuattr.e2e_execution_time.tv_nsec = 500000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 0;
  cpuattr.e2e_overload_execution_time.tv_nsec = 600000000;
  cpuattr.criticality = 3;
  cpuattr.normal_marginal_utility = 3;
  cpuattr.overloaded_marginal_utility = 3;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu = 1;
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

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  io_flag = MIDDLE_STAGE_DONT_SEND_OUTPUT;
  i=0;
  buf[0] = '\0';
  while (strstr(buf,"bye") == NULL){
    remaddr_len = sizeof (outaddr);
    prevaddr_len = sizeof(outaddr);
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
    printf("stop-distance received(%s)\n",buf);
    if (strstr(buf,"bye") == NULL){
      sprintf(buf,"msg[%d]",i++);
    }
    io_flag = 0;
    busy_timestamped(100,timestamps_nse, MAX_TIMESTAMPS,&bufidxe);
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

void *time_to_terrain_task(void *argp){
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

  printf("stop-location: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);


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

  strcpy(cpuattr.name,"time");
  cpuattr.period.tv_sec = 2;
  cpuattr.period.tv_nsec= 0;
  cpuattr.priority = 2;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 100000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 100000000;
  cpuattr.zs_instant.tv_sec=2;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_MIDDLE | APERIODIC_ARRIVAL;
  cpuattr.enforcement_mask = 0;//DONT_ENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 0;
  cpuattr.e2e_execution_time.tv_nsec = 500000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 0;
  cpuattr.e2e_overload_execution_time.tv_nsec = 600000000;
  cpuattr.criticality = 3;
  cpuattr.normal_marginal_utility = 3;
  cpuattr.overloaded_marginal_utility = 3;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu = 2;
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

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());

  io_flag = MIDDLE_STAGE_DONT_SEND_OUTPUT;
  i=0;
  int firsttime=1;
  buf[0] = '\0';
  while (strstr(buf,"bye") == NULL){
    remaddr_len = sizeof (outaddr);
    prevaddr_len = sizeof(outaddr);
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
    printf("stop-location received(%s)\n",buf);
    if (strstr(buf,"bye") == NULL){
      sprintf(buf,"msg[%d]",i++);
    }
    io_flag = 0;
    if (firsttime){
      busy_timestamped(300,timestamps_nsf, MAX_TIMESTAMPS,&bufidxf);
      //firsttime=0;
    } else {
      busy_timestamped(200,timestamps_nsf, MAX_TIMESTAMPS,&bufidxf);
    }    
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


void *terrain_warning_task(void *argp){
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

  printf("virtual-runway: params: destination(%s:%d) myport(%d)\n",stage_paramsp->destination_address, stage_paramsp->destination_port, stage_paramsp->myport);

  //MISSING THE BIND!!!!!!

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

  strcpy(cpuattr.name,"terrain");
  cpuattr.period.tv_sec = 2;
  cpuattr.period.tv_nsec= 0;
  cpuattr.priority = 2;
  cpuattr.execution_time.tv_sec = 0;
  cpuattr.execution_time.tv_nsec = 100000000;
  cpuattr.overload_execution_time.tv_sec = 0;
  cpuattr.overload_execution_time.tv_nsec = 100000000;
  cpuattr.zs_instant.tv_sec=2;
  cpuattr.zs_instant.tv_nsec=0;
  cpuattr.response_time_instant.tv_sec = 10;
  cpuattr.response_time_instant.tv_nsec =0;
  cpuattr.reserve_type = CRITICALITY_RESERVE | PIPELINE_STAGE_LEAF | APERIODIC_ARRIVAL;
  cpuattr.enforcement_mask = 0;//DONT_ENFORCE_ZERO_SLACK;
  cpuattr.e2e_execution_time.tv_sec = 0;
  cpuattr.e2e_execution_time.tv_nsec = 500000000;
  cpuattr.e2e_overload_execution_time.tv_sec = 0;
  cpuattr.e2e_overload_execution_time.tv_nsec = 600000000;
  cpuattr.criticality = 3;
  cpuattr.normal_marginal_utility = 3;
  cpuattr.overloaded_marginal_utility = 3;
  cpuattr.critical_util_degraded_mode = -1;
  cpuattr.num_degraded_modes=0;
  cpuattr.bound_to_cpu = 3;
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

  struct sembuf sops;
  sops.sem_num=0;
  sops.sem_op = -1; // down
  sops.sem_flg = 0;
  if (semop(sync_start_semid,&sops,1)<0){
    printf("error on sync star sem down\n");
  }

  zs_attach_reserve(sched,rid,gettid());


  buf[0] = '\0';
  while (strstr(buf,"bye") == NULL){
    remaddr_len = sizeof(remaddr);
    if ((err = zs_wait_next_leaf_stage_arrival(sched, rid, 
					       fd, buf, 
					       BUFSIZE,0, 
					       (struct sockaddr *)&remaddr,
					       &remaddr_len))<0){
      printf("Error(%d) in wait_next_arrival\n",err);
      break;
    }
    printf("virtual-runway received[%s] from addr(%s)\n",buf,inet_ntoa(remaddr.sin_addr));
    busy_timestamped(100,timestamps_nsg, MAX_TIMESTAMPS,&bufidxg);
  }

  zs_free_msg_packet(sched, buf);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  close(fd);

}

/******************************************
 * END    OF    TRAFFIC AWARENESS SYSTEM  *
 ******************************************/
