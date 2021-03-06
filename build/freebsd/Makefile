EXECUTABLE    = FIAL

# There's no need for a dll, the thing is 100k stripped.
ARCHIVE       = libFIAL.a
FIAL_LIB      = -lFIAL

CC            = clang
YACC          = byacc_2014_ted
LEX           = flex
PYTHON        = python 

INSTALL       = install
INSTALL_PROG  = ${INSTALL}
INSTALL_LIB   = ${INSTALL} -m 644
INSTALL_INCDIR= ${INSTALL} -d
INSTALL_INC   = ${INSTALL} -m 644

# installation directories, INCDIR is for includes 

PREFIX        = /usr/local/
LIBDIR        = ${PREFIX}lib/
BINDIR        = ${PREFIX}bin/
INCDIR        = ${PREFIX}include/FIAL/

ROOTDIR       = ../../
SOURCEDIR     = ${ROOTDIR}src/
PACKAGE_DIR   = ./package
INCLUDEDIR    = ${SOURCEDIR}

CFLAGS        = -Wall -Wextra -g -O0                   \
                -Werror-implicit-function-declaration  \
                -Wno-unused-value -Wno-unused-parameter\
                -DPTHREADS -pthread

INCLUDE_FLAGS = -I${INCLUDEDIR} -I.
YACCFLAGS     = -b parser -d -L

LDFLAGS       = -L.

OBJS          = main.o
AR_OBJS       = lex.yy.o parser.tab.o interp.o basic_types.o loader.o ast.o \
	        omni.o text_buf.o api.o sequence.o sync_pthread.o proc.o channels.o \
                chan_lib.o

DEFINE_GENERATOR  = python ${ROOTDIR}utils/gen_defines.py
GENERATED_HEADERS = ast_def_short.h value_def_short.h error_def_short.h

.PHONY : all clean package

all: ${EXECUTABLE}

parser.tab.h: parser.tab.c
parser.tab.c: ${SOURCEDIR}parser.y
	${YACC} ${YACCFLAGS} $<
lex.yy.c: ${SOURCEDIR}lexer.l
	${LEX} --header-file="flex_header.h" $<
lex.yy.o: lex.yy.c parser.tab.h
	${CC} ${INCLUDE_FLAGS} ${CFLAGS} -c lex.yy.c
parser.tab.o: parser.tab.c
	${CC} ${INCLUDE_FLAGS} ${CFLAGS} -c $<

flex_header.h: lex.yy.c

ast_def_short.h: ${SOURCEDIR}ast_defines.json
	${DEFINE_GENERATOR} ${SOURCEDIR}ast_defines.json $@ short
value_def_short.h: ${SOURCEDIR}value_defines.json
	${DEFINE_GENERATOR} ${SOURCEDIR}value_defines.json $@ short
error_def_short.h: ${SOURCEDIR}errors.json
	${DEFINE_GENERATOR} ${SOURCEDIR}errors.json $@ short

%.o: $(SOURCEDIR)%.c ${GENERATED_HEADERS}
	$(CC) ${INCLUDE_FLAGS} ${CFLAGS} -c $< -o $@

${ARCHIVE}: ${GENERATED_HEADERS} ${AR_OBJS}
	ar cr ${ARCHIVE} ${AR_OBJS}

${EXECUTABLE}: ${GENERATED_HEADERS} ${OBJS} ${ARCHIVE}
	${CC} ${CFLAGS} -o $@ ${LDFLAGS}  ${OBJS} ${FIAL_LIB}

${ROOTDIR}scripts/${EXECUTABLE}: ${EXECUTABLE}
	cp -f ${EXECUTABLE} ${ROOTDIR}scripts/${EXECUTABLE}

${ROOTDIR}scripts/libtest/${EXECUTABLE}: ${EXECUTABLE}
	cp -f ${EXECUTABLE} ${ROOTDIR}scripts/libtest/${EXECUTABLE}

install:
	${INSTALL_PROG} ${EXECUTABLE} ${BINDIR}
	${INSTALL_LIB} ${ARCHIVE} ${LIBDIR}
	${INSTALL_INCDIR} ${INCDIR}
	${INSTALL_INC} *.h ${INCDIR}
	${INSTALL_INC} ${SOURCEDIR}*.h ${INCDIR}
	
# I don't know how to set this properly -- better to not have it than have one
# that wipes someones system.   

#uninstall:
#	rm -f ${BINDIR}${EXECUTABLE}
#	rm -f ${LIBDIR}${ARCHIVE}
#	rm -rf ${

clean:
	rm -f ${OBJS} ${AR_OBJS}
	rm -f ${GENERATED_HEADERS}
	rm -f ${EXECUTABLE} ${ARCHIVE}
	rm -f flex_header.h lex.yy.c parser.tab.h parser.tab.c


# Not sure this is useful on FreeBSD, it's what I use for making the windows .zip
# file.  Leaving it around for the time being though, since I'm going to rewrite
# that to use cmd.exe commands, so I need this somewhere.  

package: ${EXECUTABLE} ${ARCHIVE}
	mkdir ${PACKAGE_DIR}
	mkdir ${PACKAGE_DIR}/include
	mkdir ${PACKAGE_DIR}/include/FIAL
	mkdir ${PACKAGE_DIR}/bin
	mkdir ${PACKAGE_DIR}/lib
	cp -f ${SOURCEDIR}*.h ${PACKAGE_DIR}/include/FIAL
	cp -f ./*.h ${PACKAGE_DIR}/include/FIAL
	cp -f ${EXECUTABLE} ${PACKAGE_DIR}/bin
	cp -f ${ARCHIVE} ${PACKAGE_DIR}/lib

