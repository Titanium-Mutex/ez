/****************************************************************/
/* ezkill.c --- EZ killer					*/
/* author:	T.Matsui
/* date:	1985-MAY-17
/* language:	UNIX system-V C
/* rewritten for sun3 unix 4.2bsd
/* 
/****************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
/* #include <bsd/sgtty.h> 	/* for i/o control via systemcall of UNIX */
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/****************************************************************/

void main(argc,argv)
long argc;
char *argv[];
{ extern char *getenv();
  long i,first,stat,pid,qid1,qid2;
  long /* key_t */ key1,key2;
   struct msqid_ds msgqdesc;
  char *msgid,ddir[128],prognm[16];
  struct msg256 {long mtype; char mtext[256];} msgp1,msgp2;
  struct msqid_ds msqbuf;
 
  msgid=getenv("LOGINPID");
  printf("LOGINPID=%s\n", msgid);
  if (msgid==0) {
    fprintf(stderr,"ez: LOGINPID is not setenv'ed\n"); exit(2);}
  /* sscanf(msgid,"%ld",&key1); */ key1=atoi(msgid) &0xffff;
  printf("LOGINPID=%x\n", key1);
  key2=key1 | 0x600000;
  key1=key1 | 0x500000;
  printf("key1=%x key2=%x\n", key1, key2);
  qid1=msgget(key1, 0);	/* msg queu exists? */
  qid2=msgget(key2, 0);
  printf("key1=%x key2=%x qid1=%x qid2=%x\n", key1, key2, qid1, qid2);
  if (qid1<0) {
    printf("ezkill: no message queu found.\n");
    exit(0);}
  else {
    msgctl(qid1,IPC_STAT,&msgqdesc);
    msgctl(qid1,IPC_RMID, 0);
    msgctl(qid2,IPC_RMID, 0);
    pid=msgqdesc.msg_lrpid;
    if (pid==0) { printf("cannot find ez proc.\n"); exit(0);}
    kill(pid,SIGTERM);
    }
  }

