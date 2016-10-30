TARGET = mgcap

CFLAGS = -Wall -O2

SRCS := mgcap.c
OBJS = $(subst .c,.o,$(SRCS))

${TARGET}: ${OBJS}
	ar rcs lib${TARGET}.a $^

.c.o:
	$(CC) ${CFLAGS} -o $@ -c $<

.PHONY: clean
clean:
	rm -f ${TARGET} ${OBJS}

