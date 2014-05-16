# Welcome to FIAL!

## Intro to the Tutorial

This is a tutorial for the FIAL.  FIAL is a language.  It is a fine, iterative,
algorithmic language. I created this language to be one that can be easily
embedded into another application, and without does restricting the design of
the embedding application.  To meet these goals, it is necessary to have a
language that can operates well in a multithreaded environment, without itself
imposing its own multithreading paradigm on the embedding application, or
imposing any system wide requirements, as these in turn would limit the design
possibilities of the embedding app.  

Like all design tasks worth undertaking, this one can easily be seen to be
impossible.  I believe, however, that the solution embodied by the language FIAL
has a large number of advantages, many of which stem from its simplicity.
However, we will not detail these advantages here, this is a tutorial, and not a
sales pitch.  

It should be noted, that this is not a particularly easy scripting language to
script in.  As far as ease of use goes, however, I think it satisfies the two
most important ease of use categories for an embedded scripting language: (a) it
is easy to learn, and (b) it is easy to master.  These qualities are
particularly important for an embedded scripting language as the majority of
people using it, will not be using it as their primary language.  

## FIAL is Still Immature

Also, while I do not feel the need to rely upon disclaimers for the quality of
the current language, as getting a simple language to even this modest level of
implementation has been no small task, I do feel the need to warn away people
that are expecting to find a fully mature, production quality implementation of
a stable language.  The language is, perhaps, somewhat approaching on the state
in which it will ultimate be, but the same cannot be said of helper libraries,
and samples.  The implementation is an altogether different story, it's data
structures are in need of an overhaul, and basic tree walking interpreters are
hardly state of the art in any event.  

In short, if the language meets your needs as it is now, it will likely meet
them in its final state, as it is not expected to change drastically moving
forward.  As to when a truly production quality implementation will be produced,
I cannot say for sure.  This one is just a simple tree walker, but the plan is
to improve this one to the point where it is something of a sane choice for
situations that do not require a heavily optomized implementation, and then to
consider what the best way forward is from that point.   

## This is a Tutorial

This is not a reference work.  As of now, the grammar of the language is
probably most easily determind by examining the parser, as it is written in
YACC, it should be easy to use that as rudimentary documentation of the grammar,
and the AST trees that come from it.  

Furthermore, it is meant to be read in order.  Sections are not necessarily self
contained, some of them rely on discussions that have already occurred.
Furthermore, some concepts must be introduced in part before they are discussed
in full, in order to properly discuss what came before.  

I suppose I should have started by writing a full reference, but I felt a
tutorial was also something I would like the project to have, and seemed
somewhat more fun to write.  

That being said, efforts are made to have some semblance of organization, and it
will be considered normal for people to consult the tutorial, as a form of
documentation.  However, this is not the place where I will try to document
every behavior, of every procedure, type, or linquistic construct.  

# First Look

While this is a language meant to be embedded into other apps, it nonetheless
has a command line interpreter, called FIAL.  We can use this to make a hello
world program:

    run
    {
        omni.print("Hello World!\n")
    }

It should be noted here that a hello world app does not really apply to the
FIAL language -- instead, this code produces a FIAL library, which has in it the
FIAL proc named "run".  It is just that the FIAL interpreter program runs this
program by default.  What procedures an embedding application runs, and when, is
up to that application, so little can be said about it here.

Nevertheless, I think this provides a reasonably good starting point to talk
about basics.  A FIAL input is a sequence of proc defs -- short for "procedure
definitions".  Since all FIAL code must be put inside of a proc def, I could not
get by without explaining them first, but it is a topic that cannot be
thoroughly explained without other concepts.  For our purposes, just know that
all the other code in a FIAL program must be inside of a proc def, and that we
will be placing all code inside of curly braces of `run { }`.

# Part 1: Variables, Values, Assignments, and Expressions

## Values

To be perfectly honest, this is the section of this tutorial I have been
thinking about, I quite simply do not know a good way to explain these concepts,
as they have become to me almost too basic to try to put into words.  What most
programming languages do at this point, is to describe these concepts in a
manner that is only sensible to people who already know how variables in
languages like theirs behave.  However, I am not entirely sure that is an
acceptible approach for this language, since here these concepts work slightly
differently than they do in many other languages.  

Let us begin, then, with the concept of computer memory.  Inside a computer,
there is memory, in which it stores information.  However, this information all
looks the same -- it is like if all the numbers in the telephone book where just
written as one giant number, and then in there maybe the scores to a ball game
from last week, and then somewhere else in there is the square root of pi to
200,000 digits, and next to that is a number representing the diameter of the
earth in meters.  In short, despite how it is some times portrayed computer
memory is not so much filled with numbers, as it is with numerals.  Thus, for a
computer program to do anything with these numerals, it must have some way of
keeping track of what these numerals are for, what they mean.  

A primary focus of FIAL is to abstract over computer memory, so that the
programmer does not have to deal with it while programming.  Instead, a FIAL
programmer deals with values.  However, I don't know how to put it simply, but
these values retain something akin to mass.  While we have abstracted away the
actually memory which is used to represent the values, we have not abstracted
away the fact that they are made up of _something_.  The FIAL value of the
integer 4 exists, somehow, inside of the variable that holds it, or inside the
sequence it is a part of.  It can be moved from one location to another, but
having it in both places isn't possible -- rather it would have to be copied.    

Secondly, we are not able to abstract away the notion that these values are
represented by memory, _somehow_.  But, since there is no notion of memory in
our language, we cannot attach the interpretation of it to the memory, so
instead we attach it to the values.  Instead of saying, for instance, "these
numerals are interpret as a phone number," instead we say "this phone number is
of type phone number."  Which of course, is a silly thing to say, but only
because we were using "this phone number" to refer to the object in the first
instance.  If instead we were saying "value X is a phone number", then our
saying approximates the use of types as attached to values, as opposed to
interpretations of numerals that exist in memory.  

## Variables

A variable in FIAL is not quite like a variable in mathematics.  Which is a
fortunate thing, because variables in mathematics are extremely elusive concepts
when you stop and think about them.  Rather, a variable in FIAL is simply a
place to put a value -- and it always has a value in it.  It is associated with
an identifier, and can be used in a FIAL script by using that identify. 

An identifier cannot be a keyword.  Fortunately, FIAL has very few keywords:

* if
* var
* leave
* redo
* and
* or
* not

### Declarations

A variable is declared using the "var" keyword.  The following is a statement:

    var my_var, my_other_var, my_favorite_var

Any number of variables can be used.  Note that statements lack punctuation.
indeed, statements in FIAL do not require any sort of punctuation.  This is a
small grace, granted for to the FIAL scriptor, as compensation for the
hardships she must endure, in comparison to scripters in languages the
implementations of whichmay take up an entire process and sprawl about as
they wish.  Whitespace is also ignored, except as needed to separate words from
one another, i.e. the above could just as well be written:

    var my_var var my_other_var var my_favorite_var

Except, of course, the first one is easier to read.  

Variables are initiated to the none value.

### Currently, No Duplicates!

The following is currently an error:

    var my_var
    var my_var  /* no good, variables can't be redeclared */

They can be declared in nested blocks.  I say "currently" because it seems to me
that it is something that I may end up allowing, in conjunction with the C API,
it could be a somewhat useful technique.  

### Identifiers

The name of a variable must be an identifier.  An identifier is a sequence of
characters, that begins with an underscore or a letter, and otherwise containes
letters, underscrores, and digits.  This is a rather common definition of
identifier, particularly in languages in which the '-' symbol cannot be used,
because it is used as an operator.  

    var ok, _ok, ok1, ok_2, ____ok4, _4ok, etc, _etc, __etc
    var CAPS_ARE_OK_TOO 

    /* The following will produce errors */
    var 1ok, !ok, ok-dokey, 54REALLY-NOT*()_OK, "im_not_an_identifier"


## Basic Values

### Simple Assigments

To examine values, it will help to be able to assign them to values.   However,
assignments is something of a topic of its own, so here we will only describe
one type of assignment:

    my_variable = EXPR 

An assignment is a statement.  

Similarly, expressions are their own topic.  We will only be dealing here only
with two types of expressions -- those that reference a variable, and those that
are comprised of the literals.  A literal is a representation of the value in
the source code.

Similar to an assignment, is the inclusion of an initializer in a variable
declaration.  In other words, the following is acceptible:

    var my_var = EXPR

### Integers

FIAL has an integer type.  Creating one is done by inputting numerals, preceded
by either + or -. 

    var my_int = 14, my_negative_int = -25

Strictly speaking, the literal is only the positive portion of the number, with
negation being an expression, but this is a distinction with little practical
difference. 

### Floats

FIAL currently has limited capacity to express floats, it can only be done using
actual numbers -- i.e., no exponential support.  However, for most purposes this
is quite ok.  It is done using 0 or more digits, followed by a period, followed
by 1 or more digits.  For example:

    var pi = 3.14159, neg_2_point_4 = -2.4 
 
### Strings

Strings are created by enclosing the desired string in quotation marks.
Strings, like other basic types, are immutable.  

    var my_string = "I am a string!"

Strings can contain escape characters.  To be perfectly honest, I cut and pasted
the escape characters into the lexer, which is where these things are handled
internally.  They all do what their C equivalents do, but I am not entirely sure
what that is in all cases (I lost my copy of K&R a few years ago, though I am
pretty sure it is around here somewhere.) -- 

1. \n -- newline
2. \t -- tab
3. \" -- quote character
4. \r -- carriage return?  idk, never use this one
5. \b -- no idea.  beep maybe?
6. \f -- well.  I've got a guess.

### Symbols

Ok, this one is a tad unusual.  Symbols follow the same rules as identifiers.
In many cases, symbols are used to reference what would otherwise be used as an
identifier in the text of a FIAL script.  But, symbols are values, whereas
identifiers in the text are syntax elements.  Their literals are a string of
text which would be a legal identifier, preceded by the '$' character (it looks
like an S, for "symbol".  This is, unfortunately, something of the opposite
usage it has in other languages.)

    var my_sym = $symbol

Symbols are used in symbol maps, which are discussed below, and also can be used
for purposes that attach semantics to identifiers -- for instance, the load
operator takes a symbol as an argument, the library thus loaded is referenced
using the identifier corresponding to the symbol that was passed to it.
However, the scripter can also use them, just as convenient values.  A symbol
always equates as equal to another symbol with the same textual representation,
and equates differently from those with different representations.  

The usefulness of this is hard to demonstrate without flow control, which is in
the next part of the tutorial. 

## Expressions

It should be noted, that expressions are not really a conceptually elegant fit
into the FIAL programming language.  Rather, they are included, because they are
extremely convenient.  FIAL's expressions, somewhat as a result of them being
somewhat at odds with the overall scheme of the language, are considerably less
powerful than they are in some other langauges.  However, expressions are
limited in where they can appear -- they cannot serve on their own, as
statements, as they can in some other languauages.  Rather, they can only appear
on the right side of the '=' mark in an assignment, or else as the test in an
'if'.  

Furthermore, not all types can be used in expressions, and some of those that
can, can essentially only be used to test equality.  Values that can be used in
expression, without immediately causing the entire expression to evaluate a
value of an "invalid expression" type, can be classified as "expressible."
Expressable types are: none, error, int, float, symbol, string, and type.  

### Literals

Literals can be used in expressions, and in fact, all of the right hand side of
the assignment statements given in the previous sections concerning the various
basic types are expressions.  

### Variables

Identifiers can be used in expressions to represent values stored in variables.
If the value is unbound, an error occurs, and interpretation will halt.  If the
value is bound, and it is bound to an expressable type, then the value of the
identifier forms an expression.  If the value is not expressable, then the value
of the expression will be of type "invalid expression".  

Expressions include C style arithmetic operators in the following precedence
(highest to lowerst):

1. Negation
2. \* /
3. \+ -
4. == < >

These are followed in precedence by the logic operators, for which we use their
english words --

* and or not

Boolean logic currently consists of non-negative integers evaluating as true,
with all other values evaluating to false.  I think I am going to make a type
"true", and make values of that type evaluate as true.  So, this is one area
that will likely change.  

Additionally, an expression can be included in parentheticals, this is the way
to force groupings.  

A few examples should suffice -- 

    var fun  = "I am a string"
    fun      = (2 + 3) * 5
    var bool = not ( (2 * 6) == (6 + 6) )
    bool     = bool and bool

Note, that equality checking on strings is currently broken -- it will return
true or false depending on whether the strings stem from the same literal. I
do not think this is proper behavior, but I am not sure what I want this to be
like.  So for now, just know that comparisons on strings are essentially broken,
and really should be avoided, unless you know what you are doing.  

## Special Compound Types

Unlike the basic types in the previous section, these types are inexpressable,
and they are also mutable.   In general, compound types are no different from
other types defined via the C api, they are more or less treated as opaque
values, that the FIAL scripter moves to and fro, using provided procs to
manipulate them when appropriate.  

However, two such types are special, in that FIAL provides syntax to support
their use.  These are the symbol map, and the sequence.  

### Symbol Maps

The first, the symbol map, is used to tie together values into a compound value.
The component values are mapped to symbols.  In other words, each component
value of a map, is associated with precisely one symbol.  

#### Map Accessor Syntax

This is another area where practicality has taken precedence over simplicity of
the language, but the practical need to have a compound data structure with
easily accessed members is real, in my opinion.  Symbol maps provide this in
FIAL. 

Symbol maps component values can, in simple cases, be accessed using map
accessor syntax.  This consists of the variable, followed by a dot, followed by
an identifier.  It can be used either in an expression, or as the left hand
side, as an assigned to value.  

    var my_map;
    map.create(my_map) /* creates a map, we haven't discussed this yet. */
    my_map.symbol = "I'm a value that will be associated to $symbol"
    my_map.another_symbol = "I will be associated to $another symbol" 
    my_map.strange_bool = not my_map.symbol
    
Variables that are not maps cannot be accessed in this manner, i.e. there is no
"automapification" or anything of that sort.  Again, this is something I have
contemplated adding, it certainly seems like something that could be convenient,
the worry I have is that its use could mask errors, producing bugs. 

Also note that nested lists can be accessed.  We do not here have the tools to
make nested lists yet, so just assume that my_nested_list below is a nested
list:

    my_nested_list.tier1.tier2.tier3 = another.nested.list

### Sequences

Sequences do not share the anything comparable to map accessors as of now.  I
have considered the idea, but in general, I believe these are best used as
stacks or queues, with the random access functionality just kind of an extra
feature.  Therefore I have not found it necessary to provide easy accessors for
them.  

As seen below, however, they do have initializers.

### Initializers

These two forms of compound data can be created using initializers.
Initializers can be used instead of an expression as the right hand side of an
assignment.  Additionally, they can be created for procedure parameters, which
will be discussed in the section on procedures.  

The following represents the syntax for creating compound types out of
expressions:

    var my_new_map = { field1 = 1, 
                       field2 = "I'm a value in field 2", 
                       field3 = $field3
                       field4 = "field3 is totally meta\n" }

    var my_seq = { $first, "second", 3, 4.0, "and I'm a string"}

Nesting is possible using this basic form.  
  
    var map = { nested_map1 = {nested_array = {$first, $second, $third}},
                nested_map2 = {nested_map   = {$hello = "hello!"}} 

#### Move Operator

In initializers (and procedure paramters), you are allowed to use move
operators, in order to move a value from its original location into the list.
The original location is determined via a variable, or a map accessor.  It is
denoted with a colon.  

    var move_me = $value, and_me = $value 
    var map = {$moved_to : move_me, $moved_to_also : and_me}

    /* move_me and add_me are now set to none
    move_me = $value and_me = $value  /* new lines aren't needed */
    var seq = {:move_me, :and_me}

#### Copy Operator

This is used just as the move operator is, accept instead of moving the value,
it is copied.  Note that copying involves calling the copier of a type, and in
the case of an application extension, that type's copier is not guaranteed to
produce behavior that actually copies the value.  Consult the documentation for
the type for types that are not a part of the base FIAL language.  

The copy operator consists of a double-colon, "::".  These must be next to one
another. 

    var copy_me = $value, and_me = $value
    var map = {copied_to :: value, copied_to_also :: and_me}
    var seq = {::copy_me, ::and_me}

# Part II - Flow Control

The flow control system of FIAL uses very few constructs.  Additionally, its
flow control construct provide ways to work with variable scoping, so it's like
a bonus.   

## Blocks and Named Blocks

A block is a series of statements surrounded by curly brackets, "{statements}".
In FIAL, all code other than proc defs occur inside of blocks, because the proc
def is a block.  And while it is not syntactically a named block, the block that
is attached to a proc is in function the same as a block with that name.
Otherwise, blocks do not need names, the name can simply be omitted.  A block
is named with an identifier, followed with a colon, immediately following the
opening curly brace.  

    /* anonymous block */
    {
        /* block named "my_block" */ 
        { my_block :
        
        }
    }        

In addition to flow control, blocks serve the purpose of determining the scope
of variables. A variable declared within a block, is only available within that
block.  

    var some_var = 10, another_var = 20, tmp
    { block_1 :
        var block_1_var = 30
        { 
            var some_var
     
            some_var = 40
            another_var = 50
            block_1_var = 60
        }
        tmp = some_var    /* 10, was covered in the anonymous block */
        tmp = another_var /* 50 */
        tmp = block_1_var /* 60 */
         
    }
    tmp = some_var /* 10 */
    tmp = another_var /* 50 */
    tmp = block_1_var /* ERROR!  block_1_var is unbound outside of block_1 }

When a variable value is overwritten, or falls out of scope, it may call a
finalizer on the value.  Application specific types may have expectations of
when these finalizers are called, consult the documentation for the extension
types to see appropriate usage patterns for types with those finalizers.  

## If Statements

Like many languages, FIAL utilizes if statements for flow control.  Indeed,
languages without an if statement, while possible, must nonetheless provide some
sort of branching in order to be turing complete.  The if statement represents
the most intuitive branching mechanism.   In fial it works simply -- it is an
"if" keyword, followed by an expression, followed by a block.  The block will
only be executed if the expression evaluates as true, currently a non zero
integer is considered true, all other values are false.   

    var test, v1 = 0, v2 = 0
    set_test (test)  /* assume this set the value of test */
    if test {
        v1 = 1
    } 
    if (v1) {  /* parenthesies are "optional" -- or, more precisely, 
                  an expression in parenthesies is also an expression */
       v2 = 1 
    }

## Leave and Redo

Leave and redo are discussed together, since they have nearly identical usage.
Leave is used to exit out of a block, redo is used to start a block over from
the beginning.  They must always appear as the last statement in a block, and
leave must always be labelled.  Redo can omit the label, in which case it
repeats the nearest block.  

    { block :
        leave block   /* legal, but useless, since the closing brace also leaves
                       * the block. */
    }
    { block :
        { leave block }
        var v1    /* this code never runs, the block is exited with leave */
        v1 = 10
    }
    { inifinite_loop :
        redo
    }
    var i = 0, count = 0
    { inf_loop :
        {
            i = i + 1
            redo inf_loop
        }
    } 

The requirements that leave and redo be at the end of a block is due to the fact
that code in the same block as them, will never be run.  As can be seen by the
second example above, satisfying this requirement does not guarantee that you
have not written code that can never be run.  Since an anonymous leave is
equivalent to a closing brace, it is disallowed, again, since it is difficult to
imagine a scenario in which it was used that way without it being a mistake.   

## Usage

These are currently the only control flow structures in FIAL.  I quite like this
simplicity.  At this point, I do not see myself adding additional control
structures, having found these to be generally adequate.  

This section demonstrates the use of these control structures to achieve similar
results as popular control structures in other languages.  This is by no means
intended bo be encyclopediac.  Indeed, I hope that the simplicity of the flow
control, combined with the simple type system, allow application programmers to
create effective idioms for a myriad of disparate areas, with a modest amount of
effort.    

Note that this is placed here because it is anticipated that some readers will
be curious about how constructs they are already familiar with would be done
using the somewhat different FIAL control flow mechanisms.  They are not here to
intimidate new comers, who should feel free to look over them to see if they
find anything of interest, but should feel free to skip over constructs that
they do not understand, as this documentation does not provide much in the way
of explanation, and agonizing over them as they are presented here will hardly
be fruitful.

### If, Then, Else

Branching in FIAL is achieved through teh combination of if, leave, and named
blocks.  Consider the folowing psuedo-code:

    if TEST then ACTION, else ANOTHER_ACTION 

This is done in FIAL in the following manner:

    /* note, this name is arbitrary */
    { else :
        if TEST {
            ACTION
            leave else
        }
        ANOTHER_ACTION
    }

This is perhaps less elegant than the original, but it does not suffer from the
"dangling else" problem, and thus any number of else, or "else ifs", can be
combined in a natural manner:

    var some_var, another_var
    set_(some_var) /* assumes this sets to an dependant value */
    { else :
        if some_var == $first {
           DO_FIRST_THING
           leave else
        }
        if some_var < 3 {
            if another_var > 4 {
                DO_SECOND_THING
                leave else
            }
            DO_THIRD_THING
            leave else
        }
        DO_ELSE_CLAUSE_THING
    }

In short, I feel that this structure is quite ok at handling if - then - else
constructs.  It is perhaps somewhat cruder than having a syntactical else
construct, however, in the case of complex constructs, I believe it works just
as well.  While the majority of time might be spend on simple cases, the
majority of difficulty, and thus frustration, flows from the complex ones.  

### Loops

Loops are of course performed utilizing the redo statement.  An example of a a
loop that runs 10 times:

    var i = 1, count = 10
    { loop :
        DO_SOMETHING        
        if ( i < count) {
            i = i + 1
            redo loop
        }
    }

Some might find it preferable to place the termination conditional at the top of
the loop, as this way one won't have to scroll the screen to the bottom of the
loop to see the termination clause.  

    var i = 0, count = 10
    { loop :
    	i = i + 1
        if(i == count) {
            leave loop
        }
        DO_STUFF
        redo
    } 

### Switches

Switches, as of now, are meant to be dealt with as a large sequence of if else
clauses.  If and when I create a compiler for FIAL, I may consider adding in a
control flow statement that is intended to compile to a jump table.  But this
will require a fair amount of consideration.  

But as of now, a sequence of if statements serves as FIAL's switch statement:

    var v
    set_v(v)  /* sets v to a run time value */
    { switch :
        if v == 0 {
           DO_0	
           leave switch
        }
        if v == 1 {
           DO_1
           leave switch
        }
        if v == 2 {
           DO_2
           leave switch
        }
        if v == 3 {
           DO_3
           leave switch
        }
        DO_DEFAULT
    }    

### For Loops

The C style flexible for loop is similar to the general loop given above, this
section is concerning the loop type which is meant to operate on all of the
members of a container.

In FIAL, this is not a special case, but it is of course common.  Unfortunately,
it has as much to do with the proc interface for the sequence type as it does
with flow control, but it is given here nonetheless.  Two versions are given --
the first is a little simpler, but is sub optimal.

    var seq, cpy
    init_seq (seq) 
    omni.copy(cpy, seq)

    { for : 
        var item, size
        seq.size(size, cpy)  
        if size  == 0 { leave for }
        
        seq.first (item, cpy)
        DO_SOMETHING (item)
        redo
    }
    
This is more efficient, but as can be seen, is slightly more complicated:

    var size, i = 1, item
    var seq, out

    init_seq(seq)
    seq.size(size, seq)
    seq.create(out)

    { for :
        if i == size + 1 {
            leave for
        }
        i = i + 1
        seq.first(item, seq)
        DO_SOMETHING(item)
        seq.in(out, item)
        redo
    }

    omni.move (seq, out) /* put it back, good as new  */

Or, again, just a C style for loop is quite ok...

    var size, i = 0, item
    var seq
    
    init_seq (seq)
    seq.size(size, seq)
    
    { for : 
        if i == size {
           leave for
        }
        i = i + 1
        seq.take(item, seq, i)
        DO_SOMETHING(item)
        seq.put(seq, i, item)  /* put it back where we got it */
        redo
    }
         

In general, this is a bit more awkward than it is in other languages, obviously,
in many cases, the actually looping code won't be very bothersome, because there
will be a lot of code in the loop body, making the loop code feel insignficant.
However, if a lot of loops of this sort had to be written back to back, well,
that could get unpleasant.  Also, note, that people used to other languages
should consider avoiding the use of "for" as a label, as I have found that makes
it more likely that the ending "redo" is accidently omitted.  

As to the choices between them, I think the first one is fine in most cases,
though it would rarely be "best".  If the type has very expensive copies,
however, there is no choice but to go with a solution like the second, or third.
The third really is the most efficient, since it doesn't require the creation of
a second sequence, but, again, for most purposes that is not terribly expensive.  

More idiomatic in FIAL, however, is to simply right the loop as a destructive
one, put that loop as a proc, and then use a copy operator in the argument 
in the event that the original needs to be kept.  However, this is roughly
equivalent to the first option when the argument is copied in, so the same
considerations apply.  

### Looping Over Nested Switches

Ladies and gentlement, when they portray hackers on tv, and show their screens,
they are more often than not writing a simple script.  Sometimes, they are
writing a switch statement.  Sometimes, they are even looping over a switch
statement.  But never, and I mean never, do you see them looping over nested
switch statements.  

Well, new FIAL scripter, there is a reason for this.  That's because the best
hackers ever, even if they hack using superhuman abilities, quite simply have a
fair bit of work to do before they _get on our level_.  In fact, as is well
known, the entire purpose of flow control abstraction is to provide people who
can't handle looped over, nested switch statements with some psychological
solace for their inadequacies.  But I digress.  

The point is, that FIAL provides an elegant way to deal with complex looping
over complex branching, since the combination of a basic branch operator (if)
and leave/redo with named blocks is adequate, and that is all that there is in
the language.  It's almost like this was a Fine, Iterative, Algorithmic Language
or something!

Ex:

    var state = $initial
    var map, seq
    { sm_top :
        { sm_switch :
            if state == $initial {
                set_map(map)
                state = $start
                leave sm_switch
            }
            if state == $start {
                { type_check : 
                    if map.type == $awesome {
                        state = $confusion
                        leave type_check
                    }
                    if map.type == $bodacious {
                        state = $delirium 
                        leave type_check
                    }
                    if map.type == $sweet {
                        state = $euphoric
                        leave type_check
                    }
                    state = $normalcy
                } 
                leave sm_switch
           }
           if state == $next {
               var none
               omni.move (map, :map.next)
               if map == none { leave sm_top} /* DONE */
               state = $start
               leave sm_switch
           }   
           if state == $awesome {
                var size, i
                omni.move(seq, :map.awesome)
                seq.size(size, seq)
                i = 0
                { for :
                    if i == size { leave for } 
                    i = i + 1
                    
                    var item  seq.firt(item, seq)
                    if item == awesome {
                        we_are_awesome
                        leave sm_top   /* DONE */
                    }
                    redo
                }    
                state = $next
                leave sm_switch
           }    
           if state == $bodacious { DO_BODACIOUS }  /* this could be actual
                                                       code, see section on
                                                       procs. */
           if state == $sweet { DO_SWEET } 
           if state == $normalcy {
               /* normally, nothing matters, unless its a pink slip */
               if (map.you_are_fired) { leave sm_top } /* DONE */
               state = $next
               leave sm_switch
           }
        }
        redo sm_top /* having one redo per label helps keep things a bit
                       saner, IMHO, but of course now "leave sm_switch: is 
                       equivalent to "redo sm_switch".  This gives you twice as
                       many chances to write working code!  */
     }

Obviously, this code does not have to be written in this fashion, it is just
that code that really requires looping over complex branching is not the sort of
code that can be read in a language tutorial, and followed along by people
completely unfamiliar with the project, who are just learning the language.  So
some nonsense code had to be written in order to demonstrate complex use of the
language faculties.  

### Exceptions

Exceptions type control structures can be created in FIAL, however, they take
advantage of dynamic scoping, which hasn't been discussed yet.  Furthermore,
they generate a large amount of nesting, so indenting a new level for each
nested block is not recommended.  The following simple uses normal indentation
for clarity's sake, however.  

A full blown exception framework is probably possible in FIAL, with some
caveats, but in general should not be needed, since leave and redo can be used
for non-local exits, and dynamic scoping for passing values up the block stack.  

    proc_that_throws
    {
        exception_value = {type = $my_exception_type, value = $vale}
        leave exception_label  /* leaves travel up the stack until they're
                                  matched */
    }

    run
    { 
        var exception_value
        /* note the double block openers. */ 
        { exception_catch :
            { exception_label : 
                { do_stuff :
                    {exception_catch :
                        {exception_label : 
                            { do_more_stuff :
                                proc_that_throws
                            }
                            leave exception_catch  /* end of try block, no exception
                                                    * thrown */
                        }
                        if exception_value.type == $some_type {
                            omni.print($some_type)
                            leave exception_catch    /* exception handled */
                        }
                        /* catch (...) */
                        omni.print("non $some_type exception, rethrowing")
                        leave exception_label
                    }
                }
                leave exception_catch  /* end of try block, no exception thrown */
            }
            if exception_value.type == $some_other_type {
                omni.print("its another type")
                leave exception_catch
            } 
            /* catch (...) */
            leave exception_label /* this will exit program, if there is no label
                                   * above us */ 
        }
    }                


# Part III - Procedures

Procedures in FIAL parlance are called procs.  Well, I call them procs, which at
the time of this writing means everybody who uses FIAL calls them procs.  Ergo,
procedures are called procs.  

All FIAL code must appear within a procedure definiton, or a proc def.  Proc
defs, however, must be on the top level of a FIAL file -- no nesting is allowed.
A proc def consists of an identifier, followed by an optional arglist contained
in parentheticals, followed by a body contained in {}

## Proc Defs

Proc defs are simple, they consist of an identifier representing the proc name,
followed a parenthitically enclosed sequence of comma separated identifiers
representing args, followed by a nameless block.  

Ex:

    my_proc (parameter_1, parameter_2, parameter_3)
    {
        /* statements .... */
    }

    my_command     /* I sometimes refer to procs without args as commands.  
                      The parenthesies can be omitted.  */
    {
        /* statements .... */ 
    }

I will at some point have syntax for allowing for an arbitrary number of
arguments.  I have been chewing on a particular solution to this for a while,
and while I am apparantly not thrilled with it, it is time to put something in.
So there will be variable argument functionality soon.  

## Proc Calls

Proc calls are made with the name of the proc, followed by an optional
parenthetically enclosed list of arguments. 

    my_proc ( arg1, arg2, arg3 )
    my_proc

All procedure arguments are always optional -- omitted arguments are passed the
none value.  

## Arguments

Arguments to lists can either be variables, expressions, initializers, or
move/copy operaters.  

### Variables

Variables passed as proc arguments are passed by reference -- however, the
reference occurs transparently to the FIAL programmer, in essence the reference
is used as if it is the original value.  The referred to value is always a
variable above the called procedure on the block stack.

As procs do _not_ have a return value, pass by reference is utilized to serve
this capacity.  As a convention, I place parameters indicating the return value
first.  

     add (return, left, right) {arg = left + right}
     run 
     {
         var ret, arg1 = 10, arg2 = 20
         add (ret, arg1, arg2)
     }

### Expressions

There is nothing here really to note, except that expressions can be passed as
arguments.  A value for them is created on the block of the called procedure, it
is destroyed when that block is popped off.   Expressions involving a single
identifier will be interpreted as passing the variable by reference, the
intended workaround is to place the identifier in parentheticals to force it to
be evaluated as an expression.  

    set_args (arg1, arg2)
    {
        arg1 = 3, arg2 = 4
    }
    run 
    {
        var arg1 = 1, arg2 = 2
        set_args(arg1, (arg2))
        if not (arg1 == 3 and arg2 == 2) {
            omni.print("Well that's wierd")
            leave else
        }
        omni.print ("arg1:", arg1, "arg2:", arg2)            
    }

### Initializers

Initializers can be passed as arguments. 

    print_stuff (map, seq)
    {
        var print_me = map.print_me
        omni.print(print_me)

        var cpy
        omni.copy(cpy, seq)

        {for :
             var item, size 
             seq.size(size, cpy) if(size == 0) {leave for}

             seq.first(item) omni.print(item)

             redo
        }
    }

    run
    {
        print_stuff({print_me = "I'm map.print_me"}, 
                    {"first", "second", "third", "pumpkin!", "fifth"})
    }

### Move/Copy

Move and copy operators are available in procedure arguments.  They function in
the same manner as they do with sequence initializers, they are placed in front
of the identifier or map accessor.  

    some_proc( :move_this_var, ::copy_me, :move.map.accessor,
                ::copy.map.accessor )

## Dynamic Scoping

Since FIAL does not have a global namespace, since that is not possible to make
thread safe,  everything would have to be passed along as arguments to functions,
where  FIAL not dynamically scoped.  As it stands, FIAL _is_ dynamically scoped.
Dynamic scoping essentially means that values are bound at run time, and can
bind to variables from the calling function.  

Consider the following

    some_command
    {
        some_var = some_var + 1
    }

    run
    {
        var some_var = 1
        some_command 
        omni.print(some_var)
    }
 
This will print 2.  It remains to be seen if I am going to keep dynamic scoping,
it should be considered on probation.  It does provide some useful properties.
Any block can be taken out of code and moved into a function, without
modification.  This provides a programmer friendly way to limit the complexity
that is present in functions.  

Additionally, dynamic scoping allows for exceptions to be provided for, without
there being any complication to our spartan control flow model. 

However, dynamic scoping presents two related, but distinct, programming
difficulties, that are currently somewhat unsolved.  The first is that a
mispelled variable, could during runtime bind to an unintended variable up the
block stack, causing an unexpected and potentially difficult to debug
modification to a nonlocal variable.  Unfortunately, there is currently no
solution to this problem.  However, the general strategy will be to have
analysis tools, that can list where a variable is bound by examining the code.
Unfortunately, this tool has not been begun, and there are no immediate plans to
add it.  

The second is that an application may wish to allow the end user to write FIAL
scripts, and to be able to run them, without risking variables in its own
scripts from being overwritten.  This problem can be solved by masking all
variables.  This can currently be done manually:

    run
    {
        var my_var = 1, hands_off = 2
        var return 
        { 
            var my_var, hands_off /* these cover the variables above */ 
            user_lib.user_proc(return)
        }
    }

Naturally, this becomes impractical in real scripts, that may have at all
various levels above a function call, any number of variables, which are likely
to change as the code is maintained.  A library function called "mask" or
something of the sort will likely be provided, that can be used to call such
functions.  

Furthermore, if this functionality is currently needed, an application
programmer may expose a proc that utilizes a separate execution environment to
run the script in.  This may still end up as the official solution to this
problem, however, for some reason that currently escapes me, I had decided to go
with a masking approach instead of utilizing a separate execution environment.
Hopefully I made a note of this somewhere.  

## Procs are Blocks

I have come accustomed to calling the "call stack" the "block stack", since in
FIAL, blocks have properties reserved for functions in other languages, and
procs are essentially just blocks after they have begun executing (obviously,
the parameters are a special case.)  Thus, a "return" type statement, is
accomplished just by leaving a block with the procs name.  

    my_proc
    {
        if 1 {
            leave my_proc
        }
    }

Just as variables are dynamicly scoped, so too can leave statements exit out of
blocks in the calling blocks --

    some_proc 
    {
        leave some_block
    }

    run
    {
        { some_block : 
            { another_block :
                some_proc
                omni.print ("I should not be printing")
            }
            omni.print("Me neither!")
        }
    }

# Part IV -- Libraries

Naturally, you can't just leave your procs all laying about, not being able to
group them in any manner whatsoever.  I mean, this isn't C you know.  Libraries
in FIAL are used with an identifier that is the same as the symbol which was
used when loading the library, followed by a dot (a "."), followed by the proc
name.  

This notation should be familiar, it is used throughout.  All libraries that are
not defined within the same source file must be called using library call
notation.  

## Loading

Libraries can be loaded by a FIAL script.  When a library is loaded the first
time, the proc named "load" is run.  Note that to some extent this is just a
convention -- an embedding application does not have to do this, on applications
that it loads directly.  Currently, any procedure loaded from a FIAL script will
run the load proc.  Furthermore, the command line interpreter will run the load
proc.

### Load Proc

Not all libraries have to be loaded.  This is necessary because of the current
scheme, which forces all non local procedures to be accessed using library
notation.  Since the loader is a procedure, it must exist within a proc.
Obviously, the answer is to have one library available everywhere.  

I call the library that must be available everywhere "omni".  As it currently
stands, maps and globals are also "omnis", in that they too can be used from
anywhere.  However, sequences and text buffers (the type that is used when
strings need to be modified), have to be loaded manually in the load function.
Clearly, this inconsistency is not good.  I think the "better" approach is
requiring the load for all libraries except the one named "omni", but I am not
entirely sold on this yet, so for now things are staying the way they are.    

But I digress.  Libraries are loaded with omni.load.  The first argument is a
symbol, which will be how that library is accessed within the string.  The
second argument is the "label" of the library, a text string that is used to
denote it.  Currently, this looks through the all loaded libraries, and then if
it is not found, it looks for a file of that name in the filesystem.  If one is
found, then it loads that library, by reading it as a FIAL script. 

    load
    {
        omni.load ($seq, "sequence")  /* sequences must be loaded manually */
        omni.load ($point, "point.fial") /* loads the filename */
        
        global.set($id, "value")  /* can set globals */
    }

    run
    {

        var p
        point.create(p) /* access the point library with $point */

        var v
        global.lookup(v, $id)  /* can lookup globals */

        var seq, map
        seq.create(seq) /* use seq to access sequence. */

        omni.print("print still is accessed via omni.print")
        map.create(map) /* map is always available */
    }
    
### Load Interpreter State

Generally, load procs can only be loaded from a "load" state.  As of now, when
the interpreter is in load state, all functionality is available.  This includes
the ability to run procs, load libraries, set globals, etc.  

However, it is typical for an application to then set the interpreter to "run"
state.  In run state, certain things are disallowed, due to the fact that
certain parts of the interpreter must be immutable, since FIAL is expected to be
able to be run in multiple threads.  This includes loading libraries, and
setting globals.  

In general, only use things that are marked in the documentation as "load only",
from within a load proc, or a proc that is called only by the load proc.  

### Load Dependencies

It is generally best to have libraries that do not need to run procs from other
libraries during loading.  However, this is not required, and the execution
model is easily explained.  Once the script of a file is parsed, its label will
be attached, and it will be made available as a loaded proc.  Then, its load
proc will be run.  If the load proc loads a library, that library will be looked
up, and if found, it will simply be attached to the symbol, without parsing it,
or running its load proc. This way, libraries can be mutually dependant without
causing an infinite loop.  

Note that there is nothing to prevent a sort of "dependency hell," where the
loading libraries become so dependant upon one another, and the exact order of
the load calls in relation to the inter library calls.  Remember, that in
general, a "load" proc should just be responsible for loading the library, and
in general, calls to procs in other libraries should be avoided, since they may
be in an incomplete load state -- its load function may have called yours, and
have more loading to do before the proc that you want to call will properly
execute.  

## Omni Lib

The "omni" lib is everywhere, hence there term "omni."  It has a number of 
procs that are necessary.  It is not currently in its final form, rather still
in a testing stage.  

### print
    print (item1, item2, item3, etc......)

This function will probably have to be reworked at some point.  It is the 
print function, it will print a reasonable representation of built in types, 
and the type number of other types.  The main difficulty with a "print" type
library function is that in an embedded setting, a script might not want to 
do a "printf" to stdout, which is what this function does.  On Windows, in 
particular, this is usually a no op.  

So in the future, this will probably be application defined, or omitted in 
favor of leaving the application to deal wtih.  The interpreter would then
have an implementation in its standalone form.  

Though, back to documenting, this proc prints the items out on the same line,
and inserts a newline afterwards.

### move

    omni.move(dest, source)

omni.move is used to move a value from a location into a variable.  
Note, than in tandem with the copy operator, it is also the simplest way to
copy using a map accessor.  

    omni.move (dest, ::map.accessor)

In this instance, omni.copy would create and then finalize a temporary copy,
which is needlessly inefficient.  

### copy

    copy (copy, original)

This is used to create a copy.  It is in some ways superfluous, as it is less
useful than the corresponding move command with a copy operator, yet it is 
included nonetheless.  However, the move proc is needed to deal with map
accessors.  I.e.

    /* equivalent */
    omni.copy (new, orig)
    omni.move (new, ::orig)

    /* no copy equivalent */
    omni.move (new, ::orig.map.accessed)

### register

    omni.register(new_type)

Returns a type variable.  This is useful for a number of reasons, predominantly,
to mark types.  However, it can be used to return a unique  value in other
circumstances as well.  Can only be used when the interpreter is in load
state.  


### type_of

    omni.type_of (type, value)

Used to get the type of a value.  Note: I will probably change this to "typeof"
at some point, type_of is needlessly difficult to input via keyboard.  

### const

    omni.const (value, $key)

Used to retrieve a constant value.  This system will likely be reworked, as 
I don't like its current implementation.  What effect this will have on its 
FIAL facing usage interface, I don't know.  

### break

    omni.break

This function is a no op.  Its purpose, is that if you are running the 
interpreter in a debugger, you can set a breakpoint on the function this calls,
thus allowing you to input break points into a FIAL script.  This is useful
for debugging the interpreter.  

### take

    omni.take (value, container, accessor)

Take is used to access the value of a map, sequence, or a structure formed
by nesting maps and sequences.  The value will be moved into the first 
argument, from the second argument.  The accessor is either a symbol for
a map, or a number for a sequence.  To access nested containers, a 
sequence of accessors can be used.  For example,

    var cont = {item = {1, 2, 3, {another_item = {$1, $2, $get_me, $4}}}}
    var get_me
    omni.take(get_me, cont, {$item, 4, $another_item, 3})
    if not get_me == $get_me {print ("uh oh")}

The use of take (and its sibling, put and dupe) is somewhat more cumbersome,
but no less vital to FIAL programming, than move and copy.  

### dupe

    omni.dupe (value, container, accessor)

Dupe is identical to take, except that it makes a copy of the value,
instead of performing a move.  

### put

    omni.put (container, accessor, value)

Put is used in a similar manner to take and dupe, except with a different order
of operands.  This is done in keeping with the predominant FIAL idiom of 
placing "to" values at the beginning of the argument list.  In the case of put,
the value is put into the container, so the container comes first.  

### load

Load is discussed in the section on loading libraries.  

## Map

Map is its own library in FIAL. It is an omni library, there is no need
to load it.  This may change in the future.  

Indeed, the entire library may be removed in the future, in favor of a few
omni procs.  

### create

    map.create (new_map)

Produces a new map, which is empty.  This will probably be doable in the 
future via syntax, but is now a proc call.  

### put

    map.put (map, accessor, value)

Like omni put, with two major differences:  

1. It can create a new entry name, where the omni version can only put into 
existing symbols.  
2.  It only works on nested maps.  

Thus, there is overlapping functionality here.  I think the best way forward
is to have just an omni.put function, which will force in new symbols in maps,
but not force extra places in sequences.  

### take

map.take is a version of omni.take, which only works on nested maps.  

### dupe

map.dupe is version of omni.dupe, which only works on maps.  


## Sequences

Sequences must be loaded.  Typical is:

    omni.load($seq, "sequence")

The FIAL script can use a value other than $seq, though for the purpose of
documentation, $seq is assumed.  

### create

    seq.create(new_seq)

Currently, the only proc which creates a seq. The created seq is empty. 


### in

    seq.in (seq, value)

Adds value into the sequence.

### first

    seq.first(value, seq)

Retrieves the value in the sequence which was placed in first.  For example:

    seq.create(seq)
    seq.in(seq, 1) seq.in(seq, 2), seq.in(seq, 3)
    seq.first(out, seq)

out1 will equal 1, out2 will equal 2, out3 will equal 3.  


### last

    seq.last(value, seq)

Retrieves the value in the sequence which was most recently placed inside.
In general, this is more efficient than using seq.first, when either will work
equally well for the algorithm it is to be preferred.  


### size

    seq.size( size, seq)

Retrieve the size of the sequence.  

### take

Take is similar to omni.take, except that it does not nest, and only works
on a sequence. 

### put

seq.put is similar to omni.put, except it does not nest, and only works on 
sequences. 

### dupe

seq.dupe is similar to omni.dupe, except it does not nest, and only works
on sequences.  

## Text Bufs

A text buf is the FIAL version of a mutable string.  It can, typically, be used
where a string can be used.  It is currently rather rudimentary, implemented
as a basic buffer.  

While this type is not one of the primitive ones in the language, it is 
nevertheless intended to be a standard building block within the language.  

The library must be loaded.  This documentation assumes it is loaded under
the symbol $txt.

    omni.load($txt, "text_buf")

### create

    txt.create (text_buf)

Creates a text buf.  It starts empty.  

### append

Adds the string to the end of the buffer.  Also works with ints and floats,
and this retrieves there decimal representaiton.  

The is currently the only way to add text to a text buf.  

### compare

Compares two strings texturally.  Returns a true value if they are the same.
Strings and text_bufs will be compared based upon their content.  

## Globals

The "global" lib is somewhat poorly named, however, this name is automatically
included, since global is an omni lib.   it's purpose is to store and retrieve
values. Values stored can then be retrieved from within that library.  It is
"global" in the sense that it spans all execution environments, not in that
it is available in all libraries.  To expose a value to other libraries, use
an accessor procedure.  


This is a library that has evolved somewhat over time, it serves the purpose
of storing global data.  However, it can only hold basic types, and it can
only be set while the interpreter is in "load" mode.  

It is necessary for storing type information retrieved from the register 
function.  

Again, do not fall too madly in love with the humble global lib, since it is
likely to be reworked.  

### set

    global.set ($some_sym, value)


Sets the symbol given in the first argument to the value in the second argument. 
Can only be performed while interpreter is in a load state.  Value is retrieved
with lookup function. 

### lookup 

    global.lookup (value, $some_sym)

Retrieve a value previously set from within the library.

