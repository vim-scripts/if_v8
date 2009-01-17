
V8DIR=..\..\v8

CFLAGS=/I$(V8DIR)\include /DWIN32
LDFLAGS=$(V8DIR)\v8.lib
VIMNAME=gvim.exe

all: if_v8.dll

if_v8.dll: if_v8.cpp vim_export.lib
	cl $(CFLAGS) /LD if_v8.cpp $(LDFLAGS) vim_export.lib
	rem mt -manifest if_v8.dll.manifest -outputresource:if_v8.dll;2

vim_export.lib: vim_export.def
	lib /DEF:vim_export.def /OUT:vim_export.lib /NAME:$(VIMNAME)

clean:
	del *.exp *.lib *.obj *.dll
