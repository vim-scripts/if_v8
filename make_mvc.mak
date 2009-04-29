# to make v8
#   > nmake -f make_mvc.mak v8
# to make gvim.exe with dll extension support
#   > nmake -f make_mvc.mak vim7
# then, make if_v8.dll
#   > nmake -f make_mvc.mak if_v8.dll

V8DIR=.\v8
V8CFLAGS=/I$(V8DIR)\include /DUSING_V8_SHARED
V8LDFLAGS=$(V8DIR)\v8.lib
CFLAGS=$(V8CFLAGS) /DWIN32 /GL /EHsc
LDFLAGS=$(V8LDFLAGS) /MD /LD gvim.lib
VIMNAME=gvim.exe

all: if_v8.dll

if_v8.dll: if_v8.cpp vimext.h gvim.lib
	cl $(CFLAGS) if_v8.cpp $(LDFLAGS)
	if exist $@.manifest mt -manifest $@.manifest -outputresource:$@;2

gvim.lib: vim_export.def
	lib /DEF:vim_export.def /OUT:gvim.lib /NAME:$(VIMNAME)

clean:
	del *.exp *.lib *.obj *.dll *.manifest



v8:
	svn co http://v8.googlecode.com/svn/trunk v8
	cd v8 && scons mode=release msvcrt=shared library=shared env="PATH:C:\Program Files\Microsoft Visual Studio 9.0\VC\bin;C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE;C:\Program Files\Microsoft Visual Studio 9.0\Common7\Tools,INCLUDE:C:\Program Files\Microsoft Visual Studio 9.0\VC\include;C:\Program Files\Microsoft SDKs\Windows\v6.0A\Include,LIB:C:\Program Files\Microsoft Visual Studio 9.0\VC\lib;C:\Program Files\Microsoft SDKs\Windows\v6.0A\Lib"


vim7:
	svn co https://vim.svn.sourceforge.net/svnroot/vim/vim7
	copy vim_export.def vim7\src
	cd vim7/src && $(MAKE) -f Make_mvc.mak linkdebug=/DEF:vim_export.def GUI=yes IME=yes MBYTE=yes

