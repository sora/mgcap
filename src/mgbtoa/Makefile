TARGET = mgbtoa

CFLAGS = -Wall -O

SRCS := hwtstamp_config.c mgbtoa.c
OBJS = $(subst .c,.o,$(SRCS))

${TARGET}: ${OBJS}
	$(CC) ${LDFLAGS} -o $@ $^

.c.o:
	$(CC) ${CFLAGS} -o $@ -c $<

.PHONY: clean
clean:
	rm -f ${TARGET} ${OBJS}

