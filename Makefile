
V8DIR=$(HOME)/tmp/v8

CFLAGS=-I$(V8DIR)/include -W -Wall -Werror -Wno-unused-parameter -fPIC
LDFLAGS=-L$(V8DIR) -lv8 -lpthread

all: if_v8.so

if_v8.so: if_v8.cpp
	g++ $(CFLAGS) -shared -o if_v8.so if_v8.cpp $(LDFLAGS)

clean:
	rm if_v8.so
