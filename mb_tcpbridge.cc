/*
 * Copyright (c) 2001 - 2003 Bernd Walter Computer Technology
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

static File device;
static Mutex device_mtx;

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
	device.microwrite(&packet.data[0], packetlen + 4);
	return;
}

void
FConnect::getpacket() {
	int tmp;

	tmp = device.microread(&packet.data[0], 256);
	if (tmp >= 0) 
		packetlen = (uint8_t)(tmp - 4);
}

void
FConnect::work() {
	ssize_t res;

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
		file->write(&header, sizeof(header));
		file->write(&packet.data[2], packetlen);
	}
}

int
main(int argc, char *argv[]) {
	char *devname;
	FConnect::Listen listen;

	if (argc <4)
		usage();
	devname = argv[1];

	device.open(devname, O_RDWR);

	listen.add_tcp(argv[2], argv[3]);
	daemon(0,0);

	listen.loop();
	return 0;
}

void
usage(void) {

	printf("usage: tcpbridge ubmbdev ip port\n");
	exit(1);
}

