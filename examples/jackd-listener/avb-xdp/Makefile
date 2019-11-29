# SPDX-License-Identifier: (GPL-2.0 OR BSD-2-Clause)
XDP_TARGETS  := xdp_avb_kern

LIBBPF_DIR = ../libbpf/src/
COMMON_DIR_BPF = ../common/

LLC ?= llc
CLANG ?= clang
CC ?= gcc

XDP_C = ${XDP_TARGETS:=.c}
XDP_OBJ = ${XDP_C:.c=.o}

OBJECT_LIBBPF = $(LIBBPF_DIR)/libbpf.a

# Extend if including Makefile already added some
COMMON_OBJS_BPF += $(COMMON_DIR_BPF)/common_params.o $(COMMON_DIR_BPF)/common_user_bpf_xdp.o

# Create expansions for dependencies
COMMON_H_BPF := ${COMMON_OBJS_BPF:.o=.h}

EXTRA_DEPS +=

# BPF-prog kern and userspace shares struct via header file:
KERN_USER_H ?= $(wildcard common_kern_user.h)

CFLAGS ?= -I$(LIBBPF_DIR)/build/usr/include/ -g
# Extra include for Ubuntu issue #44
CFLAGS += -I/usr/include/x86_64-linux-gnu
CFLAGS += -I../headers/
LDFLAGS ?= -L$(LIBBPF_DIR)

MRPCLIENT_DIR = ../../common
MRPLISTENER_OBJS = 
MRPLISTENER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPLISTENER_OBJS))

DAEMONS_DIR = ../../../daemons

OPT_AVB = -O2 -g
WARN_AVB = -Wall -Wextra -Wno-parentheses
CFLAGS_AVB = $(OPT) $(WARN) -std=gnu99
CPPFLAGS_AVB = -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(DAEMONS_DIR)/common
LDLIBS_AVB = -ljack -lpthread

LIBS = -l:libbpf.a -lelf 


all: jack_listener


jack_listener: jack_listener.o avb_sockets.o listener_mrp_client.o xdp_load_and_stats.o $(XDP_OBJ) \
	$< $(LIBS) $(LDLIBS_AVB)

jack_listener.o: jack_listener.c avb_sockets.c xdp_load_and_stats.c $(OBJECT_LIBBPF) Makefile $(COMMON_MK_BPF) $(COMMON_OBJS_BPF) $(KERN_USER_H) $(EXTRA_DEPS)
	$(CC) -Wall $(CFLAGS) $(LDFLAGS) -o $@ $(COMMON_OBJS_BPF) 

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $^ $(LDLIBS) -o $@
	
	
	
.PHONY: clean $(CLANG) $(LLC)

clean:
	rm -rf $(LIBBPF_DIR)/build
	$(MAKE) -C $(LIBBPF_DIR) clean
	$(MAKE) -C $(COMMON_DIR_BPF) clean
	rm -f $(XDP_OBJ) $(USER_OBJ)
	$(RM) jack_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
	rm -f *.ll
	rm -f *~

# For build dependency on this file, if it gets updated
COMMON_MK_BPF = $(COMMON_DIR_BPF)/common.mk

$(OBJECT_LIBBPF):
	@if [ ! -d $(LIBBPF_DIR) ]; then \
		echo "Error: Need libbpf submodule"; \
		echo "May need to run git submodule update --init"; \
		exit 1; \
	else \
		cd $(LIBBPF_DIR) && $(MAKE) all; \
		mkdir -p build; DESTDIR=build $(MAKE) install_headers; \
	fi

# Create dependency: detect if C-file change and touch H-file, to trigger
# target $(COMMON_OBJS_BPF)
$(COMMON_H_BPF): %.h: %.c
	touch $@

# Detect if any of common obj changed and create dependency on .h-files
$(COMMON_OBJS_BPF): %.o: %.h
	make -C $(COMMON_DIR_BPF)

$(XDP_OBJ): %.o: %.c  Makefile $(COMMON_MK_BPF) $(KERN_USER_H) $(EXTRA_DEPS)
	$(CLANG) -S \
	    -target bpf \
	    -D __BPF_TRACING__ \
	    $(CFLAGS) \
	    -Wall \
	    -Wno-unused-value \
	    -Wno-pointer-sign \
	    -Wno-compare-distinct-pointer-types \
	    -Werror \
	    -O2 -emit-llvm -c -g -o ${@:.o=.ll} $<
	$(LLC) -march=bpf -filetype=obj -o $@ ${@:.o=.ll}


