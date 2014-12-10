/*
Copyright (c) 2014 Carnegie Mellon University.

All Rights Reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the 
following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following 
acknowledgments and disclaimers.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
acknowledgments and disclaimers in the documentation and/or other materials provided with the distribution.
3. Products derived from this software may not include “Carnegie Mellon University,” "SEI” and/or “Software 
Engineering Institute" in the name of such derived product, nor shall “Carnegie Mellon University,” "SEI” and/or 
“Software Engineering Institute" be used to endorse or promote products derived from this software without prior 
written permission. For written permission, please contact permission@sei.cmu.edu.

ACKNOWLEDMENTS AND DISCLAIMERS:
Copyright 2014 Carnegie Mellon University
This material is based upon work funded and supported by the Department of Defense under Contract No. FA8721-
05-C-0003 with Carnegie Mellon University for the operation of the Software Engineering Institute, a federally 
funded research and development center.

Any opinions, findings and conclusions or recommendations expressed in this material are those of the author(s) and 
do not necessarily reflect the views of the United States Department of Defense.

NO WARRANTY. 
THIS CARNEGIE MELLON UNIVERSITY AND SOFTWARE ENGINEERING INSTITUTE 
MATERIAL IS FURNISHED ON AN “AS-IS” BASIS. CARNEGIE MELLON UNIVERSITY MAKES NO 
WARRANTIES OF ANY KIND, EITHER EXPRESSED OR IMPLIED, AS TO ANY MATTER INCLUDING, 
BUT NOT LIMITED TO, WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, 
EXCLUSIVITY, OR RESULTS OBTAINED FROM USE OF THE MATERIAL. CARNEGIE MELLON 
UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT TO FREEDOM FROM 
PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.

This material has been approved for public release and unlimited distribution.
Carnegie Mellon® is registered in the U.S. Patent and Trademark Office by Carnegie Mellon University.

DM-0000891
*/

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/unistd.h>
#include <time.h>
#include "zsrm.h"

int create_reserve(){
	int fd, ret;
	struct api_call call;

	fd = open("/dev/zsrmm", O_RDWR);
	if (fd < 0) {
		printf("Error: failed to access the module!\n");
		return -1;
	}
	call.api_id = CREATE_RESERVE;
	call.args.reserve_parameters.period.tv_sec = 1;
	call.args.reserve_parameters.period.tv_nsec = 0;
	call.args.reserve_parameters.zs_instant.tv_sec = 2;
	call.args.reserve_parameters.zs_instant.tv_nsec=0;
	ret = write(fd, &call, sizeof(call));
	close(fd);

	return ret;
}

int attach_reserve(int rid){
	int fd, ret;
	struct api_call call;

	fd = open("/dev/zsrmm", O_RDWR);
	if (fd < 0) {
		printf("Error: failed to access the module!\n");
		return -1;
	}
	call.api_id = ATTACH_RESERVE;
	call.args.attach_params.reserveid = rid;
	call.args.attach_params.pid = getpid();
	ret = write(fd, &call, sizeof(call));
	close(fd);

	return ret;
}

int wait_period(int rid){
	int fd, ret;
	struct api_call call;

	fd = open("/dev/zsrmm", O_RDWR);
	if (fd < 0) {
		printf("Error: failed to access the module!\n");
		return -1;
	}
	call.api_id = WAIT_NEXT_PERIOD;
	call.args.reserveid = rid;
	ret = write(fd, &call, sizeof(call));
	close(fd);

	return ret;
}

int delete_reserve(int rid){
	int fd, ret;
	struct api_call call;

	fd = open("/dev/zsrmm", O_RDWR);
	if (fd < 0) {
		printf("Error: failed to access the module!\n");
		return -1;
	}
	call.api_id = DELETE_RESERVE;
	call.args.reserveid = rid;
	ret = write(fd, &call, sizeof(call));
	close(fd);

	return ret;

}
int main(){
	int rid;
	struct timespec now;
	int count = 10;
	rid = create_reserve();
	attach_reserve(rid);

	while(count--){
		clock_gettime(CLOCK_REALTIME,&now);
		printf("start at: %d:%d\n",now.tv_sec, now.tv_nsec);
		wait_period(rid);
	}

	delete_reserve(rid);
}
