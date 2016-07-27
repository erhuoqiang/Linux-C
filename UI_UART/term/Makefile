src := $(shell ls *.c)
dest := $(patsubst %.c, %.o,$(src))

term:$(dest)
	gcc $^ -lpthread -o $@
$(dest):$(src)
	gcc $^ -c -o $@
clean:
	rm -f *.o  term 
