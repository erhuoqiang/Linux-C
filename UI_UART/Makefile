src := $(shell ls *.c *.cpp)
dest := $(patsubst %.c, %.o,$(src))

MAIN:$(dest)
	g++ $^ -lpthread  -lcurses -o $@
$(dest):$(src)
	g++ $^ -c 
clean:
	rm -f *.o  MAIN  *~
