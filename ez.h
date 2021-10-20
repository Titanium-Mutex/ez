/********************************************************/
/* ez.h:
/*------------------------------------------------------*/
/* author:	 T.Matsui (ETL)		*/
/*	OCT 1984 v1.02				10.25	*/
/********************************************************/

#define FF 12
#define CR 13
#define BS 8
#define TAB 9
#define LF 10
#define DEL 127
#define BEL 7
#define ESC 0x1b

#define TRUE 0xffff
#define FALSE 0

#define OK  1
#define ERR -1

#define BUFFUNIT (unsigned int)16384  /*edit buffer size(16k)*/
#define MAXKILLBUF (unsigned int)32768 /*maximum length of kill buffer(32k)*/ 

#define MAXKLSP 80	/*size of killstack*/
#define MAXSTR 128	/*maximum length of string*/
#define MAXBUFNO 32     /*limit of text buffer number*/
#define MAXMARK 7	/*maximum count of marking per buffer*/
#define MAXKILL 7	/*maximam count of killring per buffer*/

/****************************************************************/
/* column positions in the status line 
/****************************************************************/
#define	chngdisp	0
#define metadisp	1
#define defdisp		2
#define repdisp		3
#define kanjidisp	4
#define countdisp	5
#define posdisp		10
#define ddirdisp	17

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#define  min(a, b) (((a) < (b)) ? (a) : (b))
#define  max(a, b) (((a) > (b)) ? (a) : (b))

#define  to_upper(c) (islower(c) ? ((c)-'a'+'A') : (c))
#define  to_lower(c) (isupper(c) ? ((c)-'A'+'a') : (c))

#define C_F {putchar(CR); putchar(LF);}
#define FFS fflush(stdout);

/****************************************************************/
/* column positions in the status line 
/****************************************************************/
#define	chngdisp	0
#define metadisp	1
#define defdisp		2
#define repdisp		3
#define kanjidisp	4
#define countdisp	5
#define posdisp		10
#define ddirdisp	17

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
/* typedef's
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

typedef struct {int upp,lpp;} ptr_pair;

typedef struct {int pos,dir;} kill_pair;

typedef struct {
  unsigned char  filename[64];
  unsigned char  *buf;
  ptr_pair  markring[MAXMARK+1];
  int   up,lp,markup,marklp,x,y;
  int   pagehead,pagetail,lnhead,lntail,buftail;
  int   firstline,lastline,pagelength;
  int   changed;
  } ez_descripter;


/****************************************************************
/* References to global variables */
/****************************************************************/

/**** screen size parameters */
extern int  pagelength,halfpage,pagewidth;
extern int  firstline,lastline,statline,stringline,cmndline;
extern int  filedisp;
extern int  rightmargin;

/**** refresh mode: command to ezref (function refresh)*/
extern int  refmode;
extern int  ch;

/**** buffer management */
extern int  pagehead,pagetail,buftail,linepos;
extern int  lnhead,lntail,chpos;
extern int  up,lp,oldup,oldlp,markup,marklp;
extern unsigned char *buf,*killb;
extern unsigned char searchstr[];
extern int macrostr[];
extern ptr_pair *markring;
extern int  maxbufno, self;

/* kill buffers */
extern kill_pair killring[];
extern int  killsp;
extern int  killavail;

/**** cursor control */ 
extern int  x,y,oldx,oldy;
extern unsigned char tcap[1024];	/*termcap buffer*/
extern char alstr[32];		/*string to add(insert) a new line */
extern char dlstr[32];		/*string to delete a line */

/**** file control *****/
extern unsigned char filename[256],nextfile[256],ddirname[256];

/***** system control */
extern int running;
extern int toplevel;
extern int count;
extern int inmacro, defmacro, changed;





/****** message queues between ez and ezm */
extern int  msgsize, msgix, qid1, qid2;
extern struct mb256 {
    long mtype;
    char mtext[256];} msgp1,msgp2;
 
/****************************************************************
/* Function prototypes
/****************************************************************/

extern void gotoxy(int x,int y);
extern void home();
extern void clreol();
extern void clreos();
extern void chartype(int n);
extern void sttycooked();
extern void sttyraw();
/* extern void bell(); */
extern void newpage();


