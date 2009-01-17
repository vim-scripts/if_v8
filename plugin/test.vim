so <sfile>:p:h/init.vim

function! s:Test(name, expr)
  echo a:name ":" a:expr
  let msg = printf("%s failed: %s", a:name, a:expr)
  return printf('if !(%s) | throw "%s" | endif', a:expr, escape(msg, '\"'))
endfunction

V8Start
V8 function Test(name, expr) {
V8   print(name + " : " + expr);
V8   var msg = vim.printf("%s failed: %s", name, expr);
V8   return vim.printf('if (!(%s)) { throw "%s"; }', expr, vim.escape(msg, "\\\""));
V8 }
V8End

let s:test = {}

" test1: hello world
function s:test.test1()
  V8 print("hello, world")
endfunction

" test2: exception: v8 -> vim
function s:test.test2()
  let ok = 1
  try
    V8 throw "error from v8"
    let ok = 0
  catch
    echo printf('caught in vim "%s"', v:exception)
  endtry
  execute s:Test("test2", "ok")
endfunction

" test3: exception: vim -> v8
function s:test.test3()
  V8Start
  V8 var ok = 1;
  V8 try {
  V8   vim.execute('throw "error from vim"');
  V8   ok = 0;
  V8 } catch (e) {
  V8   print('caught in v8 "' + e + '"');
  V8 }
  V8 eval(Test("test3", "ok"));
  V8End
endfunction

" test4: exception: vim -> v8 -> vim
function s:test.test4()
  let ok = 1
  try
    V8 vim.execute('throw "error from vim"')
    let ok = 0
  catch
    echo printf('caught in vim "%s"', v:exception)
  endtry
  execute s:Test("test4", "ok")
endfunction

" test5: accessing local variable
function s:test.test5()
  let x = 1
  V8 print(vim.eval("x"))
  V8 vim.let("x", 2)
  echo x
  let x = eval(V8Eval('1 + 2'))
  echo x
endfunction

" test6: invoking vim's function
function s:test.test6()
  V8Start
  V8 vim.execute('new');
  V8 vim.append("$", "line1");
  V8 vim.execute("redraw! | sleep 200m");
  V8 vim.append("$", "line2");
  V8 vim.execute("redraw! | sleep 200m");
  V8 vim.append("$", ["line3", "line4"]);
  V8 vim.execute("redraw! | sleep 200m");
  V8 vim.execute('quit!');
  V8End
endfunction

" test7: recursive object: vim -> v8
function s:test.test7()
  let x = {}
  let x.x = x
  let y = []
  let y += [y]
  V8Start
  V8 var x = vim.eval("x");
  V8 eval(Test("test7", "x === x.x"));
  V8 var y = vim.eval("y");
  V8 eval(Test("test7", "y === y[0]"));
  V8End
endfunction

" test8: recursive object: v8 -> vim
function s:test.test8()
  V8Start
  V8 var x = {};
  V8 x.x = x;
  V8 var y = [];
  V8 y[0] = y;
  V8End
  let x = eval(V8Eval('x'))
  execute s:Test("test8", "x is x.x")
  let y = eval(V8Eval('y'))
  execute s:Test("test8", "y is y[0]")
endfunction

" test9: VimList 1
function s:test.test9()
  let x = [1, 2, 3]
  echo x
  V8Start
  V8 var x = vim.eval('x')
  V8 x[0] += 100; x[1] += 100; x[2] += 100;
  V8End
  echo x
  execute s:Test("test9", "x[0] == 101 && x[1] == 102 && x[2] == 103")
endfunction

" test10: VimList 2
function s:test.test10()
  V8Start
  V8 var x = new vim.List()
  V8 vim.extend(x, [1, 2, 3])
  V8End
  let x = eval(V8Eval('x'))
  echo x
  let x[0] += 100
  let x[1] += 100
  let x[2] += 100
  V8Start
  V8 print(x[0] + " " + x[1] + " " + x[2])
  V8 eval(Test("test10", "x[0] == 101 && x[1] == 102 && x[2] == 103"));
  V8End
endfunction

" test11: VimDict 1
function s:test.test11()
  let x = {}
  V8Start
  V8 var x = vim.eval("x")
  V8 x["apple"] = "orange"
  V8 x[9] = "nine"
  V8End
  echo x
  execute s:Test("test11", 'x["apple"] == "orange" && x[9] == "nine"')
endfunction

" test12: VimDict 2
function s:test.test12()
  V8 var x = new vim.Dict()
  let x = eval(V8Eval('x'))
  let x["apple"] = "orange"
  let x[9] = "nine"
  V8Start
  V8 print('x["apple"] = ' + x["apple"])
  V8 print('x[9] = ' + x[9])
  V8 eval(Test("test12", 'x["apple"] == "orange" && x[9] == "nine"'));
  V8End
endfunction

let s:d = {}
let s:d.name = 'd'
function s:d.func()
  echo "my name is " . self.name
  return self
endfunction
function s:d.raise(msg)
  throw a:msg
endfunction
let s:d.printf = function("printf")

let s:e = {}
let s:e.name = 'e'

" test13: VimFunc
function s:test.test13()
  V8Start
  V8 var d = vim.eval("s:d")
  V8 var e = vim.eval("s:e")
  V8 eval(Test("test13", "d.func() === d"))
  V8 e.func = d.func
  V8 eval(Test("test13", "e.func() === e"))
  V8 print(d.printf("%s", "This is printf"))
  execute V8End()
endfunction

" test14: VimFunc Exception
function s:test.test14()
  V8Start
  V8 var d = vim.eval("s:d")
  execute V8End()
  let ok = 1
  try
    V8 d.raise("error from vimfunc")
    let ok = 0
  catch
    echo printf('caught in vim "%s"', v:exception)
  endtry
  execute s:Test("test14", "ok")
endfunction

function! s:mysort(a, b)
  let a = matchstr(a:a, '\d\+')
  let b = matchstr(a:b, '\d\+')
  return a - b
endfunction

try
  for s:name in sort(keys(s:test), 's:mysort')
    echo "\n" . s:name . "\n"
    call s:test[s:name]()
    " XXX: message is not shown when more prompt is not fired.
    sleep 100m
  endfor
endtry
