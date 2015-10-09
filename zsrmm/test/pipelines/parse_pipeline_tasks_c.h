#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../zsrm.h"
#include "jsmn_c.h"
//#include "pipelinetask.h"

#define MAX_TIMESTAMPS 1000000

#define BUFSIZE 100


#include "generic-stage-params.h"

#include "pipelinetask.h"

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
	if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
			strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
		return 0;
	}
	return -1;
}

char *typenames[]={"object","string","array","other"};

char *gettypename(jsmntok_t *tok){
  switch(tok->type){
  case JSMN_STRING:
    return typenames[1];
    break;
  case JSMN_OBJECT:
    return typenames[0];
    break;
  case JSMN_ARRAY:
    return typenames[2];
    break;
  otherwise:
    return typenames[3];
    break;
  }
}

int add_reserve(struct reserve_list **head, struct reserve_list *tmp){//char *name, long period){
  /* struct reserve_list *tmp; */

  /* tmp = malloc(sizeof(struct reserve_list)); */
  /* if (tmp == NULL){ */
  /*   return -1; */
  /* } */

  /* strcpy(tmp->cpuattr.name,name); */
  /* tmp->cpuattr.period.tv_sec = period; */

  tmp->next = NULL;

  if ((*head) == NULL){
    (*head) = tmp;
  } else {
    tmp->next = (*head)->next;
    (*head)->next = tmp;
  }
}

struct reserve_list *get_reserve_list(char *json_filename, int *count){
  char *filecontent;
  long size;
  long read=0;
  int numtokens;
  char token[100];
  jsmn_parser parser;
  jsmntok_t tokens[1000];
  char name[50];
  long period=0;
  long i,j,k;
  struct reserve_list *tmp;

  struct reserve_list *reservelistptr=NULL;

  jsmn_init(&parser);

  FILE *fid = fopen(json_filename,"r");
  if (fid == NULL){
    printf("could not open file\n");
    return NULL;
  }

  fseek(fid,0L,SEEK_END);
  size = ftell(fid);
  fseek(fid,0L,SEEK_SET);

  filecontent = malloc((size+1)*sizeof(char));
  if (filecontent == NULL){
    printf("could not allocate memory\n");
    return NULL;
  }

  if ((read=fread(filecontent, sizeof(char),size,fid))<0){
    printf("error reading file\n");
    return NULL;
  }

  if (read != size){
    printf("could not read all file, expecting(%ld) read(%ld)\n",size,read);
    return NULL;
  }

  // add end of string
  filecontent[size] = '\0';

  numtokens = jsmn_parse(&parser, filecontent, strlen(filecontent), tokens, sizeof(tokens)/sizeof(tokens[0]));

  if (numtokens <0){
    printf("could not parse file as JSON format\n");
    return NULL;
  }

  // see what we got
  tmp = malloc(sizeof(struct reserve_list));
  if (tmp == NULL){
    return NULL;
  }


  for (i=0;i<numtokens;i++){
    if (tokens[i].type == JSMN_ARRAY){
      int valuescaptured=0;
      // start with j=1 to skip object
      // get directly into the 'fields'
      // size*(60 + 1 = 61) because we have 30 fields 30 values and one object
      printf("array size: %d\n",tokens[i].size);
      *count = tokens[i].size;
      for (j=1;j<tokens[i].size*61;j++){
	
	if (jsoneq(filecontent,&tokens[i+j],"name") == 0){
	  j++; // skip into value
	  strncpy(tmp->taskparams.cpuattr.name,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  tmp->taskparams.cpuattr.name[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("name(%s)\n",tmp->taskparams.cpuattr.name);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"myport") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("myport(%s)\n",token);
	  tmp->taskparams.myport = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"destination_address") == 0){
	  j++;
	  tmp->taskparams.destination_address = 
	    strndup(filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  printf("dest-address(%s)\n",tmp->taskparams.destination_address);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"destination_port") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("dest-port(%s)\n",token);
	  tmp->taskparams.destination_port = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"fd_index") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("fd_index(%s)\n",token);
	  tmp->taskparams.fd_index = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"period.tv_sec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("period_sec(%s)\n",token);
	  tmp->taskparams.cpuattr.period.tv_sec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"period.tv_nsec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("period_ns(%s)\n",token);
	  tmp->taskparams.cpuattr.period.tv_nsec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"priority") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("priority(%s)\n",token);
	  tmp->taskparams.cpuattr.priority = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"execution_time.tv_sec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("exec.sec(%s)\n",token);
	  tmp->taskparams.cpuattr.execution_time.tv_sec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"execution_time.tv_nsec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("exec.nsec(%s)\n",token);
	  tmp->taskparams.cpuattr.execution_time.tv_nsec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"overload_execution_time.tv_sec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("oexec.sec(%s)\n",token);
	  tmp->taskparams.cpuattr.overload_execution_time.tv_sec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"overload_execution_time.tv_nsec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("oexec.nsec(%s)\n",token);
	  tmp->taskparams.cpuattr.overload_execution_time.tv_nsec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"zs_instant.tv_sec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("zs.sec(%s)\n",token);
	  tmp->taskparams.cpuattr.zs_instant.tv_sec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"zs_instant.tv_nsec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("zs.nsec(%s)\n",token);
	  tmp->taskparams.cpuattr.zs_instant.tv_nsec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"response_time_instant.tv_sec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("r.sec(%s)\n",token);
	  tmp->taskparams.cpuattr.response_time_instant.tv_sec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"response_time_instant.tv_nsec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("r.nsec(%s)\n",token);
	  tmp->taskparams.cpuattr.response_time_instant.tv_nsec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"reserve_type") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("r.type(%s)\n",token);
	  tmp->taskparams.cpuattr.reserve_type = strtol(token,NULL,2);//atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"enforcement_mask") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("emask(%s)\n",token);
	  tmp->taskparams.cpuattr.enforcement_mask = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"e2e_execution_time.tv_sec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("e2eesec(%s)\n",token);
	  tmp->taskparams.cpuattr.e2e_execution_time.tv_sec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"e2e_execution_time.tv_nsec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("e2eensec(%s)\n",token);
	  tmp->taskparams.cpuattr.e2e_execution_time.tv_nsec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"e2e_overload_execution_time.tv_sec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("e2eoesec(%s)\n",token);
	  tmp->taskparams.cpuattr.e2e_overload_execution_time.tv_sec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"e2e_overload_execution_time.tv_nsec") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("e2eoensec(%s)\n",token);
	  tmp->taskparams.cpuattr.e2e_overload_execution_time.tv_nsec = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"criticality") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("criticality(%s)\n",token);
	  tmp->taskparams.cpuattr.criticality = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"normal_marginal_utility") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("norm-marginal-util(%s)\n",token);
	  tmp->taskparams.cpuattr.normal_marginal_utility = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"overloaded_marginal_utility") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("overloaded_marginal_utility(%s)\n",token);
	  tmp->taskparams.cpuattr.overloaded_marginal_utility = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"critical_util_degraded_mode") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("critical_util_degraded_mode(%s)\n",token);
	  tmp->taskparams.cpuattr.critical_util_degraded_mode = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"num_degraded_modes") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("num_degraded_modes(%s)\n",token);
	  tmp->taskparams.cpuattr.num_degraded_modes = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"bound_to_cpu") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("bound_to_cpu(%s)\n",token);
	  tmp->taskparams.cpuattr.bound_to_cpu = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"usec_delay") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("usec_delay(%s)\n",token);
	  tmp->taskparams.usec_delay = atol(token);
	  valuescaptured++;
	} else if(jsoneq(filecontent,&tokens[i+j],"busytime_ms") == 0){
	  j++;
	  strncpy(token,
		  filecontent+tokens[i+j].start,
		  tokens[i+j].end-tokens[i+j].start);
	  token[tokens[i+j].end-tokens[i+j].start] = '\0';
	  printf("busytime_ms(%s)\n",token);
	  tmp->taskparams.busytime_ms = atol(token);
	  valuescaptured++;
	} else {
	  /* printf("token[%ld].type(%s) token(%.*s)\n",i+j, */
	  /* 	 gettypename(&tokens[i+j]), */
	  /* 	 tokens[i+j].end-tokens[i+j].start, */
	  /* 	 filecontent+tokens[i+j].start); */
	}
	if (valuescaptured == 30){
	  valuescaptured=0;
	  add_reserve(&reservelistptr,tmp);
	  tmp = malloc(sizeof(struct reserve_list));
	  if (tmp == NULL){
	    return NULL;
	  }
	}
      }
    }
  }

  free(filecontent);
  return reservelistptr;
  /* printf("read [%s]\n",filecontent); */
  /* return NULL; */
}


/* int main(int argc, char *argv[]){ */
/*   struct reserve_list *tmp; */
/*   if(argc != 2){ */
/*     printf("usage %s <filename>\n",argv[0]); */
/*     return -1; */
/*   } */

/*   tmp = get_reserve_list(argv[1]); */
/*   while (tmp != NULL){ */
/*     printf("rsv[name(%s) period(%ld)] type(0x%x)\n",tmp->taskparams.cpuattr.name,tmp->taskparams.cpuattr.period.tv_sec,tmp->taskparams.cpuattr.reserve_type); */
/*     tmp = tmp->next; */
/*   } */
/* } */
