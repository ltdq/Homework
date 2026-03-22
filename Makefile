CC = gcc
CFLAGS = -Wall -Wextra -std=c17 -O3 -D_XOPEN_SOURCE=700 $(shell pkg-config --cflags notcurses 2>/dev/null)
LDFLAGS = $(shell pkg-config --libs notcurses 2>/dev/null || echo "-lnotcurses -lnotcurses-core -lpthread")
TARGET = test
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
all : $(TARGET)
$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"
%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
run : all
	./$(TARGET)
yyjson.o: yyjson.c yyjson.h
Data.o: Data.c Data.h Hash.h List.h Stack.h memory.h yyjson.h
Memory.o: Memory.c memory.h
Hash.o: Hash.c Hash.h Data.h rapidhash.h
List.o: List.c List.h Data.h
main.o: main.c Data.h Display.h
Display.o: Display.c Display.h
.PHONY: all clean

