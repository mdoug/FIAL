/*
 *  These are mostly for internal use, though they could come in handy
 *  in an embedding C application as well.  But I'm not going to worry
 *  about naming, if a user doesn't want to use them, they can cut and
 *  paste and rename or write their own, or what have you.
 *
 *  The unfortunately have to be macros, some of them are meant to use
 *  goto as the error mechanism.
 */

/* on Error, Return */
#define ER(x) do{int tmp = x; if(tmp < 0) return tmp;}while(0)

/* on Error, Goto Error  --  */
#define EGE(x) do{if((x) < 0) goto error;}while(0)

/* on Error, Set, Goto Cleanup */
#define ESGC(x, y) do{int tmp = x; if(tmp < 0) {y = tmp; goto cleanup;}}while(0)

/* this works if the "current statement" part of the environment is
more or less as good as it gets as far as that goes.  At least, I hope
it does.  */

#define E_STMT(env) ((env).block_stack->stmt)
#define E_SET_ERROR(env) do{(env).error.line = (E_STMT(env))->loc.line;  \
                            (env).error.col  = (E_STMT(env))->loc.col;   \
			    (env).error.file = (env).lib->label;}while(0)
