参考

# Env配置

- Ubuntu22.04

- llvm15

```sh
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | sudo apt-key add -
sudo apt-add-repository "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-15 main"
sudo apt-get update
sudo apt-get install -y llvm-15 llvm-15-dev llvm-15-tools clang-15
```

## Build

build `llvm-tutor` 项目

```
cd <build/dir>
cmake -DLT_LLVM_INSTALL_DIR=<installation/dir/of/llvm/15> <source/dir/llvm/tutor>
make
```

命令行直接编译pass

```sh
clang `llvm-config --cxxflags` -Wl,-znodelete -fno-rtti -fPIC -shared Hello.cpp -o LLVMHello.so `llvm-config --ldflags`
```



# Pass概述

Pass可分类为Analysis、Transformation 或 CFG。CFG pass 是一种修改CFG的 Transformation pass



LLVM pass作用于IR，生成IR文件命令：

```sh
export LLVM_DIR=<installation/dir/of/llvm/15>
# Textual form
$LLVM_DIR/bin/clang -O1 -emit-llvm input.c -S -o out.ll
# Binary/bit-code form
$LLVM_DIR/bin/clang -O1 -emit-llvm input.c -c -o out.bc
```

### Pass Managers

为了管理pass，编译器需要PM，当前LLVM使用两个PM。用哪个PM和如何注册pass的代码很不同。

- New PM： LLVM 中优化管道的默认PM。
- Legacy PM：多年以来的默认PM，LLVM14之后被弃用移除

两者使用的不同：

1. 加载plugin.so：`-load-pass-plugin` vs `-load`,
2. 指定pass/plugin：`-passes=mba-add` vs `-legacy-mba-add`,New PM使用参数`-pass`指定pass名，Legacy直接将pass名作为参数，`-pass_name`
3. 使用Legacy PM时，需要使用`--enable-new-pm=0`禁用New PM

### Analysis 和 Transformation pass区别

- transformation pass 从 [PassInfoMixin](https://github.com/llvm/llvm-project/blob/release/15.x/llvm/include/llvm/IR/PassManager.h#L371) 继承
- analysis pass 从 [AnalysisInfoMixin](https://github.com/llvm/llvm-project/blob/release/15.x/llvm/include/llvm/IR/PassManager.h#L394) 继承

这是一个NEW PM 的关键特征，Analysis pass 需要更多的bookkeeping，因此需要更多代码，比如需要添加一个[AnalysisKey](https://github.com/llvm/llvm-project/blob/release/15.x/llvm/include/llvm/IR/PassManager.h#L410)的实例才能被NEW PM识别