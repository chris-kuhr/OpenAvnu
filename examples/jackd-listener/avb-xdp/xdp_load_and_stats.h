/* SPDX-License-Identifier: GPL-2.0 */
static const char *__doc__ = "XDP loader and stats program\n"
	" - Allows selecting BPF section --progsec name to XDP-attach to --dev\n";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>

#include <locale.h>
#include <unistd.h>
#include <time.h>

#include <bpf/bpf.h>
#include <bpf/libbpf.h>

#include <net/if.h>
#include <linux/if_link.h> /* depend on kernel-headers installed */

#include "../common/common_params.h"
#include "../common/common_user_bpf_xdp.h"
#include "common_kern_user.h"
#include "bpf_util.h" /* bpf_num_possible_cpus */

#define MAX_SAMPLE_VALUE ((1U << ((sizeof(int)*8)-1))-1)

int find_map_fd(struct bpf_object *bpf_obj, const char *mapname);

struct record {
	__u64 timestamp;
	struct datarec total;
};

struct stats_record {
	struct record stats[1];
};

/* BPF_MAP_TYPE_ARRAY */
void map_get_value_array(int fd, __u32 key, struct datarec *value);

/* BPF_MAP_TYPE_PERCPU_ARRAY */
void map_get_value_percpu_array(int fd, __u32 key, struct datarec *value);

bool map_collect(int fd, __u32 map_type, __u32 key, struct record *rec);


void stats_collect(int map_fd, __u32 map_type,
			  struct stats_record *stats_rec);

void stats_poll(int map_fd, __u32 map_type, int interval);

int __check_map_fd_info(int map_fd, struct bpf_map_info *info,
			       struct bpf_map_info *exp);

int load_ebpf(int argc, char **argv);
