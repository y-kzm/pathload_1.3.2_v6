CC=gcc

CFLAGS=-g -O2 -pthread  -DHAVE_PTHREAD=1 -DHAVE_LIBM=1 -DHAVE_LIBNSL=1 -DSTDC_HEADERS=1 -DHAVE_FCNTL_H=1 -DHAVE_STRINGS_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_UNISTD_H=1 -DTIME_WITH_SYS_TIME=1 -DSIZEOF_INT=4 -DSIZEOF_LONG=8 -DHAVE_STRFTIME=1 -DHAVE_GETHOSTNAME=1 -DHAVE_GETTIMEOFDAY=1 -DHAVE_SOCKET=1  -DTHRLIB
CPPFLAGS=
LIBS=-lnsl -lm  
LDFLAGS=

SOBJS=pathload_snd.o pathload_snd_func.o
ROBJS=pathload_rcv.o pathload_rcv_func.o 
OBJS=$(SOBJS) $(ROBJS)

TARGETS=pathload_snd pathload_rcv

all:${TARGETS} clean

pathload_snd: $(SOBJS)
	 $(CC) $(SOBJS) -o pathload_snd $(LIBS) $(LDFLAGS) $(CFLAGS)

pathload_rcv: $(ROBJS)
	 $(CC) $(ROBJS) -o pathload_rcv $(LIBS) $(LDFLAGS) $(CFLAGS)

pathload_rcv.o pathload_rcv_func.o: pathload_gbls.h pathload_rcv.h

pathload_snd.o pathload_snd_func.o: pathload_gbls.h pathload_snd.h

clean: 
	 rm -f ${OBJS} config.cache config.log  config.status

distclean: 
	 rm -f ${OBJS} ${TARGETS} config.cache config.log makefile config.status
