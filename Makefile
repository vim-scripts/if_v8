CXX=g++
V8DIR=./v8
V8TARGET=native
V8CFLAGS=-I$(V8DIR) -I$(V8DIR)/include
V8LDFLAGS=-L$(V8DIR)/out/$(V8TARGET)/obj.target/tools/gyp -L$(V8DIR)/out/$(V8TARGET)/obj.target/third_party/icu/ -lv8_libplatform -lv8_base -lv8_libbase -lv8_snapshot -licui18n -licuuc -licudata -lpthread
CFLAGS=$(V8CFLAGS) -W -Wall -Werror -Wno-unused-parameter -fPIC
LDFLAGS=$(V8LDFLAGS) -shared

# To build v8 static libs for if_v8.so shared library, add -fPIC flag.
# $ cd v8 && CFLAGS=-fPIC CXXFLAGS=-fPIC make native

all: if_v8.so

if_v8.so: if_v8.cpp vimext.h
	$(CXX) $(CFLAGS) -o $@ if_v8.cpp $(LDFLAGS)

clean:
	rm if_v8.so

build-v8:
	test -d v8 || svn co http://v8.googlecode.com/svn/trunk v8
	cd v8 && CFLAGS=-fPIC CXXFLAGS=-fPIC make builddeps $(V8TARGET)

build-vim:
	test -d vim || hg clone https://vim.googlecode.com/hg/ vim
	cd vim && LDFLAGS=-rdynamic ./configure && make

