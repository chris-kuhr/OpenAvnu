#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>
#include "jack.h"
#include "defines.h"
#include "talker_mrp_client.h"

#define CHANNEL_SAMPSIZE (SAMPLE_SIZE*CHANNELS)

extern volatile int glob_unleash_jack;

static jack_port_t** inputports;
static jack_default_audio_sample_t** in;
static jack_default_audio_sample_t* samples_to_write;
jack_ringbuffer_t* ringbuffer;

pthread_mutex_t threadLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t dataReady = PTHREAD_COND_INITIALIZER;

void stop_jack(jack_client_t* client)
{
	jack_client_close(client);
	jack_ringbuffer_free(ringbuffer);
}

static int process(jack_nframes_t nframes, void* arg)
{
	int cnt;
	static int total;
	struct mrp_talker_ctx *ctx = (struct mrp_talker_ctx *) arg;

	/* Do nothing until we're ready to begin. */
	if (!glob_unleash_jack) {
		//printf ("nothing to do\n");
		return 0;
	}

	for(int i = 0; i < CHANNELS; i++) {
		in[i] = jack_port_get_buffer(inputports[i], nframes);
	}


	for(int i = 0; i < CHANNELS; i++) {
        for (size_t j = 0; j < nframes; j++) {
            total++;
            samples_to_write[i + j*CHANNELS] = *(in[i]+j);

        }
    }

    int bytes_to_write = CHANNEL_SAMPSIZE * nframes;
    if ((cnt = jack_ringbuffer_write_space(ringbuffer)) >= bytes_to_write) {
        jack_ringbuffer_write(ringbuffer, (void*) (samples_to_write), bytes_to_write);
        if (total % 4096 == 0) {
            //printf ("Available writespace: %i\n", cnt);
        }
    } else {
       // printf ("Only %i bytes available after %i samples\n", cnt, total);
        ctx->halt_tx = 1;
    }

	if (0 == pthread_mutex_trylock(&threadLock)) {
		pthread_cond_signal(&dataReady);
		pthread_mutex_unlock(&threadLock);
	}

	return 0;
}

void jack_shutdown(void* arg)
{
	struct mrp_talker_ctx *ctx = (struct mrp_talker_ctx *) arg;

	printf("JACK shutdown\n");
	ctx->halt_tx = 1;
}

jack_client_t* init_jack(struct mrp_talker_ctx *ctx)
{
	jack_client_t *client;
	const char *client_name = "simple_talker";
	const char *server_name = NULL;
	jack_options_t options = JackNullOption;
	jack_status_t status;

	client = jack_client_open (client_name, options, &status, server_name);

	if (NULL == client) {
		fprintf (stderr, "jack_client_open() failed\n ");
		exit(EXIT_FAILURE);
	}
	if (status & JackServerStarted) {
		fprintf (stderr, "JACK server started\n");
	}
	if (status & JackNameNotUnique) {
		client_name = jack_get_client_name(client);
		fprintf (stderr, "unique name `%s' assigned\n", client_name);
	}

	jack_set_process_callback(client, process, (void *)ctx);
	jack_on_shutdown(client, jack_shutdown, (void *)ctx);

    if (jack_activate (client))
		fprintf (stderr, "cannot activate client");

    int nframes = jack_get_buffer_size(client);
	inputports = (jack_port_t**) malloc (CHANNELS * sizeof (jack_port_t*));
	in = (jack_default_audio_sample_t**) malloc (CHANNELS * sizeof (jack_default_audio_sample_t*));
	samples_to_write = (jack_default_audio_sample_t*) malloc (CHANNELS * nframes * sizeof (jack_default_audio_sample_t));

	memset(in, 0, sizeof (jack_default_audio_sample_t*)* CHANNELS);
	ringbuffer = jack_ringbuffer_create (SAMPLE_SIZE * DEFAULT_RINGBUFFER_SIZE * CHANNELS);
    jack_ringbuffer_mlock(ringbuffer);
    memset(ringbuffer->buf, 0, ringbuffer->size);



	for(int i = 0; i < CHANNELS; i++) {
		char* portName;
		if (asprintf(&portName, "input%d", i) < 0) {
			fprintf(stderr, "Could not create portname for port %d", i);
			exit(EXIT_FAILURE);
		}

		inputports[i] = jack_port_register (client, portName, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
		if (NULL == inputports[i]) {
			fprintf (stderr, "cannot register input port \"%d\"!\n", i);
			jack_client_close (client);
			exit(EXIT_FAILURE);
		}
	}

	return client;
}
