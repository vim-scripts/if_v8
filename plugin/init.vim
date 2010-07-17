
command! V8Start call s:lib.v8start()
command! V8End execute V8End()
command! -nargs=* V8 execute V8(<q-args>, expand('<sfile>') == '')

augroup V8
  au!
  autocmd CursorHold,CursorHoldI * execute s:lib.v8execute("gc()")
augroup END

function! V8End()
  return s:lib.v8end()
endfunction

function! V8(...)
  return call(s:lib.v8execute, a:000, s:lib)
endfunction

" usage:
"   :let res = eval(V8Eval('3 + 4'))
function! V8Eval(expr)
  let result = '"v:[''%v8_result%'']"'
  let jsexpr = 'vim.v["%v8_result%"] = eval("(' . escape(a:expr, '\"') . ')")'
  let expr = s:lib.v8expr(jsexpr)
  return printf("eval([%s, %s][0])", result, expr)
endfunction



let s:lib = {}

let s:lib.dir = expand('<sfile>:p:h')
let s:lib.dll = s:lib.dir . '/if_v8' . (has('win32') ? '.dll' : '.so')
let s:lib.runtime = [
      \ s:lib.dir . '/runtime.js',
      \ ]
let s:lib.flags = '--expose-gc'

function s:lib.init() abort
  if exists('s:init')
    return
  endif
  let s:init = 1
  if has('win32')
    " If if_v8.dll links to separated v8.dll, we need to set $PATH.
    let path_save = $PATH
    let $PATH .= ';' . self.dir
  endif
  let err = libcall(self.dll, 'init', self.dll . "," . self.flags)
  if has('win32')
    let $PATH = path_save
  endif
  if err != ''
    echoerr err
  endif
  for file in self.runtime
    call libcall(self.dll, 'execute', printf("load(\"%s\")", escape(file, '\"')))
  endfor
endfunction

function s:lib.v8start()
  let self.script = []
endfunction

function s:lib.v8end()
  let cmd = join(self.script, "\n") . "\n"
  unlet self.script
  return self.v8execute(cmd, 0)
endfunction

function s:lib.v8execute(cmd, ...)
  let interactive = get(a:000, 0, 0)
  if exists('self.script')
    call add(self.script, a:cmd)
    return ''
  endif
  let cmd = self.v8expr(a:cmd)
  if interactive
    " interactive mode
    return ""
        \ . "try\n"
        \ . "  let v:['%v8_print%'] = ''\n"
        \ . "  echo expand('<args>')\n"
        \ . "  call eval(\"" . escape(cmd, '\"') . "\")\n"
        \ . "  echo v:['%v8_print%']\n"
        \ . "catch\n"
        \ . "  echohl Error\n"
        \ . "  for v:['%v8_line%'] in split(v:['%v8_errmsg%'], '\\n')\n"
        \ . "    echomsg v:['%v8_line%']\n"
        \ . "  endfor\n"
        \ . "  echohl None\n"
        \ . "endtry\n"
  else
    return "call eval(\"" . escape(cmd, '\"') . "\")"
  endif
endfunction

function s:lib.v8expr(expr)
  return printf("libcall(\"%s\", 'execute', \"%s\")", escape(self.dll, '\"'), escape(a:expr, '\"'))
endfunction

call s:lib.init()
