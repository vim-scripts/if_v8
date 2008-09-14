function print(msg) { vim.execute("echo '" + msg + "'"); }

print('hello, v8');

var arr = vim.eval('arr');
print('arr[3] = ' + arr[3]);

var tw = vim.eval('&tw');
print('&tw = ' + tw);

var dict = vim.eval('dict')
for (var key in dict) {
  print(key + " = " + dict[key]);
}

vim.execute("new");
vim.eval('append("$", "line1")');
vim.eval('append("$", "line2")');
vim.eval('append("$", "line3")');
