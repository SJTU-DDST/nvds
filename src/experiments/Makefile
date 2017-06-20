CC = gcc -std=c11
CFLAGS = -Wall -g -D_GNU_SOURCE
LDFLAGS = -libverbs -lrt

all:
	write-rc
	send-rc
	send-ud

write-rc: write_rc.o
	${CC} -o $@ $^ ${LDFLAGS}

write_rc.o: write_rc.c write_rc.h
	${CC} ${CFLAGS} -o $@ -c $<

send-rc: send_rc.o
	${CC} -o $@ $^ ${LDFLAGS}

send_rc.o: send_rc.c send_rc.h
	${CC} ${CFLAGS} -o $@ -c $<

send-ud: send_ud.o
	${CC} -o $@ $^ ${LDFLAGS}

send_ud.o: send_ud.c send_ud.h
	${CC} ${CFLAGS} -o $@ -c $<

clean:
	rm -f *.o write-rc send-rc send-ud
