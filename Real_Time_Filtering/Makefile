all: filter store conf

filter: filter.o rt-lib.o
	gcc -o filter filter.o rt-lib.o -lrt -pthread -lm

store: store.o rt-lib.o
	gcc -o store store.o rt-lib.o -lrt -pthread -lm

conf: conf.c parameters.h
	gcc -o conf conf.c -lrt

filter.o: filter.c rt-lib.h parameters.h
	gcc -c filter.c

store.o: store.c rt-lib.h parameters.h
	gcc -c store.c

rt-lib.o: rt-lib.c rt-lib.h
	gcc -c rt-lib.c

clean:
	rm *.o filter store conf
