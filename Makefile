CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O3
LDFLAGS =
TARGET = test
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)
all : $(TARGET)
$(TARGET) : $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"
%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean :
	rm -f $(OBJS) $(TARGET)
	@echo "Clean complete: $(TARGET) and object files removed."
run : all
	./$(TARGET)
Data.o: Data.c Data.h HashKey.h Hash.h
HashKey.o: HashKey.c HashKey.h
main.o: main.c Data.h Hash.h
.PHONY: all clean

