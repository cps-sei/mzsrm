void get_reserve_type_description(int type, char *buf){
  buf[0]='\0';

  if (type & CRITICALITY_RESERVE){
    sprintf(buf,"CRITICALITY_RESERVE");
  }
  if (type & APERIODIC_ARRIVAL){
    sprintf(buf,"%s | APERIODIC_ARRIVAL",buf);
  }
  if (type & PERIODIC_ARRIVAL){
    sprintf(buf,"%s | PERIODIC_ARRIVAL",buf);
  }
  if (type & PIPELINE_STAGE_ROOT){
    sprintf(buf,"%s | PIPELINE_STAGE_ROOT",buf);
  }
  if (type & PIPELINE_STAGE_MIDDLE){
    sprintf(buf,"%s | PIPELINE_STAGE_MIDDLE",buf);
  }
  if (type & PIPELINE_STAGE_LEAF){
    sprintf(buf,"%s | PIPELINE_STAGE_LEAF",buf);
  }
}

void *generic_stage_task(void *argp){
  //struct zs_reserve_params cpuattr;
  struct timespec now;
  //unsigned long long now_ns;
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
  int remaddr_len,prevaddr_len;
  struct sockaddr_in remaddr,prevaddr;
  char rsv_type_description[100];
  
  stage_paramsp = (struct task_stage_params *)argp;

  if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) <0){
    perror("could not create socket\n");
    return NULL;
  }

  get_reserve_type_description(stage_paramsp->cpuattr.reserve_type, rsv_type_description);
  
  printf("thread(%s): params: destination(%s:%d) myport(%d) delay(%ld) busy(%ld) reseve_type(%s)\n",
	 stage_paramsp->cpuattr.name,
	 stage_paramsp->destination_address, 
	 stage_paramsp->destination_port, 
	 stage_paramsp->myport,
	 stage_paramsp->usec_delay,
	 stage_paramsp->busytime_ms,
	 rsv_type_description
	 );

  memset((char *)&myaddr, 0, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(stage_paramsp->myport);
  
  if (!(stage_paramsp->cpuattr.reserve_type & PIPELINE_STAGE_ROOT)){
    if (bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr)) < 0) {
      perror("bind failed");
      return NULL;
    }
  }

  memset((char *)&outaddr, 0, sizeof(outaddr));
  outaddr.sin_family = AF_INET;
  outaddr.sin_port = htons(stage_paramsp->destination_port);
  if (inet_aton(stage_paramsp->destination_address, &outaddr.sin_addr)==0){
    printf("could not parse destination ip address");
    return NULL;
  }



  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(stage_paramsp->cpuattr.bound_to_cpu,&cpuset);

  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0){
    printf("Error setting CPU affinity of task 1\n");
  }
  
  if (stage_paramsp->fd_index >=0)
    fds[stage_paramsp->fd_index] = fd;

  stage_paramsp->cpuattr.insockfd = fd;

  if ((sched = zs_open_sched()) == -1){
    printf("error opening the scheduler\n");
    return NULL;
  }

  buf = zs_alloc_stage_msg_packet(sched, BUFSIZE);
  if (buf == NULL){
    printf("failed to allocate msg packet\n");
    return NULL;
  }

  rid = zs_create_reserve(sched,&(stage_paramsp->cpuattr));

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

  usleep(stage_paramsp->usec_delay);

  zs_attach_reserve(sched,rid,gettid());

  io_flag = MIDDLE_STAGE_DONT_SEND_OUTPUT;

  i=0;
  buf[0] = '\0';
  while(strstr(buf,"bye") == NULL){
    sprintf(buf,"msg[%d]",i);

    if (stage_paramsp->cpuattr.reserve_type & PIPELINE_STAGE_ROOT){
      busy_timestamped(stage_paramsp->busytime_ms,
		       stage_paramsp->timestamp_ns, 
		       MAX_TIMESTAMPS,
		       &(stage_paramsp->bufidx));
      err = zs_wait_next_root_period(sched,rid,fd, 
				     buf ,strlen(buf)+1, 
				     0, (struct sockaddr *)&outaddr, 
				     sizeof (outaddr));
      printf("stage(%s) sent(%s)\n",stage_paramsp->cpuattr.name, buf);
    } else if (stage_paramsp->cpuattr.reserve_type & PIPELINE_STAGE_MIDDLE){
      remaddr_len = sizeof (outaddr);
      prevaddr_len = sizeof(outaddr);
      //printf("stage(%s) going into wait arrival\n",stage_paramsp->cpuattr.name);
      err = zs_wait_next_stage_arrival(sched,rid,
				       fd, buf ,
				       BUFSIZE, 0, 
				       (struct sockaddr *)&outaddr, 
				       remaddr_len,
				       (struct sockaddr *)&prevaddr,
				       &prevaddr_len,
				       io_flag
				       );
      if (err<0){
	perror("error in wait_next_x");
	break;
      }
      printf("stage(%s) received(%s)\n",stage_paramsp->cpuattr.name, buf);
      busy_timestamped(stage_paramsp->busytime_ms,
		       stage_paramsp->timestamp_ns, 
		       MAX_TIMESTAMPS,
		       &(stage_paramsp->bufidx));
      io_flag = 0;
    } else { // LEAF
      remaddr_len = sizeof (outaddr);
      //printf("stage(%s) going into wait arrival\n",stage_paramsp->cpuattr.name);
      err = zs_wait_next_leaf_stage_arrival(sched, rid, 
					    fd, buf, 
					    BUFSIZE,0, 
					    (struct sockaddr *)&remaddr,
					    &remaddr_len);
      if (err<0){
	perror("error in wait_next_x");
	break;
      }
      printf("stage(%s) received(%s)\n",stage_paramsp->cpuattr.name, buf);
      busy_timestamped(stage_paramsp->busytime_ms,
		       stage_paramsp->timestamp_ns, 
		       MAX_TIMESTAMPS,
		       &(stage_paramsp->bufidx));
    }
    i++;
    if (stage_paramsp->cpuattr.reserve_type & PIPELINE_STAGE_ROOT){
      if (i>=10){
	sprintf(buf,"bye");
      }
    }
  }

  sprintf(buf,"bye");

  if (stage_paramsp->cpuattr.reserve_type & PIPELINE_STAGE_ROOT){
      err = zs_wait_next_root_period(sched,rid,fd, 
				     buf ,strlen(buf)+1, 
				     0, (struct sockaddr *)&outaddr, 
				     sizeof (outaddr));
  } else if (stage_paramsp->cpuattr.reserve_type & PIPELINE_STAGE_MIDDLE){
    io_flag = MIDDLE_STAGE_DONT_WAIT_INPUT;
    remaddr_len = sizeof (outaddr);
    err = zs_wait_next_stage_arrival(sched,rid,
				     fd, buf ,
				     BUFSIZE, 0, 
				     (struct sockaddr *)&outaddr, 
				     remaddr_len,
				     (struct sockaddr *)&prevaddr,
				     &prevaddr_len,
				     io_flag
				     );
  }

  zs_free_msg_packet(sched, buf);
  zs_detach_reserve(sched,rid);
  zs_delete_reserve(sched,rid);
  zs_close_sched(sched);

  close(fd);

}
