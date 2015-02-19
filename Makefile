CC = gcc
CFLAGS = -I/home/courses/cse533/Stevens/unpv13e_solaris2.10/lib/ -g -O2 -D_REENTRANT -Wall -D__EXTENSIONS__
LIBS = /home/courses/cse533/Stevens/unpv13e_solaris2.10/libunp.a -lresolv -lsocket -lnsl -lpthread -lm


#LIBS = -lresolv -lnsl -lpthread\
	#/home/sanket/np/unpv13e/libunp.a\

#FLAGS = -g -O2

#CFLAGS = ${FLAGS} -I/home/sanket/np/unpv13e/lib



#CLEANFILES = core core.* *.core *.o temp.* *.out typescript* \
#                *.lc *.lh *.bsdi *.sparc *.uw

CLEANFILES = *.o *.out typescript* \
                *.lc *.lh *.bsdi *.sparc *.uw

PROGS = server client

all:    ${PROGS}

get_ifi_info_plus.o: get_ifi_info_plus.c
	${CC} ${CFLAGS} -c get_ifi_info_plus.c


server:    server.o get_ifi_info_plus.o  
	${CC} ${CFLAGS} -o server server.o get_ifi_info_plus.o ${LIBS}
server.o: server.c
	${CC} ${CFLAGS} -c server.c
client:    client.o get_ifi_info_plus.o 
	${CC} ${CFLAGS} -o client client.o get_ifi_info_plus.o ${LIBS}
client.o: client.c
	${CC} ${CFLAGS} -c client.c

#get_ifi_info_plus:    get_ifi_info_plus.o
#	${CC} ${CFLAGS} -o get_ifi_info_plus get_ifi_info_plus.o ${LIBS}

#a1ech:    a1ech.o
#	${CC} ${CFLAGS} -o $@ a1ech.o ${LIBS}

#a1tim:    a1tim.o
#	${CC} ${CFLAGS} -o $@ a1tim.o ${LIBS}


#daytimecli:	daytimecli.o
#		${CC} ${CFLAGS} -o $@ daytimecli.o ${LIBS}

clean:
	rm -f ${PROGS} ${CLEANFILES}


