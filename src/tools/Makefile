TARGET = mgcap_ioctl

CFLAGS = -Wall -O

SRCS := mgcap_ioctl.c
OBJS = $(subst .c,.o,$(SRCS))

${TARGET}: ${OBJS}
	$(CC) ${LDFLAGS} -o $@ $^

.c.o:
	$(CC) ${CFLAGS} -o $@ -c $<

.PHONY: clean
clean:
	rm -f ${TARGET} ${OBJS}

