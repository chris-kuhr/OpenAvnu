XDP_TARGETS  := xdp_avb_kern
USER_TARGETS := 

LLC = llc
CLANG = clang
CC = gcc


MRPCLIENT_DIR = ../common
MRPLISTENER_OBJS = listener_mrp_client.o
MRPLISTENER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPLISTENER_OBJS))


DAEMONS_DIR = ../../daemons

OPT = -O2 -g
WARN = -Wall -Wextra -Wno-parentheses
CFLAGS = $(OPT) $(WARN) -std=gnu99
CPPFLAGS = -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(DAEMONS_DIR)/common
LDLIBS = -ljack -lpthread




XDP_C = ${XDP_TARGETS:=.c}
XDP_OBJ = ${XDP_C:.c=.o}
USER_C := ${USER_TARGETS:=.c}
USER_OBJ := ${USER_C:.c=.o}

# Expect this is defined by including Makefile, but define if not
COMMON_DIR = common/
LIBBPF_DIR = libbpf/src/

OBJECT_LIBBPF = $(LIBBPF_DIR)/libbpf.a
# For build dependency on this file, if it gets updated
COMMON_MK = $(COMMON_DIR)/common.mk

CFLAGS_BPF = -I$(LIBBPF_DIR)/build/usr/include/ -g
# Extra include for Ubuntu issue #44
CFLAGS_BPF += -I/usr/include/x86_64-linux-gnu
CFLAGS_BPF += -I./headers/
LDFLAGS_BPF = -L$(LIBBPF_DIR)


.PHONY: clean loader $(XDP_OBJ)

all: $(XDP_OBJ) loader jack_listener 

jack_listener: jack_listener.o avb_sockets.o $(MRPLISTENER_TARGETS)

jack_listener.o: jack_listener.c avb_sockets.c

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $^ $(LDLIBS) -o $@


$(XDP_OBJ): %.o: %.c  Makefile $(COMMON_MK)
	$(CLANG) -S \
	    -target bpf \
	    -D __BPF_TRACING__ \
	    $(CFLAGS_BPF) \
	    -Wall \
	    -Wno-unused-value \
	    -Wno-pointer-sign \
	    -Wno-compare-distinct-pointer-types \
	    -Werror \
	    -O2 -emit-llvm -c -g -o ${@:.o=.ll} $<
	$(LLC) -march=bpf -filetype=obj -o $@ ${@:.o=.ll}

loader:
	make -C loader
	
clean:
	$(RM) jack_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
	make -C loader clean
	make -C $(XDP_OBJ) clean
