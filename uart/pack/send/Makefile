src := $(shell ls *.c)
dest := $(patsubst %.c, %.o,$(src))

send:$(dest)
	gcc $^ -o $@ -lpthread
$(dest):$(src)
	gcc $^ -c -o $@
clean:
	rm -f *.o  send *~
