/********************************************************/
/* ezuiovt.c : ez I/O primitives			*/
/*	1983, Feb.
/*	Copyright T.Matsui (ETL)			*/
/********************************************************/

#include <stdio.h>
#include "ez.h"

init_term()
{ putst("\033>\033[r"); /*enter ANSI mode, reset scroll region*/ }  

putint(val)
int val;
{ if (val<10) putchar(val+'0');
  else {putint(val/10); putchar(val % 10 +'0');}}

gotoxy(x,y)
int x,y;
 {putst("\033["); putint(y+1); putchar(';'); putint(x+1);
  putchar('H'); FFS ;}

home()
 {putst("\033[;H"); FFS ;}

clreol()
 {putst("\033[K"); FFS ;}  /*clear to end of line*/

clreos()
 {putst("\033[J"); FFS ;}  /*clear to end of screen*/

chartype(n)
int n;
{ putchar(ESC); putchar('['); putchar(n+'0'); putchar('m'); FFS ;}

scr_region(top,bottom)
int top,bottom;
{ putst("\033["); putint(top+1); putchar(';'); putint(bottom+1); putchar('r');}

save_cursor()
{ putchar(ESC); putchar('7');}

restore_cursor()
{ putchar(ESC); putchar('8');}

scr_up()
{ putst("\033D");}

scr_down()
{ putst("\033M");}

delline(ln)  /*delete lnth line from the top of page*/
{ if (dlstr[0]) putst(dlstr);
  else {
    scr_region(ln,lastline); save_cursor(); gotoxy(0,lastline);
    scr_up(); restore_cursor(); scr_region(0,cmndline);} }

insline()
{ if (alstr[0]) putst(alstr);
  else {
    scr_region(y,lastline); save_cursor(); gotoxy(0,y);
    scr_down(); restore_cursor(); scr_region(0,cmndline);} }

