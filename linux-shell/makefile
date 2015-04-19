user-sh : bison.tab.o execute.o
	cc -o user-sh bison.tab.o execute.o
bison.tab.o : bison.tab.c global.h lex.yy.c
	cc -c -std=c99 bison.tab.c lex.yy.c
execute.o : execute.c global.h
	cc -c execute.c
bison.tab.c:
	bison -d bison.y
lex.yy.c :
	flex lex.l
clean :
	rm user-sh bison.tab.o execute.o bison.tab.c
