# Overview of C API

## Parts of API

There are 3 distinct parts to the FIAL C api -- 

* top level 
* procs
* types

To some degree, libraries are intricately related to procs -- the relationship
nearly being one of part to whole.  They are nonetheless listed as part of the
top level interface, due to the fact that loading libraries is handled as a
matter of top level usage. 

Naturally, there is interaction between these separate parts, but nevertheless
the parts remain fairly distinct, in practice and conceptually.  

Each level of the interface has associates structures, and procedures.  The API
is discussed in the order listed here, which is also an order which provides
some semblance of sanity to the exposition of the API as a whole.  

As goes without saying, there is quite a bit of work left to be done, and
nothing here should be taken as a stable api.  Furthermore, wherever possible
follow along in the source, as with any code base undergoing change, it is
difficult to prevent documentational errors from occurring.  

## General Considerations

Errors nearly always signalled by returning a negative integer value, 0 is
typically the value of successful operation.  However, positive values are often
quite harmless, so the test for error must take the form `function_call() < 0`.   

Also note that the `FIAL_value` structure is discussed in the section on
procedures, due to expository reasons (I could no longer discuss the API without
it.)  It's usage is ubiquitious when writing code dealing with FIAL. 

# Top Level Interface

## Interpret, Interpreters, and Execution Environments

The core of the FIAL interpreter is an exposed function called `FIAL_interpret`,
it lives in the "interp.c" file, and is declared in "interp.h".  While it
appears to be a rather manageable function, with proper compilation flags a
large portion of the file should be inlined into it.  I examined its assembly
code not too long ago, with optimizers on, and it was approximately 3000 lines
of x86 assembler.  So it is something of a complex of a function, in operation,
if not appearance.   

But, in addition to `FIAL_interpret`, which could rightly be said to be the
"FIAL interpreter", there is a data structure called `struct FIAL_interpreter`,
and another called an "execution environment", or `struct FIAL_exec_env`, which
hold all the data necessary for interpreting -- i.e., the function
FIAL_interpret is reentrant, storing all state data in the aforementioned
structures.  

Typical usage is to load libraries into an interpreter structure.  Once loading
is complete, the interpreter is changed from a load state to a run state, which
renders it immutable from FIAL, and conceptually immutable to the C API --
though this is not enforced, I would wager that anyone who attempts to modify
this structure while multithreaded interpreting is going on is either much
better, or much worse, than I am at programming.  If there is only one thread of
execution, it would be simplest to just leave the interpreter in "load" mode, as
it can, and does, interpret in that state.  

Values produced from one execution environment, can be put into another, and it
will work properly provided that both execution environments use the same
interpreter.  

Having multiple interpreters may make sense for some applications -- for
instance, if part of the app is implemented by the developers in FIAL, and FIAL
is intended for use by the end users as a macro language.  In such an instance,
the interpreters would have different libraries available.  In this scenario,
care is required to ensure that values from one interpreter do not go into the
other.  

There is not currently, but there really should be, a series of helper functions
to assist in the conversion of a value from one interpreter to the other.  Off
the top of my head, the user would be required to convert symbols, and types,
potentially constants.  Also, note that user data pointers for libraries are
kept per interpreter, as libraries are installed per interpreter. 

In short, the case of sharing data between interpreters is somewhat problematic
in the general case.  Most applications would not have to solve this problem
with generality, however, instead only for the particular use case.  Perhaps
simplest would often be to just to be to resolve the FIAL values to an
underlying representation in the C app, and then recreate a value for it in the
other interpreter.    

## The Interpreter

The top level interface concerns itself with managing FIAL structures that are
created and used to support the interpretation of FIAL scripts.   It is only
natural, then, that our first type declaration be the FIAL interpreter itself -- 

This section is a description of the data structure, `FIAL_interpreter`.  There
is a description of how to control the actually interpreting in the section on
procedures.  The `FIAL_interpret` function is not discussed here, for while
being exposed, it is not part of the API.  

### Definition

    struct FIAL_interpreter {
    	int                                    state;
    	union  FIAL_lib_entry                  *libs;
    	struct FIAL_symbol_map            *constants;
    	struct FIAL_symbol_map                *omnis;
    	struct FIAL_master_symbol_table      symbols;
    	struct FIAL_master_type_table          types;
    };

It is, perhaps, worth noting a couple of things: in FIAL, at the global
namespace, FIAL is used as a prefix, and typedefs are eschewed whenever
possible.  The reason FIAL_ is used as a prefix should be obvious, but they are:
(a) the name of this scripting language, and (b) something no sane person would
ever use as a prefix, thus somewhat reducing the possibilities of conflict.
Typedefs are generally eschewed for the reason that they cannot be undone -- if
a user prefers using typedef'd typenames, they can provide their own, whereas a
user who prefers using the `struct` and `union` namespaces explicitly cannot
remove the typedef typenames if they are used in global declarations.  

Secondly, it should be noted that struct `FIAL_symbol_map` are used as containers,
this is currently typical.  The usage of FIAL values and containers as part of
the interpreter is done to limit the amount of problems faced in development.
Indeed, as is evident from the use of the language, there is little difference
between a symbol map as a value, and a symbol map that represents the symbols
used as identifiers in the text of the script.  However, it has also proved
convenient to use FIAL values to hold all sorts of interpreter specific data,
and to correlate it using FIAL containers.  

This approach may be abandoned, but it is retained in the hope that this will
lead to better, and not worse, data structures for the interpreter at
intermediate stages of development, by reducing the number of problems that must
be solved. 

The interpreter is typically used by the user as an opaque type.  However, it is
hardly necessary to suggest that the user may desire to access these, and that
doing so as needed may be appropriate.  When it becomes more apparent what parts
of the structure are intended for use by the user, and which should be hidden,
an interface using an incomplete type may be considered.

### Related API Functions

    int FIAL_init_interpreter (struct FIAL_interpreter *interp);
    struct FIAL_interpreter *FIAL_create_interpreter (void);

    void FIAL_deinit_interpreter (struct FIAL_interpreter *interp);
    void FIAL_destroy_interpreter (struct FIAL_interpreter *interp);


These functions only need to be commented on in that init/deinit is intended for
use for stack allocated interpreters, though they could be used for ones the
user malloc's on her own.  The create/destroy functions form their own pair, and
are meant to be used in conjunction, and largely serve for convenience.  
    
    int FIAL_set_interp_to_run(struct FIAL_interpreter *interp);

This is a function that is to be called after the user is finished loading the
libraries.  It's purpose is to put the interpreter into something of a
"read-only" state.  By preventing changes to it, it can be used in a
multithreaded fashion without locks.  However, it is not really "read only" in
any meaningful sense as far as the C API is concerned -- the embedding
application can still change it, it simply cannot be changed by a FIAL program.
There is a saying, however, about having enough rope to hang yourself with, and
it should be stated that the multithreaded model intended to be used assumes 
this structure is immutable.  

## Execution Environment

    struct FIAL_exec_env {
    	int                                state;
    	struct FIAL_interpreter          *interp;
    	struct FIAL_block           *block_stack;
    	struct FIAL_library                 *lib;  //active lib....
    	struct FIAL_error_info             error;
    	int skip_advance;
    };

The reader may rightly ask himself, "how does an immutable interpreter,
interpret?"  To this question, I am sure there is a very good answer.  It is
not the answer given here.  Here the answer is that the "execution environment"
is a separate entity, and that entity contains all the state necessary to carry
forward a thread of execution.  

I am not terribly interested in the semantics here -- in the name of
clarification, however, it could perhaps rightly be said that the "interpreter"
struct should really be "interpreter state info" or something of the sort, that
the execution environment be named something about mutable state necessary for
interpretation, and that really the "interpreter" is the C program which
manipulates the data, and is not a data structure at all.  In practice, however,
these concerns are not of much concern, since these structures are used in such
a ubiquitous manner, that they could be named anything, and the programmer would
remember them well enough through repetition alone.  

So, it should simply be noted that an execution environments is not to use in
multiple threads of execution.  In fact, it would be an exceedingly rare case
which did not fail in short order where that to be attempted.  However, values
that are produced in one environment, can be input into another environment as a
procedure argument, provided both environments are executing upon the same
interpreter.  

    int FIAL_init_exec_env(struct FIAL_exec_env *env);
    void FIAL_deinit_exec_env(struct FIAL_exec_env *env);
    struct FIAL_exec_env *FIAL_create_exec_env(void);
    void FIAL_destroy_exec_env(struct FIAL_exec_env *env);
    
These functions are analogous to their corresponding functions discussed in
interpreter section. 

    int FIAL_unwind_exec_env(struct FIAL_exec_env *env);

This functions will unwind the stack of the environment.  This will call the
destructors for all of the objects on the stack.  Normally, when an environment
returns, this would have already occurred.  However, errors are also not
entirely unexpected result of running code written by human beings.  In the
event of an error, the environment is returned with the stack unchanged, so that
it may be inspected if desired.  

The stack is automatically unwound, if the environment is passed to either the
deinit or the destroy function.  In the event that the application requires the
stack values to remain intact, and the environment destroyed, it could set the
`block_stack` field to NULL prior to deinit/destroy.  A "disembodied"
block_stack could be attached to any exec_env structure attached to the
same interpreter to be unwound. 

## Loader Functions

Clearly, there must be some way to load the script files into the interpreter.
In FIAL, this interface takes the form of a few functions.  There is a certain
"flavor" to the arguments that are passed to these functions, but unfortunately
they require different arguments, so there is only so much uniformity. 

A small confession: this interface is not very good.  It is probably not long
for this world, yet, were I to wait for everything to be perfect before I began
documenting, the documentation would never get written.  And, in large part, the
essential bits are here, it is just the particulars that are likely to change.
Yet, it is the particulars that are the subject of documentation, thus the
apology is appropriate.  

Additionally, there is not enough control allowed to the embedding application
using this interface.  Currently, the proper way to set libraries to have
labels other than by filename, is to load by filename, and then manually change
the label.  This is obviously cumbersome, and is indicative of a fundamentally
flawed scheme.  Furthermore, the only way to alter the behavior of the loader
callable from FIAL, is to expose a custom library, and not load the normal omni
libraries.  This could entail manually exposing the remaining procedures that
are considered desirable to have. 

### Load String 

    int FIAL_load_string (struct FIAL_interpreter *interp,
    		      const char *lib_label,
    		      union FIAL_lib_entry **new_lib,
    		      struct FIAL_error_info     *error);
    

This is the confusing part, and the crux of my belief that I need a new scheme
for the loader interface.  This function is intended to be the "high level"
loading function, which can be called to a library by its label.  The good thing
about this idea, is that it will work even if the library is already loaded --
it will simply return the corresponding library.  However, the name is
confusing.  And, in actuality, the name is confusing because what this function
does cannot be succinctly expressed. 
 
It is likely, then, that I will eliminate this function, either in favor of a
somewhat lower level interface, which hopefully will be less confusing, or in
favor of a high level interface that corresponds more clearly to what an end
user would think of as a high level interface.  In short, this is a higher level
function that does not really correspond to a comprehensible abstraction. 

But anyhow, this can be used to load a library that has already been loaded, and
has a label that matches the string passed in as a parameter.  If there is no
such library, a FIAL script will be loaded that has a filename matching that
library.  If there is no file with the name corresponding to the string passed
as a parameter, or if the loading of the script results in an error, an error
will be returned, and the error parameter will be marked accordingly.  

The new_lib parameter is the address of a pointer that will be set to the new
library.  This can be NULL, in which case the function will proceed normally,
and just not a pointer to a library in the event of a successful load. 

For now, see the code in "loader.c" to determine the nature of the error.  

### Load Lookup

    int FIAL_load_lookup (struct FIAL_interpreter *interp,
    		     const char *lib_label,
    		      union FIAL_lib_entry **ret_lib);
    
This function looks for a library with a matching library label in interp.  If
found, the pointer that is referenced by the pointer to a pointer ret_lib is set
to the library, and 1 is returned.  If it is not found, 0 is returned.  

### FIAL Script Loader

    int FIAL_load_file(struct FIAL_interpreter   *interp,
    		   const char *filename,
    		   union FIAL_lib_entry     **ret_lib,
    		   struct FIAL_error_info     *error  );
    
This is currently not intended for use by the users, in preference for the load
string function, but it is nonetheless exposed.  It will process the filename,
and add the library with the filename as a label.  

This does not check to see if the label is sane.  The user may alter the label
after the library is returned, in case a scheme of library labelling not
dependant on filename is preferred.  

### C Library Loader

     int FIAL_load_c_lib (struct FIAL_interpreter    *interp,
    		     char                       *lib_label,
    		     struct FIAL_c_func_def     *lib_def,
    		     struct FIAL_c_lib         **new_lib);

This function is used to expose C functions that FIAL scripts can call as
procedures.  The interp, lib_label, and new_lib behave in manner analogous to
how they behave in other loader functions.  

The `lib_def` parameter is an array of `struct FIAL_c_fund_def`'s, which are defined
in interp.h:

    struct FIAL_c_func_def {
    	char            *name;
    	int (*func) (int, struct FIAL_value **, struct FIAL_exec_env *, void *);
    	void             *ptr;
    };

This array should be terminated with an entry containing all NULLs.  The name
field is a null terminated string expressing the symbol that will identifier
each procedure.  The `ptr` field is a user pointer, this will be passed as the
final void * parameter to the function called as a procedure.  The func is a
function pointer, the type is the type that procedures will hold.  

This will be discussed in the section concerning procedures as a category of the
overall C API.  This is included here because it is a considered a loader
function, it occurs in the header loader.h, and it has parameters indicative of
a loader function. 

### Install Functions  

There are several functions that are marked install that install functionality
into an interpreter.  These are:

    int FIAL_install_constants (struct FIAL_interpreter *);
    int FIAL_install_std_omnis (struct FIAL_interpreter *);
    int FIAL_install_text_buf (struct FIAL_interpreter *interp);
    int FIAL_install_seq (struct FIAL_interpreter *interp);

Again, this interface could use improvement.  In particular, it would be an odd
interpreter that doesn't include support for sequences, since that is an
essential data type, that is supported with special initializer syntax.
Constants, as well, is something that is just hanging around because I haven't
gotten around to changing it yet, and it isn't quite broken, just inelegant.  

But, there is not much to say here, these functions should generally be called
on each and every interpreter, otherwise the usefulness of the FIAL language is
greatly reduced.  

# Procedures 

Now that we know how to create interpreters, and how to load them up with
things, it is time to concern ourselves with the second major part of the FIAL c
api -- procedures.  

Procedures, in FIAL parlance, are shortened to "procs".  This informal lingo
refers to both procedures defined in FIAL, and procedures defined in C, as they
are both accessed in an identical manner from FIAL.  In fact, there is really no
way to tell one from the other from within a FIAL script.  

However, outside of the FIAL script, we are of course dealing with different
things, and it will behoove us to keep straight three different concepts.  

1. FIAL procs, as a component of FIAL library.  As the interpreter is a tree
   walking interpreter, this is actually just a pointer to an ast node. 
2. C functions that fit a certain prototype, that can be exposed to FIAL as a
   procedure, by loading it into the interpreter as part of a library.  
3. a datum of the type `struct FIAL_proc`, which is a standalone proc object,
   i.e., one that contains references to the interpreter it needs to use, and
   also what library it is from.  The purpose of this object is to allow for
   efficient calling into FIAL from the embedding C program. 

The first type is not likely to be pertinent in the context of the FIAL C API
from the perspective of the application developer.  The second and third,
have rather different purposes -- the one is to provide FIAL scripts
with the useful things that you want them to do, the second is to invoke the
fial scripts, to get them to begin being interpreted in the first place.  While
the first would typically be called "extending," and the second "embedding",
with the supposed purposes of making these operations appear to serve different
ends, in the case of an embedded language it is simplest to think of them as
separate stages of the same process.  

The embedded app needs something interpreted in FIAL, so it invokes a FIAL
function.  Obviously, that function has to somehow further the goals of the app,
and so it needs some functionality of this app.  It accesses this functionality
via a proc, which happens to be one provided by the embedding app, which allows
it to do something useful for the app.  

Hopefully we should have everything laid out in a clear and obvious manner, and
can safely move on to discussing the particulars of the API for the second and
third types of procs.  

## Extending FIAL

### Prototypes

The structure that must be provided to create a FIAL C library is an array of
`struct FIAL_func_def`, which is defined in interp.h as:

    struct FIAL_c_func_def {
    	char            *name;
    	int (*func) (int, struct FIAL_value **, struct FIAL_exec_env *, void *);
    	void             *ptr;
    };

Name allows us to name our functions, and ptr allows us to set a user pointer.
When our function is called, this pointer will be provided as the final
parameter.  Our function takes the prototype of the func value shown here.  

A fuller prototype of a callback function can be given to ease discussion, this
is the function "seq_in", which is the function which places a value into a
sequence (the FIAL aggregate type).   

    static int seq_in (int argc, struct FIAL_value **argv,
                       struct FIAL_exec_env *env, void *ptr)

Hopefully this is a little clearer with argument names.  The argc represents the
number of arguments passed, argv will be NULL if argc is 0, otherwise it is a
pointer to an array of `struct FIAL_value` pointers, env is the execution
environment, and ptr is our user pointer.  

### FIAL Value Structure

While I am reluctant to place this here, as it breaks the flow of the
discussion, we can no longer get by without listing the ubiquitous "struct
FIAL_value", defined in "basic_types.h" --

    #ifndef FIAL_VALUE_USER_FIELDS
    #define FIAL_VALUE_USER_FIELDS
    #endif
    
    struct FIAL_value {
    	int type;
    	union {
    		int n;
    		float x;
    		FIAL_symbol sym;
    		struct FIAL_symbol_map  *map;
    		struct FIAL_value       *ref;
    		struct FIAL_ast_node   *node;
    		struct FIAL_c_func     *func;
    		struct FIAL_library     *lib;
    		struct FIAL_c_lib     *c_lib;
    		struct FIAL_text_buf   *text;
    		struct FIAL_seq         *seq;
    		char *str;
    		void *ptr;
    		FIAL_VALUE_USER_FIELDS
    	};
    };

As can be seen, this is a very simple structure.  It is just a type, followed by
an anonymous union, that has a value for every built in type.  Extra types can
be added via the C API via the `void *ptr` member.  Also, note, that the zlib
license of FIAL permits the user to simply add their own pointer types into the
structure, but that comes at the maintenance cost of not being able to update
FIAL versions without additional modification.  

The solution I have just added as I was writing this (and as such, it remains
untested), is to allow the user to add fields via a `FIAL_VALUE_USER_FIELDS`
define.  This must occur prior to including "basic_types.h", which will
typically be included simply by including the meta header FIAL.h.  It should be
noted, that there is the possibility of a future version of FIAL clobbering a
name of a user defined field.  This would cause the unpleasant alternative of
not updating, or of modifying the text of the field accessors.  This, in turn,
could be guarded against by using functions to access the user defined fields,
which could greatly reduce the unpleasantness involved where the field names to
get clobbered by a later release.  All of this is untested, however, as I just
noticed the problem while writing this documentation.   

It should also be noted that this structure makes little sense on 64 bit
machines -- there would be room for doubles instead of floats, for instance.  It
would also be possible to fit in an extra value, at least on architectures in
which the structures ends up being padded to be a full 16 bytes.  In short, this
layout is also not set in stone, and is only somewhat sensible on 32 bit
architectures ( and even there, the type could probably be safely shortened to
16 bits, allowing users to sneak in an extra value on user defined types.)

It would appear my humble little FIAL_value structure is in need of some
sophistication, but in any event, this is how it currently appears, and it
performs all tasks currently required of it.  

It should be noted that the values that go into "type" are automatically
generated.  They are listed in the source directory as "value\_defs.json", and
the generator is a short python script in utils.  There are a few reasons for
this, the pertinent one being that there can be a header "value\_defs\_short.h"
which forgoes the FIAL prefix, allowing, e.g., `VALUE_INT` instead of
`FIAL_VALUE_INT`.  Obviously, a library cannot pollute the global namespace this
way, so by having automatically generated headers, the FIAL.h can instead
include defines of the sort `FIAL_VALUE_INT`, while using `VALUE_INT` internally,
and allowing the user the option of using the shortened defines.  This is a
pertinent reason, since the following code uses the shortened defines, so the
point had to be clarified.   


### Sample C Function as FIAL proc

To continue our largely chosen at random function seq_in --

    static int seq_in (int argc, struct FIAL_value **argv,
    		   struct FIAL_exec_env *env, void *ptr)
    {
    	int i;
    	enum {SEQ = 0};
    
    	(void)ptr;
    	if(argc < 2 || argv[0]->type != VALUE_SEQ) {
    		env->error.code = ERROR_INVALID_ARGS;
    		env->error.static_msg =
    			"To put item in sequence, arg1 must be a sequence, it "
    			"must be\nfollowed by one or more items";
    		FIAL_set_error(env);
    		return -1;
    	}
    	assert(argv);
    	for(i = 0; i < (argc - 1); i++) {
    		if(FIAL_seq_in(argv[SEQ]->seq, argv[i+1]) < 0) {
    			env->error.code = ERROR_BAD_ALLOC;
    			env->error.static_msg = "Unable to add space to "
    				                "sequence for new value.";
    			FIAL_set_error(env);
    			return -1;
    		}
    	}
    	return 0;
    }

As has become idiomatic in my FIAL procs, I do type checking of the inputs
first, and wherever possible, I check them all in the first if clause.  This
allows the function to be easily scanned to see what parameters are required for
the FIAL proc.  

Also, what I would call "typical" usage at this stage, is to take advantage of
the variable length arguments to provide functionality -- here the ability to
add multiple items from a single FIAL call, by listing them as arguments.  I am
reluctant to use the term typical though, because other than a few demo apps,
the majority of code is of a nature pertaining to a language core, which is not
itself a typical usage area.  

Note, that the user does not have to manage memory.  The interpreter owns all of
the memory that is passed in.  The simplicity of this API is paid for dearly by
the FIAL scripter, who must toil without the benefit of references.  

It is probably worth taking a moment to familiarize oneself with the basic model
of FIAL programming, since the programmer is expected to carry its design out in
the extension procedures.  Thus, if you take a copy of the contents of a value,
you should either really copy it using `FIAL_copy_value`, or else you should move
it with `FIAL_move_value`.  If for some reason this is impractical, the "old
value" can just be memset to 0.  Furthermore, if a value is to be deleted, its
finalizer must be called, this is most simply accomplished with
`FIAL_clear_value`.   

The rules are fairly simple, but it is important that they be followed, since
otherwise things fall apart quickly.  Furthermore, after creating types, it will
become clear what is happening as the values are being moved around, and
overwritten.  Hopefully, by the time you read this, I would have already written
an overview, and you would have read it.  The basic overview will be given again
in the types section here. 

## Execution of FIAL Procs

This section is concerned with the functions that allow the embedding
application to interpret a FIAL procedure, which is how FIAL code must be run.   

As of now, typical usage of the FIAL proc is as an opaque structure, however,
they easily fit on the stack.  The generally do not need initialization, or
rather, they are generally initialized using the calls that return usable proc
structures.  I will maybe add create/destroy for them, they do not appear to
have this at the moment.

As can be seen, they are not particularly large structures, so arrays of them
should fit easily on the stack: 

     struct FIAL_proc {
             int type;
             struct FIAL_interpreter *interp;
             union  FIAL_lib_entry      *lib;
             struct FIAL_value          proc;
     };

As is stated, it would be somewhat advanced usage to have to deal with these
fields directly, simplest is to just use the functions which manage these things
using library and procedure names given as strings.  

If performance is a concern, then obviously it can be recommended to simply
cache the procedure object.  The lookups are not free in any event.  

### Set Proc Structure from Strings  

This is currently the "high level" way to set a proc structure to something that
can later be called with the run proc. 

    int FIAL_set_proc_from_strings(struct FIAL_proc *proc,
                                   const  char *lib_label,
                                   const  char *proc_name,
                                   struct FIAL_interpreter *interp);
    
The "proc" object is a pointer the proc structure that is to be set by the
function.  The "lib_label" is the string that is attached to the libraries as
their primary source of identification.  In the case of loaded FIAL scripts,
this will generally be the filename.  The proc name is a string representation
of the symbol which denotes the function, inside of the library.  The
interpreter is the one in which the library is loaded.  

Note that there is no corresponding function that allows the proc to be given by
symbol.  This is because symbols retain their meaning only in relation to
interpreters, so it is, as a general matter, simplest to look up the symbol at
the time that the procedure is created. 

If the symbol is all that is available, it can be lookup up in the interpreter,
for instance:

    FIAL_symbol sym = my_sim_value;
    struct FIAL_interpreter *interp = my_interpreter;
    struct FIAL_proc proc;

    FIAL_set_proc_from_strings(&proc, "my_lib", 
                               interp->symbols.symbols[sym],
                               interp);
    
Note, however, that the interp.h header should be examined, to ensure that the
data structures for the interpreter are still as they are when this document was
written.  

This API is still something of a work in progress, but the lack of a "lookup by
symbol value" is largely a conscious choice to avoid complexity.  What will
probably be added, however, is a helper that gets the constant string from the
interpreter.  This would provide the needed data abstraction.  

I am also not terribly fond of the name of this function, if this is the only
function to set a proc object (as is the current intention), its name could be
shortened.  

### Run Proc

This is where the magic happens.  After setting a proc object, it can be run at
any time using this function.  

    int FIAL_run_proc (struct FIAL_proc *proc,
                       struct FIAL_value *args,
                       struct FIAL_exec_env *env);
        
The proc object is a proc structure that has been set with
`FIAL_set_proc_from_strings`.  The env is an initialized environment.  Args is
an array of `struct FIAL_values` terminated with a value of type `END_OF_ARGS`.
If the user is using full value definitions, this will be
`FIAL_VALUE_END_OF_ARGS`, with the short version being `VALUE_END_OF_ARGS`.  

This returns an negative integer on error.  -2 would indicate that the arguments
could not be set (allocation error), and should be rather rare.  A -1 indicates
an error from the interpretation of the proc, env->error should contain error
information.  Note that environments passed back from errors retain their
stacks, this should be unwound to call finalizers and free memory.  

There is another function called "run_from_node" or some such, which is exposed
in the api.h header, but is really not meant to be used, and so it is skipped.
Clearly, some code cleanup is in order, though I suspect that particular
function is used internally, so examination would be required.  However, these
two functions are perfectly adequate for executing FIAL procedures, so I will
leave them to stand on their own, and move on to the type system.   

# Types

FIAL provides the C api with simple means to expose types.  The FIAL philosophy
on this matter is that "less is more" -- in other words, that the less FIAL
requires in the process of defining a type, the more freedom the extension
writer has in creating abstractions.  Thus, a lightweight API results in
freedom, this freedom results in power.  Less, in this instance, is more.  

## Properties

Types have 3 basic properties:

1. A type tag.  This is given arbitrarily at the time the type is registered,
and cannot be changed. 
2. A finalizer -- This is a function that is called when a value is destroyed.  
3. A copier -- something of a misnomer, since a copier need not actually copy.
It is nonetheless called via `omni.copy` in FIAL, and `FIAL_copy_value` from the
C API, and is how an type that wants to be copyable, becomes copyable.   

Perhaps it is worth mentioning that there are some things not considered to be
properties of types:

1. Constructors -- any instance of a user defined type will have to come either
   from the application in the form of a procedure argument, or else via a user
   defined procedure.  
2. movers -- FIAL retains the right to move a value, the only guarantee is that
   this operation does not result in multiple copies of the value without
   calling the copier. 

These three types will be briefly discussed.  Then the registration process will
be detailed.  Types are easy! 

### Type Tag

Types are differentiated from one another via a type tag.  A FIAL value has a
type field, which is set to the appropriate type tag.   

This tag is obtained when the type is registered.  FIAL does not provide any
mechanisms directly to help the C programmer keep track of this type, instead
the user pointer values from the proc def must be used.  

Let us consider the illustration code, then, to this process briefly.  I have
abstracted over the registration process, since that has not yet been covered.
This means that this code cannot be tested, so it should be considered
psuedo-code.  Also, note to self, switch to something other than markdown, this
is just silly.  How is this thing popular?

    char const * const my_lib_label="My_Lib"; 
    struct my_types {
            int type_1;
            int type_2;
    
    /* it is idiomatic in FIAL libraries to sneak all the symbols used by a
     * library into the type structure, after having forgotten that they will be
     * needed until after the structure has been named something involving
     * "types", and used throughout the entire library 
     */
    
            FIAL_symbol no_op;
            FIAL_symbol print_hi; 
    };
    
    static int my_proc (int argc, struct FIAL_value **argv,
                       struct FIAL_exec_env *env, void *ptr)
    {
    	struct my_types *mt = ptr;  /* C++ programmers will have to cast. */
    
        /* 
         * I personally think its clearest to type check the arguments inside of
         * the test of the first if clause, unless this is impossible (due to
         * variable length argument lists), or extremely messy.  
         *
         * In general, complex type checking logic is complex no matter where it
         * is, and by putting it at the top of the function it is obvious what
         * the logic is doing (i.e. checking arguments for type errors).  
         */
    
        if(argc == 0 ||
           (argv[0]->type != mt->type_1 &&
    	    argv[0]->type != mt->type_2 &&
            !(argv[0]->type == FIAL_VALUE_SYMBOL &&
              argv[0]->sym  == mt->no_op) &&
            !(argv[0]->type == FIAL_VALUE_SYMBOL &&
              argv[0]->sym  == mt->print_hi)) {
                    printf("What are you bothering me about?\n"
                           "Those aren't my types!\n");
                    return 0;
    	}
        if(argv[0]->type == FIAL_VALUE_SYMBOL) {
                if (argv[0]->sym == mt->no_op)
                        return 0;
                assert(argv[0]->sym == mt->print_hi); 
                printf("hi.\n");
        }
    	printf("Well, I should know what to do about this thing.  But I'm just"
    	       "sample code that\n doesn't do anything!  My cousin gets to"
    	       "compute astrophysics, and here I am, not\n doing anything, just"
    	       "held up as an example of something that correctly does\n"
    	       "absolutely nothing.  You know what that does to the self"
    	       "esteem?\n");
    	return 0;
    }
    
    int install_my_lib (struct FIAL_interpreter *interp)
    {
    	my_types *mt;
    	struct FIAL_c_func_def *my_f_defs = {
    		{"proc", my_proc, NULL},
    		{NULL, NULL, NULL}
    	}

        /* 
         * Allocate memory for our per interpreter library data.
         */ 

    	mt = calloc(sizeof(*mt), 1);
    	if(!mt)
    		return -1;
    	
    	/* 
         *  set the user data to be our types data
         */ 

    	my_f_defs[0].ptr = mt;
    
        /* 
         * Fill in our library data.
         */

    	/* assume these get the appropriate type tags.  See the section on
         * registration to find out how this is done. */
    
    	mt->type_1 = get_my_type1_tag(); 
    	mt->type_2 = get_my_type2_tag();
    
        /* don't forget that symbols can only be added while the interpreter is
         * in a load state.  */ 

        if ( FIAL_get_symbol(&mt->no_op, "no_op", interp) < 0 ||
             FIAL_get_symbol(&mt->print_hi, "print_hi", interp) < 0) {
                free(mt);
                return -1;
        }

        /* 
         * And, without further ado, add the lib....
         */ 

    	if (FIAL_load_c_lib(interp, my_lib_label, my_f_defs, NULL) < 0) {
                free(mt);
                return -1;
        }

    	return 0;
    }
    
    /* library must be uninstalled prior to deleting the interpreter, otherwise
     * my_types data will be leaked */
    
    void uninstall_my_lib (struct FIAL_interpreter *interp)
    {
    	union FIAL_lib_entry *lib;
    
    	if(!FIAL_load_lookup(interp, my_lib_label, &lib)) {
    		return;  /* bugging out, nothing we can do here */
    	}
    	/* ok, this is not as bad as it looks.  It is just freeing the void *ptr
    	 * associated with the first proc in the lib we just looked up.
    	 * 
    	 * For the sake of clarity, and NULL ptr checking at various points
    	 * along the wa, we could do this dereferencing in stages, but this is
    	 * not the point of the demonstration here, so I will leave it as is.  
    	 */
    
    	free(lib->c_lib.funcs->first->c_func->ptr);
    }

Note that the C application is responsible for uninstalling libraries, just as
it is responsible for installing them.  I have contemplated having a module
system of sorts, that would allow these things to be done automatically, perhaps
from a precompiled DLL, but no such system is immediately planned.   

I believe it is generally simplest to use one giant "god" structure per library
as the user data, as this object can be freed all at once, from any procedure in
the library.  This is not required, however, each procedure's user pointer is
held separately.  Furthermore, "simplest" is not always best, and sometimes it
may be necessary to use different data for different procedures, and then loop
over the procedures when uninstalling.  This could require bookkeeping.  Sigh,
programming can be such a chore!

### Finalizers

Finalizers are created from C functions that have a certain prototype, this
being :


    int func (struct FIAL_value *, struct FIAL_interpreter *, void *);

This hopefully does not present any suprises.  The value arguments is a pointer
to the value that is goint to be freed, the interpreter pointer is the a pointer
to the interpreter structure that the value is associated with, and the void
pointer is a user pointer, set at the time of type registration.  

This function is called whenever a value is deleted.  Obviously, I am not in
control of what people do inside of extension functions added to the language,
yet the idea is that whenever a value is overwritten the value currently on it
should be cleared.  Obviously, not all objects need be immutable, and the
library that implements the functionality of a type may prefer to modify it
instead of clearing it.  The application programmer is ultimately responsible
for what occurs within C functions attached to the FIAL interpreter as
procedures.  

But, inside the interpreter, any assignment to the value will cause it to be
overwritten, and the finalizer called.  Furthermore, falling out of scope causes
the finalizer to be called.  

As far as the basic library goes, moving onto the value, or copying onto the
value, will cause that value to be overwritten, and the finalizer will be
called.  This is true whether the object is on the stack, or whether it is in a
container.  

A structure which holds the finalizer and the user pointer is used during
registration, it appears in interp.h as:

    struct FIAL_finalizer {
    	int (*func)(struct FIAL_value *,
    		    struct FIAL_interpreter *,
    		    void   *ptr);
    	void *ptr;
    };

### Copiers

A copier is called whenever a copy is required.  This is usually the result of a
request from the scripter.  However, it can also be indirect -- copying a
container, will cause that containers copier to call the copiers of its members.  

It is roughly analogous to the case of the finalizer, except it has two values
as arguments.  The structure is as follows -- 

    struct FIAL_copier {
    	int (*func) (struct FIAL_value *to,
    		     struct FIAL_value *from,
    		     struct FIAL_interpreter *interp,
    		     void *ptr);
    	void *ptr;
    };

There are no requirements that this function actually copy the data, nor that it
leave the original unchanged.  This is nonetheless the purpose of this property,
any type which does not allow itself to be copied will be much more difficult to
use.

The copier can also return an error value, which is necessary if the copy cannot
be made due to an allocation error. 

## Registration

To register a type, use the `FIAL_register_type` function.  

  
    int FIAL_register_type(int *new_type,
                           struct FIAL_finalizer *fin,
                           struct FIAL_copier *cpy,
                           struct FIAL_interpreter *interp);

The value of the new type is recorded on the integer pointed to by `new_type`.
The finalizer and copier structures are passed via pointers as fin, and cpy,
repsectively, and the interp is the interpreter the types are installed into.  


