SOURCEDIR=../src/
PACKAGE_DIR=./package
INCLUDEDIR=${SOURCEDIR}
CC=gcc
AR_OBJS=lex.yy.o parser.tab.o interp.o basic_types.o loader.o ast.o omni.o \
	text_buf.o api.o sys_bsd.o
OBJS=main.o
CFLAGS=-I${INCLUDEDIR} -I. -Wall -Wno-unused-value -g
BISONFLAGS=-d
LDFLAGS=-L.
LDLIBS=-lfial
EXECUTABLE=FIAL
ARCHIVE=libfial.a

GENERATED_HEADERS=ast_def_short.h value_def_short.h error_def_short.h
DEFINE_GENERATOR=python ../utils/gen_defines.py

.PHONY : all clean package

all: ${EXECUTABLE}

package: ${EXECUTABLE} ${ARCHIVE}
	mkdir ${PACKAGE_DIR}
	mkdir ${PACKAGE_DIR}/include
	mkdir ${PACKAGE_DIR}/bin
	mkdir ${PACKAGE_DIR}/lib
	cp -f ${SOURCEDIR}*.h ${PACKAGE_DIR}/include
	cp -f ./*.h ${PACKAGE_DIR}/include
	cp -f ${EXECUTABLE} ${PACKAGE_DIR}/bin
	cp -f ${ARCHIVE} ${PACKAGE_DIR}/lib

parser.tab.h: parser.tab.c
parser.tab.c: ${SOURCEDIR}parser.y
	bison ${BISONFLAGS} $<
lex.yy.c: ${SOURCEDIR}lexer.l
	flex --header-file="flex_header.h" $<
lex.yy.o: lex.yy.c parser.tab.h
	${CC} ${CFLAGS} -c lex.yy.c
parser.tab.o: parser.tab.c
	${CC} ${CFLAGS} -c $<

flex_header.h: lex.yy.c

ast_def_short.h: ${SOURCEDIR}ast_defines.json
	${DEFINE_GENERATOR} ${SOURCEDIR}ast_defines.json $@ short
value_def_short.h: ${SOURCEDIR}value_defines.json
	${DEFINE_GENERATOR} ${SOURCEDIR}value_defines.json $@ short
error_def_short.h: ${SOURCEDIR}errors.json
	${DEFINE_GENERATOR} ${SOURCEDIR}errors.json $@ short

%.o: $(SOURCEDIR)%.c ${GENERATED_HEADERS}
	$(CC) $(CFLAGS) -c $< -o $@

${ARCHIVE}: ${GENERATED_HEADERS} ${AR_OBJS}
	ar cr ${ARCHIVE} ${AR_OBJS}

${EXECUTABLE}: ${GENERATED_HEADERS} ${OBJS} ${ARCHIVE}
	${CC} -o $@ ${LDFLAGS}  ${OBJS} ${LDLIBS}

../scripts/${EXECUTABLE}: ${EXECUTABLE}
	cp -f ${EXECUTABLE} ../scripts/${EXECUTABLE}

../scripts/libtest/${EXECUTABLE}: ${EXECUTABLE}
	cp -f ${EXECUTABLE} ../scripts/libtest/${EXECUTABLE}

clean:
	rm -f ${OBJS} ${AR_OBJS}
	rm -f ${GENERATED_HEADERS}
	rm -f ${EXECUTABLE} ${ARCHIVE}
	rm -f flex_header.h lex.yy.c parser.tab.h parser.tab.c
