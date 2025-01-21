# Compiler and flags
CC = gcc
CFLAGS = -Iinclude -I$(NETWORK_DIR) -I. -Wall -Wextra -O2
LDFLAGS =

# Directories
INCLUDE_DIR = include
NETWORK_DIR = network
SERVER_DIR = server
MOCK_DIR = mock

# Source files
NETWORK_SRC = $(wildcard $(NETWORK_DIR)/*.c)
SERVER_SRC = $(wildcard $(SERVER_DIR)/*.c)
MOCK_SRC = $(wildcard $(MOCK_DIR)/*.c)
NETWORK_OBJS = $(NETWORK_SRC:.c=.o)
SERVER_OBJS = $(SERVER_SRC:.c=.o)
MOCK_OBJS = $(MOCK_SRC:.c=.o)

# Target executables
TARGET = server_app
MOCK_TARGETS = mock_krx_server mock_oms_server

# Targets
.PHONY: all clean run subdirs_clean subdirs_make

# Default target: Build main server and mocks
all: $(TARGET) $(MOCK_TARGETS) subdirs_make

# Build main server executable
$(TARGET): $(NETWORK_OBJS) $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build mock executables
mock_krx_server: $(NETWORK_OBJS) mock/mock_krx_server.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

mock_oms_server: $(NETWORK_OBJS) mock/mock_oms_server.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean all build files
clean: subdirs_clean
	rm -f $(NETWORK_OBJS) $(SERVER_OBJS) $(MOCK_OBJS) $(TARGET) $(MOCK_TARGETS)

# Run the compiled executable
run: $(TARGET)
	./$(TARGET)

# Build and clean subdirectories
subdirs_make:
	@for dir in $(MOCK_DIR); do \
	  echo "Running make in $$dir"; \
	  $(MAKE) -C $$dir; \
	done

subdirs_clean:
	@for dir in $(MOCK_DIR); do \
	  echo "Running make clean in $$dir"; \
	  $(MAKE) -C $$dir clean; \
	done
