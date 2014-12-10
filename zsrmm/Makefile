#Copyright (c) 2014 Carnegie Mellon University.
#
#All Rights Reserved.
#
#Redistribution and use in source and binary forms, with or without modification, are permitted provided that the 
#following conditions are met:
#
#1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following 
#acknowledgments and disclaimers.
#2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following 
#acknowledgments and disclaimers in the documentation and/or other materials provided with the distribution.
#3. Products derived from this software may not include “Carnegie Mellon University,” "SEI” and/or “Software 
#Engineering Institute" in the name of such derived product, nor shall “Carnegie Mellon University,” "SEI” and/or 
#“Software Engineering Institute" be used to endorse or promote products derived from this software without prior 
#written permission. For written permission, please contact permission@sei.cmu.edu.
#
#ACKNOWLEDMENTS AND DISCLAIMERS:
#Copyright 2014 Carnegie Mellon University
#This material is based upon work funded and supported by the Department of Defense under Contract No. FA8721-
#05-C-0003 with Carnegie Mellon University for the operation of the Software Engineering Institute, a federally 
#funded research and development center.
#
#Any opinions, findings and conclusions or recommendations expressed in this material are those of the author(s) and 
#do not necessarily reflect the views of the United States Department of Defense.
#
#NO WARRANTY. 
#THIS CARNEGIE MELLON UNIVERSITY AND SOFTWARE ENGINEERING INSTITUTE 
#MATERIAL IS FURNISHED ON AN “AS-IS” BASIS. CARNEGIE MELLON UNIVERSITY MAKES NO 
#WARRANTIES OF ANY KIND, EITHER EXPRESSED OR IMPLIED, AS TO ANY MATTER INCLUDING, 
#BUT NOT LIMITED TO, WARRANTY OF FITNESS FOR PURPOSE OR MERCHANTABILITY, 
#EXCLUSIVITY, OR RESULTS OBTAINED FROM USE OF THE MATERIAL. CARNEGIE MELLON 
#UNIVERSITY DOES NOT MAKE ANY WARRANTY OF ANY KIND WITH RESPECT TO FREEDOM FROM 
#PATENT, TRADEMARK, OR COPYRIGHT INFRINGEMENT.
#
#This material has been approved for public release and unlimited distribution.
#Carnegie Mellon® is registered in the U.S. Patent and Trademark Office by Carnegie Mellon University.
#
#DM-0000891

TARGET = mzsrmm
OBJS = main.o
UNAME_P := $(shell uname -p)

#INCDIR = ../include/
ifeq ($(UNAME_P),x86_64)
INCDIR = /lib/modules/$(shell uname -r)/build/include
else
ifeq ($(UNAME_P),armv7l)
INCDIR = ../../odroid-linux/include
EXTRA_CFLAGS += -D__ARMCORE__
else
$(error platform (uname -p) must be x86_64 or armv7l)
endif
endif
# If KERNELRELEASE is define, we have been invoked from the
# kernel build system and can use its languages.
ifneq ($(KERNELRELEASE),)
	obj-m := $(TARGET).o
	$(TARGET)-objs := $(OBJS)
	EXTRA_CFLAGS += -I$(src)/$(INCDIR)

# Otherwise we were called directly from the command line;
# invoke the kernel build system
else
#	KERNELDIR ?= /home/dionisio/LinuxRK/drone-rk/kernel/linux-2.6.27-rk
	ifeq ($(UNAME_P),x86_64)
		KERNELDIR ?= /lib/modules/$(shell uname -r)/build
	else
		KERNELDIR ?= ../../odroid-linux
	endif
	PWD := $(shell pwd)
	EXTRA_CFLAGS += -I$(PWD)/$(INCDIR) -O1

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

$(TARGET).o: $(OBJS)
	$(LD) $(LD_RFLAG) -r -o $@ $(OBJS)

clean:
	rm -fr .tmp* .*.o.cmd *.mod.* *.ko *.o .$(TARGET)* modules.order Module.* *~

endif
