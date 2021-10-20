/*------------------------------------------------------*/
/* EZ ... a display oriented text editor 		*/
/*------------------------------------------------------*/
/* Author: T.Matsui					*/
/*	   ETL, Automatic Control Division 		*/
/* Dec 1983 v1.20			 		*/
/* Jan 1984 v1.30  file operation	 		*/
/* Feb 1984 v1.34  new command system  			*/
/* 	    v1.41  fast refresh algorithm		*/
/* 	    v1.49  find matching (),  help		*/
/*	    v1.50  written in unix fashion		*/
/*	    v1.55  unkilling				*/
/* Apr 1984 v2.01  runs as a subprocess			*/
/*	    v2.13  multiple buffers			*/
/*	    v2.26  mailbox communication		*/
/* Sep 1984 v2.34  killring				*/
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* Implemented  on UNIX  by N.Yamamoto (KSL)		*/
/* Oct 1984 v1.04				10.31	*/
/* Nov 1984 v1.103 ezabort,sysexe,writefile	11.06	*/
/*	    	   lots of wrong comments are introduced
/*------------------------------------------------------*/
/* ez process  (by T.Matsui)
/*	v2.51  keeps editting environment in ez process	*/
/*	v2.58  message queu communication		*/
/****************************************************************/
/* 	v2.66	pc9801 $Hvje$rMQ$$$?F|K\8l2= 
/*		%3%a%s%H$N:o=|$HD{@5:;3K\;a$N$G$?$i$a$J1Q8l
/*		$K$h$k$G$?$i$a$J%3%a%s%H$r=$@5
/****************************************************************
/*  v2.70	implementation on Sun3 1987-May-15
/* 		taking advantage of job control feature,
/*		ez is kept alive at the back.
/****************************************************************/
/*  v2.80	1987-June
/* 		discard job control, use message queu for synchronization
/*		To use ez with euslisp and geomap, ez is required
/*		to be a detached subprocess. 
/**************************************************************/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

/*for message queu*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "ez.h"

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* global variables					*/
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* extern int errno; */
/* extern char *sys_errlist[];*/
#include <errno.h>
static int kanji=0, euc=0;

/**** screen size parameters */
int  pagelength=20,halfpage=11,pagewidth=79;
int  firstline=0,lastline=20,statline=21,stringline=22,cmndline=23;
int  filedisp=50;
int  rightmargin=80;

/**** boolean var to control global execution*/
int  running=0,found=0,toplevel=1;
int  inmacro=0,defmacro=0,changed=0;

/**** refresh mode: command to ezref (function refresh)*/
int  refmode=0;

/**** repeat and macro control */
int  count=0,ezindex=0,macindex=0;

/**** buffer management */
int  pagehead=0,pagetail=0,buftail=65535,linepos=0;
int  lnhead=0,lntail=0,chpos=0;
int  up=0,lp=0,oldup=0,oldlp=0,markup=0,marklp=0;

/**** kill ring and mark*/
int  killavail=0;
int  killsp= -1, killstack[MAXKLSP+2]={0};
int  marksp= -1;

/**** cursor position*/ 
int  x=0,y=0,oldx=0,oldy=0;
int  ch = ' '; 		/*ascii=0-127, kanji=128-255, eof= -1 */

/**** special keys interpreted in get char functions*/
int  k_repeat=18, k_abort=3, k_bfind=2, k_ffind=6;
int  k_quote=17;

void   (*func[512])()={0},(*dfunc[512])()={0};  /*pointers to functions*/

unsigned char searchstr[MAXSTR]={0};
int           macrostr[MAXSTR]={0};

ptr_pair   *markring= 0;
kill_pair  killring[MAXKILL+1]={0};

/* file/directory name strings*/
unsigned char filename[256]={0}, nextfile[256]={0}, ddirname[256]={0};

struct mb256 msgp1, msgp2;
int msgix=0,msgsize=0,qid1=0,qid2=0;
unsigned char *buf=0,*killb=0;

/*multi buffer management*/
int  self= -1 ,maxbufno=0;
ez_descripter ezbuff[MAXBUFNO]={0};

extern FILE *fopen();


/****************************************************************/
/* OS and language dependent primitives
/****************************************************************/

void copybuf(dest,src,cnt)	/*byte block transfer from src to dest, cnt bytes*/
register char *dest,*src;
register long cnt;
{ if (src>dest)    /*forward copy*/
    while(cnt-- >0) *dest++ = *src++; 
  else {  	   /*backward copy*/
    src+=cnt; dest+=cnt;    
    while(cnt-- >0) *--dest = *--src;}}

int uppercase(c)
int c;
{ if (c<128) return(to_upper(c));
  else return(c);}

void toupperstr(str)
unsigned char *str;
{ while (*str>0) 
    if (*str & 0x80) str++;
    else { *str=to_upper(*str); str++;}}

/****************************************************************/
/* terminal oriented I/O
/****************************************************************/

void putst(str)
unsigned char *str;
{ fputs(str,stdout);}

static int inkanji=0;

void kanjioff()
{ /* if (inkanji) putst("\033(J"); */
  inkanji=0;} 

void eatch(c)	/*erase one char in the command-line*/
char c;
{ if (toplevel) {
    kanjioff();
    if (c & 0x100) {eatch(c & 0xff); eatch(ESC);}
    else if (c>=' ') putst("\b \b");
    else putst("\b\b  \b\b");} }

int dispchar(c)	/*display one char, add ^ to control code*/
register unsigned char c;
{ if (c & 0x100) return(dispchar(ESC)+dispchar(c & 0xff));
  if (c & 0x80) {
/*    if (!inkanji) { putst("\033$B"); inkanji=1;} 
    putchar(c & 0x7f); */
    if (c == 0x8e) puts("hankaku");
    inkanji=1;
    putchar(c); 
    return(2);}
  if (inkanji) kanjioff();
  if (c==DEL) { putst("^?"); return(2);}
  else if (c>=' ') {putchar(c); return(1);}
  else { putchar('^'); putchar(c+'@'); return(2);}}

int dispstr(str)	/*put string, return no. of columns requiered for displaying*/
register unsigned char *str;
{ unsigned int l=0;
  unsigned char c;
  while ((c= *str++)>0) 
    if (c & 0x80) { dispchar(c); dispchar(*str++); l+=2;}
    else l += dispchar(c);
  kanjioff();
  return(l);}
  
void prompt(str)	/*put the string as prompt in command-line*/
char *str;
{ gotoxy(0,cmndline);
  clreol(0,pagewidth);
  chartype(1);	/*select bold rendition*/
  putst(str);
  chartype(0);}

void status(col,c)	/*display status code c at col*/
int col;
char c;
{ if (toplevel) {
    gotoxy(col,statline);
    if (c != ' ') chartype(7);
    putchar(c);
    chartype(0);
    gotoxy(x,y+firstline);}}

void putcount(pos,w,n)	/*display intval n in w digit at pos'th column*/
int pos,w,n;
{ int nc[10],i,j;
  gotoxy(pos,statline);
  for (i=0; i<w;) {
    nc[i++]=n%10+'0';
    if ((n/=10)==0) break;}
  for (j=w-1; j>=0; j--) if (j>=i) putchar(' '); else putchar(nc[j]);}

void putsearchstr()	/*display searchstr*/
{ if (toplevel) {
    gotoxy(0,stringline); chartype(0); clreol(0,pagewidth);
    if ((strlen(searchstr))>0) {
      putst("search=");
      chartype(7); 	/*reversed*/
      dispstr(searchstr);}
    chartype(0);}}

void displaystatus()	/*display whole status line*/
{ char *dname;
  if (toplevel) {
    gotoxy(0,statline); chartype(7); clreol(0,pagewidth);
    if (changed) {gotoxy(chngdisp,statline); putchar('C');}
    if (defmacro) {gotoxy(defdisp,statline); putchar('D');}
    gotoxy(kanjidisp,statline);	/* Euc or Jis */
    if (euc)  putchar('E'); else putchar('J');
    if ((buftail-lp+up)>0) {
      putcount(posdisp,3,(((100*up)/(buftail-lp+up)))); putchar('%');}
    else {gotoxy(posdisp,statline); putst(" new");}
    gotoxy(ddirdisp,statline);
    if (strlen(ddirname)>50) dname= &ddirname[strlen(ddirname)-50];
    else dname=ddirname;
    dispstr(dname); 
    printf(" #%d ",self); dispstr(filename);
    putsearchstr(); gotoxy(0,cmndline); clreol(0,pagewidth);}}

/****************************************************************/
/* get char or string
/****************************************************************/

int getchr()	/*get a next char from tty or macrostr*/
{ if (inmacro) {  /*get char from macro definition string*/
      if (macindex>ezindex) ch=macrostr[ezindex++]; else ch= EOF;}
    else if (msgix>=0) {
      ch=msgp1.mtext[msgix++]; 
      if (msgix>=msgsize) {
	msgsize=msgrcv(qid1,&msgp1,255,0,0); /*read next message*/
	/* printf("msgsize=%d mtype=%d\n",msgsize,msgp1.mtype); */
	if (msgp1.mtype>1) msgix= -1; /*EOF*/
	else if (msgsize<0) {
	  printf("error of msgrcv=%d\n",errno); msgix= -2;}
	else msgix=0;}} /*reset message index*/
    else {
      ch=getchar();  /*all 8bits are valid to represent kanji */
      if (ch==EOF) { ch=3; return(3);}
      /* ch= ch & 0x7f; */
      /* printf("getchr=%d\n", ch);*/
      if (defmacro)
	if (macindex<MAXSTR) macrostr[macindex++]=ch;
	else {bell(); return(0);}; }
    return(ch);}

int getcmnd()	/*get a next command character*/
/* meta character is indicated by bit9 turned on*/
{ if (getchr()==ESC) {
    status(metadisp,'M');
    getchr();
    if (ch<=DEL) ch=(to_upper(ch) | 256);
    status(metadisp,' ');}
  return(ch);}

void echoch(ch)
char ch;
{ if (toplevel) dispchar(ch);}

int getstp(char *str, char *pmsg)	/*put prompt and get a string with echo*/
{ int l=0, inkanji=0;
  if (toplevel) prompt(pmsg);
  while ((ch=getchr()) != CR) {
    if (ch==DEL || ch==8) {
      if (l>0) {
	if (str[--l] & 0x80)  eatch(str[l--]);
	eatch(str[l]);}}
    else if (ch==ESC) {
      getchr();
      if (ch=='$') { getchr(); /*B*/ inkanji=1;}
      else if (ch=='(') { getchr(); /*J*/ inkanji=0;}
      else if (ch<=DEL) { str[l]=to_upper(ch) | 256; echoch(str[l++]);} }
    else if (ch==k_quote) {
      getchr();
      if (ch==CR) ch=LF;	/*exchange CR and LF*/
      else if (ch==LF) ch=CR;
      echoch(ch); str[l++]=ch;} 
    else if (ch>=' ') {
      if (inkanji) ch |= 0x80;
      str[l++]=ch;
      echoch(ch); }
    else if (ch==0x15) {
      while (l>0) eatch(str[--l]); }
    else if ((ch==k_bfind || ch==k_ffind) && l==0) break;
    else if (ch==k_abort) {
      *str=0; putchar(CR); clreol(0,pagewidth); return(ERR);}
    else bell();
    FFS }
  str[l]=0;
  if (toplevel) {
    kanjioff();
    chartype(0); putchar(CR); clreol(0,pagewidth); putchar(CR);}
  return(l); /*returns string length*/ }

/****************************************************************/
/* file handling
/****************************************************************/

int readfile(FILE *fp)
{ int c,c2,kanji=0;
  while (lp-up>255) {
    c=getc(fp);
    if (c==EOF) break;
    else if (c==ESC) {
      c2=getc(fp);
      if (c2=='$') {	/*start kanji*/
	getc(fp);	/*discard @ or B*/
	buf[up++]=getc(fp) | 0x80;
	kanji=1;}
      else if (c2=='(') {	/*terminate kanji*/
	getc(fp);		/*discard J*/
	kanji=0;}
      else { buf[up++]=c; buf[up++]=c2;} }
    else if (kanji) buf[up++]=c | 0x80;
    else buf[up++]= c /* & 0x7f */; }
  fclose(fp);
  return(up);}

void movtob();

int loadfile(char *fn)	/*allocate a buffer and load a file fn*/
  { int  f,s,ss;
    char answer[64];
    FILE *infile;
    extern void *malloc();
    struct stat file_stat;
    infile=fopen(fn,"r");
    if (infile>0) {
      f=stat(fn, &file_stat);  /*get the file status*/
      if (f== -1) { 
	printf("fstat error\015\012"); fclose(infile); return(ERR); }
      s=file_stat.st_size;  /*size of load-file*/
      ss=min(max(s*2, BUFFUNIT*2),s+256*1024); }
    else if (toplevel) {
      if (getstp(answer,"no such file, create(y/n)? ")<=0) return(ERR);
      else if (to_upper(*answer)=='Y') { ss=BUFFUNIT;}
      else return(ERR);}
    else ss=BUFFUNIT;
    buf=malloc(ss);
    markup=up=0; marklp=lp=buftail=ss-2;
    strcpy(filename,fn);
    changed=0;

    if (infile>0) {
      chartype(1); gotoxy(0,cmndline); clreol(0,pagewidth);
      printf("reading %s ...(%d bytes)",fn, s) ; fflush(stdout);
      up=readfile(infile);
      chartype(0);
      movtob(); return(OK);}
    else {printf("buff[%d] allocated\n",buftail); newpage(1); return(0);}}
  
void writefile(FILE *fp, int from, int count)
{ int ch,inkanji=0;
  
  if (euc) {
    while (count-- >0) {
      ch=buf[from++];    putc(ch,fp);}  }
  else {
    while (count-- >0) {
      ch=buf[from++];
      if (inkanji) {
        if (ch & 0x80) {	/*if still kanji, write as it is*/
          putc(ch & 0x7f,fp); }
        else {		/*terminate kanji sequence*/
          fputs("\033(J",fp); putc(ch,fp); inkanji=0; } }
      else if (ch & 0x80) { /*start kanji?*/
        fputs("\033$B",fp); putc(ch & 0x7f,fp); inkanji=1; }
      else putc(ch,fp);} 
    if (inkanji) fputs("\033(J",fp); } }

int cleanup(int emergency)
{ char str[64];
  FILE *outfile;
  int  count;
  if ((outfile=fopen(filename,"w"))>0) {
    writefile(outfile,0,up);
    writefile(outfile,lp+1,buftail-lp);
    fclose(outfile);
    changed=0;
    return(OK);}
  else if (emergency) {
    char b[256];
    sprintf(b, "Can't write due to \"%s\", <CR> to resume: ", strerror(errno));
    getstp(str,b);
    return(ERR);}}

/* dump all the editing files as the names prefixed by "ez."*/
/* Invoked when HUP signal is sent*/
void cleanup_all()
{ register int i,j;
  char fname[256];
  for (i=0; i<maxbufno; i++) {
    if (ezbuff[i].changed) {
      switchbuffer(self,i);
      for (j=0; j<strlen(ezbuff[i].filename); j++) {
        fname[j]=ezbuff[i].filename[j];
        if (fname[j]=='/') fname[j]='.';}
      fname[j]=0;
      sprintf(filename,"ez%d.%s",i,fname);
      self=i;
      cleanup(1);} } }

/*[mS]*/
int savefile()
{ char savemsg[128];
  int stat;
  if (strlen(filename)<=0) {
    if (getstp(nextfile,"file name: ")<=0) return(ERR);
    /*toupperstr(nextfile);*/
    strcpy(ezbuff[self].filename,nextfile); strcpy(filename,nextfile);}
  strcpy(savemsg,"saving in "); strcat(savemsg,filename);
  prompt(savemsg); chartype(1);
  printf("   %d bytes",up+buftail-lp); chartype(0); FFS ;
  stat=cleanup(0); displaystatus();
  gotoxy(x,firstline+y); return(stat);}

/*[mR]*/
void renamefile()
  { if (getstp(nextfile,"rename to: ")<=0) return;
    /*toupperstr(nextfile);*/
    strcpy(filename,nextfile); strcpy(ezbuff[self].filename,nextfile);
    displaystatus();}

/*[mQ]g*/
void savexit()	/*save current buffer and return from ez*/
{ if (savefile()>0) running=0;
  else bell();}

/*[mI]*/
void mergefile()	/*read aux file*/
{ FILE *aifd;
  unsigned char aifile[64];
  if (getstp(aifile,"read file: ")<=0) return;
  if ((aifd=fopen(aifile,"r")) == NULL) {bell(); return;}
  else readfile(aifd);
  killsp= -1; newpage(0); changed=1; refmode=128;}

void rereadfile()
{ FILE *aifd;
  if ((aifd=fopen(filename,"r")) == NULL) {bell(); return;}
  else { 
    lp=ezbuff[self].buftail;
    up=0;
    up=readfile(aifd);
    killsp= -1; newpage(0); changed=1; refmode=128;}
  }

/*[mO]*/
void putfile()	/*write region between marker and cursor to aux file*/
{ FILE *aofd;
  unsigned char aofile[64];
  if (getstp(aofile,"write file: ")<=0) return;
  if ((aofd=fopen(aofile,"w"))==NULL) {bell(); return;}
  else {
    popmark();
    if (markup<up) writefile(aofd,markup,up-markup);
    else writefile(aofd,lp+1,marklp-lp);
    /* refmode=0x17; newpage(0); */
    fclose(aofd);}}

/*--------------------------------------*/
/* change directry
/*--------------------------------------*/

/*[m$]*/
void nextdir()	/*change the default directry in status line*/
  { char ddirstr[256];
    if (getstp(ddirstr,"working directory: ")<=0) return;
    /*toupperstr(ddirstr);*/
    if (chdir(ddirstr)<0) bell();
    else {
      getcwd(ddirname, 255);
      displaystatus();}}


/****************************************************************/
/* multibuffer management
/****************************************************************/

switchbuffer(from,to)	/*switch the buffer from #f to #t*/
int from,to;
  { if (from<0) from=0;  /*save current buffer status*/
    ezbuff[from].changed=changed; 
    ezbuff[from].lp=lp;	 	ezbuff[from].up=up; 
    ezbuff[from].markup=markup;	ezbuff[from].marklp=marklp;
    /* set up new buffer variables */
    lp=ezbuff[to].lp;		  up=ezbuff[to].up;
    markup=ezbuff[to].markup;	  marklp=ezbuff[to].marklp;
    markring=ezbuff[to].markring;
    buf=ezbuff[to].buf; 		  buftail=ezbuff[to].buftail;
    changed=ezbuff[to].changed;}

findbuffer(str)	/*find the filename from buffer table and return buf#.*/
  char *str;
  { int i,no;
    while (*str<=' ') str++;  /*skip preceeding blanks*/
    if (*str=='#') {
      str++; no=0;
      while (isdigit(*str)) no= no*10+*(str++)-'0';
      if (no>=maxbufno) {bell(); return(ERR);}
      else return(no);}
    else {
      /*toupperstr(str);*/
      for (i=0; i<maxbufno; i++) if (strcmp(str,ezbuff[i].filename)==0) break;
      return(i);}}

nextbuffer(fn)	/*switch to the buffer named 'fn'*/
  char *fn;	/*return buf# or ERR if no such buff*/
  { int i,j;
    if ((i=findbuffer(fn))<0) return(0);
    if (i>=MAXBUFNO) {prompt("too many buffers"); bell(); return(ERR);}
    if (i>=maxbufno) {  /*create a new buffer*/
      strcpy(ezbuff[i].filename,fn);
      switchbuffer(self,i);
      if ( loadfile(fn)==ERR ) {
	if (self== -1) { return(ERR);} 
	else { /*back to old-buffer*/
	  switchbuffer(i,self); i=self; /* newpage(1)*/ ; return(i);}
      }
      for (j=0; j<=MAXMARK; j++) {markring[j].upp=0; markring[j].lpp=buftail;}
      ezbuff[i].buf=buf; ezbuff[i].buftail=buftail; maxbufno++;}
    else switchbuffer(self,i); /*switch from old buffer to new one*/
    strcpy(filename,ezbuff[i].filename);
    filedisp=pagewidth-strlen(filename)-3;
    newpage(1) ; self=i; return(i);}

/*[mF]*/
void newbuffer()
{ int i;
  char fname[64];
  if (getstp(fname,"file name: ")<=0) { bell(); return(0);}
  else { i=nextbuffer(fname); return(i);}}

/*[m^Z]*/
void killbuffer()	/*remove edit buffer and release memory for neighboring buf*/
{ char bufname[64];
  int  b,s,i,cnt;
  if (getstp(bufname,"delete buffer: ")<0) return(0);
  if (*bufname==0) b=self; else b=findbuffer(bufname);
  if ((b>=maxbufno)||(b<0)) {
    prompt("buffer does not exist"); bell(); return(ERR);}
  s= &(ezbuff[b].buf[ezbuff[b].buftail]) - 
     &(ezbuff[b-1].buf[ezbuff[b-1].buftail]);
  if (b==self) {
    ezbuff[b].lp=ezbuff[b].marklp=marklp=lp=buftail;
    ezbuff[b].up=ezbuff[b].markup=markup=up=0;
    for (i=0; i<=MAXMARK; i++) {markring[i].upp=0; markring[i].lpp=buftail;}
    ezbuff[b].filename[0]= *filename=0;
    ezbuff[b].changed=changed=0;
    newpage(1); return(OK);}
  else {
    free(ezbuff[b].buf);
    copybuf(&ezbuff[b],&ezbuff[b+1],
  	   (long)((maxbufno-(b+1)) * sizeof(ez_descripter)));
    if (self>b) {self--; displaystatus();} 
    maxbufno--; return(1);}}

/*[mB]*/
void listbuffer()	/*listout the buffer name from the buffer table*/
  { int i;
    /*put back buf pointers to reflect current buffer status*/
    ezbuff[self].lp=lp; ezbuff[self].up=up; ezbuff[self].changed=changed;
    gotoxy(0,firstline); clreol(0,pagewidth); 
/*    putst("     *****list of EZ buffers*****\015\012"); */
    clreol(0,pagewidth);
    putst("file-no.    used/free   change file-name\015\012");
    clreol(0,pagewidth);    
    printf("xxxxxxxx  %6d/%-6d         killbuffer\015\012",
	   MAXKILLBUF-killavail,killavail);
    for (i=0; i<maxbufno; i++) {
      clreol(0,pagewidth);    
      printf("  #%-3d    %6d/%-6d    %c    %s\015\012",
  	   i,ezbuff[i].buftail-ezbuff[i].lp+ezbuff[i].up,
  	   ezbuff[i].lp-ezbuff[i].up,
  	   (ezbuff[i].changed ? 'C' : ' '), ezbuff[i].filename);
      }
    clreol(0,pagewidth);}


/****************************************************************/
/* set new screen size parameters (pagewidth and pagelength)
/****************************************************************/

void setwidth()
{ if (count==0) pagewidth=79;
  else pagewidth=count-1;
  count=0;
  refmode=128;}

void setlength()
{ if (count==0) lastline=20; else lastline=count-4;
  statline=lastline+1;
  stringline=lastline+2;
  cmndline=lastline+3;
  pagelength=lastline-firstline;
  halfpage=pagelength/2 +1;
  count=0;
  refmode=128;}


/****************************************************************/
/* primitives for traversing in the gap buffer
/* search for the line delimitter (LF) and find current column
/****************************************************************/
findhead(p)	/*find the head of the line*/
int p;
{ if (p<0) return(0);
  else while ((p>0)&&(buf[p-1]!=LF)) p--;
  return(p);}

findtail(p)	/*find the tail of the line*/
int p;
{ if (p>=0) {
    while ((p<buftail) && (buf[p]!=LF)) p++;
    if (p>buftail) return(buftail); else return(p);}
  else return(buftail);}

nextcol(pos,c)	/*return the next col pos advanced by char c*/
int pos;	
int c;
{ if (c<256 && c>=' ') return(1+pos);
  else if (c==TAB) return((pos+8) & 0xfff8);
  else if ((c==LF)||(c==CR)) return(pos);
  else return(pos+2);}

void descend()	/*go ahead by one char, advance x pos*/
{ buf[up++] = buf[++lp];
  x=nextcol(x,buf[lp]);
  x=min(x,pagewidth);}

void ascend()	/*go back by one char*/
{ buf[lp--] = buf[--up];}

void moveup(u)	/*move up to position 'u'*/
int u;
{ if (u<up) {
    killsp= -1;
    copybuf(buf+lp+u-up+1,buf+u,(long)(up-u));
    lp=lp+u-up; up=u;}}

void movedown(l)	/*move down to position 'l'*/
int l;
{ if (l>lp) {
    killsp= -1;
    copybuf(buf+up,buf+lp+1,(long)(l-lp));
    up=up+l-lp; lp=l;}}

void newlength()	/*calculate current column position*/
{ int p;
  p=lnhead;
  x=0;
  while (p<up) x=nextcol(x,buf[p++]);
  x=min(x,pagewidth);}

/****************************************************************/
/* reset new page parameters
/****************************************************************/

void newpage(centering)	/*set every parameter for new page*/
int centering;
/*if centering=1, cursor is forced to be at the center of page*/
   {int p;
    y=0;
    if ((centering) || (up<pagehead) || (lp>pagetail)) {
      /*whole page should be reset*/
      pagehead=up; /*temporary page head*/
      lnhead=(pagehead=findhead(pagehead));
      while ((y<halfpage) && (pagehead>0)) {
        y++; pagehead=findhead(pagehead-1);}
      refmode=128;}
    else { /*only count and locate cursor*/
      for (p=pagehead; p<up; ++p) if (buf[p]==LF) ++y;
      lnhead=findhead(up);
      if (y>pagelength) newpage(1);}
    lntail=findtail(lp+1); linepos=0; pagetail=lntail;
    while ((linepos<pagelength-y)&&(pagetail<buftail)) {
      linepos++;
      pagetail=findtail(pagetail+1);}
    newlength(); linepos += y;  oldup=up; oldlp=lp; oldx=x; oldy=y;}

/****************************************************************/
/* move to neighbor column/line
/****************************************************************/
/*[^A]*/
void movbol()	/*move up to beginning-of-line*/
{if (lnhead!=up) {moveup(lnhead); x=0; killsp= -1;}}

/*[^L],[ff]*/
void moveol()	/*move down to end-of-line*/
{ if ((lntail-1>=lp) && (buf[lntail]==LF)) movedown(lntail-1);
  else movedown(lntail);
  newlength();}

/*[^H],[bs]*/
void movleft()
{ if (x>0) { /*in the middle of line?*/
    killsp= -1;
    ascend();
    if (buf[up]>0x80) ascend();	/* if kanji, one more byte*/
    newlength();}
  else if (up>0) { prevline(); moveol();}
  else bell();}

/*[^K]*/
int movright()
{ if ((lp<lntail-1)||((buf[lntail]!=LF) && (lp<lntail))) {
    killsp= -1;
    descend();
    if (buf[lp]>0x80) descend(); /*two bytes for kanji*/
    return(OK);}
  else if (lntail<buftail) {x=0; nextline(); return(OK);}
  else {bell(); return(ERR);}}

/*[^U]*/
void prevline()	/*move up to the previous line*/
{ int newx,p;
  if (lnhead>0) {
    moveup(lnhead);
    p=lnhead=findhead(up-1);
    lntail=lp; newx=0;
    while ((newx<x) && (p<up)) {
      newx=nextcol(newx,buf[p]);
      if (buf[p++]>0x80) newx=nextcol(newx,buf[p++]);}	/*advance two for kanji*/
    moveup(p);
    if ((lp==lntail) && (buf[p-1]==LF)) ascend();
    x=min(newx,pagewidth);
    if (--y<0) newpage(1);}
  else bell();}

/*[^J],[lf]*/
void nextline()
 {int i,oldx;
  if (lntail<buftail) {
    movedown(lntail);
    lntail=findtail(lp+1);
    lnhead=up; oldx=x; x=0;
    while ((x<oldx) && (lp<lntail)) {
      descend();
      if (buf[lp]>0x80) descend();}	/*one more move for kanji*/
    if ((lp==lntail) && (buf[lp]==LF)) ascend();
    if (++y>linepos) newpage(1);}
  else bell();}

/****************************************************************/
/* cursor movement word by word
/****************************************************************/
void nextword(mode)
int mode;
/*advance to next word converting to upper/lower/capital-initial*/
{ while ((!isalnum(buf[lp+1]))&&(lp<buftail)) 
    if (movright()==ERR) return(ERR);
  while ((isalnum(buf[lp+1]))&&(lp<buftail)) {
    ++lp;
    switch(mode) {
      case 0: buf[up++]=buf[lp]; break;
      case 1: buf[up++]=to_lower(buf[lp]); break;
      case 2: buf[up++]=to_upper(buf[lp]); break;
      case 3: buf[up++]=to_upper(buf[lp]); mode=1; break;
      }}
  newlength(); return(OK);}

void prevword(delete)	/*back to next word(delete=0)*/
  int delete;
  { if (delete) {refmode=0x1e; pushkill(up); pushkill(11);}
    if (up<=lnhead) movleft();
    while ((up>lnhead)&&(!isalnum(buf[up-1]))) if (delete) up--; else ascend();
    while ((up>lnhead)&&(isalnum(buf[up-1]))) if (delete) up--; else ascend();
    newlength();}  

/*[mK]*/    
void movnextword() {nextword(0);}	/*move to next word*/

/*[mH]*/
void movprevword() {prevword(0);}	/*move to previous word*/

/*[mC]*/
void capitalword() {refmode=0x1b; nextword(3);}

/*[mL]*/
void lowerword() {refmode=0x1b; nextword(1);}

/*[mU]*/
void upperword() {refmode=0x1b; nextword(2);}

/*[mD]*/
void killword() 
{ if (lp<buftail) {
    pushkill(lp); pushkill(7); refmode=0x16;
    while ((lp<buftail)&&(!isalnum(buf[lp+1]))) {
      lp++;
      if (buf[lp]==LF) refmode=0x37;}
    while ((lp<buftail)&&(isalnum(buf[lp+1]))) lp++;
    if (refmode==0x37) newpage(0); else newlength();}
  else bell();}

/*[m[del]]*/
void delword() {prevword(1);}	/*delete the previous word*/

/****************************************************************/
/* cursor movement across pages
/****************************************************************/
/*[m<]*/
void movtob()	/*move to the top of buffer*/
  { pushmark(); moveup(0); newpage(1);}

/*[m>]*/
void moveob()	/*move to the end of buffer*/
  { pushmark(); movedown(buftail); lntail=buftail; lnhead=findhead(up-1);
    newlength(); newpage(0);}

/*[m,]*/
void toppage()	/*move to the top of page*/
  { moveup(pagehead); newpage(0);}

/*[m.]*/
void bottompage()	/*move to the bottom of page*/
  { if (buf[pagetail]==LF) movedown(pagetail-1);
    else movedown(pagetail);
    newpage(0);}

/* locate the cursor in the line given absolutely*/
void abspos()
{ register long p=0;
  if (count<=0) return(-1);
  if (count>1) 
    while (p<up)
      if (buf[p++] == LF) if (--count<=1) break;
  if (count<=1) {moveup(p); newpage(0); return(0);}
  else {
    p=lp+1;
    while (p<buftail)
      if (buf[p++] == LF) if (--count<=1) break;
    movedown(p-1);}
  count=0;
  newpage(0);}

/*[^T],[mV]*/
void prevpage()	/*go back to previous page*/
  { moveup(pagehead); newpage((pagehead!=0));}

/*[^V]*/
void nextpage()	/*advance to next page*/
  { if ((pagetail==buftail)&&(lp!=pagetail)) movedown(pagetail-1);
    else movedown(pagetail);
    newpage((pagetail!=buftail));}

/*****************************************************************/
/* marking
/****************************************************************/

/*[^@]*/
void pushmark()	/*mark at the current position*/
  { int i;
    for (i=MAXMARK; i>0; i--) {
      markring[i].upp=markring[i-1].upp;
      markring[i].lpp=markring[i-1].lpp;}
    markring[0].upp=up; markring[0].lpp=lp; 
    if (toplevel) {chartype(7); putchar(buf[lp+1]); chartype(0);}}

void popmark()	/*return the last mark*/
  { int i;
    markup=markring[0].upp; marklp=markring[0].lpp;
    for (i=0; i<MAXMARK; i++) {
      markring[i].upp=markring[i+1].upp;
      markring[i].lpp=markring[i+1].lpp;}
    markring[MAXMARK].upp=markup; markring[MAXMARK].lpp=marklp;}

/*[m@]*/
void exchgmarker()	/*exchange marked position and cursor position*/
  { markup=markring[0].upp;
    marklp=markring[0].lpp;
    if (markup<up) {
      markring[0].upp=up;
      markring[0].lpp=lp;
      moveup(markup);}
    else if (lp<marklp) {
      markring[0].upp=up;
      markring[0].lpp=lp;
      movedown(marklp);}
    else return;
    newpage(0);}

/*[m^@]*/
void gotomarker()	/*move the cursor to marked position*/
  { popmark();
    if (markup<up) moveup(markup);
    else if (lp<marklp) movedown(marklp);
    else return;
    newpage(0);}

/****************************************************************
/* killing and deleting
/****************************************************************/

void pushkill(mode)
  int mode;
  { int i;
    if (killsp>=MAXKLSP) {
      for (i=0; i<MAXKLSP; i++) killstack[i]=killstack[i+1];
      killsp--;}
    killstack[++killsp]=mode; changed=1;}

int popkill()
  { if (killsp<0) return(ERR);
    else return(killstack[killsp--]);}
  
void killline(mode)  /*kill line*/
int mode;
{ if (lp<buftail) {
    refmode=0x16;
    if (mode & 2) 
      if ((buf[lntail]==LF)&&(lp==lntail-1)) mode=1;
      else {
        pushkill(lp); pushkill(7);
        if (buf[lntail]==LF) lp=lntail-1; else lp=lntail;}      
    if (mode & 1) killch();
    if (mode & 4) {
      refmode=0x1e;
      if (up==lnhead) mode=8;
      else {pushkill(up); pushkill(11); up=lnhead;}}
    if (mode & 8) delch();
    newlength();}
  else bell();}

/*[^Z]*/
void forkillln()	/*kill line after cursor */
  { if (toplevel) killline(2); else killline(3);}

/*[mZ]*/
void revkillln()	/*kill line before cursor */
  { if (toplevel) killline(4); else killline(12);}

/*[del]*/
void delch()		/*delete char before cursor*/
{ if (up>0) {
    refmode=0x39;
    if (buf[--up]==LF) {
      lnhead=findhead(lnhead-1); pushkill(10);
      if (y>0) {
        --y; refmode=0x3a;
  	if (pagetail<buftail) pagetail=findtail(pagetail+1);
  	else --linepos;}
      else newpage(1);}
    else {
      if (buf[up]>0x80) { up--; pushkill(9);}
      pushkill(9);}
    newlength();}
  else bell();}

/*[^D]*/
void killch() /*kill one char just on the cursor*/
{ if (lp<buftail) {
    refmode=0x35;
    if (buf[++lp]==LF) {
      lntail=findtail(lntail+1); refmode=0x36; pushkill(6);
      if (pagetail<buftail) pagetail=findtail(pagetail+1);
      else --linepos;}
    else {
      if (buf[lp]>0x80) { lp++; pushkill(5);}	/*kanji: delete 2bytes*/
      pushkill(5);}
    newlength();}
  else bell();}

/*[^N]*/
void unkill()	/*recover deleted chars*/
  { int mode;
    if ((mode=popkill())<0) {bell(); return;}
    switch(mode) {
      case  5: lp--; newlength(); refmode=0x25; break;
      case  6: lp--; refmode=0x26; newpage(0); break;
      case  7: lp=popkill(); refmode=0x2d; newpage(0); break;
      case  9: up++; newlength(); refmode=0x29; break;
      case 10: up++; refmode=0x2a; newpage(0); break;
      case 11: up=popkill(); refmode=0x2d; newpage(0); break;
      }}

/*[mW]*/
int saveregion()	/*save the region between mark and cursor*/
  { int i,leng,dir;
    popmark();
    if ((markup<up)&&(up-markup<killavail)) {
       leng=up-markup;
       if (leng>=killavail) {bell(); return(0);}
       copybuf(killb+leng,killb,(long)killring[MAXKILL-1].pos);
       copybuf(killb,buf+markup,(long)leng);
       dir=0;}
    else if ((lp<marklp)&&(marklp-lp<killavail)) {
       leng=marklp-lp;
       if (leng>=killavail) {bell(); return(0);}
       copybuf(killb+leng,killb,(long)killring[MAXKILL-1].pos);
       copybuf(killb,buf+lp+1,(long)leng);
       dir=1;}
    else {bell(); return(0);};
    for (i=MAXKILL; i>0; i--) {
      killring[i].pos=killring[i-1].pos+leng;
      killring[i].dir=killring[i-1].dir;}
    killring[0].pos=leng;
    killring[0].dir=dir;
    killavail -= leng;
    return(1);}

/*[^W]*/
void killregion()	/*kill the region and save it into killbuff.*/
  { if (saveregion()) {
      if (markup<up) {changed=1; up=markup; refmode=0x17;}
      else if (lp<marklp) {changed=1; lp=marklp; refmode=0x17;}
      newpage(0);}}

/*[^Y]*/
void yankregion()	/*yank(recover) the region from killbuf*/
  { int ou,ox,oy,i,leng,dir,cnt,topsave;
    if (count==0) cnt=1; else cnt=count;
    leng=killring[0].pos;
    dir=killring[0].dir;
    ou=oldup; ox=oldx; oy=oldy;
    if (leng>0) {
      if (lp-up>leng) {
	topsave=toplevel; toplevel=FALSE;
	pushmark();
	toplevel=topsave;
	changed=1; 
	if (dir) {
	  for (i=0; i<cnt; i++) {
  	  copybuf(buf+up,killb,(long)leng);
  	  up += leng;}
	  refmode=0x29;
	  for (i=0; i<leng; i++) if (killb[i]==LF)  {refmode=0x2b; break;}
	  }
	else {
	  for (i=0; i<cnt; i++) {
  	  copybuf(buf+lp-leng+1,killb,(long)leng);
  	  lp = lp-leng;}
	  refmode=0x25;
	  for (i=0; i<leng; i++) if (killb[i]==LF)  {refmode=0x2b; break;}
	  }
	newpage(0);
	if (refmode<128) {oldup=ou; oldx=ox; oldy=oy;}
	copybuf(killb,killb+leng,(long)killring[MAXKILL].pos-leng);}
      for (i=0; i<MAXKILL; i++) {
	killring[i].pos=killring[i+1].pos-leng;
	killring[i].dir=killring[i+1].dir;}
      killavail += leng;
      return(OK);}
    else {bell(); return(ERR);}}

/*[m^W]*/
void popoutkill()
  { int i,leng;
    leng=killring[0].pos;
    copybuf(killb,killb+leng,(long)killring[MAXKILL].pos-leng);
    for (i=0; i<MAXKILL; i++) {
      killring[i].pos=killring[i+1].pos-leng;
      killring[i].dir=killring[i+1].dir;}
    killavail += leng;}

/*--------------------------------------*/
/* mail operation
/*  use mbx(mail box) for vax/vms
/*  or message queu for unix
/*--------------------------------------*/
mbx_write(qid,mtp,mtx,length)
int qid,mtp,length;
char *mtx;
{ struct msg512  {
    long mtype;
    char mtext[512];} msgp;
  msgp.mtype=mtp;
  if (length>511) {printf("too long line, truncated to 512"); length=511;}
  strncpy(msgp.mtext,mtx,length);
  return(msgsnd(qid,&msgp,length,0));}

/*[mM]*/  
void mailregion()	/*send marked region via mailbox*/
{ int c,s,e,t,tt,stat;
  popmark();
  if (markup<up) {s=markup; e=up;}
  else if (lp<marklp) {s=lp; e=marklp;}
  else {bell(); return;}
  gotoxy(0,cmndline); printf("sending to mailbox  %d bytes",e-s); putchar(CR);
  FFS ; c=0;
  while (s<e) {
    tt=findtail(s);
    if (tt>e) t=e; else t=tt;
    if ((stat=mbx_write(qid2,1,&buf[s],t-s+1))<0) {
      gotoxy(0,cmndline); printf("error in sending, stat=%d  ",-stat);
      break;}
    c++;
    s=findhead(tt+1);}
  mbx_write(qid2,2,"",0);
  printf("%d lines sent",c); clreol(0,30);
  }

/*[^P]*/
void mailparagraph()
{ int s,e,p,t,tt,stat;
  s=lnhead;
  while ((s>0) && ((buf[s]!=LF) || (buf[s-1]!=LF))) s--;
  e=lntail;
  while ((e<buftail) && ((buf[e]!=LF) || (buf[e+1]!=LF))) e++;
  gotoxy(0,cmndline);
  printf("sending to MBX  %d bytes",e-lp+up-s); putchar(CR);
  FFS ; p=s;
  while (p<up) {
    tt=findtail(p);
    if (tt>=up) t=up-1; else t=tt;
    if ((stat=mbx_write(qid2,1,&buf[p],t-p+1))<0) {
      gotoxy(0,cmndline); printf("error in sending, stat=%d  ",-stat); break;}
     p=tt+1;}
  p=lp+1;
  while (p<=e) {
    tt=findtail(p);
    if (tt>e) t=e; else t=tt;
    if ((stat=mbx_write(qid2,1,&buf[p],t-p+1))<0) {
      gotoxy(0,cmndline); printf("error in sending, stat=%d  ",-stat); break;}
     p=tt+1;}
  mbx_write(qid2,2,"",0);
  clreol(0,30);}

/*------------------------------------------------------*/
/* balanced matching for (){}
/*------------------------------------------------------*/

void leftmatch(left,right)	/*search balancing 'left' backward*/
  char left,right;
  { int p,cnt;
    for (cnt=1, p=up; (--p>=0)&&(cnt>0); )
      if (buf[p]==left) --cnt;
      else if (buf[p]==right) cnt++;
    if (cnt==0) {moveup(p+1); newpage(0);}
    else bell();}

void rightmatch(left,right)	/*search balancing 'right' forward*/
  char left,right;
  { int p,cnt;
    for (cnt=1, p=lp+1; (++p<=buftail)&&(cnt>0); )
      if (buf[p]==right) --cnt;
      else if (buf[p]==left) ++cnt;
    if (cnt==0) {movedown(p-2); newpage(0);}
    else bell();}

/*[m(]*/
void lpmatch() {leftmatch('(',')');}		/*search balancing '(' backward*/

/*[m)]*/
void rparenmatch() {rightmatch('(',')');}	/*search balancing ')' forward*/

/*[m{]*/
void lcmatch() {leftmatch('{','}');} 	/*search balancing '{' backward*/

/*[m}]*/
void rcmatch() {rightmatch('{','}');}	/*search balancing '}' forward*/


/****************************************************************/
/* string search  forward/backward
/****************************************************************/
/*[^F]*/
void search()
  { register int p,pp,l;
    register unsigned char firstch;
    unsigned char target[128];
    if (toplevel || inmacro)
      if (getstp(target,"f-find: ")<0) return(0);
      else if (strlen(target)!=0) {
	strcpy(searchstr,target); toupperstr(searchstr); putsearchstr();}
    found=FALSE; p=lp; firstch=searchstr[0];
    while ((!found) && (p<buftail)) {
      p++;
      if (uppercase(buf[p])==firstch) {
	l=0; pp= p; p++;  /*remember p for the mismatch*/
	while ((uppercase(buf[p])==searchstr[++l]) && (searchstr[l]!=0)) p++;
	if (searchstr[l]==0) found=TRUE;
	else p=pp;}}
    if (found) {movedown(p-1); newpage(0);}
    else  bell(); 
    return(found);}

/*[^B]*/
void revsearch()
{ register unsigned char lastch;
  unsigned char target[128];
  register int l,leng,p,pp;
  if (toplevel || inmacro) 
    if (getstp(target,"b-find: ")<0) return(0);
    else if (strlen(target)!=0) {
      strcpy(searchstr,target); toupperstr(searchstr); putsearchstr();}
  leng=strlen(searchstr);
  lastch=searchstr[leng-1];
  found=FALSE; p=up;
  while ((found==FALSE) && (p>0)) {
    p--;
    if (uppercase(buf[p])==lastch) {
      l=leng-1; pp= p; p--;
      while ((l >0) & (uppercase(buf[p])==searchstr[l-1])) {l--;p--;}
      if (l<=0) found=TRUE; else p=pp;}}
  if (found) {moveup(p+1); newpage(0); killsp= -1;}
  else bell();
  return(found);}


/****************************************************************/
/* insert char
/* Every graphic character is inserted at the cursor position.
/* EZ has no replace mode.
/****************************************************************/

void enterkanji()
{ getchr();
  puts("\033$B");
  kanji=1;}

void leavekanji()
{ getchr();
  puts("\033(J");
  kanji=0;}

void insert(c)	/*insert char 'c' at the cursor*/
unsigned char c;
{
  if (lp-up<128) bell();
  else {
    killsp= -1;	/*invalidate kill stack*/
    if (changed<1) {changed=1; status(chngdisp,'C');}
    if (kanji>1) {
      buf[up++]=kanji;
      buf[up++]=c | 0x80;	/*bit 8 of second byte must be 1*/
      refmode = 0x29;
      kanji=1;
      newlength();}
    else if (kanji) kanji=c | 0x80;
    else {	/*ascii characters*/
      buf[up++]=c;
      if (c==LF) {
	lnhead=up;
	if (++y>=pagelength) newpage(1);
	else if (++linepos>=pagelength) {
	  pagetail=findhead(pagetail)-1;
	  linepos=pagelength;
	  refmode=0x2a;}
	else refmode=0x2a;}
      else if ((c==TAB)||(c<' ')) refmode=0x2d;
      else refmode=0x29;
      newlength();}}
  if (toplevel==TRUE && x==rightmargin) bell();}

/*any visible chars or tab*/
void inschar()
  { insert(ch); }

/*[^M],[cr]*/
void insnewline()	/*insert newline code 'lf'*/
{ insert(LF); }
  
/*[^O]*/
void openline()	/*devide a line at the cursor*/
  { int i,p;
    p=0;
    for (i=lnhead; (i<up) && ((buf[i]==' ') || (buf[i]==TAB)); i++)
      p=nextcol(p,buf[i]);    
    insert(LF);
    for (i=0; i<p; i++) insert(' ');
    if (refmode<128) refmode=0x26; prevline();}

void quotedins()	/*insert an any folowing code(control code is available)*/
  { status(metadisp,'Q'); getchr(); status(metadisp,' '); insert(ch);}


/* message queu and terminals	*/

void chgmsgq()
{ char keystr[16];
  int key;
  FILE *ezlog;
  getstp(keystr,"new message queu key: ");
  sscanf(keystr,"%d",&key);
  key=key & 0xffff;
  qid1=msgget(key | 0x500000, 0 /*IPC_ALLOC*/);	/* msg queu exists? */
  qid2=msgget(key | 0x600000, 0 /*IPC_ALLOC*/);
  ezlog=fopen("ezlog","w");
  fprintf(ezlog,"new qid=%x, %x\n",qid1,qid2);
  fclose(ezlog);}

void chgterm()
{ char termname[64];
  int stat;

  getstp(termname,"term: ");
  close(0); close(1);
  stat=open(termname,0);
  stat=open(termname,1);
  if (stat<0) fprintf(stderr,"error=%d\n",errno);
  }
  
void change_kanji_mode()
{ euc= !euc;}

/* justification */

void justify()
{ register int c, c2, p, len, just=count;
  if (count<=1) { bell(); return(ERR);}
  popmark();
  if (markup<up) { len=up-markup; moveup(markup);}
  else len=markup-up;
  while (len>0) {
    just=count;
    while (just>0) {
      len--;
      c=buf[++lp];
      if (c==LF) {
	c=buf[lp+1]; c2=buf[lp+2];
	if (c==LF || c==' ' || (c==0xa1 && c2==0xa1)) {
	  buf[up++]=LF; just=0; break;}
	else continue; /*ignore LF*/ }
      buf[up++]=c; just--;
      if (c & 0x80) { /*kanji*/
        c2=buf[++lp]; buf[up++]=c2; just--; len--;
	c=buf[lp+1]; c2=buf[lp+2];
	if (c==0xa1 && 0xa1<c2 && c2<=0xaa) {
	  /*move one more kanji if it is a punctuation letter*/
	  buf[up++]=c; buf[up++]=c2; lp+=2;  just-=2; len-=2; }
	}
      }
    buf[up++]=LF; }
  refmode=128;
  count=0; return(OK);}

/****************************************************************/
/* H E L P 
/****************************************************************/
/*[m?]*/
void printhelp()	/*display command menu to help*/
{
gotoxy(0,firstline); clreol(0,pagewidth);
printf("*** default key-define table *** '84.11  NOTE:	");
printf("^:control e:escape (mk):marked\015\012");clreol(0,pagewidth);

printf("^I,HT:tab.ins.		CR,^M:line ins.		");
printf("^P:quoted ins.	^O:open line\015\012"); clreol(0,pagewidth);

printf("^H,BS:move left by one	^K:move right 1ch.	");
printf("^U:prev. line	^J,LF:next line\015\012"); clreol(0,pagewidth);
printf("^A:head of line		^L:tail of line		");
printf("^T,eV:prev.page	^V:next page\015\012"); clreol(0,pagewidth);
printf("e<:head of text(mk)	e>:tail of text(mk)	");
printf("e,:page top	e.:page tail\015\012"); clreol(0,pagewidth);

printf("^@:mark 		e@:exchange mk/cursor	");
printf("e^@:goto marker\015\012"); clreol(0,pagewidth);

printf("^D:del one ch		DEL:1ch. before cursor	");
printf("^Z:del. to EOL	eZ:del. to TOL\015\012"); clreol(0,pagewidth);
printf("eW:copy region(killbuf)	^W:save region & delete	");
printf("e^W:(pop out killb.)?\015\012"); clreol(0,pagewidth);
printf("^Y:load from killb.(mk)	");
printf("^N:recover deleted ch...\015\012");
clreol(0,pagewidth);

printf("^F:search str./forward	^B:search str./backward	");
printf("e(,e),e{,e}:balanced pair\015\012"); clreol(0,pagewidth);

printf("eK:move to next word	eH:back to prev.word	");
printf("eD:del. word	eDEL:pre.word\015\012"); clreol(0,pagewidth);
printf("eC:initial to upper	eL:word to lower	");
printf("eU:word to upper\015\012"); clreol(0,pagewidth);
printf("--- file I/O and buffer operation ---\015\012"); clreol(0,pagewidth);

printf("eS:save to file		eQ:save and exit	");
printf("^C:no-save exit\015\012"); clreol(0,pagewidth);
printf("eI:load subfile		eO:write subfile	");
printf("eR:rename file	e$:rename ddir\015\012"); clreol(0,pagewidth);

printf("eF:switch/create buf.	eB:display buf.table	");
printf("e^Z:del. buf.\015\012"); clreol(0,pagewidth);

printf("e\042:def./end of macro	^E:execute macro	");
printf("e%%:load key-def file\015\012"); clreol(0,pagewidth);
printf("eM:(mail region)?	eP:(mail paragraph)?	");
printf("^R:repeat command\015\012"); clreol(0,pagewidth);
printf("e?:command list(HELP)	e^X:system command exe.");
printf("^G:refresh page");
}


/****************************************************************/
/* macro, repeat and command dispaching
/****************************************************************/
/*[m"]*/
void definemac()	/*define macro command string between M" and M" */
  { if (defmacro) {
      defmacro=FALSE; status(defdisp,' '); macrostr[macindex -=2] = -1;}
    else {defmacro=TRUE; status(defdisp,'D'); macindex=0;}}

/*[^E]*/
void exmacro()	/*execute defined macro command string*/
  { int ref,macrocount;
    if (defmacro) return(ERR);
    found=TRUE;
    ref=0;
    if (count==0) macrocount=1; else macrocount=count;
    while ((found) && (macrocount>0)) {
      ezindex=0;
      inmacro=TRUE;
      toplevel=FALSE;
      while ((getcmnd() >= 0) && (found)) {
	inmacro=TRUE;
	count=0;
	branch(ch);
	ref |= refmode;}
      macrocount--;}
    inmacro=FALSE;
    if (ref) refmode=128;}

void branch(c)	/*branch to specific function according to 'c' */
  { (*func[c])();}

int readint()	/*get repeat count; return by nondigit input*/
{ int val=0;
  while (ch<128 && isdigit(ch)) {
    val=val*10+ch-'0';
    if (toplevel) {
      chartype(7);
      putcount(countdisp,5,val);
      chartype(0);
      gotoxy(x,y+firstline); FFS ;}
    getcmnd();}
  return(val);}

/*[^R]*/
void repcmnd()
  { int c,rm;
    count=1; status(repdisp,'R');
    while (ch==k_repeat) {
      count<<=2;
      if (toplevel) {
	chartype(7); putcount(countdisp,5,count); chartype(0);
	gotoxy(x,y+firstline); FFS ;}
      getcmnd();};
    if (ch<128 && isdigit(ch)) count=readint();
    found=TRUE; c=ch; rm=0;
    if (toplevel) {gotoxy(repdisp,statline); chartype(7); putst("      ");
      chartype(0); gotoxy(x,y+firstline);}
    while ((c!=k_abort) && (found) && (count>0)) {
      branch(c); rm |= refmode; toplevel=FALSE; count--;}
    if (rm) {
      if ((rm & 3)==2) refmode=128;
    else refmode=rm;}}

/****************************************************************/
/* error, system control and miscelaneous 
/****************************************************************/

void bell()	/*ring a bell and reset count & found to stop repeating*/
{ putchar(BEL); FFS ; count=0; found=0; return(ERR);}

void ignore() {;}	/*do nothing -- ignore error*/

/*[^C]*/
void ezabort()	/*exit from current edit session without saving*/
{ running=0; 	/*abort immediately*/}

/*[^G]*/
void newrefresh()	/*force refreshing*/
{ newpage(1); ezrefresh(); refmode=0;}

void setrightmargin()
{ rightmargin=count; count=0;}

/****************************************************************/
/* set dispatching table
/*   assign function to keycode
/****************************************************************/

int getfuncode(fd)	/*get the key code definition from the file fd*/
  FILE *fd;
  { int code;
    ch=getc(fd);
    while (((int)ch!=EOF) && (ch<=' ')) ch=getc(fd);
    if ((int)ch!=EOF) {
      if (ch=='^') {ch=getc(fd); code=to_upper(ch)-'@';}
      else if ((ch=='M')||(ch=='m')) {
	ch=getc(fd);
	if (ch=='^') {ch=getc(fd); ch=to_upper(ch)-'@';} /*meta+ctrl*/
	code=to_upper(ch) | 256;}
      else code=ch;
      return(code & 0xff);}
    else return(ERR);}

void setfunctab(keyfile)	/*set func tab by reading keyfile*/
char *keyfile;
  { int i,cmnd,code;
    FILE *fd;

    if ((fd=fopen(keyfile,"r"))==NULL) {
      gotoxy(0,cmndline); printf("cannot find %s.\015\012",keyfile); return;}
    else prompt("reading key-def file");
    while ((cmnd=getfuncode(fd))>=0) {
      code=getfuncode(fd);
      func[cmnd]=dfunc[code];
      if (code==k_repeat) k_repeat=cmnd;
      else if (code==k_abort)  k_abort=cmnd; 
      else if (code==k_quote)  k_quote=cmnd; 
      else if (code==k_bfind)  k_bfind=cmnd; 
      else if (code==k_ffind)  k_ffind=cmnd;  
      }
    fclose(fd);
    for (i=0; i<26; i++) func['a'+i | 256]=func['A'+i | 256]; /*but touppered*/
    putchar(CR); clreol(0,pagewidth);}

/*[m%]*/
void initfunctab();

void newfunctab()	/*change key definition or reset to default assignment*/
  { int s;
    char keydef[64];
    s=getstp(keydef,"keydef file: ");
    if (s<0) return(ERR);
    else if (s==0) initfunctab();
    else setfunctab(keydef);
    /*return(OK);*/}


/****************************************************************/
/*--- default key setting
/****************************************************************/

void initfunctab()	/*initialize keydef table*/
  { int i;
    k_repeat=18; k_abort=3; k_bfind=2; k_ffind=6;
    k_quote=17;
    for (i=0; i<512; ) dfunc[i++]=bell;  /*all undefined keys cause error*/
    /*control commands*/
    dfunc[0]=  pushmark;		dfunc[1]=  movbol;
    dfunc[2]=  revsearch;		dfunc[3]=  ezabort;
    dfunc[4]=  killch;			dfunc[5]=  exmacro;
    dfunc[6]=  search;			dfunc[7]=  newrefresh;
    dfunc[8]=  movleft;			dfunc[9]=  inschar;
    dfunc[10]= nextline;		dfunc[11]= movright;
    dfunc[12]= moveol;			dfunc[13]= insnewline;
    dfunc[14]= unkill;			dfunc[15]= openline;
    dfunc[16]= pushmark;	        dfunc[17]= quotedins;
    dfunc[18]= repcmnd;			dfunc[19]= ignore;
    dfunc[20]= prevpage;		dfunc[21]= prevline;
    dfunc[22]= nextpage;		dfunc[23]= killregion;
    dfunc[24]= bell;			dfunc[25]= yankregion;
    dfunc[26]= forkillln;	
    dfunc[28]= ignore;

    /*all graphic characters including kanji are inserted*/  
    for (i=' '; i<256; ) dfunc[i++]=inschar;	
    dfunc[127]=delch;

    /*meta commands*/
    dfunc[',' | 256]= toppage;		dfunc['.' | 256]= bottompage;
    dfunc['<' | 256]= movtob;		dfunc['>' | 256]= moveob;
    dfunc['"' | 256]= definemac;	dfunc['[' | 256]= lpmatch;
    dfunc[']' | 256]= rparenmatch;	dfunc['{' | 256]= lcmatch;
    dfunc['}' | 256]= rcmatch;		dfunc['@' | 256]= exchgmarker;
    dfunc['$' | 256]= enterkanji;	dfunc['(' | 256]= leavekanji;
    dfunc['%' | 256]= newfunctab;	dfunc['/' | 256]= nextdir;
    dfunc['A' | 256]= bell;		dfunc['B' | 256]= listbuffer;
    dfunc['C' | 256]= capitalword;	dfunc['D' | 256]= killword;
    dfunc['E' | 256]= bell;		dfunc['F' | 256]= newbuffer;
    dfunc['G' | 256]= bell;		dfunc['H' | 256]= movprevword;
    dfunc['I' | 256]= mergefile;	dfunc['J' | 256]= abspos;
    dfunc['K' | 256]= movnextword;	dfunc['L' | 256]= lowerword;
    dfunc['M' | 256]= mailregion;	dfunc['N' | 256]= bell;
    dfunc['O' | 256]= putfile;          dfunc['P' | 256]= mailparagraph;
    dfunc['Q' | 256]= savexit;		dfunc['R' | 256]= renamefile;
    dfunc['S' | 256]= savefile;		dfunc['T' | 256]= bell;
    dfunc['U' | 256]= upperword;	dfunc['V' | 256]= prevpage;
    dfunc['W' | 256]= saveregion;	dfunc['X' | 256]= bell;
    dfunc['Y' | 256]= bell;		dfunc['Z' | 256]= revkillln;
    dfunc['?' | 256]= printhelp;
    /*escape control command*/
    dfunc[('['-'@') | 256]= ignore;
    dfunc[('@'-'@') | 256]= gotomarker;
    dfunc[('E'-'@') | 256]= change_kanji_mode;
    dfunc[('I'-'@') | 256]= rereadfile;
    dfunc[('J'-'@') | 256]= justify;
    dfunc[('L'-'@') | 256]= setwidth;
    dfunc[('P'-'@') | 256]= setlength;
    dfunc[('Q'-'@') | 256]= chgmsgq;
    dfunc[('R'-'@') | 256]= setrightmargin;
    dfunc[('T'-'@') | 256]= chgterm;
    dfunc[('W'-'@') | 256]= popoutkill;
    dfunc[('Z'-'@') | 256]= killbuffer;
    for (i=0; i<26; i++) dfunc['a'+i | 256]=dfunc['A'+i | 256];
    dfunc[127 | 256]= delword;
    for (i=0; i<512; i++) func[i]=dfunc[i];}

