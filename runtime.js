(function(global, vim){

  var _load = global.load;
  var _vim_execute = vim.execute;
  var _g = vim.g;
  var _v = vim.v;

  var vim_execute = function() {
    var args = [];
    for (var i = 0; i < arguments.length; i++) {
      args.push(arguments[i]);
    }
    _v['%v8_result%'] = 0;
    _v['%v8_args%'] = args;
    _vim_execute("try | execute v:['%v8_args%'][0] | let v:['%v8_exception%'] = '' | catch | let v:['%v8_exception%'] = v:exception | endtry");
    if (_v['%v8_exception%'] != '') {
      throw _v['%v8_exception%'];
    }
    return _v['%v8_result%'];
  };

  global.load = function(file) {
    var save = global['%script_name%'];
    global['%script_name%'] = file;
    try {
      _load(file)
    } finally {
      global['%script_name%'] = save;
    }
  };

  global.echo = function(obj) {
    vim.echo(String(obj));
  };

  global.print = global.echo;

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
    vim_execute("execute v:['%v8_args%'][1]", cmd);
  };

  vim.call = function(func, args, obj) {
    if (obj === undefined) {
      return vim_execute("let v:['%v8_result%'] = call(v:['%v8_args%'][1], v:['%v8_args%'][2])", func, args);
    } else {
      return vim_execute("let v:['%v8_result%'] = call(v:['%v8_args%'][1], v:['%v8_args%'][2], v:['%v8_args%'][3])", func, args, obj);
    }
  };

  vim.let = function(varname, value) {
    vim_execute("execute 'let ' . v:['%v8_args%'][1] . ' = v:[''%v8_args%''][2]'", varname, value);
  };

  vim.echo = function(obj) {
    vim_execute("echo v:['%v8_args%'][1]", obj);
  };

  vim['function'] = vim.call('function', ['function'])
  vim._function = vim['function'];

  vim.abs = vim._function('abs');
  vim.add = vim._function('add');
  vim.append = vim._function('append');
  vim.argc = vim._function('argc');
  vim.argidx = vim._function('argidx');
  vim.argv = vim._function('argv');
  vim.atan = vim._function('atan');
  vim.browse = vim._function('browse');
  vim.browsedir = vim._function('browsedir');
  vim.bufexists = vim._function('bufexists');
  vim.buflisted = vim._function('buflisted');
  vim.bufloaded = vim._function('bufloaded');
  vim.bufname = vim._function('bufname');
  vim.bufnr = vim._function('bufnr');
  vim.bufwinnr = vim._function('bufwinnr');
  vim.byte2line = vim._function('byte2line');
  vim.byteidx = vim._function('byteidx');
  vim.ceil = vim._function('ceil');
  vim.changenr = vim._function('changenr');
  vim.char2nr = vim._function('char2nr');
  vim.cindent = vim._function('cindent');
  vim.clearmatches = vim._function('clearmatches');
  vim.col = vim._function('col');
  vim.complete = vim._function('complete');
  vim.complete_add = vim._function('complete_add');
  vim.complete_check = vim._function('complete_check');
  vim.confirm = vim._function('confirm');
  vim.copy = vim._function('copy');
  vim.cos = vim._function('cos');
  vim.count = vim._function('count');
  vim.cscope_connection = vim._function('cscope_connection');
  vim.cursor = vim._function('cursor');
  vim.deepcopy = vim._function('deepcopy');
  vim['delete'] = vim._function('delete');
  vim._delete = vim['delete'];
  vim.did_filetype = vim._function('did_filetype');
  vim.diff_filler = vim._function('diff_filler');
  vim.diff_hlID = vim._function('diff_hlID');
  vim.empty = vim._function('empty');
  vim.escape = vim._function('escape');
  vim.eval = vim._function('eval');
  vim.eventhandler = vim._function('eventhandler');
  vim.executable = vim._function('executable');
  vim.exists = vim._function('exists');
  vim.expand = vim._function('expand');
  vim.extend = vim._function('extend');
  vim.feedkeys = vim._function('feedkeys');
  vim.filereadable = vim._function('filereadable');
  vim.filewritable = vim._function('filewritable');
  vim.filter = vim._function('filter');
  vim.finddir = vim._function('finddir');
  vim.findfile = vim._function('findfile');
  vim.float2nr = vim._function('float2nr');
  vim.floor = vim._function('floor');
  vim.fnameescape = vim._function('fnameescape');
  vim.fnamemodify = vim._function('fnamemodify');
  vim.foldclosed = vim._function('foldclosed');
  vim.foldclosedend = vim._function('foldclosedend');
  vim.foldlevel = vim._function('foldlevel');
  vim.foldtext = vim._function('foldtext');
  vim.foldtextresult = vim._function('foldtextresult');
  vim.foreground = vim._function('foreground');
  vim.garbagecollect = vim._function('garbagecollect');
  vim.get = vim._function('get');
  vim.getbufline = vim._function('getbufline');
  vim.getbufvar = vim._function('getbufvar');
  vim.getchar = vim._function('getchar');
  vim.getcharmod = vim._function('getcharmod');
  vim.getcmdline = vim._function('getcmdline');
  vim.getcmdpos = vim._function('getcmdpos');
  vim.getcmdtype = vim._function('getcmdtype');
  vim.getcwd = vim._function('getcwd');
  vim.getfontname = vim._function('getfontname');
  vim.getfperm = vim._function('getfperm');
  vim.getfsize = vim._function('getfsize');
  vim.getftime = vim._function('getftime');
  vim.getftype = vim._function('getftype');
  vim.getline = vim._function('getline');
  vim.getloclist = vim._function('getloclist');
  vim.getmatches = vim._function('getmatches');
  vim.getpid = vim._function('getpid');
  vim.getpos = vim._function('getpos');
  vim.getqflist = vim._function('getqflist');
  vim.getreg = vim._function('getreg');
  vim.getregtype = vim._function('getregtype');
  vim.gettabwinvar = vim._function('gettabwinvar');
  vim.getwinposx = vim._function('getwinposx');
  vim.getwinposy = vim._function('getwinposy');
  vim.getwinvar = vim._function('getwinvar');
  vim.glob = vim._function('glob');
  vim.globpath = vim._function('globpath');
  vim.has = vim._function('has');
  vim.has_key = vim._function('has_key');
  vim.haslocaldir = vim._function('haslocaldir');
  vim.hasmapto = vim._function('hasmapto');
  vim.histadd = vim._function('histadd');
  vim.histdel = vim._function('histdel');
  vim.histget = vim._function('histget');
  vim.histnr = vim._function('histnr');
  vim.hlID = vim._function('hlID');
  vim.hlexists = vim._function('hlexists');
  vim.hostname = vim._function('hostname');
  vim.iconv = vim._function('iconv');
  vim.indent = vim._function('indent');
  vim.index = vim._function('index');
  vim.input = vim._function('input');
  vim.inputdialog = vim._function('inputdialog');
  vim.inputlist = vim._function('inputlist');
  vim.inputrestore = vim._function('inputrestore');
  vim.inputsave = vim._function('inputsave');
  vim.inputsecret = vim._function('inputsecret');
  vim.insert = vim._function('insert');
  vim.isdirectory = vim._function('isdirectory');
  vim.islocked = vim._function('islocked');
  vim.items = vim._function('items');
  vim.join = vim._function('join');
  vim.keys = vim._function('keys');
  vim.len = vim._function('len');
  vim.libcall = vim._function('libcall');
  vim.libcallnr = vim._function('libcallnr');
  vim.line = vim._function('line');
  vim.line2byte = vim._function('line2byte');
  vim.lispindent = vim._function('lispindent');
  vim.localtime = vim._function('localtime');
  vim.log10 = vim._function('log10');
  vim.map = vim._function('map');
  vim.maparg = vim._function('maparg');
  vim.mapcheck = vim._function('mapcheck');
  vim.match = vim._function('match');
  vim.matchadd = vim._function('matchadd');
  vim.matcharg = vim._function('matcharg');
  vim.matchdelete = vim._function('matchdelete');
  vim.matchend = vim._function('matchend');
  vim.matchlist = vim._function('matchlist');
  vim.matchstr = vim._function('matchstr');
  vim.max = vim._function('max');
  vim.min = vim._function('min');
  vim.mkdir = vim._function('mkdir');
  vim.mode = vim._function('mode');
  vim.nextnonblank = vim._function('nextnonblank');
  vim.nr2char = vim._function('nr2char');
  vim.pathshorten = vim._function('pathshorten');
  vim.pow = vim._function('pow');
  vim.prevnonblank = vim._function('prevnonblank');
  vim.printf = vim._function('printf');
  vim.pumvisible = vim._function('pumvisible');
  vim.range = vim._function('range');
  vim.readfile = vim._function('readfile');
  vim.reltime = vim._function('reltime');
  vim.reltimestr = vim._function('reltimestr');
  vim.remote_expr = vim._function('remote_expr');
  vim.remote_foreground = vim._function('remote_foreground');
  vim.remote_peek = vim._function('remote_peek');
  vim.remote_read = vim._function('remote_read');
  vim.remote_send = vim._function('remote_send');
  vim.remove = vim._function('remove');
  vim.rename = vim._function('rename');
  vim.repeat = vim._function('repeat');
  vim.resolve = vim._function('resolve');
  vim.reverse = vim._function('reverse');
  vim.round = vim._function('round');
  vim.search = vim._function('search');
  vim.searchdecl = vim._function('searchdecl');
  vim.searchpair = vim._function('searchpair');
  vim.searchpairpos = vim._function('searchpairpos');
  vim.searchpos = vim._function('searchpos');
  vim.server2client = vim._function('server2client');
  vim.serverlist = vim._function('serverlist');
  vim.setbufvar = vim._function('setbufvar');
  vim.setcmdpos = vim._function('setcmdpos');
  vim.setline = vim._function('setline');
  vim.setloclist = vim._function('setloclist');
  vim.setmatches = vim._function('setmatches');
  vim.setpos = vim._function('setpos');
  vim.setqflist = vim._function('setqflist');
  vim.setreg = vim._function('setreg');
  vim.settabwinvar = vim._function('settabwinvar');
  vim.setwinvar = vim._function('setwinvar');
  vim.shellescape = vim._function('shellescape');
  vim.simplify = vim._function('simplify');
  vim.sin = vim._function('sin');
  vim.sort = vim._function('sort');
  vim.soundfold = vim._function('soundfold');
  vim.spellbadword = vim._function('spellbadword');
  vim.spellsuggest = vim._function('spellsuggest');
  vim.split = vim._function('split');
  vim.sqrt = vim._function('sqrt');
  vim.str2float = vim._function('str2float');
  vim.str2nr = vim._function('str2nr');
  vim.strftime = vim._function('strftime');
  vim.stridx = vim._function('stridx');
  vim.string = vim._function('string');
  vim.strlen = vim._function('strlen');
  vim.strpart = vim._function('strpart');
  vim.strridx = vim._function('strridx');
  vim.strtrans = vim._function('strtrans');
  vim.submatch = vim._function('submatch');
  vim.substitute = vim._function('substitute');
  vim.synID = vim._function('synID');
  vim.synIDattr = vim._function('synIDattr');
  vim.synIDtrans = vim._function('synIDtrans');
  vim.synstack = vim._function('synstack');
  vim.system = vim._function('system');
  vim.tabpagebuflist = vim._function('tabpagebuflist');
  vim.tabpagenr = vim._function('tabpagenr');
  vim.tabpagewinnr = vim._function('tabpagewinnr');
  vim.tagfiles = vim._function('tagfiles');
  vim.taglist = vim._function('taglist');
  vim.tempname = vim._function('tempname');
  vim.tolower = vim._function('tolower');
  vim.toupper = vim._function('toupper');
  vim.tr = vim._function('tr');
  vim.trunc = vim._function('trunc');
  vim.type = vim._function('type');
  vim.values = vim._function('values');
  vim.virtcol = vim._function('virtcol');
  vim.visualmode = vim._function('visualmode');
  vim.winbufnr = vim._function('winbufnr');
  vim.wincol = vim._function('wincol');
  vim.winheight = vim._function('winheight');
  vim.winline = vim._function('winline');
  vim.winnr = vim._function('winnr');
  vim.winrestcmd = vim._function('winrestcmd');
  vim.winrestview = vim._function('winrestview');
  vim.winsaveview = vim._function('winsaveview');
  vim.winwidth = vim._function('winwidth');
  vim.writefile = vim._function('writefile');

})(this, vim);
