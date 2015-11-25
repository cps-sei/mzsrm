#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_TIMESTAMPS 1000000

#define BIG_LL 4223372036854775807L

long bufidx1=0;
long bufidx2=0;
long bufidx3=0;
long bufidx4=0;
unsigned long long timestamps_ns1[MAX_TIMESTAMPS];
unsigned long long timestamps_ns2[MAX_TIMESTAMPS];
unsigned long long timestamps_ns3[MAX_TIMESTAMPS];
unsigned long long timestamps_ns4[MAX_TIMESTAMPS];

long outbufidx1=0;
long outbufidx2=0;
long outbufidx3=0;
long outbufidx4=0;
unsigned long long outtimestamps_ns1[MAX_TIMESTAMPS];
unsigned long long outtimestamps_ns2[MAX_TIMESTAMPS];
unsigned long long outtimestamps_ns3[MAX_TIMESTAMPS];
unsigned long long outtimestamps_ns4[MAX_TIMESTAMPS];



long nextstart =0;
long nextend=0;
int done=0;
int idx1=0;
int idx2=0;
int idx3=0;
int idx4=0;
int running =0;
unsigned long long ts=1;


void find_new_start(){
  ts=0;
  // find first next start
  if ( (idx1 < bufidx1) &&
       (timestamps_ns1[idx1] < timestamps_ns2[idx2]) &&
       (timestamps_ns1[idx1] < timestamps_ns3[idx3]) &&
       (timestamps_ns1[idx1] < timestamps_ns4[idx4])){
    nextstart = idx1++;
    running = 1;
    ts = timestamps_ns1[nextstart];
  } else if ( (idx2 < bufidx2) &&
	      (timestamps_ns2[idx2] < timestamps_ns1[idx1]) &&
	      (timestamps_ns2[idx2] < timestamps_ns3[idx3]) &&
	      (timestamps_ns2[idx2] < timestamps_ns4[idx4])){
    nextstart = idx2++;
    running = 3;
    ts = timestamps_ns2[nextstart];
  } else if ( (idx3 < bufidx3) &&
	      (timestamps_ns3[idx3] < timestamps_ns1[idx1]) &&
	      (timestamps_ns3[idx3] < timestamps_ns2[idx2]) &&
	      (timestamps_ns3[idx3] < timestamps_ns4[idx4])){
    nextstart = idx3++;
    running = 3;
    ts = timestamps_ns3[nextstart];
  } else if ( (idx4 < bufidx4) &&
	     (timestamps_ns4[idx4] < timestamps_ns1[idx1]) &&
	      (timestamps_ns4[idx4] < timestamps_ns2[idx2]) &&
	      (timestamps_ns4[idx4] < timestamps_ns3[idx3])){
    nextstart = idx4++;
    running = 4;
    ts = timestamps_ns4[nextstart];
  } else {
    running = -1;
  }
}

int main(int argc, char *argv[]){
  char buf[200];
  int idx;
  char outfn1[100];
  char outfn2[100];
  char outfn3[100];
  char outfn4[100];
  FILE * outfd1;
  FILE * outfd2;
  FILE * outfd3;
  FILE * outfd4;
  FILE * infd1;
  FILE * infd2;
  FILE * infd3;
  FILE * infd4;

  if (argc != 5){
    printf("usage: %s <file1> <file2> <file3> <file4>\n",argv[0]);
    return -1;
  }

  if ( (infd1 = fopen(argv[1],"r")) == NULL){
    printf("could not open file %s\n",argv[1]);
    return -1;
  }

  if ( (infd2 = fopen(argv[2],"r")) == NULL){
    printf("could not open file %s\n",argv[2]);
    return -1;
  }

  if ( (infd3 = fopen(argv[3],"r")) == NULL){
    printf("could not open file %s\n",argv[3]);
    return -1;
  }

  if ( (infd4 = fopen(argv[4],"r")) == NULL){
    printf("could not open file %s\n",argv[4]);
    return -1;
  }

  sprintf(outfn1,"%s.out",argv[1]);
  sprintf(outfn2,"%s.out",argv[2]);
  sprintf(outfn3,"%s.out",argv[3]);
  sprintf(outfn4,"%s.out",argv[4]);

  if ((outfd1 = fopen(outfn1,"w+")) == NULL){
    printf("could not open %s\n",outfn1);
    return -1;
  }
  if ((outfd2 = fopen(outfn2,"w+")) == NULL){
    printf("could not open %s\n",outfn2);
    return -1;
  }
  if ((outfd3 = fopen(outfn3,"w+")) == NULL){
    printf("could not open %s\n",outfn3);
    return -1;
  }
  if ((outfd4 = fopen(outfn4,"w+")) == NULL){
    printf("could not open %s\n",outfn4);
    return -1;
  }

  while ((fgets(buf,200,infd1)!= NULL)){// && (ts !=0L)){
    sscanf(buf,"%Lu",&ts);
    //    if (ts != 0)
      timestamps_ns1[bufidx1++]=ts;
  }
  timestamps_ns1[bufidx1] = BIG_LL;
  ts =1;
  while ((fgets(buf,200,infd2)!= NULL)){// && (ts != 0L)){
    sscanf(buf,"%Lu",&ts);
    //    if (ts != 0)
      timestamps_ns2[bufidx2++] = ts;
  }
  timestamps_ns2[bufidx2] = BIG_LL;
  ts=1;
  while ((fgets(buf,200,infd3)!= NULL)){// && (ts !=0L)){
    sscanf(buf,"%Lu",&ts);
    //    if (ts != 0)
      timestamps_ns3[bufidx3++] = ts;
  }
  timestamps_ns3[bufidx3] = BIG_LL;
  ts =1;
  while ((fgets(buf,200,infd4)!= NULL)){// && (ts != 0L)){
    sscanf(buf,"%Lu",&ts);
    //    if (ts != 0)
      timestamps_ns4[bufidx4++] = ts;
  }
  timestamps_ns4[bufidx4] = BIG_LL;

  /* printf("File: %s\n",argv[1]); */
  /* for (idx = 0 ; idx < bufidx1 ; idx++){ */
  /*   printf("%llu\n",timestamps_ns1[idx]); */
  /* } */
  /* printf("File: %s\n",argv[2]); */
  /* for (idx = 0 ; idx < bufidx2 ; idx++){ */
  /*   printf("%llu\n",timestamps_ns2[idx]); */
  /* } */
  /* printf("File: %s\n",argv[3]); */
  /* for (idx = 0 ; idx < bufidx3 ; idx++){ */
  /*   printf("%llu\n",timestamps_ns3[idx]); */
  /* } */
  /* printf("File: %s\n",argv[4]); */
  /* for (idx = 0 ; idx < bufidx4 ; idx++){ */
  /*   printf("%llu\n",timestamps_ns4[idx]); */
  /* } */

  /* long nextstart =0; */
  /* long nextend=0; */
  /* int done=0; */
  /* int idx1=0; */
  /* int idx2=0; */
  /* int idx3=0; */
  /* int idx4=0; */
  /* int running =0; */
  /* ts=0; */
  /* // find first next start */
  /* if ( (timestamps_ns1[idx1] < timestamps_ns2[idx2]) && */
  /*      (timestamps_ns1[idx1] < timestamps_ns3[idx3]) && */
  /*      (timestamps_ns1[idx1] < timestamps_ns4[idx4])){ */
  /*   nextstart = idx1++; */
  /*   running = 1; */
  /*   ts = timestamps_ns1[nextstart]; */
  /* } else if ( (timestamps_ns2[idx2] < timestamps_ns1[idx1]) && */
  /* 	      (timestamps_ns2[idx2] < timestamps_ns3[idx3]) && */
  /* 	      (timestamps_ns2[idx2] < timestamps_ns4[idx4])){ */
  /*   nextstart = idx2++; */
  /*   running = 3; */
  /*   ts = timestamps_ns2[nextstart]; */
  /* } else if ( (timestamps_ns3[idx3] < timestamps_ns1[idx1]) && */
  /* 	      (timestamps_ns3[idx3] < timestamps_ns2[idx2]) && */
  /* 	      (timestamps_ns3[idx3] < timestamps_ns4[idx4])){ */
  /*   nextstart = idx3++; */
  /*   running = 3; */
  /*   ts = timestamps_ns3[nextstart]; */
  /* } else if ( (timestamps_ns4[idx4] < timestamps_ns1[idx1]) && */
  /* 	      (timestamps_ns4[idx4] < timestamps_ns2[idx2]) && */
  /* 	      (timestamps_ns4[idx4] < timestamps_ns3[idx3])){ */
  /*   nextstart = idx4++; */
  /*   running = 4; */
  /*   ts = timestamps_ns4[nextstart]; */
  /* } */

  find_new_start();

  do{
    // output the start
    switch(running){
    case 1:
      if (outbufidx1 == 0 || outtimestamps_ns1[outbufidx1-1] != timestamps_ns1[nextstart]){
	ts = outtimestamps_ns1[outbufidx1++] = timestamps_ns1[nextstart];
      }
      break;
    case 2:
      if (outbufidx2 == 0 || outtimestamps_ns2[outbufidx2-1] != timestamps_ns2[nextstart]){
	ts = outtimestamps_ns2[outbufidx2++] = timestamps_ns2[nextstart];
      }
      break;
    case 3:
      if (outbufidx3 == 0 || outtimestamps_ns3[outbufidx3-1] != timestamps_ns3[nextstart]){
	ts = outtimestamps_ns3[outbufidx3++] = timestamps_ns3[nextstart];
      }
      break;
    case 4:
      if (outbufidx4 == 0 || outtimestamps_ns4[outbufidx4-1] != timestamps_ns4[nextstart]){
	ts = outtimestamps_ns4[outbufidx4++] = timestamps_ns4[nextstart];
      }
      break;
    otherwise:
      printf("invalid runner\n");
      break;
    }

    printf("running: %d idx: %ld ts: %llu\n",
	   running,
	   nextstart,
	   ts);

    // search for end and next start
    nextend = 0;
    int same=0;
    do{
      switch(running){
      case 1:
	if (idx1 >= bufidx1){
	  nextend = idx1 -1;
	  if (outtimestamps_ns1[outbufidx1-1] != timestamps_ns1[nextend]){
	    outtimestamps_ns1[outbufidx1++] = timestamps_ns1[nextend];
	  }
	  find_new_start();
	  same=0;
	} else
	if (timestamps_ns1[idx1] < timestamps_ns2[idx2]) {
	  if (timestamps_ns1[idx1] < timestamps_ns3[idx3]){
	    if (timestamps_ns1[idx1] < timestamps_ns4[idx4]){
	      idx1++;
	      same=1;	      
	    } else {
	      same=0;
	      running = 4;
	      nextstart = idx4++;
	      nextend = idx1++;
	      if (outtimestamps_ns1[outbufidx1-1] != timestamps_ns1[nextend]){
		outtimestamps_ns1[outbufidx1++] = timestamps_ns1[nextend];
	      }
	    }
	  } else {
	    same=0;
	    running = 3;
	    nextstart=idx3++;
	    nextend = idx1++;	    
	    if (outtimestamps_ns1[outbufidx1-1] != timestamps_ns1[nextend]){
	      outtimestamps_ns1[outbufidx1++] = timestamps_ns1[nextend];
	    }
	  }
	} else {
	    same=0;
	    running = 2;
	    nextstart=idx2++;
	    nextend = idx1++;
	    if (outtimestamps_ns1[outbufidx1-1] != timestamps_ns1[nextend]){
	      outtimestamps_ns1[outbufidx1++] = timestamps_ns1[nextend];
	    }
	}
	break;
      case 2:
	if (idx2 >= bufidx2){
	  nextend = idx2 -1;
	  if(outtimestamps_ns2[outbufidx2-1] != timestamps_ns2[nextend]){
	    outtimestamps_ns2[outbufidx2++] = timestamps_ns2[nextend];
	  }
	  find_new_start();
	  same=0;
	} else
	if (timestamps_ns2[idx2] < timestamps_ns1[idx1]) {
	  if (timestamps_ns2[idx2] < timestamps_ns3[idx3]){
	    if (timestamps_ns2[idx2] < timestamps_ns4[idx4]){
	      idx2++;
	      same=1;	      
	    } else {
	      same=0;
	      running = 4;
	      nextstart = idx4++;
	      nextend = idx2++;
	      if (outtimestamps_ns2[outbufidx2-1] != timestamps_ns2[nextend]){
		outtimestamps_ns2[outbufidx2++] = timestamps_ns2[nextend];
	      }
	    }
	  } else {
	    same=0;
	    running = 3;
	    nextstart=idx3++;
	    nextend = idx2++;	    
	    if (outtimestamps_ns2[outbufidx2-1] != timestamps_ns2[nextend]){
	      outtimestamps_ns2[outbufidx2++] = timestamps_ns2[nextend];
	    }
	  }
	} else {
	    same=0;
	    running = 1;
	    nextstart=idx1++;
	    nextend = idx2++;
	    if (outtimestamps_ns2[outbufidx2-1] != timestamps_ns2[nextend]){
	      outtimestamps_ns2[outbufidx2++] = timestamps_ns2[nextend];
	    }
	}
	break;
      case 3:
	if (idx3 >= bufidx3){
	  nextend = idx3 -1;
	  if (outtimestamps_ns3[outbufidx3-1] != timestamps_ns3[nextend]){
	    outtimestamps_ns3[outbufidx3++] = timestamps_ns3[nextend];
	  }
	  find_new_start();
	  same=0;
	} else
	if (timestamps_ns3[idx3] < timestamps_ns1[idx1]) {
	  if (timestamps_ns3[idx3] < timestamps_ns2[idx2]){
	    if (timestamps_ns3[idx3] < timestamps_ns4[idx4]){
	      idx3++;
	      same=1;	      
	    } else {
	      same=0;
	      running = 4;
	      nextstart = idx4++;
	      nextend = idx3++;
	      if (outtimestamps_ns3[outbufidx3-1] != timestamps_ns3[nextend]){
		outtimestamps_ns3[outbufidx3++] = timestamps_ns3[nextend];
	      }
	    }
	  } else {
	    same=0;
	    running = 2;
	    nextstart=idx2++;
	    nextend = idx3++;	    
	    if (outtimestamps_ns3[outbufidx3-1] != timestamps_ns3[nextend]){
	      outtimestamps_ns3[outbufidx3++] = timestamps_ns3[nextend];
	    }
	  }
	} else {
	    same=0;
	    running = 1;
	    nextstart=idx1++;
	    nextend = idx3++;
	    if (outtimestamps_ns3[outbufidx3-1] != timestamps_ns3[nextend]){
	      outtimestamps_ns3[outbufidx3++] = timestamps_ns3[nextend];
	    }
	}
	break;
      case 4:
	if (idx4 >= bufidx4){
	  nextend = idx4 -1;
	  if (outtimestamps_ns4[outbufidx4-1] != timestamps_ns4[nextend]){
	    outtimestamps_ns4[outbufidx4++] = timestamps_ns4[nextend];
	  }
	  find_new_start();
	  same = 0;
	} else
	if (timestamps_ns4[idx4] < timestamps_ns1[idx1]) {
	  if (timestamps_ns4[idx4] < timestamps_ns2[idx2]){
	    if (timestamps_ns4[idx4] < timestamps_ns3[idx3]){
	      idx4++;
	      same=1;	      
	    } else {
	      same=0;
	      running = 3;
	      nextstart = idx3++;
	      nextend = idx4++;
	      if (outtimestamps_ns4[outbufidx4-1] != timestamps_ns4[nextend]){
		outtimestamps_ns4[outbufidx4++] = timestamps_ns4[nextend];
	      }
	    }
	  } else {
	    same=0;
	    running = 2;
	    nextstart=idx2++;
	    nextend = idx4++;	    
	    if (outtimestamps_ns4[outbufidx4-1] != timestamps_ns4[nextend]){
	      outtimestamps_ns4[outbufidx4++] = timestamps_ns4[nextend];
	    }
	  }
	} else {
	    same=0;
	    running = 1;
	    nextstart=idx1++;
	    nextend = idx4++;
	    if (outtimestamps_ns4[outbufidx4-1] != timestamps_ns4[nextend]){
	      outtimestamps_ns4[outbufidx4++] = timestamps_ns4[nextend];
	    }
	}
	break;
      }
      
    } while(same );

    done = ((timestamps_ns1[idx1] == BIG_LL &&
	    timestamps_ns2[idx2] == BIG_LL &&
	    timestamps_ns3[idx3] == BIG_LL &&
	     timestamps_ns4[idx4] == BIG_LL)  ||
	    (timestamps_ns1[idx1] == 0L &&
	     timestamps_ns2[idx2] == 0L &&
	     timestamps_ns3[idx3] == 0L &&
	     timestamps_ns4[idx4] == 0L));
  } while (!done || running !=-1);

  for (idx=0;idx < outbufidx1; idx++){
    fprintf(outfd1,"%llu\n",outtimestamps_ns1[idx]);
  }
  close(outfd1);
  for (idx=0;idx < outbufidx2; idx++){
    fprintf(outfd2,"%llu\n",outtimestamps_ns2[idx]);
  }
  close(outfd2);
  for (idx=0;idx < outbufidx3; idx++){
    fprintf(outfd3,"%llu\n",outtimestamps_ns3[idx]);
  }
  close(outfd3);
  for (idx=0;idx < outbufidx4; idx++){
    fprintf(outfd4,"%llu\n",outtimestamps_ns4[idx]);
  }
  close(outfd4);
}
