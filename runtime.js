var vim = {};

(function(global, vim){

  var internal = global['%internal%'];

  var makefunc = function(name) {
    return function() {
      var args = [];
      for (var i = 0; i < arguments.length; i++) {
        args.push(arguments[i]);
      }
      return vim.call(name, args);
    };
  };

  global.load = function(file) {
    var save = global['%script_name%'];
    global['%script_name%'] = file;
    try {
      internal.load(file)
    } finally {
      global['%script_name%'] = save;
    }
  };

  global.echo = function(obj) {
    vim.echo(String(obj));
  };

  global.print = global.echo;

  vim.List = internal.VimList;
  vim.Dict = internal.VimDict;

  vim.ListToArray = function(list) {
    var arr = new Array(list.length);
    for (var i in list) {
      arr.push(list[i]);
    }
    return arr;
  };

  vim.ArrayToList = function(arr) {
    return vim.extend(new vim.List(), arr);
  };

  vim.DictToObject = function(dict) {
    var o = {}
    for (var key in dict) {
      o[key] = dict[key];
    }
    return o;
  };

  vim.ObjectToDict = function(obj) {
    return vim.extend(new vim.Dict(), obj);
  };

  vim.execute = function(cmd) {
    internal.vim_execute("execute g:['%v8_args%'][1] | let g:['%v8_result%'] = 0", cmd);
  };

  vim.call = function(funcname, args, obj) {
    if (obj === undefined) {
      return internal.vim_execute("let g:['%v8_result%'] = call(g:['%v8_args%'][1], g:['%v8_args%'][2])", funcname, args);
    } else {
      return internal.vim_execute("let g:['%v8_result%'] = call(g:['%v8_args%'][1], g:['%v8_args%'][2])", funcname, args, obj);
    }
  };

  vim.let = function(varname, value) {
    internal.vim_execute("execute 'let ' . g:['%v8_args%'][1] . ' = g:[''%v8_args%''][2]' | let g:['%v8_result%'] = 0", varname, value);
  };

  vim.echo = function(obj) {
    internal.vim_execute("echo g:['%v8_args%'][1] | let g:['%v8_result%'] = 0", obj);
  };

  vim.abs = makefunc('abs');
  vim.add = makefunc('add');
  vim.append = makefunc('append');
  vim.argc = makefunc('argc');
  vim.argidx = makefunc('argidx');
  vim.argv = makefunc('argv');
  vim.atan = makefunc('atan');
  vim.browse = makefunc('browse');
  vim.browsedir = makefunc('browsedir');
  vim.bufexists = makefunc('bufexists');
  vim.buflisted = makefunc('buflisted');
  vim.bufloaded = makefunc('bufloaded');
  vim.bufname = makefunc('bufname');
  vim.bufnr = makefunc('bufnr');
  vim.bufwinnr = makefunc('bufwinnr');
  vim.byte2line = makefunc('byte2line');
  vim.byteidx = makefunc('byteidx');
  //vim.call = makefunc('call');
  vim.ceil = makefunc('ceil');
  vim.changenr = makefunc('changenr');
  vim.char2nr = makefunc('char2nr');
  vim.cindent = makefunc('cindent');
  vim.clearmatches = makefunc('clearmatches');
  vim.col = makefunc('col');
  vim.complete = makefunc('complete');
  vim.complete_add = makefunc('complete_add');
  vim.complete_check = makefunc('complete_check');
  vim.confirm = makefunc('confirm');
  vim.copy = makefunc('copy');
  vim.cos = makefunc('cos');
  vim.count = makefunc('count');
  vim.cscope_connection = makefunc('cscope_connection');
  vim.cursor = makefunc('cursor');
  vim.deepcopy = makefunc('deepcopy');
  vim['delete'] = makefunc('delete');
  vim._delete = vim['delete'];
  vim.did_filetype = makefunc('did_filetype');
  vim.diff_filler = makefunc('diff_filler');
  vim.diff_hlID = makefunc('diff_hlID');
  vim.empty = makefunc('empty');
  vim.escape = makefunc('escape');
  vim.eval = makefunc('eval');
  vim.eventhandler = makefunc('eventhandler');
  vim.executable = makefunc('executable');
  vim.exists = makefunc('exists');
  vim.expand = makefunc('expand');
  vim.extend = makefunc('extend');
  vim.feedkeys = makefunc('feedkeys');
  vim.filereadable = makefunc('filereadable');
  vim.filewritable = makefunc('filewritable');
  vim.filter = makefunc('filter');
  vim.finddir = makefunc('finddir');
  vim.findfile = makefunc('findfile');
  vim.float2nr = makefunc('float2nr');
  vim.floor = makefunc('floor');
  vim.fnameescape = makefunc('fnameescape');
  vim.fnamemodify = makefunc('fnamemodify');
  vim.foldclosed = makefunc('foldclosed');
  vim.foldclosedend = makefunc('foldclosedend');
  vim.foldlevel = makefunc('foldlevel');
  vim.foldtext = makefunc('foldtext');
  vim.foldtextresult = makefunc('foldtextresult');
  vim.foreground = makefunc('foreground');
  vim['function'] = makefunc('function');
  vim._function = vim['function'];
  vim.garbagecollect = makefunc('garbagecollect');
  vim.get = makefunc('get');
  vim.getbufline = makefunc('getbufline');
  vim.getbufvar = makefunc('getbufvar');
  vim.getchar = makefunc('getchar');
  vim.getcharmod = makefunc('getcharmod');
  vim.getcmdline = makefunc('getcmdline');
  vim.getcmdpos = makefunc('getcmdpos');
  vim.getcmdtype = makefunc('getcmdtype');
  vim.getcwd = makefunc('getcwd');
  vim.getfontname = makefunc('getfontname');
  vim.getfperm = makefunc('getfperm');
  vim.getfsize = makefunc('getfsize');
  vim.getftime = makefunc('getftime');
  vim.getftype = makefunc('getftype');
  vim.getline = makefunc('getline');
  vim.getloclist = makefunc('getloclist');
  vim.getmatches = makefunc('getmatches');
  vim.getpid = makefunc('getpid');
  vim.getpos = makefunc('getpos');
  vim.getqflist = makefunc('getqflist');
  vim.getreg = makefunc('getreg');
  vim.getregtype = makefunc('getregtype');
  vim.gettabwinvar = makefunc('gettabwinvar');
  vim.getwinposx = makefunc('getwinposx');
  vim.getwinposy = makefunc('getwinposy');
  vim.getwinvar = makefunc('getwinvar');
  vim.glob = makefunc('glob');
  vim.globpath = makefunc('globpath');
  vim.has = makefunc('has');
  vim.has_key = makefunc('has_key');
  vim.haslocaldir = makefunc('haslocaldir');
  vim.hasmapto = makefunc('hasmapto');
  vim.histadd = makefunc('histadd');
  vim.histdel = makefunc('histdel');
  vim.histget = makefunc('histget');
  vim.histnr = makefunc('histnr');
  vim.hlID = makefunc('hlID');
  vim.hlexists = makefunc('hlexists');
  vim.hostname = makefunc('hostname');
  vim.iconv = makefunc('iconv');
  vim.indent = makefunc('indent');
  vim.index = makefunc('index');
  vim.input = makefunc('input');
  vim.inputdialog = makefunc('inputdialog');
  vim.inputlist = makefunc('inputlist');
  vim.inputrestore = makefunc('inputrestore');
  vim.inputsave = makefunc('inputsave');
  vim.inputsecret = makefunc('inputsecret');
  vim.insert = makefunc('insert');
  vim.isdirectory = makefunc('isdirectory');
  vim.islocked = makefunc('islocked');
  vim.items = makefunc('items');
  vim.join = makefunc('join');
  vim.keys = makefunc('keys');
  vim.len = makefunc('len');
  vim.libcall = makefunc('libcall');
  vim.libcallnr = makefunc('libcallnr');
  vim.line = makefunc('line');
  vim.line2byte = makefunc('line2byte');
  vim.lispindent = makefunc('lispindent');
  vim.localtime = makefunc('localtime');
  vim.log10 = makefunc('log10');
  vim.map = makefunc('map');
  vim.maparg = makefunc('maparg');
  vim.mapcheck = makefunc('mapcheck');
  vim.match = makefunc('match');
  vim.matchadd = makefunc('matchadd');
  vim.matcharg = makefunc('matcharg');
  vim.matchdelete = makefunc('matchdelete');
  vim.matchend = makefunc('matchend');
  vim.matchlist = makefunc('matchlist');
  vim.matchstr = makefunc('matchstr');
  vim.max = makefunc('max');
  vim.min = makefunc('min');
  vim.mkdir = makefunc('mkdir');
  vim.mode = makefunc('mode');
  vim.nextnonblank = makefunc('nextnonblank');
  vim.nr2char = makefunc('nr2char');
  vim.pathshorten = makefunc('pathshorten');
  vim.pow = makefunc('pow');
  vim.prevnonblank = makefunc('prevnonblank');
  vim.printf = makefunc('printf');
  vim.pumvisible = makefunc('pumvisible');
  vim.range = makefunc('range');
  vim.readfile = makefunc('readfile');
  vim.reltime = makefunc('reltime');
  vim.reltimestr = makefunc('reltimestr');
  vim.remote_expr = makefunc('remote_expr');
  vim.remote_foreground = makefunc('remote_foreground');
  vim.remote_peek = makefunc('remote_peek');
  vim.remote_read = makefunc('remote_read');
  vim.remote_send = makefunc('remote_send');
  vim.remove = makefunc('remove');
  vim.rename = makefunc('rename');
  vim.repeat = makefunc('repeat');
  vim.resolve = makefunc('resolve');
  vim.reverse = makefunc('reverse');
  vim.round = makefunc('round');
  vim.search = makefunc('search');
  vim.searchdecl = makefunc('searchdecl');
  vim.searchpair = makefunc('searchpair');
  vim.searchpairpos = makefunc('searchpairpos');
  vim.searchpos = makefunc('searchpos');
  vim.server2client = makefunc('server2client');
  vim.serverlist = makefunc('serverlist');
  vim.setbufvar = makefunc('setbufvar');
  vim.setcmdpos = makefunc('setcmdpos');
  vim.setline = makefunc('setline');
  vim.setloclist = makefunc('setloclist');
  vim.setmatches = makefunc('setmatches');
  vim.setpos = makefunc('setpos');
  vim.setqflist = makefunc('setqflist');
  vim.setreg = makefunc('setreg');
  vim.settabwinvar = makefunc('settabwinvar');
  vim.setwinvar = makefunc('setwinvar');
  vim.shellescape = makefunc('shellescape');
  vim.simplify = makefunc('simplify');
  vim.sin = makefunc('sin');
  vim.sort = makefunc('sort');
  vim.soundfold = makefunc('soundfold');
  vim.spellbadword = makefunc('spellbadword');
  vim.spellsuggest = makefunc('spellsuggest');
  vim.split = makefunc('split');
  vim.sqrt = makefunc('sqrt');
  vim.str2float = makefunc('str2float');
  vim.str2nr = makefunc('str2nr');
  vim.strftime = makefunc('strftime');
  vim.stridx = makefunc('stridx');
  vim.string = makefunc('string');
  vim.strlen = makefunc('strlen');
  vim.strpart = makefunc('strpart');
  vim.strridx = makefunc('strridx');
  vim.strtrans = makefunc('strtrans');
  vim.submatch = makefunc('submatch');
  vim.substitute = makefunc('substitute');
  vim.synID = makefunc('synID');
  vim.synIDattr = makefunc('synIDattr');
  vim.synIDtrans = makefunc('synIDtrans');
  vim.synstack = makefunc('synstack');
  vim.system = makefunc('system');
  vim.tabpagebuflist = makefunc('tabpagebuflist');
  vim.tabpagenr = makefunc('tabpagenr');
  vim.tabpagewinnr = makefunc('tabpagewinnr');
  vim.tagfiles = makefunc('tagfiles');
  vim.taglist = makefunc('taglist');
  vim.tempname = makefunc('tempname');
  vim.tolower = makefunc('tolower');
  vim.toupper = makefunc('toupper');
  vim.tr = makefunc('tr');
  vim.trunc = makefunc('trunc');
  vim.type = makefunc('type');
  vim.values = makefunc('values');
  vim.virtcol = makefunc('virtcol');
  vim.visualmode = makefunc('visualmode');
  vim.winbufnr = makefunc('winbufnr');
  vim.wincol = makefunc('wincol');
  vim.winheight = makefunc('winheight');
  vim.winline = makefunc('winline');
  vim.winnr = makefunc('winnr');
  vim.winrestcmd = makefunc('winrestcmd');
  vim.winrestview = makefunc('winrestview');
  vim.winsaveview = makefunc('winsaveview');
  vim.winwidth = makefunc('winwidth');
  vim.writefile = makefunc('writefile');

})(this, vim);
