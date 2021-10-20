/****************************************/
/* ezuref1.c				*/
/* displaying and refreshing    	*/
/*  for intelligent terminals  		*/
/*  which have DL and IL functions.	*/
/*	OCT 1984 v1.02		10.25	*/
/****************************************/

#include <stdio.h>

#include "ez.h"

displine(s,e) /*returns line numbers actually displayed*/
int s,e;
 { unsigned char  ch;
   int  ln,pw,kanji=0;

  ln=0; pw=pagewidth-1;
  while ((s>=0)&&(s<=e)) {
    ch=buf[s++];
    if (chpos<=pw) {
      if (ch==TAB) do putchar(' ');
        while ((++chpos<=pw) && ((chpos & 7) != 0));
      else if (ch==0x7f) {
	putchar('^');
	if (chpos<pw) putchar('?');
        chpos+=2;}
      else if (ch>=' ') {
        chpos++;
        if (kanji) {
	  putchar(ch);
	  if (ch & 0x80 == 0) { /*putst("\033(J");*/ kanji=0;} }
	else if (ch & 0x80) {
	  /* putst("\033$B"); */
	  putchar(ch); kanji=1;}
	else putchar(ch);}
      else if (ch==LF) {ln++; clreol(); if (s<=e) C_F chpos=0;}
      else if (chpos<pw) {putchar('^'); putchar(ch+'@'); chpos+=2;}}
    else if (ch==LF) {
      FFS ; C_F ln++; chpos=0;}}
  if (e==buftail) clreol();
  /* if (kanji) putst("\033(J"); */
  return(ln);}

reflast()
{ gotoxy(0,lastline); clreol(); chpos=0;
  if (linepos>=pagelength-1) displine(findhead(pagetail),pagetail);}

refline()
{ gotoxy(0,y+firstline); FFS ; chpos=0;
  displine(lnhead,up-1); displine(lp+1,lntail); chpos=x;}

refpage()
{ int ln;
  sttycooked();
  ln=0; gotoxy(0,firstline); chpos=0;
  ln=displine(pagehead,up-1); gotoxy(x,y+firstline); FFS ;
  ln+=displine(lp+1,pagetail);
  while (ln++ <=pagelength) {C_F clreol();}
  sttyraw();}

ezrefresh()
{ char m;
  int l,i;
  m=refmode & 0xf;
  kanjioff(); 
  if (refmode>127) refpage();
  else switch(refmode>>4) {
    case 1: switch(m) {
      case 0xb: gotoxy(oldx,oldy+firstline); chpos=oldx;
		displine(oldup,up-1); break;
      case 6:   chpos=x; displine(lp+1,lntail); clreol(); chpos=x; break;
      case 0xe: refline(); break;
      default:  gotoxy(x,y+firstline); clreos();
		chpos=x; displine(lp+1,pagetail);}
      break;
    case 2: switch(m) {  /*insert*/
      case 5:   chpos=x; displine(lp+1,lntail); break;
      case 9:   chpos=oldx; displine(oldup,up-1); displine(lp+1,lntail); break;
      case 6:   insline(); refline(); gotoxy(0,y+1+firstline); chpos=0;
		displine(lntail+1,findtail(lntail+1)); clreol();
		gotoxy(0,statline); clreol();	break;
      case 0xa: clreol(); gotoxy(x,y+firstline); insline(); refline();
		gotoxy(0,statline); clreol(); break;
      case 0xb: gotoxy(oldx,oldy+firstline); chpos=oldx; displine(oldup,up-1);
		gotoxy(x,y+firstline); displine(lp+1,pagetail); break;
      case 0xd: refline(); break;}
      break;
    case 3: switch(m) {  /*delete/kill*/
      case 9:   gotoxy(x,y+firstline); chpos=x; displine(lp+1,lntail); break;
      case 5:   chpos=x; displine(lp+1,lntail); break;
      case 0xa: delline(firstline+oldy); gotoxy(x,y+firstline);
		chpos=x; displine(lp+1,lntail);	reflast(); break;
      case 6:   chpos=0; displine(lp+1,lntail); gotoxy(0,y+1+firstline);
		delline(y+1+firstline); reflast(); break;
      default:  chpos=oldx; displine(lp+1,pagetail); break;}
      break;
    }
  if (((m & 3)>=2)||(refmode>127))  displaystatus();    
  oldx=x; oldy=y; oldup=up; oldlp=lp;
  }

