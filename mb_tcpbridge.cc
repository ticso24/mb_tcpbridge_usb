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
#include <usb.h>

#ifndef MAXSOCK
#define MAXSOCK 10
#endif

int main(int argc, char *argv[]);
void usage(void);
int usb_get_string_ascii(usb_dev_handle *dev, int index, char *buf, size_t buflen);

static usb_dev_handle *device;
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

	// TODO: check for errors
	usb_bulk_write(device, EP_out, (char*)&packet.data[0], packetlen + 4, 1000);
	return;
}

void
FConnect::getpacket() {
	int tmp;

	tmp = usb_bulk_read(device, EP_in, (char*)&packet.data[0], 256, 1000);
	if (tmp >= 0) 
		packetlen = (uint8_t)(tmp - 4);
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
usb_get_string_ascii(usb_dev_handle *dev, int index, char *buf, size_t buflen)
{
	char tbuf[256];
	int ret, langid, si, di;

	ret = usb_get_string(dev, index, 0, tbuf, sizeof(tbuf));
	if (ret < 0)
		return ret;
	if (ret < 4)
		return -1;

	langid = tbuf[2] | (tbuf[3] << 8);

	ret = usb_get_string(dev, index, langid, tbuf, sizeof(tbuf));
	if (ret < 0)
		return ret;

	for (di = 0, si = 2; si <= tbuf[0] - 2; si += 2) {
		if (di >= (int)(buflen - 1))
		break;

		buf[di++] = tbuf[si];
	}

	buf[di] = 0;

	return di;
}

int
main(int argc, char *argv[]) {
	FConnect::Listen listen;
	int interface;
	const char* serial;
	struct usb_bus *busses;
	struct usb_bus *bus;
	int c, i, a, e;
	char tempstring[256];
	int res, ch;
	struct usb_endpoint_descriptor *ep;
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

#if 0
	usb_debug = 2;
#endif

	usb_init();
	usb_find_busses();
	usb_find_devices();
	busses = usb_get_busses();
	
	device = NULL;
	for (bus = busses; bus; bus = bus->next) {
		struct usb_device *dev;

		for (dev = bus->devices; dev; dev = dev->next) {
			/* Check if this device is a BWCT device */
			if (dev->descriptor.iManufacturer == 0)
				continue;
			device = usb_open(dev);
			res = usb_get_string_ascii(device, dev->descriptor.iManufacturer, tempstring, sizeof(tempstring));
			usb_close(device);
			device = NULL;
			if (strncmp("BWCT", tempstring, res) != 0)
				continue;
			if (serial != NULL) {
				if (dev->descriptor.iSerialNumber == 0)
					continue;
				device = usb_open(dev);
				res = usb_get_string_ascii(device, dev->descriptor.iSerialNumber, tempstring, sizeof(tempstring));		
				usb_close(device);
				device = NULL;
				if (strncmp(serial, tempstring, res) != 0)
					continue;
			}
			/* Loop through all of the configurations */
			for (c = 0; c < dev->descriptor.bNumConfigurations; c++) {
				/* Loop through all of the interfaces */
				for (i = 0; i < dev->config[c].bNumInterfaces; i++) {
					if (interface >= 0 && i != interface)
						continue;
					/* Loop through all of the alternate settings */
					for (a = 0; a < dev->config[c].interface[i].num_altsetting; a++) {
						/* Check if this interface is a ubmb */
						if (dev->config[c].interface[i].altsetting[a].bInterfaceClass == 0xff &&
						    dev->config[c].interface[i].altsetting[a].bInterfaceSubClass == 0x02) {
							if (probe) {
								device = usb_open(dev);
								res = usb_get_string_ascii(device, dev->descriptor.iProduct, tempstring, sizeof(tempstring));		
								printf ("found \"%s\" ", tempstring);
								res = usb_get_string_ascii(device, dev->descriptor.iSerialNumber, tempstring, sizeof(tempstring));		
								printf ("serial=\"%s\" ", tempstring);
								printf ("interface=%i\n", i); 
								usb_close(device);
								device = NULL;
								continue;
							}
							/* Loop through all of the endpoints */
							for (e = 0; e < dev->config[c].interface[i].altsetting[a].bNumEndpoints; e++) {
								ep = &dev->config[c].interface[i].altsetting[a].endpoint[e];
								if (ep->bDescriptorType == USB_DT_ENDPOINT &&
								    (ep->bmAttributes & USB_ENDPOINT_TYPE_MASK) == USB_ENDPOINT_TYPE_BULK) {
									if (ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
										EP_in = ep->bEndpointAddress;
									else
										EP_out = ep->bEndpointAddress;
								}
							}
							device = usb_open(dev);
							usb_claim_interface(device, i);
							goto done;
						}
					}
				}
			}
		}
	}
	done:
	if (probe != 0) {
		exit(0);
	}
	if (device == NULL) {
		printf("failed to open device\n");
		exit(1);
	}

	listen.add_tcp(argv[0], argv[1]);
	daemon(0,0);

	listen.loop();
	return 0;
}

void
usage(void) {

	printf("usage: tcpbridge [-s serial] [-i interface] ip port\n");
	printf("       tcpbridge -p\n");
	exit(1);
}

