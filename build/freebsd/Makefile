EXECUTABLE    = FIAL

ARCHIVE       = libfial.a
FIAL_LIB      = -lfial

CC            = clang
YACC          = byacc_2014_ted

PYTHON        = python 

ROOTDIR       = ../../
SOURCEDIR     = ${ROOTDIR}src/
PACKAGE_DIR   = ./package
INCLUDEDIR    = ${SOURCEDIR}

CFLAGS        = -Wall -Wextra -g -O0                  \
                -Werror-implicit-function-declaration \
                -Wno-unused-value -Wno-unused-parameter\

INCLUDE_FLAGS = -I${INCLUDEDIR} -I.
YACCFLAGS     = -b parser -d -L

LDFLAGS       = -L.

OBJS          = main.o
AR_OBJS       = lex.yy.o parser.tab.o interp.o basic_types.o loader.o ast.o \
	        omni.o text_buf.o api.o sequence.o

DEFINE_GENERATOR  = python ${ROOTDIR}utils/gen_defines.py
GENERATED_HEADERS = ast_def_short.h value_def_short.h error_def_short.h

.PHONY : all clean package

all: ${EXECUTABLE}

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

parser.tab.h: parser.tab.c
parser.tab.c: ${SOURCEDIR}parser.y
	${YACC} ${YACCFLAGS} $<
lex.yy.c: ${SOURCEDIR}lexer.l
	flex --header-file="flex_header.h" $<
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

clean:
	rm -f ${OBJS} ${AR_OBJS}
	rm -f ${GENERATED_HEADERS}
	rm -f ${EXECUTABLE} ${ARCHIVE}
	rm -f flex_header.h lex.yy.c parser.tab.h parser.tab.c
