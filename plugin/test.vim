so <sfile>:p:h/init.vim

let s:test = {}

" test1: hello world
function s:test.test1()
  call V8Execute('print("hello, world")')
endfunction

" test2: exception: v8 -> vim
function s:test.test2()
  try
    call V8Execute('throw "error from v8"')
  catch
    echo printf('caught in vim "%s"', v:exception)
  endtry
endfunction

" test3: exception: vim -> v8
function s:test.test3()
  call V8Execute("try { vim.execute('throw \"error from vim\"'); } catch (e) { print('caught in v8 \"' + e + '\"'); }")
endfunction

" test4: exception: vim -> v8 -> vim
function s:test.test4()
  try
    call V8Execute("vim.execute('throw \"error from vim\"')")
  catch
    echo printf('caught in vim "%s"', v:exception)
  endtry
endfunction

" test5: accessing local variable
function s:test.test5()
  let x = 1
  " We have to use eval() trick to execute v8 script in the caller context.
  " Otherwise we cannot access function local variable.
  call eval(V8ExecuteX('print(vim.eval("x")); vim.let("x", 2);'))
  echo x
  " Of course, V8Eval can be used.
  let x = V8Eval('1 + 2')
  echo x
endfunction

" test6: invoking vim's function
function s:test.test6()
  new
  call V8Execute('vim.append("$", "line1")')
  redraw! | sleep 200m
  call V8Execute('vim.append("$", "line2")')
  redraw! | sleep 200m
  call V8Execute('vim.append("$", "line3")')
  redraw! | sleep 200m
  quit!
endfunction

" test7: recursive object: vim -> v8
function s:test.test7()
  let x = {}
  let x.x = x
  let y = []
  let y += [y]
  call eval(V8ExecuteX('var x = vim.eval("x")'))
  call eval(V8ExecuteX('var y = vim.eval("y")'))
  call eval(V8ExecuteX('if (x === x.x) {print("x === x.x");} else {throw "x !== x.x";}'))
  call eval(V8ExecuteX('if (y === y[0]) {print("y === y[0]");} else {throw "y !== y[0]";}'))
endfunction

" test8: recursive object: v8 -> vim
function s:test.test8()
  call V8Execute('var x = {}')
  call V8Execute('x.x = x')
  call V8Execute('var y = []')
  call V8Execute('y[0] = y')
  let x = V8Eval('x')
  let y = V8Eval('y')
  if x is x.x
    echo "x is x.x"
  else
    throw "x isnot x.x"
  endif
  if y is y[0]
    echo "y is y[0]"
  else
    throw "y isnot y[0]"
  endif
endfunction

" test9: VimList 1
function s:test.test9()
  let x = [1, 2, 3]
  echo x
  call eval(V8ExecuteX('var x = vim.eval("x")'))
  call eval(V8ExecuteX('x[0] += 100; x[1] += 100; x[2] += 100;'))
  echo x
  if x[0] != 101 || x[1] != 102 || x[2] != 103
    throw "test9 faield"
  endif
endfunction

" test10: VimList 2
function s:test.test10()
  call eval(V8ExecuteX('var x = new vim.List()'))
  call eval(V8ExecuteX('vim.extend(x, [1, 2, 3])'))
  let x = V8Eval('x')
  echo x
  let x[0] += 100
  let x[1] += 100
  let x[2] += 100
  call eval(V8ExecuteX('print(x[0] + " " + x[1] + " " + x[2])'))
  call eval(V8ExecuteX('if (x[0] != 101 || x[1] != 102 || x[2] != 103) { throw "test10 failed"; }'))
endfunction

" test11: VimDict 1
function s:test.test11()
  let x = {}
  call eval(V8ExecuteX('var x = vim.eval("x")'))
  call eval(V8ExecuteX('x["apple"] = "orange"'))
  echo x
  if x["apple"] != "orange"
    throw "test11 failed"
  endif
endfunction

" test12: VimDict 2
function s:test.test12()
  call eval(V8ExecuteX('var x = new vim.Dict()'))
  let x = V8Eval('x')
  let x["apple"] = "orange"
  call eval(V8ExecuteX('print("x[\"apple\"] = " + x["apple"])'))
  call eval(V8ExecuteX('if (x["apple"] != "orange") { throw "test12 failed"; }'))
endfunction

function! s:mysort(a, b)
  let a = matchstr(a:a, '\d\+')
  let b = matchstr(a:b, '\d\+')
  return a - b
endfunction

for s:name in sort(keys(s:test), 's:mysort')
  echo "\n" . s:name . "\n"
  call s:test[s:name]()
  " XXX: message is not shown when more prompt is not fired.
  sleep 100m
endfor
