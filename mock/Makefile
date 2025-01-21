CC = gcc
CFLAGS = -Wall -Wextra -O2 -I../include

TARGETS = mock_krx_server mock_oms_server

OBJS_KRX = mock_krx_server.o
OBJS_OMS = mock_oms_server.o

all: $(TARGETS)

mock_krx_server: $(OBJS_KRX)
	$(CC) $(CFLAGS) -o $@ $^

mock_oms_server: $(OBJS_OMS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGETS) $(OBJS_KRX) $(OBJS_OMS)

.PHONY: all clean
