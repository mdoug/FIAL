EXECUTABLE    = FIAL.exe

ARCHIVE       =  FIAL.lib
FIAL_LIB      =  FIAL.lib

CC            = cl 
YACC          = byacc.exe
AR            = lib.exe
PYTHON        = python

ROOTDIR       = ../../
BUILDDIR      = .
SOURCEDIR     = $(ROOTDIR)src/
PACKAGE_DIR   = ./package
INCLUDEDIR    = $(SOURCEDIR)

CFLAGS        = /Dinline="" /MD /Zi 
INCLUDE_FLAGS = /I$(INCLUDEDIR) /I.
YACCFLAGS     = -b parser -d -L

LDFLAGS       = /DEBUG

B             = $(BUILDDIR)
OBJS          = $B/main.o
AR_OBJS       = $B/lex.yy.o $B/parser.tab.o $B/interp.o $B/basic_types.o $B/loader.o $B/ast.o \
	        $B/omni.o $B/text_buf.o $B/api.o $B/sequence.o

DEFINE_GENERATOR  = $(PYTHON) $(ROOTDIR)utils/gen_defines.py
GENERATED_HEADERS = ast_def_short.h value_def_short.h error_def_short.h

# .PHONY : all clean package

all: $(EXECUTABLE)

package: $(EXECUTABLE) $(ARCHIVE)
	mkdir $(PACKAGE_DIR)
	mkdir $(PACKAGE_DIR)/include
	mkdir $(PACKAGE_DIR)/include/FIAL
	mkdir $(PACKAGE_DIR)/bin
	mkdir $(PACKAGE_DIR)/lib
	cp -f $(SOURCEDIR)*.h $(PACKAGE_DIR)/include/FIAL
	cp -f ./*.h $(PACKAGE_DIR)/include/FIAL
	cp -f $(EXECUTABLE) $(PACKAGE_DIR)/bin
	cp -f $(ARCHIVE) $(PACKAGE_DIR)/lib

parser.tab.h: parser.tab.c
parser.tab.c: $(SOURCEDIR)parser.y
	$(YACC) $(YACCFLAGS) $?
lex.yy.c: $(SOURCEDIR)lexer.l
	flex --header-file="flex_header.h" $?
lex.yy.o: lex.yy.c parser.tab.h
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) /Fo$@ /c lex.yy.c
parser.tab.o: parser.tab.c
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) /Fo$@ /c parser.tab.c 

flex_header.h: lex.yy.c

ast_def_short.h: $(SOURCEDIR)ast_defines.json
	$(DEFINE_GENERATOR) $(SOURCEDIR)ast_defines.json $@ short
value_def_short.h: $(SOURCEDIR)value_defines.json
	$(DEFINE_GENERATOR) $(SOURCEDIR)value_defines.json $@ short
error_def_short.h: $(SOURCEDIR)errors.json
	$(DEFINE_GENERATOR) $(SOURCEDIR)errors.json $@ short

{$(SOURCEDIR)}.c{$(BUILDDIR)}.o: 
	$(CC) $(INCLUDE_FLAGS) $(CFLAGS) /c /Fo$@ $? 

$(OBJS): 

$(AR_OBJS):

$(ARCHIVE): $(GENERATED_HEADERS) $(AR_OBJS)
	$(AR) /OUT:$(ARCHIVE) $(AR_OBJS)

$(EXECUTABLE): $(GENERATED_HEADERS) $(OBJS) $(ARCHIVE)
	$(CC) $(CFLAGS) $(OBJS) $(FIAL_LIB) /link $(LDFLAGS) /OUT:$@  

../scripts/$(EXECUTABLE): $(EXECUTABLE)
	cp -f $(EXECUTABLE) ../scripts/$(EXECUTABLE)

../scripts/libtest/$(EXECUTABLE): $(EXECUTABLE)
	cp -f $(EXECUTABLE) ../scripts/libtest/$(EXECUTABLE)

clean:
	rm -f $(OBJS) $(AR_OBJS)
	rm -f $(GENERATED_HEADERS)
	rm -f $(EXECUTABLE) $(ARCHIVE)
	rm -f flex_header.h lex.yy.c parser.tab.h parser.tab.c
