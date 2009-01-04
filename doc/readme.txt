V8 interface to Vim

V8 is Google's open source JavaScript engine.
http://code.google.com/p/v8/


Requirements:
  linux or windows.
  Vim executable file with some exported symbol that if_v8 requires.
    On linux:
      Compile with gcc's -rdynamic option.
    On windows (msvc):
      Use vim_export.def and add linker flag "/DEF:vim_export.def".
      nmake -f Make_mvc.mak linkdebug=/DEF:vim_export.def


Usage:
  let if_v8 = '/path/to/if_v8.so'
  let err = libcall(if_v8, 'init', if_v8)
  let err = libcall(if_v8, 'execute', 'load("/path/to/runtime.js")')
  let err = libcall(if_v8, 'execute', 'vim.execute("echo \"hello, v8\"")')
  let err = libcall(if_v8, 'execute', 'var tw = vim.eval("&tw")')
  let err = libcall(if_v8, 'execute', 'print(tw)')
  let err = libcall(if_v8, 'execute', 'load("foo.js")')


if_v8 libcall returns error message for initialization error.  Normal
execution time error is raised in the same way as :echoerr command.


if_v8 uses g:['%v8*%'] variables for internal purpose.


You can access Vim's List and Dictionary from JavaScript.
For example:

  :let x = [1, 2, 3]
  :call libcall(if_v8, 'execute', 'var y = vim.eval("x")')

Now, x and y is same object.  If you change y, x is also changed:

  :call libcall(if_v8, 'execute', 'y[0] = 100')
  :echo x
  => [100, 2, 3]

To create List or Dictionary from JavaScript:

  :call libcall(if_v8, 'execute', 'var list = vim.eval("[]")')
  :call libcall(if_v8, 'execute', 'var dict = vim.eval("{}")')

Or

  :call libcall(if_v8, 'execute', 'var list = new vim.List()')
  :call libcall(if_v8, 'execute', 'var dict = new vim.Dict()')


When calling Vim's function, JavaScript's Array and Object are
automatically converted to Vim's List and Dictionary (copy by value).
Number and String are simply copied.

For result of Vim's function, Vim's List and Dictionary is converted to
wrapper object, VimList and VimDict (copy by reference).


TODO:
- V8 execute garbage collector not so often as Vim does.
- Convert Funcref to function.  VimFunc object?
