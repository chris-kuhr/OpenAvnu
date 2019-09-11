/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
Copyright (c) 2016-2017, Harman International Industries, Incorporated
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Attributions: The inih library portion of the source code is licensed from
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt.
Complete license and copyright information can be found at
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

/*
* MODULE SUMMARY : JACK interface module.
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "openavb_types_pub.h"
#include "openavb_audio_pub.h"
#include "openavb_trace_pub.h"
#include "openavb_mediaq_pub.h"
#include "openavb_map_uncmp_audio_pub.h"
#include "openavb_map_aaf_audio_pub.h"
#include "openavb_intf_pub.h"
#include "openavb_mcs.h"

#define	AVB_LOG_COMPONENT	"JACK Interface"
#include "openavb_log_pub.h"

#include <jack/jack.h>
#include <jack/ringbuffer.h>


#define DEFAULT_JACK_RINGBUFFER_SIZE 136


typedef enum {
    TALKER_PORT_IS_JACK_INPUT_PORT = 0x1,
    LISTENER_PORT_IS_JACK_OUTPUT_PORT = 0x2
} jack_port_type_constant_t;

typedef struct {
	/////////////
	// Config data
	/////////////
	// Ignore timestamp at listener.
	bool ignoreTimestamp;

	// JACK Client and Server names
	char *pJACKClientName;
;
    char *pJACKServerName;

	U32 startThresholdPeriods;

	U32 periodTimeUsec;

	/////////////
	// Variable data
	/////////////
	// map_nv_audio_rate
	avb_audio_rate_t audioRate;

	// map_nv_audio_type
	avb_audio_type_t audioType;

	// map_nv_audio_bit_depth
	avb_audio_bit_depth_t audioBitDepth;

	// map_nv_audio_endian
	avb_audio_endian_t audioEndian;

	// map_nv_channels
	avb_audio_channels_t audioChannels;


	// JACK Client Handle
    jack_client_t *jack_client_ctx;
    jack_port_t **jackPorts;
    jack_ringbuffer_t **jackRingBuffer;

	// ALSA read/write interval //required?
	U32 intervalCounter;

	// Media clock synthesis for precise timestamps
	mcs_t mcs;

	// Estimate of media clock skew in Parts Per Billion (ns per second)
	S32 clockSkewPPB;

	// Use Media Clock Synth module instead of timestamps taken during Tx callback
	bool fixedTimestampEnabled;
} pvt_data_t;


int jack_process_period_cb( jack_nframes_t nframes, void *arg )
{
    pvt_data_t *pPvtData = (pvt_data_t *)arg;
    int collectedSampleBytes = sizeof( jack_default_audio_sample_t ) * nframes;

    for( int k=0; k<pPvtData->audioChannels; k++){
        jack_default_audio_sample_t *jack_samples_in_frame
                = jack_port_get_buffer( pPvtData->jackPorts[ k ], nframes );

        int portFlags
                = jack_port_flags( pPvtData->jackPorts[ k ] );

        if(  TALKER_PORT_IS_JACK_INPUT_PORT == portFlags) {
            if ( jack_ringbuffer_write_space( pPvtData->jackRingBuffer[ k ] ) >= collectedSampleBytes ){
                int bytes_written
                        = jack_ringbuffer_write( pPvtData->jackRingBuffer[ k ],
                                                    (char*)(jack_samples_in_frame),
                                                    collectedSampleBytes );
            }
        } else if( LISTENER_PORT_IS_JACK_OUTPUT_PORT == portFlags){
            if ( jack_ringbuffer_read_space( pPvtData->jackRingBuffer[ k ] ) >= collectedSampleBytes ){
                int bytes_written
                        = jack_ringbuffer_read( pPvtData->jackRingBuffer[ k ],
                                                    (char*)(jack_samples_in_frame),
                                                    collectedSampleBytes );
            }
        }
    }
	return 0;
}


int jack_shutdown_cb( void *arg )
{
    pvt_data_t *pPvtData = (pvt_data_t *)arg;
	return 0;
}

int init_jack_ports(pvt_data_t *pPvtData, jack_port_type_constant_t tl_jack_port_type, int channelNumber)
{
    char portString[32];
    sprintf( portString, "aaf_ch_%d", channelNumber );
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("Init JaCk Ports:");
    AVB_LOG_INFO(portString);

    pPvtData->jackPorts[ channelNumber ]
            = jack_port_register( pPvtData->jack_client_ctx,
                                    portString,
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    tl_jack_port_type, 0 );

    if( NULL == pPvtData->jackPorts[ channelNumber ]){
        AVB_LOG_ERROR("no more JACK ports available.");
        AVB_TRACE_EXIT(AVB_TRACE_INTF);
        return -1;
    }

    AVB_LOG_INFO("Init JaCk Ringbuffers:");
    pPvtData->jackRingBuffer[ channelNumber ]
            = jack_ringbuffer_create( sizeof(jack_default_audio_sample_t) * DEFAULT_JACK_RINGBUFFER_SIZE );

    if( NULL == pPvtData->jackRingBuffer[ channelNumber ]){
        AVB_LOG_ERROR("failed to create JACK Ringbuffer.");
        AVB_TRACE_EXIT(AVB_TRACE_INTF);
        return -1;
    }

    memset(pPvtData->jackRingBuffer[ channelNumber ]->buf, 0, pPvtData->jackRingBuffer[ channelNumber ]->size);

    return 0;
}

int init_jack_client(pvt_data_t *pPvtData, jack_port_type_constant_t tl_jack_port_type )
{
    AVB_TRACE_ENTRY(AVB_TRACE_INTF);



    AVB_LOG_INFO("Init JaCk jack_get_buffer_size");
    int nframes = jack_get_buffer_size(pPvtData->jack_client_ctx);

    AVB_LOG_INFO("Init JaCk jack_set_process_callback");
    jack_set_process_callback( pPvtData->jack_client_ctx, jack_process_period_cb, (void*)pPvtData );
    jack_on_shutdown( pPvtData->jack_client_ctx, jack_shutdown_cb, (void*)pPvtData );

    AVB_LOG_INFO("Init JaCk malloc jackPorts");
    pPvtData->jackPorts
            = (jack_port_t**) malloc( sizeof(jack_port_t*) * pPvtData->audioChannels );

    AVB_LOG_INFO("Init JaCk malloc Ringbuffers");
    pPvtData->jackRingBuffer
            = (jack_ringbuffer_t**) malloc( sizeof(jack_ringbuffer_t*) * pPvtData->audioChannels );

    for( int k=0; k<pPvtData->audioChannels; k++){
        if( init_jack_ports(pPvtData, tl_jack_port_type, k) ){
            AVB_LOG_ERROR("Unable to init JACK clientPorts.");
            AVB_TRACE_EXIT(AVB_TRACE_INTF);
            return -1;
        }
    }

    if( jack_activate( pPvtData->jack_client_ctx ) ) {
        AVB_LOG_ERROR("Unable to activate JACK client.");
        AVB_TRACE_EXIT(AVB_TRACE_INTF);
        return -1;
    }

    AVB_LOG_INFO("JACK client active.");
    return 0;
}


// Each configuration name value pair for this mapping will result in this callback being called.
void openavbIntfJACKCfgCB(media_q_t *pMediaQ, const char *name, const char *value)
{
    AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("openavbIntfJACKCfgCB called.");
    AVB_LOG_INFO(name);
    AVB_LOG_INFO(value);

	if (pMediaQ) {
		char *pEnd;
		long tmp;
		U32 val;

		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

        AVB_LOG_INFO("pPubMapUncmpAudioInfo");
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo;
		pPubMapUncmpAudioInfo = (media_q_pub_map_uncmp_audio_info_t *)pMediaQ->pPubMapInfo;
		if (!pPubMapUncmpAudioInfo) {
			AVB_LOG_ERROR("Public map data for audio info not allocated.");
			return;
		}


		if (strcmp(name, "intf_nv_ignore_timestamp") == 0) {
            AVB_LOG_INFO("intf_nv_ignore_timestamp");
			tmp = strtol(value, &pEnd, 10);
			if (*pEnd == '\0' && tmp == 1) {
				pPvtData->ignoreTimestamp = (tmp == 1);
			}
            AVB_LOG_INFO(pPvtData->ignoreTimestamp);
		}


		else if (strcmp(name, "intf_nv_audio_rate") == 0) {
            AVB_LOG_INFO("intf_nv_audio_rate");
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_RATE_8KHZ && val <= AVB_AUDIO_RATE_192KHZ) {
				pPvtData->audioRate = val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio rate configured for intf_nv_audio_rate.");
				pPvtData->audioRate = AVB_AUDIO_RATE_48KHZ;
			}
            AVB_LOG_INFO(name);
            if( NULL != pPvtData->jack_client_ctx ){

                AVB_LOG_INFO("Query JACK server to config intf_nv_audio_rate.");
                if( pPvtData->audioRate != (avb_audio_rate_t) jack_get_sample_rate(pPvtData->jack_client_ctx))
				    AVB_LOG_ERROR("Invalid audio rate configured for intf_nv_audio_rate.");
            }

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioRate = pPvtData->audioRate;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
            AVB_LOG_INFO(pPubMapUncmpAudioInfo->audioRate);
		}

		else if (strcmp(name, "intf_nv_audio_bit_depth") == 0) {
            AVB_LOG_INFO("intf_nv_audio_bit_depth");
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_BIT_DEPTH_1BIT && val <= AVB_AUDIO_BIT_DEPTH_64BIT) {
				pPvtData->audioBitDepth = val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_bits.");
				pPvtData->audioBitDepth = AVB_AUDIO_BIT_DEPTH_24BIT;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioBitDepth = pPvtData->audioBitDepth;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
            AVB_LOG_INFO(pPubMapUncmpAudioInfo->audioBitDepth);
		}

		else if (strcmp(name, "intf_nv_audio_type") == 0) {
            AVB_LOG_INFO("intf_nv_audio_type");
			if (strncasecmp(value, "float", 5) == 0)
				pPvtData->audioType = AVB_AUDIO_TYPE_FLOAT;
			else if (strncasecmp(value, "sign", 4) == 0
					 || strncasecmp(value, "int", 4) == 0)
				pPvtData->audioType = AVB_AUDIO_TYPE_INT;
			else if (strncasecmp(value, "unsign", 6) == 0
					 || strncasecmp(value, "uint", 4) == 0)
				pPvtData->audioType = AVB_AUDIO_TYPE_UINT;
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_type.");
				pPvtData->audioType = AVB_AUDIO_TYPE_UNSPEC;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioType = pPvtData->audioType;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
            AVB_LOG_INFO(pPubMapUncmpAudioInfo->audioType);
		}

		else if (strcmp(name, "intf_nv_audio_endian") == 0) {
            AVB_LOG_INFO("intf_nv_audio_endian");
			if (strncasecmp(value, "big", 3) == 0)
				pPvtData->audioEndian = AVB_AUDIO_ENDIAN_BIG;
			else if (strncasecmp(value, "little", 6) == 0)
				pPvtData->audioEndian = AVB_AUDIO_ENDIAN_LITTLE;
			else {
				AVB_LOG_ERROR("Invalid audio type configured for intf_nv_audio_endian.");
				pPvtData->audioEndian = AVB_AUDIO_ENDIAN_UNSPEC;
			}

			// Give the audio parameters to the mapping module.
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioEndian = pPvtData->audioEndian;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
            AVB_LOG_INFO(pPvtData->audioEndian);
		}

		else if (strcmp(name, "intf_nv_audio_channels") == 0) {
            AVB_LOG_INFO("intf_nv_audio_channels");
			val = strtol(value, &pEnd, 10);
			// TODO: Should check for specific values
			if (val >= AVB_AUDIO_CHANNELS_1 && val <= AVB_AUDIO_CHANNELS_8) {
				pPvtData->audioChannels = val;
			}
			else {
				AVB_LOG_ERROR("Invalid audio channels configured for intf_nv_audio_channels.");
				pPvtData->audioChannels = AVB_AUDIO_CHANNELS_2;
			}

			// Give the audio parameters to the mapping module.
            AVB_LOG_INFO("pPubMapUncmpAudioInfo->audioChannels");
			if (pMediaQ->pMediaQDataFormat) {
				if (strcmp(pMediaQ->pMediaQDataFormat, MapUncmpAudioMediaQDataFormat) == 0
					|| strcmp(pMediaQ->pMediaQDataFormat, MapAVTPAudioMediaQDataFormat) == 0) {
					pPubMapUncmpAudioInfo->audioChannels = pPvtData->audioChannels;
				}
				//else if (pMediaQ->pMediaQDataFormat == MapSAFMediaQDataFormat) {
				//}
			}
            AVB_LOG_INFO(pPubMapUncmpAudioInfo->audioChannels);

		}

        if (strcmp(name, "intf_nv_start_threshold_periods") == 0) {
            AVB_LOG_INFO("intf_nv_start_threshold_periods");
			pPvtData->startThresholdPeriods = strtol(value, &pEnd, 10);
		}

		else if (strcmp(name, "intf_nv_period_time") == 0) {
            AVB_LOG_INFO("intf_nv_period_time");
			pPvtData->periodTimeUsec = strtol(value, &pEnd, 10);
		}

		else if (strcmp(name, "intf_nv_clock_skew_ppb") == 0) {
            AVB_LOG_INFO("intf_nv_clock_skew_ppb");
			pPvtData->clockSkewPPB = strtol(value, &pEnd, 10);
		}

	}



    AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfJACKGenInitCB(media_q_t *pMediaQ)
{
    AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("openavbIntfJACKGenInitCB called.");

    /*


    init JACK Client here... RxCB and TxCB Init??


    */
    jack_options_t jackOptions = JackNoStartServer;
    jack_status_t jackStatus;
    char *server_name = NULL;

    // Open the jack client.
    AVB_LOG_INFO("jack_client_open");
    if( 0 == ( pPvtData->jack_client_ctx = jack_client_open( "AVB_Client", jackOptions, &jackStatus, server_name))) {
        AVB_LOGF_ERROR("Unable to connect to JACK server; jack_client_open() failed, status = 0x%2.0x.", jackStatus);
        AVB_TRACE_EXIT(AVB_TRACE_INTF);
        return -1;
    }
    AVB_LOG_INFO("jack_client_open_done");
    AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// A call to this callback indicates that this interface module will be
// a talker. Any talker initialization can be done in this function.
void openavbIntfJACKTxInitCB(media_q_t *pMediaQ)
{

	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("\t\t openavbIntfJACKTxInitCB");

	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
		}
        if( init_jack_client( pPvtData, TALKER_PORT_IS_JACK_INPUT_PORT ) < 0 ){
			AVB_LOG_ERROR("JACK Client Initialization failed.");
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
        }

	}

    AVB_LOG_INFO("\t\t openavbIntfJACKTxInitCB Done");
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback will be called for each AVB transmit interval.
bool openavbIntfJACKTxCB(media_q_t *pMediaQ)
{
	bool moreItems = TRUE;
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
    AVB_LOG_INFO("\t\t openavbIntfJACKTxCB");

	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		media_q_item_t *pMediaQItem = NULL;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			return FALSE;
		}

		if (pPvtData->intervalCounter++ % pPubMapUncmpAudioInfo->packingFactor != 0) {
			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			return TRUE;
		}

        int k = 0;
		while (moreItems) {
			pMediaQItem = openavbMediaQHeadLock(pMediaQ);
			if (pMediaQItem) {

                if (pMediaQItem->itemSize < pPubMapUncmpAudioInfo->itemSize) {
                    AVB_LOG_ERROR("Media queue item not large enough for samples");
                    AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
                    return FALSE;
                }

                int bytes_read = 0;

                for( int samplePos = 0; samplePos < pPubMapUncmpAudioInfo->framesPerItem;samplePos++){
                    for( int k = 0; k<pPvtData->audioChannels; k++ ){

                        //pPubMapUncmpAudioInfo->framesPerItem - (pMediaQItem->dataLen / pPubMapUncmpAudioInfo->itemFrameSizeBytes;

                        if ( ( bytes_read = jack_ringbuffer_read_space( pPvtData->jackRingBuffer[ k ] ) ) >= pPubMapUncmpAudioInfo->itemFrameSizeBytes ) {
                            /* READ JACK RINGBUFFER */
                            jack_ringbuffer_read ( pPvtData->jackRingBuffer[ k ],
                                                    (char*)/*&*/(pMediaQItem->pPubData + pMediaQItem->dataLen),
                                                    pPubMapUncmpAudioInfo->itemFrameSizeBytes);
                        } else {
                            AVB_LOG_ERROR("Unhandled ringbuffer read error");
                            openavbMediaQHeadUnlock(pMediaQ);
                        }

                        pMediaQItem->dataLen += bytes_read * pPubMapUncmpAudioInfo->itemFrameSizeBytes;
                        if (pMediaQItem->dataLen != pPubMapUncmpAudioInfo->itemSize) {
                            openavbMediaQHeadUnlock(pMediaQ);
                        }
                        else {
                            // Always get the timestamp.  Protocols such as AAF can choose to ignore them if not needed.
                            if (!pPvtData->fixedTimestampEnabled) {
                                openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
                            } else {
                                openavbMcsAdvance(&pPvtData->mcs);
                                openavbAvtpTimeSetToTimestampNS(pMediaQItem->pAvtpTime, pPvtData->mcs.edgeTime);
                            }
                            openavbMediaQHeadPush(pMediaQ);
                        }
                    }
				}
			}
			else {
				moreItems = FALSE;
			}
		}
	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return !moreItems;
}

// A call to this callback indicates that this interface module will be
// a listener. Any listener initialization can be done in this function.
void openavbIntfJACKRxInitCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("\t\t openavbIntfJACKRxInitCB");
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

        if( init_jack_client( pPvtData, LISTENER_PORT_IS_JACK_OUTPUT_PORT ) < 0 ){
			AVB_LOG_ERROR("JACK Client Initialization failed.");
			AVB_TRACE_EXIT(AVB_TRACE_INTF);
			return;
        }
    }
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// This callback is called when acting as a listener.
bool openavbIntfJACKRxCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF_DETAIL);
    AVB_LOG_INFO("\t\t openavbIntfJACKRxCB");

	if (pMediaQ) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
			return FALSE;
		}

		bool moreItems = TRUE;

		while (moreItems) {
			media_q_item_t *pMediaQItem = openavbMediaQTailLock(pMediaQ, pPvtData->ignoreTimestamp);
			if (pMediaQItem) {
				if (pMediaQItem->dataLen) {

                    int bytes_to_read = 0;

                    for( int samplePos = 0; samplePos < pPubMapUncmpAudioInfo->framesPerItem;samplePos++){
                        for( int k = 0; k<pPvtData->audioChannels; k++ ){

                            if ( ( bytes_to_read = jack_ringbuffer_write_space( pPvtData->jackRingBuffer[ k ] ) ) >= pPubMapUncmpAudioInfo->itemFrameSizeBytes ) {
                                /* READ JACK RINGBUFFER */
                                jack_ringbuffer_write( pPvtData->jackRingBuffer[ k ],
                                                        (char*)&(pMediaQItem->pPubData),
                                                        pPubMapUncmpAudioInfo->itemFrameSizeBytes);
                            } else {
                                AVB_LOGF_ERROR("Unhandled ringbuffer write error: %s", snd_strerror(bytes_to_read));
                                openavbMediaQHeadUnlock(pMediaQ);
                            }

                            if (bytes_to_read < pPubMapUncmpAudioInfo->framesPerItem) {
                                AVB_LOGF_WARNING("Not all pcm data consumed written:%u  consumed:%u", pMediaQItem->dataLen, pPubMapUncmpAudioInfo->audioChannels);
                            }
                        }
                    }
				}
				openavbMediaQTailPull(pMediaQ);
			}
			else {
				moreItems = FALSE;
			}
		}
	}
	AVB_TRACE_EXIT(AVB_TRACE_INTF_DETAIL);
	return TRUE;
}

// This callback will be called when the interface needs to be closed. All shutdown should
// occur in this function.
void openavbIntfJACKEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("\t\t openavbIntfJACKEndCB");
	if (pMediaQ) {
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;
		if (!pPvtData) {
			AVB_LOG_ERROR("Private interface module data not allocated.");
			return;
		}

        for( int k=0; k<pPvtData->audioChannels; k++){
            if( jack_port_unregister(pPvtData->jack_client_ctx, pPvtData->jackPorts[ k ]));
        }
    }
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfJACKGenEndCB(media_q_t *pMediaQ)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("\t\t openavbIntfJACKGenEndCB");

    jack_client_close(pPvtData->jack_client_ctx);

    for( int k=0; k<pPvtData->audioChannels; k++){
        jack_free( pPvtData->jackPorts[ k ] );
        jack_ringbuffer_free( pPvtData->jackRingBuffer[ k ] );
    }
	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

void openavbIntfJACKEnableFixedTimestamp(media_q_t *pMediaQ, bool enabled, U32 transmitInterval, U32 batchFactor)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("\t\t openavbIntfJACKEnableFixedTimestamp");
	if (pMediaQ && pMediaQ->pPvtIntfInfo && pMediaQ->pPubMapInfo) {
		media_q_pub_map_uncmp_audio_info_t *pPubMapUncmpAudioInfo = pMediaQ->pPubMapInfo;
		pvt_data_t *pPvtData = (pvt_data_t *)pMediaQ->pPvtIntfInfo;

		pPvtData->fixedTimestampEnabled = enabled;
		if (pPvtData->fixedTimestampEnabled) {
			U32 per, rate, rem;
			S32 skewEst = pPvtData->clockSkewPPB;
			/* Ignore passed in transmit interval and use framesPerItem and audioRate so
			   we work with both AAF and 61883-6 */
			/* Carefully scale values to avoid U32 overflow or loss of precision */
			per = MICROSECONDS_PER_SECOND * pPubMapUncmpAudioInfo->framesPerItem * 10;
			per += (skewEst/10);
			rate = pPvtData->audioRate/100;
			transmitInterval = per/rate;
			rem = per % rate;
			if (rem != 0) {
				rem *= 10;
				rem /= rate;
			}
			openavbMcsInit(&pPvtData->mcs, transmitInterval, rem, 10);
			AVB_LOGF_INFO("Fixed timestamping enabled: %d %d/%d", transmitInterval, rem, 10);
		}

	}

//    /* the estimated current time in frames. This function is intended for use
//    in other threads (not the process callback). The return value can be
//    compared with the value of jack_last_frame_time to relate time in other
//    threads to JACK time. */
//    jack_nframes_t jack_frame_time( const jack_client_t* client);
//    /* the estimated time in microseconds of the specified frame time */
//    jack_time_t jack_frames_to_time( const jack_client_t* client, jack_nframes_t frames );

	/**
	int jack_get_cycle_times 	( 	const jack_client_t *  	client,
		jack_nframes_t *  	current_frames,
		jack_time_t *  	current_usecs,
		jack_time_t *  	next_usecs,
		float *  	period_usecs
	)

This function may only be used from the process callback. It provides the internal cycle timing information as used by most of the other time related functions. This allows the caller to map between frame counts and microseconds with full precision (i.e. without rounding frame times to integers), and also provides e.g. the microseconds time of the start of the current cycle directly (it has to be computed otherwise).
If the return value is zero, the following information is provided in the variables pointed to by the arguments:

current_frames: the frame time counter at the start of the current cycle, same as jack_last_frame_time(). current_usecs: the microseconds time at the start of the current cycle. next_usecs: the microseconds time of the start of the next next cycle as computed by the DLL. period_usecs: the current best estimate of the period time in microseconds.

NOTES:
Because of the types used, all the returned values except period_usecs are unsigned. In computations mapping between frames and microseconds *signed* differences are required. The easiest way is to compute those separately and assign them to the appropriate signed variables, int32_t for frames and int64_t for usecs. See the implementation of jack_frames_to_time() and Jack_time_to_frames() for an example.
Unless there was an xrun, skipped cycles, or the current cycle is the first after freewheeling or starting Jack, the value of current_usecs will always be the value of next_usecs of the previous cycle.
The value of period_usecs will in general NOT be exactly equal to the difference of next_usecs and current_usecs. This is because to ensure stability of the DLL and continuity of the mapping, a fraction of the loop error must be included in next_usecs. For an accurate mapping between frames and microseconds, the difference of next_usecs and current_usecs should be used, and not period_usecs.

Returns:    zero if OK, non-zero otherwise.

jack_time_t jack_get_time 	( 		)

Returns:    return JACK's current system time in microseconds, using the JACK clock source.

The value returned is guaranteed to be monotonic, but not linear.
jack_nframes_t jack_last_frame_time 	( 	const jack_client_t *  	client	)

Returns:
    the precise time at the start of the current process cycle. This function may only be used from the process callback, and can be used to interpret timestamps generated by jack_frame_time() in other threads with respect to the current process cycle.

This is the only jack time function that returns exact time: when used during the process callback it always returns the same value (until the next process callback, where it will return that value + nframes, etc). The return value is guaranteed to be monotonic and linear in this fashion unless an xrun occurs. If an xrun occurs, clients must check this value again, as time may have advanced in a non-linear way (e.g. cycles may have been skipped).
jack_nframes_t jack_time_to_frames 	( 	const jack_client_t *  	client,
		jack_time_t
	)

Returns:    the estimated time in frames for the specified system time.

    **/


	AVB_TRACE_EXIT(AVB_TRACE_INTF);
}

// Main initialization entry point into the interface module
extern DLL_EXPORT bool openavbIntfJACKInitialize(media_q_t *pMediaQ, openavb_intf_cb_t *pIntfCB)
{
	AVB_TRACE_ENTRY(AVB_TRACE_INTF);
    AVB_LOG_INFO("openavbIntfJACKInitialize called");

	if (pMediaQ) {
		pMediaQ->pPvtIntfInfo = calloc(1, sizeof(pvt_data_t));		// Memory freed by the media queue when the media queue is destroyed.

		if (!pMediaQ->pPvtIntfInfo) {
			AVB_LOG_ERROR("Unable to allocate memory for AVTP interface module.");
			return FALSE;
		}

        AVB_LOG_INFO("register callbacks");
		pvt_data_t *pPvtData = pMediaQ->pPvtIntfInfo;


		pPvtData->jack_client_ctx = NULL;

		pIntfCB->intf_cfg_cb = openavbIntfJACKCfgCB;
		pIntfCB->intf_gen_init_cb = openavbIntfJACKGenInitCB;

		pIntfCB->intf_tx_init_cb = openavbIntfJACKTxInitCB;
		pIntfCB->intf_tx_cb = openavbIntfJACKTxCB;
		pIntfCB->intf_rx_init_cb = openavbIntfJACKRxInitCB;
		pIntfCB->intf_rx_cb = openavbIntfJACKRxCB;

		pIntfCB->intf_end_cb = openavbIntfJACKEndCB;
		pIntfCB->intf_gen_end_cb = openavbIntfJACKGenEndCB;

		pIntfCB->intf_enable_fixed_timestamp = openavbIntfJACKEnableFixedTimestamp;



		pPvtData->ignoreTimestamp = FALSE;
		pPvtData->intervalCounter = 0;
		pPvtData->startThresholdPeriods = 2;	// Default to 2 periods of frames as the start threshold
		pPvtData->periodTimeUsec = 100000;

		pPvtData->fixedTimestampEnabled = FALSE;
		pPvtData->clockSkewPPB = 0;


	}

	AVB_TRACE_EXIT(AVB_TRACE_INTF);
	return TRUE;
}
