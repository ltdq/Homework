CC = gcc
PKG_CONFIG = pkg-config
SODIUM_CFLAGS = $(shell $(PKG_CONFIG) --cflags libsodium)
SODIUM_LIBS = $(shell $(PKG_CONFIG) --static --libs libsodium)

CFLAGS = -Wall -Wextra -Werror -std=c23 $(SODIUM_CFLAGS)
LDFLAGS = $(SODIUM_LIBS)

APP_TARGET = student_system
SMOKE_TARGET = core_smoke_test

CORE_SRCS = Auth.c Data.c Hash.c List.c Log.c Stack.c Trie.c memory.c yyjson.c
APP_SRCS = $(CORE_SRCS) Display.c main.c
SMOKE_SRCS = $(CORE_SRCS) core_smoke_test.c

APP_OBJS = $(APP_SRCS:.c=.o)
SMOKE_OBJS = $(SMOKE_SRCS:.c=.o)

all: $(APP_TARGET)

$(APP_TARGET): $(APP_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

$(SMOKE_TARGET): $(SMOKE_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $@"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

run: $(APP_TARGET)
	./$(APP_TARGET).exe

smoke-test: $(SMOKE_TARGET)

run-smoke: $(SMOKE_TARGET)
	./$(SMOKE_TARGET).exe

.PHONY: all clean run smoke-test run-smoke

clean:
	del /Q *.o *.exe 2>nul
