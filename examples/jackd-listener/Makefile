MRPCLIENT_DIR = ../common
MRPLISTENER_OBJS = listener_mrp_client.o
MRPLISTENER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPLISTENER_OBJS))

DAEMONS_DIR = ../../daemons

CC ?= gcc
OPT = -O2 -g
WARN = -Wall -Wextra -Wno-parentheses
CFLAGS = $(OPT) $(WARN) -std=gnu99
CPPFLAGS = -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(DAEMONS_DIR)/common
LDLIBS = -ljack -lpthread

all: jackd_listener

jackd_listener: jackd_listener.o avb_sockets.o $(MRPLISTENER_TARGETS)

jackd_listener.o: jackd_listener.c avb_sockets.c

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $^ $(LDLIBS) -o $@

clean:
	$(RM) jackd_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
