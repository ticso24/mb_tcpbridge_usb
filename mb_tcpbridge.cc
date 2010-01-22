/*
 * Copyright (c) 2001 - 2004 Bernd Walter Computer Technology
 * http://www.bwct.de
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $URL$
 * $Date$
 * $Author$
 * $Rev$
 */

#include <bwct/bwct.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <termios.h>
#include <unistd.h>
#include <libusb.h>

#ifndef MAXSOCK
#define MAXSOCK 10
#endif

int main(int argc, char *argv[]);
void usage(void);

static libusb_device_handle *handle;
static Mutex device_mtx;
static int EP_in, EP_out;

class FConnect : public FTask {
private:
	union {
		uint8_t data[256];
		struct {
			uint16_t refno;
			uint8_t address;
			uint8_t function;
			uint8_t cmd[1];
		};
	} packet;
	uint8_t packetlen;
	uint8_t header[6];

	void sendpacket();
	void getpacket();
	virtual void *threadstart();
	virtual void threadend();
	void work();

	class Network : public ::Network::Net {
	public:
		Network(int nfd)
		    : ::Network::Net(nfd) {}
	};

public:
        class Listen : public ::Network::Listen {
	private:
		virtual FTask *newtask();
		virtual ::Network::Net * newcon(int clientfd);
	};
	FConnect() {
	}
	~FConnect() {
	}

};

FTask *
FConnect::Listen::newtask() {
	return new FConnect();
}

::Network::Net *
FConnect::Listen::newcon(int clientfd) {
	cassert(clientfd >= 0);
	socklen_t addrlen;
	Matrix<char> addrdt(MAXSOCKADDR);
	struct sockaddr *addr = (struct sockaddr*)addrdt.get();
	addrlen = MAXSOCKADDR;
	if (::getsockname(clientfd, addr, &addrlen) < 0)
		throw Error("getsockname failed");
	FConnect::Network* nobj;
	nobj = new FConnect::Network(clientfd);
	return nobj;
}

void *
FConnect::threadstart() {
	log("new connect");
	// TODO do a type checking cast
	((::Network::Net*)file.get())->nodelay(1);
	work();
	return NULL;
}

void
FConnect::threadend() {
	delete this;
}

void
FConnect::sendpacket() {
	int rlen;
	int err;

	// TODO: check for errors
	printf("bulk write %p %i\n", (uint8_t*)&packet.data[0], packetlen + 4);
	err = libusb_bulk_transfer(handle, EP_out, (uint8_t*)&packet.data[0], packetlen + 4, &rlen, 1000);
	printf("bulk write did %i\n", rlen);
	if (err < 0) {
		printf("bulk write got %i\n", err);
	}
	return;
}

void
FConnect::getpacket() {
	int rlen;
	int err;

	printf("bulk read %p\n", (char*)&packet.data[0]);
	libusb_bulk_transfer(handle, EP_in, (uint8_t*)&packet.data[0], 256, &rlen, 1000);
	printf("bulk read %i\n", rlen);
	if (err < 0) {
		printf("bulk read got %i\n", err);
	}
	packetlen = (uint8_t)(rlen - 4);
}

void
FConnect::work() {
	ssize_t res;
	uint8_t sbuf[256+6];

	for (;;) {
		// TODO: timeout handling
		res = file->read(&header, sizeof(header));
		if (res < (ssize_t)sizeof(header)) {
			return;
		}
		// TODO check header arguments;
		packetlen = header[4] << 8 | header[5];
		// TODO check packetlen;
		res = file->read(&packet.data[2], packetlen);
		if (res < packetlen) {
			return;
		}
		packetlen -= 2;	// drop address and function bytes
		// TODO add refno handling to ubmb
		device_mtx.lock();
		sendpacket();
		getpacket();
		device_mtx.unlock();
		packetlen += 2; // add address and function bytes
		header[0] = 0;
		header[4] = 0;
		header[5] = packetlen;
		memcpy(&sbuf[0], header, sizeof(header));
		memcpy(&sbuf[sizeof(header)], &packet.data[2], packetlen);
		file->write(sbuf, sizeof(header) + packetlen);
	}
}

int
main(int argc, char *argv[]) {
	FConnect::Listen listen;
	int interface;
	const char* serial;
	int i, a, e;
	char tempstring[256];
	int res, ch;
	struct libusb_endpoint_descriptor *ep;
	char probe;

	interface = -1;
	serial = NULL;
	probe = 0;

	while ((ch = getopt(argc, argv, "i:ps:")) != -1)
		switch (ch) {
		case 'i':
			interface = atol(optarg);
			break;
		case 'p':
			probe = 1;
			break;
		case 's':
			serial = optarg;
			break;
		case '?':
			default:
			usage();
			/* NOTREACHED */
	}
	argc -= optind;
	argv += optind;

	if (argc != 2 && probe == 0)
		usage();

	libusb_context *ctx;
	libusb_init(&ctx);
	libusb_device **devlist;
	ssize_t devcnt = libusb_get_device_list(NULL, &devlist);
	
	int devno;
	for (devno = 0; devno < devcnt; devno++) {
		libusb_device *device = devlist[devno];
		libusb_device_descriptor dev;
		libusb_get_device_descriptor(device, &dev);
		//printf("scan device\n");
		/* Check if this device is a BWCT device */
		if (dev.iManufacturer == 0)
			continue;
		libusb_open(device, &handle);
		res = libusb_get_string_descriptor_ascii(handle, dev.iManufacturer, (uint8_t*)tempstring, sizeof(tempstring));
		libusb_close(handle);
		handle = NULL;
		//printf("found device %s\n", tempstring);
		if (strncmp("BWCT", tempstring, res) != 0)
			continue;
		if (serial != NULL) {
			if (dev.iSerialNumber == 0)
				continue;
			libusb_open(device, &handle);
			res = libusb_get_string_descriptor_ascii(handle, dev.iSerialNumber, (uint8_t*)tempstring, sizeof(tempstring));		
			libusb_close(handle);
			handle = NULL;
			if (strncmp(serial, tempstring, res) != 0)
				continue;
		}
		//printf("scan configuration %i\n", c);
		/* Loop through all of the interfaces */
		libusb_config_descriptor *config;
		libusb_get_config_descriptor(device, 0, &config);
		for (i = 0; i < config->bNumInterfaces; i++) {
			//printf("scan interface %i\n", i);
			if (interface >= 0 && i != interface)
				continue;
			/* Loop through all of the alternate settings */
			for (a = 0; a < config->interface[i].num_altsetting; a++) {
				/* Check if this interface is a ubmb */
				if (config->interface[i].altsetting[a].bInterfaceClass == 0xff &&
				    config->interface[i].altsetting[a].bInterfaceSubClass == 0x02) {
					if (probe) {
						libusb_open(device, &handle);
						res = libusb_get_string_descriptor_ascii(handle, dev.iProduct, (uint8_t*)tempstring, sizeof(tempstring));		
						printf("found \"%s\" ", tempstring);
						res = libusb_get_string_descriptor_ascii(handle, dev.iSerialNumber, (uint8_t*)tempstring, sizeof(tempstring));		
						printf("serial=\"%s\" ", tempstring);
						printf("interface=%i\n", i); 
						libusb_close(handle);
						handle = NULL;
						continue;
					}
					/* Loop through all of the endpoints */
					for (e = 0; e < config->interface[i].altsetting[a].bNumEndpoints; e++) {
						ep = &config->interface[i].altsetting[a].endpoint[e];
						if (ep->bDescriptorType == LIBUSB_DT_ENDPOINT &&
						    (ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) == LIBUSB_TRANSFER_TYPE_BULK) {
							if (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK)
								EP_in = ep->bEndpointAddress;
							else
								EP_out = ep->bEndpointAddress;
						}
					}
					libusb_open(device, &handle);
					libusb_claim_interface(handle, i);
					goto done;
				}
			}
		}
	}
	done:
	if (probe != 0) {
		exit(0);
	}
	if (handle == NULL) {
		printf("failed to open device\n");
		exit(1);
	}

	listen.add_tcp(argv[0], argv[1]);
	//daemon(0,0);

	listen.loop();
	return 0;
}

void
usage(void) {

	printf("usage: mb_tcpbridge [-s serial] [-i interface] ip port\n");
	printf("       mb_tcpbridge -p\n");
	exit(1);
}

