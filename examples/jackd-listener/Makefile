MRPCLIENT_DIR = ../common
MRPLISTENER_OBJS = listener_mrp_client.o
MRPLISTENER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPLISTENER_OBJS))

DAEMONS_DIR = ../../daemons

OPT_AVB = -O2 -g
WARN_AVB = -Wall -Wextra -Wno-parentheses
CFLAGS_AVB = $(OPT) $(WARN) -std=gnu99
CPPFLAGS_AVB = -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(DAEMONS_DIR)/common
LDLIBS_AVB = -ljack -lpthread


all: jack_listener

jack_listener: jack_listener.o avb_sockets.o $(MRPLISTENER_TARGETS)

jack_listener.o: jack_listener.c avb_sockets.c

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $^ $(LDLIBS_AVB) -o $@

clean:
	$(RM) jack_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
