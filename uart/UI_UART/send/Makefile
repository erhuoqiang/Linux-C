src := $(shell ls *.c)
dest := $(patsubst %.c, %.o,$(src))

send:$(dest)
	gcc $^ -o $@
$(dest):$(src)
	gcc $^ -c -o $@
clean:
	rm -f *.o  send
