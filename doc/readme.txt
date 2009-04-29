V8 interface to Vim

V8 is Google's open source JavaScript engine.
http://code.google.com/p/v8/


Requirements:
  c++ compiler (gcc or visual c++).
  linux or windows.
  Vim executable file with some exported symbol that if_v8 requires.
    On Linux:
      Compile vim with gcc's -rdynamic option.
    On Windows:
      Use vim_export.def when compiling.
      msvc:
        nmake -f Make_mvc.mak linkdebug=/DEF:vim_export.def
      mingw:
        make -f Make_ming.mak LFLAGS=vim_export.def


Usage:
  :source /path/to/if_v8/init.vim
  :V8 print('hello, world')
  => hello, world
  :V8 3 + 4
  => 7
  :V8 vim.execute('version')
  => ... version message
  :V8 var tw = vim.eval('&tw')
  :V8 tw
  => 78
  :V8 vim.let('&tw', '40')
  :echo &tw
  => 40
  :V8 load('foo.js')


You can access Vim's List and Dictionary from JavaScript.
For example:

  :let x = [1, 2, 3]
  :V8 var y = vim.eval('x')

Now, x and y is same object.  If you change y, x is also changed:

  :V8 y[0] = 100
  :echo x
  => [100, 2, 3]

To create List or Dictionary from JavaScript:

  :V8 var list = vim.eval('[]')
  :V8 var dict = vim.eval('{}')

Or

  :V8 var list = new vim.List()
  :V8 var dict = new vim.Dict()


Also, you can call Funcref:

  :let d = {}
  :function d.func()
  :  echo "This is d.func()"
  :endfunction
  :V8 var d = vim.eval('d')
  :V8 d.func()
  => This is d.func()
  :V8 var f = vim._function('printf')
  :V8 print(f("%s", "This is printf"))
  => This is printf

  (Since function is keyword, use vim._function or vim['function'])

When calling Vim's function, JavaScript's Array and Object are
automatically converted to Vim's List and Dictionary (copy by value).
Number and String are simply copied.

For result of Vim's function, Vim's List and Dictionary is converted to
wrapper object, VimList and VimDict (copy by reference).


To execute multi line script, use V8Start and V8End:

  :V8Start
  :V8 function f() {
  :V8   print('f()');
  :V8 }
  :V8End

Of course, you can use load() function:

  :V8 load('lib.js')

When |line-continuation| is used:

  :V8 function f() {
    \   print('f()');
    \ }

This is same as following:

  :V8 function f() { print('f()'); }


Because of implementation limit, you cannot access script local variable
with :V8 command.  To access script local variable, use V8() or V8End()
with :execute command.

  :let s:var = 99
  :execute V8('var svar = vim.eval("s:var")')

Or

  :V8Start
  :V8 var svar = vim.eval("s:var")
  :execute V8End()


if_v8 uses v:['%v8_*%'] variables for internal purpose.


TODO:
- V8 don't run garbage collector as often as possible.
