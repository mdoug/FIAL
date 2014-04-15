/*
 * this file is for definitions that must be shared between the parser
 * and the lexor.
 */

struct FIAL_ltype {
    int line;
    int col;
    const char *file;
};  /* not a define, I should rename this file */

#define YYLTYPE struct FIAL_ltype
#define YYLTYPE_IS_DECLARED

extern YYLTYPE yylloc;  /* not exactly a define, is it?  */

#ifdef USE_BISON_YYLLOC
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

#else /* USE_BISON_YYLLOC -- this assumes byacc */

#define YYLLOC_DEFAULT(loc, rhs, n) \
do \
{ \
    if (n == 0) \
    { \
        (loc).line = ((rhs)[-1]).line; \
        (loc).col  = ((rhs)[-1]).col;  \
        (loc).file = ((rhs)[-1]).file; \
    } \
    else \
    { \
        (loc).line   = ((rhs)[ 0 ]).line; \
        (loc).col    = ((rhs)[ 0 ]).col;  \
	(loc).file   = ((rhs)[ 0 ]).file; \
    } \
} while (0)

#endif /* USE_BISON_YYLLOC */
