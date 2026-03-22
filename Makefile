CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -O3
LDFLAGS =
TARGET = student_system
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: all
	./$(TARGET).exe

.PHONY: all clean

clean:
	rm -f *.o *.exe
