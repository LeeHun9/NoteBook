[文档地址](https://software.intel.com/sites/landingpage/pintool/docs/98579/Pin/doc/html/index.html)

# Pin

Pin是一个JIT编译器，他的输入是一个常规可执行文件。Pin可以拦截可执行文件的第一条指令，并为后续的直线代码生成（编译）新代码，然后将控制流转移到生成的序列代码。生成的序列代码与原始代码序列几乎相同，Pin 会确保它在分支退出序列时重新获得控制权。重新获得控制权后，Pin 为分支目标生成更多代码并继续执行。Pin 通过将所有生成的代码保存在内存中来提高效率，这样它就可以被重用并直接从一个序列分支到另一个序列。

JIT 模式下，唯一执行的代码是生成的代码。原始代码仅供参考，在生成代码时，Pin 让用户有机会注入自己的代码（插桩）

Pin 检测所有实际执行的指令，如果一条指令从未执行过，那么它就不会被检测。

# Pintools

要通过插桩完成对目标程序的分析，需要了解插桩的位置以及插入的代码，也就是**插桩部分**和**分析代码部分**。这两部分在Pin中被集成到了Pintool，Pintools 可以被认为是可以修改 Pin 内部代码生成过程的插件。

Pintool 使用 Pin 注册检测回调函数，每当需要生成新代码时Pin就会调用这些回调函数。 此检测回调函数代表了插桩组件。这些回调函数检查要生成的代码，调查其静态属性，并决定是否以及在何处注入对分析函数的call。

分析函数收集应用程序相关数据，Pin确保在必要时被保存和恢复整数和浮点寄存器状态，并允许将参数传递给函数。

Pintool 还可以为线程创建或进程Fork等事件注册通知回调函数。这类回调函数通常用于收集数据、工具初始化或清理。

> Pintools 必须在与 Pin 和要检测的可执行文件相同的地址空间中运行。因此 Pintool 可以访问可执行文件的所有数据，还与可执行文件共享文件描述符和其他进程信息。
>
> 对于使用共享库编译的可执行文件，其动态加载程序和所有共享库的执行将对 Pintool 可见。
>
> 编写工具时，分析代码比插桩代码更重要。 这是因为插桩代码执行一次，而分析代码被多次调用。

## Instrumentation 

Pin的插桩主要有四种模式（即粒度），分别是instruction instrumentation, trace instrumentation, routine instrumentation和image instrumentation。其中，最细粒度的就是instruction instrumentation即指令级插桩。routine instrumentation可以理解为函数级插桩，可以对目标程序所调用的函数进行遍历，并在每一个routine中对instruction做更细粒度的遍历。image instrumentation可以理解为对整个程序映像做插桩，Pintool通过这种插桩，可以对section进行遍历。

Pin是 JIT 插桩，在代码序列第一次执行之前会插入，这种模式成为跟踪插桩（Trace instrumentation）。trace是指从一个branch开始，以一个无条件跳转branch结束。Pin将trace分割成了basic block（BBL），BBL是指单一入口、单一出口的指令序列。

> 由于 Pin 在程序执行时动态发现程序的控制流，Pin 的 BBL 可能与编译器教科书中找到的 BBL 的经典定义不同。

EXAMPLE 无参调用

```shell
$ ../../../pin -t obj-intel64/inscount0.so -- /bin/ls
Makefile          atrace.o     imageload.out  itrace      proccount
Makefile.example  imageload    inscount0      itrace.o    proccount.o
atrace            imageload.o  inscount0.o    itrace.out
$ cat inscount.out
Count 422838
$
```

执行上述命令，可以使用inscount0.so来对 ls 的执行过程进行分析，并将结果输出到文件inscount.out中，可以使用-o 指定输出文件。这里inscount0.so是统计执行指令数量的Pintool，对其代码进行分析。

```cpp
/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <iostream>
#include <fstream>
#include "pin.H"
using std::cerr;
using std::endl;
using std::ios;
using std::ofstream;
using std::string;

ofstream OutFile;

// The running count of instructions is kept here
// make it static to help the compiler optimize docount
static UINT64 icount = 0;

// This function is called before every instruction is executed
VOID docount() { icount++; }

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID* v)
{
    // Insert a call to docount before every instruction, no arguments are passed
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);
}

KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "inscount.out", "specify output file name");

// This function is called when the application exits
VOID Fini(INT32 code, VOID* v)
{
    // Write to a file since cout and cerr maybe closed by the application
    OutFile.setf(ios::showbase);
    OutFile << "Count " << icount << endl;
    OutFile.close();
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    cerr << "This tool counts the number of dynamic instructions executed" << endl;
    cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */
/*   argc, argv are the entire command line: pin -t <toolname> -- ...    */
/* ===================================================================== */

int main(int argc, char* argv[])
{
    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    OutFile.open(KnobOutputFile.Value().c_str());

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
```

- PIN_Init函数是对输入的参数进行解析；

- open输出文件，一旦没有通过-o指定输出文件，就会以KnobOuputFile中的inscount.out作为默认输出文件；

- 调用INS_AddInstrumentFunction(Instruction,0)函数，这个函数也是instruction级别插桩分析的核心函数之一，指定用于在instruction粒度插桩的函数，使得在每次执行一条instruction时执行所指定的函数，插桩时调用的函数为`Instruction函数`

  - Instruction函数调用了INS_InsertCall函数，该函数用于每个执行的指令附近插入指定的函数，这一步是通过传递函数指针来实现

    ```cpp
    VOID Instruction(INS ins, VOID *v)
    {
        // Insert a call to docount before every instruction, no arguments are passed
        INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)docount, IARG_END);
    }
    ```

    - ins代表需要插桩的指令
    - 第二个参数表面需要插桩的位置，IPOINT_BEFORE、IPOINT_AFTER、IPOINT_ANYWHERE、IPOINT_TAKEN_BRANCH四种类型，这里使用的是IPOINT_BEFORE，代表在指令前进行插桩，这种插桩方式在任何情况下都是有效。
    - 第三个参数代表需要插桩的函数的指针，docount()
    - 第四个参数就直接为IARG_END，表示插桩函数参数，这里没有参数

- 程序通过PIN_AddFiniFunction(Fini,0)指定结束时执行的函数Fini和PIN_StartProgram()来执行目标程序。

EXAMPLE 有参调用

itrace.cpp

```cpp
/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include "pin.H"

FILE* trace;

// This function is called before every instruction is executed
// and prints the IP
VOID printip(VOID* ip) { fprintf(trace, "%p\n", ip); }

// Pin calls this function every time a new instruction is encountered
VOID Instruction(INS ins, VOID* v)
{
    // Insert a call to printip before every instruction, and pass it the IP
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)printip, IARG_INST_PTR, IARG_END);
}

// This function is called when the application exits
VOID Fini(INT32 code, VOID* v)
{
    fprintf(trace, "#eof\n");
    fclose(trace);
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    PIN_ERROR("This Pintool prints the IPs of every instruction executed\n" + KNOB_BASE::StringKnobSummary() + "\n");
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[])
{
    trace = fopen("itrace.out", "w");

    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    // Register Instruction to be called to instrument instructions
    INS_AddInstrumentFunction(Instruction, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
```

将`IARG_INST_PTR`作为参数传递给 printip 函数

IARG_INST_PTR表示当前被插桩指令的地址