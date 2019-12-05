/*
Copyright (C) 2016-2019 Christoph Kuhr

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef _AVB_SOCKET_H_
#define _AVB_SOCKET_H_

#ifdef __cplusplus
extern "C"
{
#endif

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <ifaddrs.h>
#include <linux/if_packet.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <linux/filter.h>
#include <poll.h>

#include <netinet/in.h>
//

#define RETURN_VALUE_FAILURE 0
#define RETURN_VALUE_SUCCESS 1

#define MILISLEEP_TIME 1000000
#define USLEEP_TIME 1000

#define MAX_DEV_STR_LEN               32
#define BUFLEN 1500
#define ETHERNET_Q_HDR_LENGTH 18
#define ETHERNET_HDR_LENGTH 14
#define IP_HDR_LENGTH 20
#define UDP_HDR_LENGTH 8
#define AVB_ETHER_TYPE    0x22f0
#define ARRAYSIZE(arr) (sizeof(arr) / sizeof(arr[0]))

typedef struct etherheader_q
{
    unsigned char  ether_dhost[6];    // destination eth addr
    unsigned char  ether_shost[6];    // source ether addr
    unsigned int vlan_id;            // VLAN ID field
    unsigned short int ether_type;    // packet type ID field
} etherheader_q_t;

typedef struct etherheader
{
    unsigned char  ether_dhost[6];    // destination eth addr
    unsigned char  ether_shost[6];    // source ether addr
    unsigned short int ether_type;    // packet type ID field
} etherheader_t;



int enable_1722avtp_filter( FILE* filepointer, int raw_transport_socket, unsigned char *destinationMacAddress);
int create_RAW_AVB_Transport_Socket( FILE* filepointer, int* raw_transport_socket, const char* eth_dev);

#ifdef __cplusplus
}
#endif

#endif //_AVB_SOCKET_H_
