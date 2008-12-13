let arr = [1, 2, 3, 4, 5]
let dict = {'a':'aaa', 'b':'bbb', 'c':'ccc'}

let if_v8 = has('win32') ? './if_v8.dll' : './if_v8.so'
echo libcall(if_v8, 'init', if_v8)
echo libcall(if_v8, 'execute', 'load("test.js")')
