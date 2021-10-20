/****************************************************************/
/* ezm.c --- main and initialization for ez
/****************************************************************/

#include <stdio.h> 
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <string.h>
#include <sys/param.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <term.h>
/*
#include <termio.h>
#include <termios.h>
*/
#include <errno.h>
/* #include <bsd/sgtty.h> */

/*#include "ezudef.h"*/

#include "ez.h"

/* extern int errno; */

char dlstr[32], alstr[32];
static pid_t loginpid;
static int sleeping;
static int ttypgrp;
static FILE *logfile;
struct termios tcmode, tcsave;
unsigned char tcap[1024];

extern void initfunctab(), setfunctab();

/****************************************************************/

void sttyraw()
{   /*
    ttymode.sg_flags |= RAW;
    ttymode.sg_flags &= ~(CRMOD | ECHO | LCASE );
    stat=ioctl(1,TIOCSETP,&ttymode);
    stat=ioctl(0,TIOCSETP,&ttymode); */
  tcmode.c_iflag &= ~(INLCR | ICRNL | IUCLC | IXON | IXOFF);
  tcmode.c_cflag &= ~(ECHO);
  tcmode.c_oflag &= ~(OPOST);
  tcmode.c_lflag &= ~(ISIG | ICANON | ECHO);
  tcsetattr(0,0,&tcmode);}

sttycooked()
{ /*ioctl(0,TIOCSETP,&ttysave);*/
  tcsetattr(0,0,&tcsave);}

ezsighandler(s,x,y)
{ pid_t pid;

  pid=getpid();
  sleeping=0;

/*  printf("ezsignal pid=%d sig=%d x=0x%x\n", pid, s, x); */
  switch(s) {
    case SIGCONT:  printf("continue ezu...\n");  return(0);    
    case SIGHUP:  printf("ezu: SIGHUP, saving...\n");
		   cleanup_all();
		   printf("now exiting\n");
		   exit(s);
    case SIGSEGV:  printf("ezu: pid=%d sigsegv, exiting...\n", pid); exit(s);
    case SIGBUS:   printf("ezu: sigbus, exiting...\n"); exit(s);
    case SIGTTOU: break;
    case SIGQUIT: printf("ezu: SIGQUIT, exitting."); exit(s); 
    case SIGALRM: if (kill(loginpid,0)) {
		    printf("ezu: login process is lost, exiting\n");
		    msgctl(qid1,IPC_RMID,0);
		    msgctl(qid2,IPC_RMID,0);
		    exit(0); }
		  /* printf("ez: alrm continueing\n"); */
		  alarm(3600*3);
		  sleeping=1;	/*continue sleeping*/
		  break;
    default:
	printf("ezu pid=%d signal=%d trapped\n",getpid(),s);
	break;}
  /* reinstall signal handler*/
  signal(s,ezsighandler);
  }


/****************************************************************/

main(argc,argv)
long argc;
char *argv[];
{
  extern char *getenv();
  extern FILE *fopen();

  long i,refsave,first,key1,key2,stat;
  pid_t pid, pgrp;
  int one;
  char ddir[256];
  char *ezkey,*loginpidstr, **xstr;
  struct msqid_ds msqbuf;

  if (getcwd(ddirname,256)==NULL) exit(83);
  loginpidstr=getenv("LOGINPID");
  if (loginpidstr==NULL) {
    printf("LOGINPID is not setenv'ed\n");
    printf("insert line 'setenv LOGINPID $$' in your .login file and retry\n");
    exit(0);}
  sscanf(loginpidstr,"%d",&loginpid);
  key1=loginpid & 0xffff;
  key2=key1 | 0x600000;
  key1=key1 | 0x500000;	/*create unique key*/
  qid1=msgget(key1,0);	/* msg queu exists? */
  qid2=msgget(key2,0);
  printf("msgget key=0x%x 0x%x qid=%d %d err=%d\n",
    key1,key2,qid1,qid2,errno);  

  for (i=1; i<32; i++) signal(i,ezsighandler);
  printf("msgqueue assigned %x %x\n", qid1, qid2);

  if (qid1<0) {  /*No, then this is the first time of EZ invocation*/
    first=1;
    qid1=msgget(key1, IPC_CREAT | 00666);  /*command*/
    qid2=msgget(key2, IPC_CREAT | 00666);  /*completion report*/
    printf("msgq created %x %x errno=%d %s\n", 
	qid1, qid2, errno, strerror(errno));
    pid=fork(); 
    printf("fork pid=%d\n", pid);
    if (pid>0)  { printf("Mother\n");  goto mother; }
    if (pid==0)  goto child;
    else {printf("error in fork: pid=%d\n",pid); exit(5);}}
  else first=0;

mother:
  printf("ezmother\n");
  signal(SIGINT,SIG_IGN);
  signal(SIGQUIT,SIG_DFL); 
  printf("ezmother pid=%d \n", getpid());
  getcwd(ddir,256);
  msgp1.mtype=1;
  msgp1.mtext[0]=msgp2.mtext[0]=0;
  strcpy(msgp1.mtext,"\033/");
  strcat(msgp1.mtext,ddir);	/*set ddir*/
  strcat(msgp1.mtext,"\015");
  if (argc>1) {
    strcat(msgp1.mtext,"\033F");
    strcat(msgp1.mtext,argv[1]);	/*set file name*/
    strcat(msgp1.mtext,"\015");}
  else if (first) {
    strcat(msgp1.mtext,"\033Ffoo.baz\015");
    }
  printf("mother msgsize=%d\n",strlen(msgp1.mtext));
  sleep(1); 
  stat=msgsnd(qid1,(struct msgbuf *)&msgp1,strlen(msgp1.mtext),0);
  msgp1.mtype=2;
  stat=msgsnd(qid1,(struct msgbuf *)&msgp1,0,0);	/*eof message*/
  /*printf("two messages sent to child ezu\n"); */
  /*wait for the completion of editing*/
  msgp2.mtype=0;
  while (msgp2.mtype!=99)  {/*wait for the completion of ez process*/
    stat=msgrcv(qid2,&msgp2,256,0,0); msgp2.mtext[stat]=0;
    /*printf("%s",msgp2.mtext);*/
    }
  exit(0);

child:
  /* logfile=fopen("ez.log","w");  */
  /* setbuf(logfile,NULL); */

  signal(SIGTERM,SIG_DFL);
  signal(SIGQUIT,SIG_IGN);
  signal(SIGALRM,ezsighandler);
  pgrp=pid=getpid();

  printf("ez child pid=%d going to initfunctab\n", pid);

  initfunctab();  /*define default functions to each key*/
  ezkey=getenv("EZKEY");
  printf("ezchild: initfunctab, ezkey ok\n");
  if (ezkey!=NULL) setfunctab(ezkey);
/*  fprintf(logfile,"ezkey=%s\n",ezkey); */

  setbuf(stdin,NULL);
  inmacro=FALSE; defmacro=FALSE;
  printf("ezchild: clearing kill rings\n");
  for (i=0; i<=MAXKILL; i++)  {killring[i].pos=0; killring[i].dir=0;}
  killavail=MAXKILLBUF; killsp= -1; searchstr[0]=0; macrostr[0]=0;
  maxbufno=0; self= -1;
  firstline=0;
  printf("ezchild: allocating kill buffer\n");
  killb = (unsigned char *)malloc(MAXKILLBUF); /* allocate kill buffer */
/* termcap */
  printf("ezchild: termcap and tgetent\n");
  stat=tgetent(tcap,getenv("TERM"));
  printf("tgetent =%d ok\n", stat);
  xstr=alstr;  stat=tgetstr("al",&xstr);
  printf("tgetstr=%d alstr=%s\n", stat, alstr);
  xstr=dlstr;  tgetstr("dl",&xstr);
  printf("ezchild two tgetstr ok\n");
  pagewidth=tgetnum("co")-1; pagelength=tgetnum("li")-1;
  lastline=pagelength-4; 
  statline=lastline+1; stringline=lastline+2; cmndline=lastline+3;
  pagelength=lastline-firstline;
  halfpage=pagelength/2+1; 
  printf("ezchild: init finished\n");
/*************************  LOOP of EDIT SESSIONS  ***************************/
  while (1) {
    /*synchronize with caller*/
    alarm(3000); sleeping=1;
    while (sleeping) {
      msgsize=msgrcv(qid1,&msgp1,255,0,0);
      if (msgsize<0 && !sleeping) {
        printf("\033[30;7mezu=%d terminated msgsize=%d sleeping=%d\033[m\n",
		getpid(),msgsize,sleeping);
	exit(0);}
      else if (msgsize>=0) sleeping=0; }
    alarm(0);

    msgp1.mtext[msgsize]=0;
    /* printf("child ez wake up. size=%d msg='%s'\n", msgsize, &msgp1.mtext[0]);
    sleep(5); */
    
    /*start of each editting session: include resuming procedure*/
    running=1;

    ioctl(0,TIOCGPGRP, &ttypgrp);	/*old tty process group*/
    /*fprintf(logfile,"pgrp=%d oldttypgrp=%d\n",pgrp,ttypgrp);  */
    /* stat=ioctl(0,TIOCGETP, &ttymode);  	/*get sgtty's sgflags*/
    /* fprintf(logfile,"TIOCGETP=%d \n",stat); */
/*    ttysave=ttymode; */

    tcgetattr(0, &tcmode);
    tcsave=tcmode;
    
    /*printf("ioctl OK.\n");*/
    
    setpgid(pid,ttypgrp);
    stat=ioctl(0,TIOCSPGRP, &pid);	/*occupy tty*/
    /*fprintf(logfile,"TIOCSPGRP pgrp=%d stat=%d err=%d\n",pid,stat,errno);
    */
    stat=setpgid(pid,pid);
    one=1;

    /*fprintf(logfile,"setpgrp=%d err=%d\n",stat,errno); */
    printf("\n\033[7;34mEZ v2.83 for Linux\033[0m\n");
    init_term();
    sttyraw();	/* set console into RAW mode */
    printf("\n\033[7;34mttyset\033[0m\n");

    if (msgp1.mtype>1) msgix= -1; else msgix=0;
    /*printf("msgix=%d\n", msgix); */
    msgctl(qid1,IPC_STAT,&msqbuf);

    setvbuf(stdin,NULL,_IONBF,1);

    while (running && (msgix>=0)) {
      toplevel=FALSE; refmode=0;
      getcmnd(); /* printf("cmnd=%x\n",ch); */
      count=0;
      branch(ch);}

    /*printf("end of message processing; msgix=%d\015\n",msgix); */
    fflush(stdout);
    refmode=128; refsave=0;
/*    getcmnd(); */
/************** main loop of editting ****************/
    while (running) {
      toplevel=TRUE; 

      ezrefresh();
      gotoxy(x,y+firstline);
      refmode=0;
      getcmnd();
      count=0;
      branch(ch);
      }
/**************  end of edit mainloop  ******************/

    init_term();
    gotoxy(0,cmndline-1);
    clreos();
    FFS;
    sttycooked();
    ioctl(0,TIOCSPGRP, &ttypgrp);
    msgp2.mtype=99;
    if ((msgsnd(qid2,&msgp2,0,0)) < 0) {
      printf("parent process is lost.  EXIT(11)\n"); exit(11);}
  }}


