%{
	#include "stdio.h"
	#include "stdlib.h"
	#include "bison.tab.h"
	#include "global.h"
	//int yywrap();
	int len = 0;
%}

%%

[\n] { inputBuff[len++] = 0;len = 0;return EOF;}
[ \t] {
	inputBuff[len++] = yytext[0];
	}
[^ \t<>|&\n]+ {
		for (int i = 0;i < strlen(yytext); i++)
			inputBuff[len++] = yytext[i];
		return STRING;}
[<]	{inputBuff[len++] = yytext[0];return '<';}
[>]	{inputBuff[len++] = yytext[0];return '>';}
[|]	{inputBuff[len++] = yytext[0];return '|';}
[&]	{inputBuff[len++] = yytext[0];return '&';}


%%

int yywrap()
{
	return 1;
}
