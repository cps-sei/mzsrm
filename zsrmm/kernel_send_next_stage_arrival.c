int wait_for_next_stage_arrival(int rid, int outdatalen, unsigned long *flags){
  //struct msghdr msg;
  //struct iovec iov[1];
  struct msghdr msg = {NULL,};
  struct kvec iov = {reserve_table[rid].indata, reserve_table[rid].indatalen};
  int len;
  struct sched_param p;
  struct pipeline_header *pipeline_hdrp;
  //mm_segment_t oldms;

  memset(&msg,0,sizeof(msg));
  //msg.msg_iov = iov;
  //msg.msg_iovlen = 1;
  iov.iov_base = reserve_table[rid].outdata;
  iov.iov_len = outdatalen; 
  msg.msg_name = (caddr_t) & reserve_table[rid].params.outaddr;
  msg.msg_namelen = sizeof(reserve_table[rid].params.outaddr);
  msg.msg_flags = 0;//MSG_DONTWAIT | MSG_NOSIGNAL;
  msg.msg_control = NULL;
  msg.msg_namelen = 0;


  end_of_job_processing(rid, 1);

  // add to output message the e2e_job_exectime
  pipeline_hdrp = (struct pipeline_header *) reserve_table[rid].outdata;
  pipeline_hdrp->cumm_exectime_nanos = reserve_table[rid].e2e_job_executing_nanos;
  
  kernel_sendmsg(reserve_table[rid].outsock, &msg, &iov, 1, outdatalen);

  //sock_sendmsg(reserve_table[rid].outsock, &msg, outdatalen);

  // now wait for arrival of next socket message
  iov.iov_base = reserve_table[rid].indata;
  iov.iov_len = reserve_table[rid].indatalen;
  msg.msg_name = NULL;
  msg.msg_namelen = 0;


  p.sched_priority = scheduler_priority;
  sched_setscheduler(current, SCHED_FIFO, &p);

  // release zsrm locks
  spin_unlock_irqrestore(&zsrmlock, *flags);
  up(&zsrmsem);

  //oldms = get_fs(); set_fs(KERNEL_DS);
  len = kernel_recvmsg(reserve_table[rid].insock, &msg, &iov, 1, reserve_table[rid].indatalen, 0);
  //len = sock_recvmsg(reserve_table[rid].insock, &msg,reserve_table[rid].indatalen, MSG_WAITALL);
  //set_fs(oldms);

  if (len <0){
    printk("zsrmm: sock_recvmsg failed\n");
  }
  // reacquire zsrm locks
  down(&zsrmsem);
  spin_lock_irqsave(&zsrmlock, *flags);

  // read the pipeline header
  pipeline_hdrp = (struct pipeline_header *) reserve_table[rid].indata;
  reserve_table[rid].e2e_job_executing_nanos = pipeline_hdrp->cumm_exectime_nanos;

  start_of_job_processing(rid);

  return len;
}

int wait_for_next_root_period(int rid, int fd, void __user *buf, size_t buflen, unsigned int flags, struct sockaddr __user *addr, int addrlen){
  int len=0;
  struct pipeline_header_with_signature pipeline_hdrs; 

  end_of_job_processing(rid, 1);
  
  printk("zsrmm: wait_next_root_period: trying to send msglen(%d) to iet address(0x%X) port(%d) from pointer(%p)\n", 
	 (int)buflen,
	 reserve_table[rid].params.outaddr.sin_addr.s_addr, 
	 reserve_table[rid].params.outaddr.sin_port,
	 reserve_table[rid].outdata);

  copy_from_user(&pipeline_hdrs, (buf-sizeof(struct pipeline_header_with_signature)), 
		 sizeof(struct pipeline_header_with_signature));  
  if (pipeline_hdrs.signature != MODULE_SIGNATURE){
    printk("zsrmm: wait_next_root_period: wrong packet signature\n");
    len = -1;
  } else {
    pipeline_hdrs.header.cumm_exectime_nanos = reserve_table[rid].e2e_job_executing_nanos;
    copy_to_user(buf - sizeof(struct pipeline_header_with_signature), 
		 &pipeline_hdrs, sizeof(struct pipeline_header_with_signature));
    if ((len = sys_sendtop(fd, buf-sizeof(struct pipeline_header), 
			   buflen+sizeof(struct pipeline_header), 
			   flags, addr, addrlen)) <0){
      printk("zsrmm: kernel_sendmsg returned %d\n",len);
      if (len == -EINVAL){
	printk("zsrmm: kernel_sendmsg return EINVAL\n");
      }
    }
    
    set_current_state(TASK_UNINTERRUPTIBLE);
  }

  return len;
}
