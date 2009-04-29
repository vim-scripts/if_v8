
V8DIR=./v8
V8CFLAGS=-I$(V8DIR)/include
V8LDFLAGS=-L$(V8DIR) -lv8 -lpthread
CFLAGS=$(V8CFLAGS) -W -Wall -Werror -Wno-unused-parameter -fPIC
LDFLAGS=$(V8LDFLAGS) -shared

all: if_v8.so

if_v8.so: if_v8.cpp vimext.h
	g++ $(CFLAGS) -o $@ if_v8.cpp $(LDFLAGS)

clean:
	rm if_v8.so



v8:
	svn co http://v8.googlecode.com/svn/trunk v8
	cd v8 && scons mode=release

vim7:
	svn co https://vim.svn.sourceforge.net/svnroot/vim/vim7
	cd vim7/src && LDFLAGS=-rdynamic ./configure && make
