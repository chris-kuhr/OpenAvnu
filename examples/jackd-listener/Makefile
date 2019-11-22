MRPCLIENT_DIR = ../common
MRPLISTENER_OBJS = listener_mrp_client.o
MRPLISTENER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPLISTENER_OBJS))

# Departing from the implicit _user.c scheme
XDP_TARGETS  := xdp_avb_kern
USER_TARGETS := jack_listener

LIBBPF_DIR = ../../lib/libbpf/src/

include ./common.mk


DAEMONS_DIR = ../../daemons

CC ?= gcc
OPT = -O2 -g
WARN = -Wall -Wextra -Wno-parentheses
CFLAGS = $(OPT) $(WARN) -std=gnu99
CPPFLAGS = -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(DAEMONS_DIR)/common
LDLIBS = -ljack -lpthread

all: jack_listener  common_params.o common_user_bpf_xdp.o common_libbpf.o

CFLAGS := -g -Wall

LIBBPF_DIR = ../libbpf/src/
CFLAGS += -I$(LIBBPF_DIR)/build/usr/include/  -I../headers

common_user_bpf_xdp.o: common_user_bpf_xdp.c common_user_bpf_xdp.h
	$(CC) $(CFLAGS) -c -o $@ $<

common_libbpf.o: common_libbpf.c common_libbpf.h
	$(CC) $(CFLAGS) -c -o $@ $<

jack_listener: jack_listener.o avb_sockets.o $(MRPLISTENER_TARGETS)

jack_listener.o: jack_listener.c avb_sockets.c

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $^ $(LDLIBS) -o $@

clean:
	$(RM) jack_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
