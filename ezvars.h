/**** screen size parameters */
int  pagelength,halfpage,pagewidth;
int  firstline,lastline,statline,stringline,cmndline;
int  filedisp;
int  rightmargin;

/**** refresh mode: command to ezref (function refresh)*/
int  refmode;

/**** buffer management */
int  pagehead,pagetail,buftail,linepos;
int  lnhead,lntail,chpos;
int  up,lp,oldup,oldlp,markup,marklp;
unsigned char *buf,*killb;

/**** cursor position*/ 
int  x,y,oldx,oldy;

