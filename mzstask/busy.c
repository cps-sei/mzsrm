
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

#include <modaltask.h>

#define NUM_SILENT_ITERATIONS 10000

// at frequency: 1500000 
#define IN_LOOP_ONE_MS 247500
#define NUM_IN_LOOPS_ONE_MS 1
void  busy(long millis){
  long i,j,k;

  for (k=0;k<millis;k++)
    for (i=0;i<NUM_IN_LOOPS_ONE_MS;i++)
      for (j=0;j<IN_LOOP_ONE_MS;j++)
	;
}

void busy_timestamped(long millis, unsigned long long tsbuffer[], long bufsize, long *bufidx){

  long i,j,k,s;

  s=0;
  for (k=0;k<millis;k++)
    for (i=0;i<NUM_IN_LOOPS_ONE_MS;i++)
      for (j=0;j<IN_LOOP_ONE_MS;j++){
	s++;
	if (s >= NUM_SILENT_ITERATIONS){
	  s=0;
	  if (*bufidx < bufsize){
	    tsbuffer[(*bufidx)++] = now_ns();
	  }
	}
      }
}
