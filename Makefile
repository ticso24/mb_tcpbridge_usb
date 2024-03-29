#
# Copyright (c) 2004 Bernd Walter Computer Technology
# http://www.bwct.de.de
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the author nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $URL$
# $Date$
# $Author$
# $Rev$
#

CFLAGS = -O2 -g -Wall -Wsystem-headers -Wno-format-y2k -Wno-uninitialized -I/usr/local/include -I/usr/include/libusb-1.0`libbwctmb-config --cflags`
LDFLAGS = -L/usr/local/lib -lusb `libbwct-config --libs`
# Debian sid has -lusb-1.0

BIN = mb_tcpbridge_usb
OBJ = mb_tcpbridge_usb.o
BINDIR ?= /usr/local/sbin

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ) $(BIN).core

$(BIN): $(OBJ)
	$(CXX) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

.cc.o:
	$(CXX) $(CFLAGS) -c $<

install:
	mkdir -p $(BINDIR)
	install $(BIN) $(BINDIR)
