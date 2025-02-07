CC = gcc
CFLAGS = -Iinclude -I$(NETWORK_DIR) -I. -I$(NETWORK_DIR)/waitfree-mpsc-queue -Wall -Wextra -O2
LDFLAGS = -lmysqlclient

INCLUDE_DIR = include
NETWORK_DIR = network
SERVER_DIR = server
MOCK_DIR = mock
MPSC_DIR = $(NETWORK_DIR)/waitfree-mpsc-queue

NETWORK_SRC = $(wildcard $(NETWORK_DIR)/*.c)
SERVER_SRC = $(wildcard $(SERVER_DIR)/*.c)
MOCK_SRC = $(wildcard $(MOCK_DIR)/*.c)

DB_THREAD_SRC = network/pools/db_pool.c network/pools/thread_pool.c
MPSC_SRC = $(MPSC_DIR)/mpsc.c
MPSC_OBJ = $(MPSC_SRC:.c=.o)

NETWORK_OBJS = $(NETWORK_SRC:.c=.o) $(DB_THREAD_SRC:.c=.o) $(MPSC_OBJ)
SERVER_OBJS = $(SERVER_SRC:.c=.o)
MOCK_OBJS = $(MOCK_SRC:.c=.o)

TARGET = server_app
MOCK_TARGETS = mock_krx_server mock_oms_server

.PHONY: all clean run subdirs_clean subdirs_make

all: $(TARGET) $(MOCK_TARGETS) subdirs_make

$(TARGET): $(NETWORK_OBJS) $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

mock_krx_server: $(NETWORK_OBJS) mock/mock_krx_server.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

mock_oms_server: $(NETWORK_OBJS) mock/mock_oms_server.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(MPSC_DIR)/mpsc.o: $(MPSC_DIR)/mpsc.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean: subdirs_clean
	rm -f $(NETWORK_OBJS) $(SERVER_OBJS) $(MOCK_OBJS) $(MPSC_OBJ) $(TARGET) $(MOCK_TARGETS)

run: $(TARGET)
	./$(TARGET)

subdirs_make:
	@for dir in $(MOCK_DIR); do \
	  $(MAKE) -C $$dir; \
	done

subdirs_clean:
	@for dir in $(MOCK_DIR); do \
	  $(MAKE) -C $$dir clean; \
	done
