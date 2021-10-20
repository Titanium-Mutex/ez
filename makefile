#makefile to make EZ

all:	ez ezu
ezu:	ezm.o ezu.o ezuref1.o ezuiovt.o
	cc -o ezu ezm.o ezu.o ezuref1.o ezuiovt.o -ltermcap
#	cc  -o ezu  ezm.o ezu.o ezuref1.o ezuiovt.o -lbsd -ltermcap
ez:	ez.o ez.h
	cc -o ez ez.c 

ezm.o:	ez.h  ezm.c
	cc -c -g -w ezm.c

ezu.o:		ez.h ezu.c
		cc -c  -g -w ezu.c

ezuref1.o:	ez.h  ezuref1.c
		cc -c -w ezuref1.c

ezuiovt.o:	ez.h  ezuiovt.c
		cc -c -w ezuiovt.c



