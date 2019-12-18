/* This common_kern_user.h is used by kernel side BPF-progs and
 * userspace programs, for sharing common struct's and DEFINEs.
 */
#ifndef __COMMON_KERN_USER_H
#define __COMMON_KERN_USER_H

#define AUDIO_CHANNELS 2
#define SAMPLEBUF_SIZE 64
#define SAMPLEBUF_PACKET_RATIO 10

/* This is the data record stored in the map */
struct datarec{
	__u64 accu_rx_timestamp;
	__u32 rx_pkt_cnt;
	int sampleCounter;
	int sampleBuffer[AUDIO_CHANNELS][SAMPLEBUF_SIZE];
};

#ifndef XDP_ACTION_MAX
#define XDP_ACTION_MAX (XDP_REDIRECT + 1)
#endif

#endif /* __COMMON_KERN_USER_H */
