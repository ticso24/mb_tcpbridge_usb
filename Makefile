#
# Copyright (c) 2001,2002 Bernd Walter Computer Technology
# http://www.bwct.de.de
# All rights reserved.
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

CXX = g++
CFLAGS = -g -Wall -I/usr/local/include
CFLAGS +=`libbwct-config --cflags`
LDFLAGS = -L/usr/local/lib -lusb

BIN = mb_tcpbridge
OBJ = mb_tcpbridge.o

all: $(BIN)

clean:
	rm -f $(BIN) $(OBJ) $(BIN).core

$(BIN): $(OBJ)
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ) `libbwct-config --libs`

.c.o:
	$(CXX) $(CFLAGS) -c $<
