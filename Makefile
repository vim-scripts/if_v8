
CFLAGS=-I$(HOME)/tmp/v8/include
LDFLAGS=-L$(HOME)/tmp/v8 -lv8 -lpthread

all: if_v8.so

if_v8.so: if_v8.cpp
	g++ $(CFLAGS) -shared -o if_v8.so if_v8.cpp $(LDFLAGS)

clean:
	rm if_v8.so
