/*
 * this file is for definitions that must be shared between the parser
 * and the lexor.
 */

struct FIAL_ltype {
    int line;
    int col;
    const char *file;
};

#define YYLTYPE struct FIAL_ltype

#define YYLLOC_DEFAULT(Cur, Rhs, N)                      \
    do{                                                  \
	if(N) {                                          \
	    (Cur).line = YYRHSLOC(Rhs, 1).line;          \
	    (Cur).col = YYRHSLOC(Rhs, 1).col;            \
	    (Cur).file = YYRHSLOC(Rhs, 1).file;          \
	} else {                                         \
	    (Cur).line = YYRHSLOC(Rhs, 0).line;          \
	    (Cur).col = YYRHSLOC(Rhs, 0).col;            \
	    (Cur).file = YYRHSLOC(Rhs, 0).file;          \
        }                                                \
    }while(0)
