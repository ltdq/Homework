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
yyjson.o: yyjson.c yyjson.h
Data.o: Data.c Data.h Hash.h List.h yyjson.h
Hash.o: Hash.c Hash.h Data.h rapidhash.h
List.o: List.c List.h Data.h
main.o: main.c Data.h
.PHONY: all clean

