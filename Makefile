
LIBLITMUS=/home/common_shared/litmus/liblitmus
#JEVENTS=/home/common_shared/PAStime/PMU/pmu-tools/jevents

all:
	gcc -I${LIBLITMUS}/include/ -I${LIBLITMUS}/arch/x86/include/ -ljevents -c lib.c -fPIC -o pastime.o
	#gcc -shared -o libpastime.so pastime.o
	ar rcs libpastime.a pastime.o

clean:
	rm -f *.o *.a
