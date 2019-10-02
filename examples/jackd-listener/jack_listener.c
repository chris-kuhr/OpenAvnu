/*
Copyright (c) 2013 Katja Rohloff <katja.rohloff@uni-jena.de>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>

#include <pcap/pcap.h>
#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include "listener_mrp_client.h"


#define VERSION_STR "1.1"

#define ETHERNET_HEADER_SIZE (18)
#define SEVENTEEN22_HEADER_PART1_SIZE (4)
#define STREAM_ID_SIZE (8)
#define SEVENTEEN22_HEADER_PART2_SIZE (10)
#define SIX1883_HEADER_SIZE (10)
#define HEADER_SIZE (ETHERNET_HEADER_SIZE		\
			+ SEVENTEEN22_HEADER_PART1_SIZE \
			+ STREAM_ID_SIZE		\
			+ SEVENTEEN22_HEADER_PART2_SIZE \
			+ SIX1883_HEADER_SIZE)
#define SAMPLES_PER_SECOND (48000)
#define SAMPLES_PER_FRAME (6)
#define CHANNELS (2)
#define SAMPLE_SIZE (4)
#define DEFAULT_RINGBUFFER_SIZE (32768)
#define MAX_SAMPLE_VALUE ((1U << ((sizeof(int32_t) * 8) -1)) -1)


#define TOTAL_SAMPLESIZE_PER_FRAME  (SAMPLE_SIZE * SAMPLES_PER_FRAME)
#define TOTAL_SAMPLE_CNT_PER_FRAME  (SAMPLES_PER_FRAME * CHANNELS)
#define BUF_LVL                     (SAMPLE_SIZE * SAMPLES_PER_FRAME * DEFAULT_RINGBUFFER_SIZE)
#define BUFPRELOAD_LVL              (BUF_LVL/4)
struct mrp_listener_ctx *ctx_sig;//Context pointer for signal handler

struct ethernet_header{
	u_char dst[6];
	u_char src[6];
	u_char stuff[4];
	u_char type[2];
};

/* globals */

static const char *version_str = "jack_listener v" VERSION_STR "\n"
    "Copyright (c) 2013, Katja Rohloff, Copyright (c) 2019, Christoph Kuhr\n";

pcap_t* handle;
u_char glob_ether_type[] = { 0x22, 0xf0 };
static jack_port_t** outputports;
static jack_default_audio_sample_t** out;
jack_ringbuffer_t* ringbuffer[CHANNELS];
uint32_t frame[TOTAL_SAMPLE_CNT_PER_FRAME];
jack_default_audio_sample_t jackframe[TOTAL_SAMPLE_CNT_PER_FRAME];
jack_client_t* client;
volatile int ready = 0;
unsigned char glob_station_addr[] = { 0, 0, 0, 0, 0, 0 };
unsigned char glob_stream_id[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
/* IEEE 1722 reserved address */
unsigned char glob_dest_addr[] = { 0x91, 0xE0, 0xF0, 0x00, 0x0e, 0x80 };

static void help()
{
	fprintf(stderr, "\n"
		"Usage: jack_listener [-h] -i interface -f file_name.wav"
		"\n"
		"Options:\n"
		"    -h  show this message\n"
		"    -i  specify interface for AVB connection\n"
		"\n" "%s" "\n", version_str);
	exit(EXIT_FAILURE);
}

void shutdown_and_exit(int sig)
{
	int ret;

	if (sig != 0)
		fprintf(stdout,"Received signal %d:", sig);
	fprintf(stdout,"Leaving...\n");

	if (0 != ctx_sig->talker) {
		ret = send_leave(ctx_sig);
		if (ret)
			printf("send_leave failed\n");
	}

	ret = mrp_disconnect(ctx_sig);
	if (ret)
		printf("mrp_disconnect failed\n");

	close(ctx_sig->control_socket);

	if (NULL != handle) {
		pcap_breakloop(handle);
		pcap_close(handle);
	}

	if (NULL != client) {
		fprintf(stdout, "jack\n");
		jack_client_close(client);
		for(int i= 0; i< CHANNELS; i++)
            jack_ringbuffer_free(ringbuffer[i]);
	}

	if (sig != 0)
		exit(EXIT_SUCCESS); /* actual signal */
	else
		exit(EXIT_FAILURE); /* fail condition */
}

void pcap_callback(u_char* args, const struct pcap_pkthdr* packet_header, const u_char* packet)
{
	unsigned char* test_stream_id;
	struct ethernet_header* eth_header;
	uint32_t* mybuf;
	int cnt;
	static int total;
	struct mrp_listener_ctx *ctx = (struct mrp_listener_ctx*) args;
	(void) packet_header; /* unused */

	eth_header = (struct ethernet_header*)(packet);

	if (0 != memcmp(glob_ether_type, eth_header->type,sizeof(eth_header->type))) {
		return;
	}

	test_stream_id = (unsigned char*)(packet + ETHERNET_HEADER_SIZE + SEVENTEEN22_HEADER_PART1_SIZE);
	if (0 != memcmp(test_stream_id, ctx->stream_id, STREAM_ID_SIZE)) {
		return;
	}

	mybuf = (uint32_t*) (packet + HEADER_SIZE);

	for(int j = 0; j < TOTAL_SAMPLE_CNT_PER_FRAME; j+=CHANNELS) {
		for(int i = 0; i < CHANNELS; i++) {
            memcpy(&frame[j + i], &mybuf[j + i], sizeof(frame));

			uint32_t channel = ntohl(frame[j + i]);   /* convert to host-byte order */
			channel &= 0x00ffffff;       /* ignore leading label */
			channel <<= 8;               /* left-align remaining PCM-24 sample */

			jackframe[j + i*SAMPLES_PER_FRAME] = ((int32_t)channel)/(float)(MAX_SAMPLE_VALUE);
		}
    }


    for(int i = 0; i < CHANNELS; i++) {
        if ((cnt = jack_ringbuffer_write_space(ringbuffer[i])) >= TOTAL_SAMPLESIZE_PER_FRAME) {
            jack_ringbuffer_write(ringbuffer[i], (void*)&jackframe[i*SAMPLES_PER_FRAME], TOTAL_SAMPLESIZE_PER_FRAME);
            total += CHANNELS;

        } else {
            //fprintf(stdout, "Only %i bytes available after %i samples.\n", cnt, total);
        }

        if (jack_ringbuffer_write_space(ringbuffer[i]) <= BUFPRELOAD_LVL) {
            /** Ringbuffer has only 25% or less write space available, it's time to tell jackd
            to read some data. */
            ready = 1;
        }
    }
}

static int process_jack(jack_nframes_t nframes, void* arg)
{
	(void) arg; /* unused */

	if (!ready) {
		return 0;
	}

	for(int i = 0; i < CHANNELS; i++) {
		out[i] = jack_port_get_buffer(outputports[i], nframes);
	}

	for(int i = 0; i < CHANNELS; i++) {

		if (jack_ringbuffer_read_space(ringbuffer[i]) >= SAMPLE_SIZE * nframes) {

            jack_ringbuffer_read (ringbuffer[i], (char*)(out[i]), SAMPLE_SIZE * nframes);

		} else {
			printf ("underrun\n");
			ready = 0;

			return 0;
		}
	}

	return 0;
}

void jack_shutdown(void* arg)
{
	(void)arg; /* unused*/

	printf("JACK shutdown\n");
	shutdown_and_exit(0);
}

jack_client_t* init_jack(struct mrp_listener_ctx *ctx)
{
	const char* client_name = "simple_listener";
	const char* server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	client = jack_client_open (client_name, options, &status, server_name);

	if (NULL == client) {
		fprintf (stderr, "jack_client_open() failed\n ");
		shutdown_and_exit(0);
	}

	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}

	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	jack_set_process_callback(client, process_jack, (void *)ctx);
	jack_on_shutdown(client, jack_shutdown, (void *)ctx);

	outputports = (jack_port_t**) malloc (CHANNELS * sizeof (jack_port_t*));
	int nframes = jack_get_buffer_size(client);
	int mallocSize = CHANNELS * nframes * sizeof (jack_default_audio_sample_t*);

	out = (jack_default_audio_sample_t**) malloc (mallocSize);

	for(int i = 0; i < CHANNELS; i++) {

        ringbuffer[i] = jack_ringbuffer_create (BUF_LVL);
        jack_ringbuffer_mlock(ringbuffer[i]);

        memset(out, 0, mallocSize );
        memset(ringbuffer[i]->buf, 0, ringbuffer[i]->size);


		char* portName;
		if (asprintf(&portName, "output%d", i) < 0) {
			fprintf(stderr, "could not create portname for port %d\n", i);
			shutdown_and_exit(0);
		}

		outputports[i] = jack_port_register (client, portName, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
		if (NULL == outputports[i]) {
			fprintf (stderr, "cannot register output port \"%d\"!\n", i);
			shutdown_and_exit(0);
		}
	}

	const char** ports;
	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client\n");
		shutdown_and_exit(0);
	}

	return client;
}


int main(int argc, char *argv[])
{
	char* dev = NULL;
	int dstStreamUId = -1;
	int dstEndpointId = -1;
	char errbuf[PCAP_ERRBUF_SIZE];
	struct bpf_program comp_filter_exp;		/** The compiled filter expression */
	char filter_exp[100];	                /** The filter expression */
	int rc;
	struct mrp_listener_ctx *ctx = malloc(sizeof(struct mrp_listener_ctx));
	struct mrp_domain_attr *class_a = malloc(sizeof(struct mrp_domain_attr));
	struct mrp_domain_attr *class_b = malloc(sizeof(struct mrp_domain_attr));
	ctx_sig = ctx;
	signal(SIGINT, shutdown_and_exit);


	int c;
	while((c = getopt(argc, argv, "hi:s:e:")) > 0)	{
		switch (c)		{
            case 'h':
                help();
                break;
            case 'i':
                dev = strdup(optarg);
                break;
            case 's':
                dstStreamUId = atoi(optarg);
                break;
            case 'e':
                dstEndpointId = atoi(optarg);
                break;
            default:
                    fprintf(stderr, "Unrecognized option!\n");
		}
	}

	if (NULL == dev || -1 == dstStreamUId || -1 == dstEndpointId) {
		help();
	}

	rc = mrp_listener_client_init(ctx);
	if (rc)
	{
		printf("failed to initialize global variables\n");
		return EXIT_FAILURE;
	}


	/*
        Set Dest MAC
	*/

	glob_dest_addr[4] = dstStreamUId;
	glob_dest_addr[5] = dstEndpointId;

	glob_station_addr[4] = dstStreamUId;
	glob_station_addr[5] = dstEndpointId;

	memset(glob_stream_id, 0, sizeof(glob_stream_id));
	memcpy(glob_stream_id, glob_station_addr, sizeof(glob_station_addr));
	memcpy(ctx->stream_id, glob_stream_id, sizeof(glob_stream_id));

	printf("Stream ID: %02x%02x%02x%02x%02x%02x%02x%02x",
                                     ctx->stream_id[0], ctx->stream_id[1],
                                     ctx->stream_id[2], ctx->stream_id[3],
                                     ctx->stream_id[4], ctx->stream_id[5],
                                     ctx->stream_id[6], ctx->stream_id[7]);

	if (create_socket(ctx)) {
		fprintf(stderr, "Socket creation failed.\n");
		return errno;
	}

	rc = mrp_monitor(ctx);
	if (rc)
	{
		printf("failed creating MRP monitor thread\n");
		return EXIT_FAILURE;
	}
	rc=mrp_get_domain(ctx, class_a, class_b);
	if (rc)
	{
		printf("failed calling mrp_get_domain()\n");
		return EXIT_FAILURE;
	}

	printf("detected domain Class A PRIO=%d VID=%04x...\n",class_a->priority,class_a->vid);

	rc = report_domain_status(class_a,ctx);
	if (rc) {
		printf("report_domain_status failed\n");
		return EXIT_FAILURE;
	}

	rc = join_vlan(class_a, ctx);
	if (rc) {
		printf("join_vlan failed\n");
		return EXIT_FAILURE;
	}

	init_jack(ctx);

	fprintf(stdout,"Waiting for talker...\n");
	await_talker(ctx);

	rc = send_ready(ctx);
	if (rc) {
		printf("send_ready failed\n");
		return EXIT_FAILURE;
	}

	/** session, get session handler */
	handle = pcap_open_live(dev, BUFSIZ, 1, -1, errbuf);
	if (NULL == handle) {
		fprintf(stderr, "Could not open device %s: %s.\n", dev, errbuf);
		shutdown_and_exit(0);
	}

	/** compile and apply filter */
	sprintf(filter_exp,"ether dst %02x:%02x:%02x:%02x:%02x:%02x",ctx->dst_mac[0],ctx->dst_mac[1],ctx->dst_mac[2],ctx->dst_mac[3],ctx->dst_mac[4],ctx->dst_mac[5]);
	if (-1 == pcap_compile(handle, &comp_filter_exp, filter_exp, 0, PCAP_NETMASK_UNKNOWN)) {
		fprintf(stderr, "Could not parse filter %s: %s.\n", filter_exp, pcap_geterr(handle));
		shutdown_and_exit(0);
	}

	if (-1 == pcap_setfilter(handle, &comp_filter_exp)) {
		fprintf(stderr, "Could not install filter %s: %s.\n", filter_exp, pcap_geterr(handle));
		shutdown_and_exit(0);
	}

	/** loop forever and call callback-function for every received packet */
	pcap_loop(handle, -1, pcap_callback, (u_char*)ctx);

	usleep(-1);
	free(ctx);
	free(class_a);
	free(class_b);

	return EXIT_SUCCESS;
}
