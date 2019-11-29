MRPCLIENT_DIR = ../common
MRPLISTENER_OBJS = listener_mrp_client.o
MRPLISTENER_TARGETS = $(addprefix $(MRPCLIENT_DIR)/,$(MRPLISTENER_OBJS))

LOADER = loader
LOADER_CLEAN = $(addsuffix _clean,$(LOADER))

DAEMONS_DIR = ../../daemons

OPT = -O2 -g
WARN = -Wall -Wextra -Wno-parentheses
CFLAGS = $(OPT) $(WARN) -std=gnu99
CPPFLAGS = -I$(DAEMONS_DIR)/mrpd -I$(MRPCLIENT_DIR) -I$(DAEMONS_DIR)/common
LDLIBS = -ljack -lpthread


all: jack_listener $(LOADER)

jack_listener: jack_listener.o avb_sockets.o $(MRPLISTENER_TARGETS)

jack_listener.o: jack_listener.c avb_sockets.c

$(MRPCLIENT_DIR)/%.o: $(MRPCLIENT_DIR)/%.c $(MRPCLIENT_DIR)/%.h
	make -C $(MRPCLIENT_DIR) $@

%: %.o
	$(CC) $^ $(LDLIBS) -o $@


$(LOADER):
	$(MAKE) -C $@

$(LOADER_CLEAN):
	$(MAKE) -C $(subst _clean,,$@) clean
	

clean:
	$(RM) jack_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
	$(LOADER_CLEAN)
