csrc := $(shell cd ./lib;ls *.c;cd ../usr;ls *.c; cd ../)
cppsrc := $(shell ls *.cpp;cd ./UI; ls *.cpp;cd ./lib;ls *.cpp;cd ../usr;ls *.cpp;cd ../../)
cdest := $(patsubst %.c, %.o,$(csrc))
cppdest := $(patsubst %.cpp, %.o,$(cppsrc))
dest := $(cdest) $(cppdest)
src := $(csrc) $(cppsrc)

CC := g++
LINK := -lpthread -lcurses
OBJ := main

INC := -I ./usr -I ./lib -I ./UI -I ./UI/lib -I ./UI/usr 
VPATH = lib:usr:UI:UI/lib:UI/usr

#vpath %.h lib:usr

$(OBJ):$(dest)
	$(CC) $^ $(LINK) -o $@;rm *.o
$(dest):$(src)
	$(CC) $^ -c $(INC)
clean:
	rm -f *~ $(OBJ) *.o
