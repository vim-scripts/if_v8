
let s:dir = expand('<sfile>:p:h')
let s:dll = s:dir . '/if_v8' . (has('win32') ? '.dll' : '.so')
let s:runtime = [
      \ s:dir . '/runtime.js',
      \ ]

function! V8Init()
  if exists('s:init')
    return
  endif
  let s:init = 1
  echo libcall(s:dll, 'init', s:dll)
  for file in s:runtime
    echo libcall(s:dll, 'execute', printf("this['%%internal%%'].load(\"%s\")", escape(file, '\"')))
  endfor
endfunction

function! V8ExecuteX(expr)
  return printf("libcall(\"%s\", 'execute', \"%s\")", escape(s:dll, '\"'), escape(a:expr, '\"'))
endfunction

function! V8LoadX(file)
  return V8ExecuteX(printf('load("%s")', escape(a:file, '\"')))
endfunction

function! V8EvalX(expr)
  let x = V8ExecuteX(printf("vim.let('g:[\"%v8_result%\"]', eval(\"%s\"))", '(' . escape(a:expr, '\"') . ')'))
  return printf("eval(get({}, %s, 'g:[\"v8_result%\"]'))", x)
endfunction

function! V8Load(file)
  return eval(V8LoadX(a:file))
endfunction

function! V8Execute(expr)
  return eval(V8ExecuteX(a:expr))
endfunction

function! V8Eval(expr)
  return eval(V8EvalX(a:expr))
endfunction

call V8Init()
