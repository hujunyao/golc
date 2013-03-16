%{
#include <stdio.h>
#include "base.h"

enum {
	JFLAG_PACKAGE	=		(1<<0),
	JFLAG_IMPORT	=		(1<<1),
	JFLAG_CLASS		=		(1<<2),
	JFLAG_CLASS_RELATION = (1<<3)
};

enum {
	
};

#define DD(...) fprintf(stderr, "[JLEX] "__VA_ARGS__)
cmt_sta_t cmtsta = CMT_STA_NULL;
gint jflag = 0;

%}

%option header-file="java-lex.h"
%option 8bit outfile="java-lex.c" prefix="java_"
%option nounput nomain noyywrap warn reentrant
%option extra-type="baseinfo_t *"

%%
"package" {
	if(cmtsta == CMT_STA_NULL)
		SET_FLAG(jflag, JFLAG_PACKAGE);
}

"import" {
	if(cmtsta == CMT_STA_NULL)
		SET_FLAG(jflag, JFLAG_IMPORT);
}

"class" {
	if(cmtsta == CMT_STA_NULL) {
		SET_FLAG(jflag, JFLAG_CLASS);
	}
}

"{" {
	if(CHK_FLAG(jflag, JFLAG_CLASS_RELATION)) {
		CLR_FLAG(jflag, JFLAG_CLASS_RELATION);
	}
}

[A-Za-z.]+ {
	if(CHK_FLAG(jflag, JFLAG_PACKAGE)) {
		DD("package=%s\n", yytext);
		CLR_FLAG(jflag, JFLAG_PACKAGE);
	} else if(CHK_FLAG(jflag, JFLAG_IMPORT)) {
		DD("import = %s\n", yytext);
		CLR_FLAG(jflag, JFLAG_IMPORT);
	} else if(CHK_FLAG(jflag, JFLAG_CLASS)) {
		DD("class = %s\n", yytext);
		CLR_FLAG(jflag, JFLAG_CLASS);
		SET_FLAG(jflag, JFLAG_CLASS_RELATION);
	} else if(CHK_FLAG(jflag, JFLAG_CLASS_RELATION)) {
		DD("relation = %s\n", yytext);
		CLR_FLAG(jflag, JFLAG_CLASS_RELATION);
	}
}

"/*" {
	cmtsta = CMT_STA_START;
}

"*/" {
	cmtsta = CMT_STA_END;
}

"//" {
	cmtsta = CMT_STA_END;
}

\n {
	if(cmtsta == CMT_STA_NULL) {
		yyextra->pure_line ++;
	}
	if(cmtsta == CMT_STA_END)
		cmtsta = CMT_STA_NULL;
}

. {
	if(cmtsta == CMT_STA_NULL) {
		yyextra->pure_char ++;
	}
}
%%
