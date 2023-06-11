CC=gcc
CFLAGS=-Iinclude -Wall -Werror -g -Wno-unused

SSRC=$(shell find src -name '*.c')
DEPS=$(shell find include -name '*.h')

LIBS=-lpthread

all: setup server 

setup:
	mkdir -p bin 
	cp lib/zotVote_client bin/zotVote_client

server: setup
	$(CC) $(CFLAGS) $(SSRC) lib/protocol.o -o bin/zotVote_server $(LIBS)
	
.PHONY: clean

clean:
	rm -rf bin 
