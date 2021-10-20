/****************************************************************/
/* ez.c --- EZ starter						*/
/*	originated from 'ezmother.c', writen by T.Matsui (ETL)	*/
/*--------------------------------------------------------------*/	
/* function:    resume ez process as quickly as possible
/*		if ez process does not exist, execute ezu
/* author:	T.Matsui
/* date:	1985-MAY-17
/* language:	UNIX system-V C
/* 
/****************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
/* #include <bsd/sgtty.h> 	/* for i/o control via systemcall of UNIX */
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/****************************************************************/

void main(int argc,char *argv[])
{ extern char *getenv();
  long i,first,stat;
  pid_t pid, ppid;
  int oldqid1,oldqid2,qid1,qid2;
  key_t oldkey,key;
  char *msgid,ddir[128],prognm[16];
  struct msg256 {long mtype; char mtext[256];} msgp1,msgp2;
  struct msqid_ds msqbuf;
 
  msgid=getenv("LOGINPID");
  if (msgid==0) {
    fprintf(stderr,"ez: LOGINPID is not setenv'ed\n"); exit(2);}
  sscanf(msgid,"%d",&key);
  key = key & 0xffff;
  qid1=msgget(key | 0x500000, /*IPC_CREAT |*/ 00666 /*IPC_ALLOC*/);	/* msg queu exists? */
  qid2=msgget(key | 0x600000, /*IPC_CREAT |*/ 00666 /*IPC_ALLOC*/);
 
  printf("ez qid1=%d qid2=%d\n",qid1,qid2);  fflush(stdout);
  if (argc==2 && (*argv[1]=='-')) {	/*find old ez*/
    sscanf(argv[1],"-%x", &oldkey);
    oldqid1=msgget(oldkey | 0x500000,0 /*IPC_ALLOC*/);	/* msg queu exists? */
    oldqid2=msgget(oldkey | 0x600000,0 /*IPC_ALLOC*/);
    fprintf(stderr,"old qid=%d, %d\n",oldqid1,oldqid2);
    if (oldqid1<0) {
      fprintf(stderr,"no such message queu %d\n",oldkey);
      exit(1);}
    qid1=msgget(key | 0x500000,IPC_CREAT | 00666);
    qid2=msgget(key | 0x600000,IPC_CREAT | 00666);
    msgp1.mtype=1;
    strcpy(msgp1.mtext,"\033\024");	/*esc^T*/
    strcat(msgp1.mtext,ttyname(0));
    strcat(msgp1.mtext,"\015\033\021"); /*CResc^Q*/
    strcat(msgp1.mtext,msgid);
    strcat(msgp1.mtext,"\015");
    }
  else if (qid1<0) {
    strcpy(prognm,argv[0]);
    strcat(prognm,"u");
    argv[0]=prognm;
    execvp(prognm,argv);
    printf("error: cannot exec %s\n",prognm);}
  else {
    oldqid1=qid1; oldqid2=qid2;
    msgp1.mtype=1;
    msgp1.mtext[0]=0;
    strcat(msgp1.mtext,"\033\033");
    if (argc>1) {
      strcat(msgp1.mtext,"\033F"); strcat(msgp1.mtext,argv[1]);
      strcat(msgp1.mtext,"\015");} }
  stat=msgsnd(oldqid1,&msgp1,strlen(msgp1.mtext),0);
  msgp1.mtype=2;
  stat=msgsnd(oldqid1,&msgp1,0,0);	/*eof message*/
/*    printf("msgsnd2 stat=%d\n",stat); */
  /*wait for the completion of editing*/
  msgp2.mtype=0;
  while (msgp2.mtype!=99) {
    stat=msgrcv(qid2,&msgp2,256,0,0);
    msgp2.mtext[stat]=0;
    printf("%s\015",msgp2.mtext);}
  exit(0);
  }

