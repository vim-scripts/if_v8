# nmake -f make_mvc.mak build-v8 build-vim if_v8.dll
V8DIR=.\v8
V8CFLAGS=/I$(V8DIR)\include /DUSING_V8_SHARED
V8LDFLAGS=$(V8DIR)\v8.lib
CFLAGS=$(V8CFLAGS) /DWIN32 /GL /EHsc
LDFLAGS=$(V8LDFLAGS) /MD /LD gvim.lib
VIMNAME=gvim.exe

!IF [echo $(_NMAKE_VER) | findstr "^6\." > NUL] == 0
MSVCVER=6.0
!ELSEIF [echo $(_NMAKE_VER) | findstr "^7\." > NUL] == 0
MSVCVER=7.0
!ELSEIF [echo $(_NMAKE_VER) | findstr "^8\." > NUL] == 0
MSVCVER=8.0
!ELSEIF [echo $(_NMAKE_VER) | findstr "^9\." > NUL] == 0
MSVCVER=9.0
!ELSEIF [echo $(_NMAKE_VER) | findstr "^10\." > NUL] == 0
MSVCVER=10.0
!ELSE
!ERROR "Can't detect MSVCVER"
!ENDIF

all: if_v8.dll

if_v8.dll: if_v8.cpp vimext.h gvim.lib
	cl $(CFLAGS) if_v8.cpp $(LDFLAGS)
	if exist $@.manifest mt -manifest $@.manifest -outputresource:$@;2

gvim.lib: vim_export.def
	lib /DEF:vim_export.def /OUT:gvim.lib /NAME:$(VIMNAME)

clean:
	del *.exp *.lib *.obj *.dll *.manifest

build-v8:
	if not exist v8 svn co http://v8.googlecode.com/svn/trunk v8
	cd v8
	scons mode=release msvcrt=shared library=shared
	cd ..
	copy v8\v8.dll v8.dll

build-vim:
	if not exist vim hg clone https://vim.googlecode.com/hg/ vim
	copy vim_export.def vim\src
	cd vim\src
	$(MAKE) -f Make_mvc.mak linkdebug=/DEF:vim_export.def MSVCVER=$(MSVCVER) USE_MSVCRT=yes GUI=yes IME=yes MBYTE=yes
	cd ..\..

