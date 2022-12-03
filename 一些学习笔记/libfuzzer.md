[TOC]

# Sanitizer-Coverage

LLVM插桩

参考：[Source-and-Fuzzing/12.深入解析libfuzzer与asan.md at master · lcatro/Source-and-Fuzzing (github.com)](https://github.com/lcatro/Source-and-Fuzzing/blob/master/12.深入解析libfuzzer与asan.md)

插桩技术包含`Sanitizer-Coverage`和ASAN,它们在LLVM中分别存在于`Pass`和`Compiler-RT`中.简单地说,Pass提供插桩的功能,Compiler-RT中提供了运行时支持的内部接口函数

下面从最容易入手的`Sanitizer-Coverage`开始实现代码覆盖率的统计.

Sanitizer-Coverage原理

```c
#include <stdlib.h>

int function1(int a) {
    if (1 == a)
        return 0;

    return 1;
}

int function2() {
    return -1;
}

int main() {
    if (rand() % 2)
        function1(rand() % 3);
    else
        function2();

    return 0;
}
```

```sh
clang -fsanitize-coverage=trace-pc-guard ./test_case.c -g -o ./test_case
```

将得到的文件IDA反汇编：

```c
int __cdecl main(int argc, const char **argv, const char **envp)
{
  __int64 v3; // rcx
  __int64 v4; // r8
  unsigned __int64 v5; // r9
  int v6; // eax
  __int64 v7; // r8
  unsigned __int64 v8; // r9
  __int64 v9; // rdx
  int v10; // eax

  _sanitizer_cov_trace_pc_guard(dword_439BC0, (__int64)argv, (__int64)envp, v3, v4, v5);
  v6 = rand();
  v9 = (unsigned int)(v6 >> 31);
  LODWORD(v9) = v6 % 2;
  if ( v6 % 2 )
  {
    _sanitizer_cov_trace_pc_guard(&dword_439BC0[1], (__int64)argv, v9, 2LL, v7, v8);
    v10 = rand();
    function1(v10 % 3);
  }
  else
  {
    _sanitizer_cov_trace_pc_guard(&dword_439BC0[2], (__int64)argv, v9, 2LL, v7, v8);
    function2();
  }
  return 0;
}
```

可以看到在每个基本块前都插入了_sanitizer_cov_trace_pc_guard()，其实就是插桩回调函数。,如果没有重写该函数,那就LLVM就会使用默认版本,官方文档有一处示例代码,使用自定义该回调函数打印插桩分支信息.

```c
extern "C" void __sanitizer_cov_trace_pc_guard(uint32_t *guard) {
  if (!*guard) return;
  void *PC = __builtin_return_address(0);
  char PcDescr[1024];
    
  __sanitizer_symbolize_pc(PC, "%p %F %L", PcDescr, sizeof(PcDescr));
  printf("guard: %p %x PC %s\n", guard, *guard, PcDescr);
}
```





# libfuzzer

[源码](https://github.com/llvm/llvm-project/tree/main/compiler-rt/lib/fuzzer)

FuzzerDriver.cpp文件的FuzzerDriver()函数是libFuzzer的入口点.

```c
int FuzzerDriver(int *argc, char ***argv, UserCallback Callback) {
   // 省略代码
  if (EF->LLVMFuzzerInitialize) // 如果用户有自定义LLVMFuzzerInitialize()实现,那么就执行该函数,提供这个函数的作为用户自定义实现接口是因为要对库/程序进行初始化
    EF->LLVMFuzzerInitialize(argc, argv);
   // 省略程序解析外部参数代码
   
  unsigned Seed = Flags.seed; // 如果外部有传递随机数种子的话.参数为-seed=?
  if (Seed == 0)
    Seed = std::chrono::system_clock::now().time_since_epoch().count() + GetPid(); // 外部没有指定随机数种子,那就使用时间戳+pid
  if (Flags.verbosity) // 调试输出,参数为-verbosity
    Printf("INFO: Seed: %u\n", Seed);

  Random Rand(Seed); // 随机数生成器
  auto *MD = new MutationDispatcher(Rand, Options); // 数据变异生成器
  auto *Corpus = new InputCorpus(Options.OutputCorpus); // 数据收集器
  auto *F = new Fuzzer(Callback, *Corpus, *MD, Options); // Fuzzer核心逻辑模块

  StartRssThread(F, Flags.rss_limit_mb); // 创建内存检测线程,如果当前进程的内存占用超过阈值之后就退出Fuzzer报告异常

  Options.HandleAbrt = Flags.handle_abrt;
  Options.HandleBus = Flags.handle_bus;
  Options.HandleFpe = Flags.handle_fpe;
  Options.HandleIll = Flags.handle_ill;
  Options.HandleInt = Flags.handle_int;
  Options.HandleSegv = Flags.handle_segv;
  Options.HandleTerm = Flags.handle_term;
  Options.HandleXfsz = Flags.handle_xfsz;
  SetSignalHandler(Options);  // 初始化信号捕获回调函数
    
   // 省略代码
  F->Loop(); // 开始Fuzzing
    
  exit(0);
}
```

核心框架：

- MutationDispatcher 数据变异生成器
- InputCorpus 数据收集器
- Fuzzer 核心逻辑模块

Fuzzer源码：

```c
void Fuzzer::Loop() {
   // 省略代码
  while (true) {
    // 省略代码
    if (TimedOut()) break; // 由参数-max_total_time指定的运行时间控制,超时执行就退出
    // Perform several mutations and runs.
    MutateAndTestOne(); // 执行一次Fuzzing
  }
  // 省略代码
}
```

MutateAndTestOne() :

```cpp
void Fuzzer::MutateAndTestOne() {
  auto &II = Corpus.ChooseUnitToMutate(MD.GetRand());  // 从数据收集器中随机挑一个测试数据出来,要结合下面的核心逻辑代码才能理解它的用意
  const auto &U = II.U;
  size_t Size = U.size();
  memcpy(CurrentUnitData, U.data(), Size);  //  获取测试数据

  // 省略代码
    
  for (int i = 0; i < Options.MutateDepth; i++) {  // 对数据变异多次.由参数-mutate_depth控制,默认值是5
    size_t NewSize = 0;
    NewSize = MD.Mutate(CurrentUnitData, Size, CurrentMaxMutationLen); // 使用前面随机抽取获取到的测试数据作为变异输入生成测试数据
    Size = NewSize;
    if (i == 0)  // 注意,第一次Fuzzing时,会启用数据追踪功能,简而言之就是hook strstr(),strcasestr(),memmem()函数,然后从参数中获取到一些有意思的字符串
      StartTraceRecording();
    II.NumExecutedMutations++;
    if (size_t NumFeatures = RunOne(CurrentUnitData, Size)) {  // 开始Fuzzing,如果使用前面生成的变异数据拿去Fuzzing,发现了新的路径数量,就会保存到NumFeatures,没有发现新路径则NumFeatures=0.
      Corpus.AddToCorpus({CurrentUnitData, CurrentUnitData + Size}, NumFeatures,
                         /*MayDeleteFile=*/true);  // 注意,这一段代码是libFuzzer的核心逻辑之一,如果变异数据发现新路径,那就记录该数据到数据收集器.这是libFuzzer路径探测的核心原理.
      ReportNewCoverage(&II, {CurrentUnitData, CurrentUnitData + Size});
      CheckExitOnSrcPosOrItem();
    }
    StopTraceRecording();
    TryDetectingAMemoryLeak(CurrentUnitData, Size,
                            /*DuringInitialCorpusExecution*/ false);
  }
}
```

RunOne

```cpp
size_t Fuzzer::RunOne(const uint8_t *Data, size_t Size) {
  ExecuteCallback(Data, Size);  // 往下就是调用到LLVMFuzzerTestOneInput()
  TPC.UpdateCodeIntensityRecord(TPC.GetCodeIntensity());  // 获取当前执行过的代码分支总数

  size_t NumUpdatesBefore = Corpus.NumFeatureUpdates();
  TPC.CollectFeatures([&](size_t Feature) {
    Corpus.AddFeature(Feature, Size, Options.Shrink);
  });
  size_t NumUpdatesAfter = Corpus.NumFeatureUpdates();  // 从SanitizerCoverage插桩记录的信息中获取分支数据

  // 省略代码
    
  return NumUpdatesAfter - NumUpdatesBefore;  // 计算发现了多少新分支路径
}
```

ExecuteCallback()

```cpp
void Fuzzer::ExecuteCallback(const uint8_t *Data, size_t Size) {
  // 省略代码
  uint8_t *DataCopy = new uint8_t[Size];
  memcpy(DataCopy, Data, Size);  // 从变异的数据中复制一份到这个内存,后面会用到
  // 省略代码
  TPC.ResetMaps();  // 清空所有路径信息
  RunningCB = true;
  int Res = CB(DataCopy, Size);  // 执行用户自定义的LLVMFuzzerTestOneInput()
  RunningCB = false;
  // 省略代码
  if (!LooseMemeq(DataCopy, Data, Size))  // 注意这个坑,如果传递给LLVMFuzzerTestOneInput()的data会被程序修改,那么libFuzzer会强制退出
    CrashOnOverwrittenData();
  delete[] DataCopy;
}
```

MutationDispatcher()

```cpp
MutationDispatcher::MutationDispatcher(Random &Rand,const FuzzingOptions &Options)
    : Rand(Rand), Options(Options) {  // Rand是随机数生成器
  DefaultMutators.insert(
      DefaultMutators.begin(),  // 添加数据变异算法
      {
          {&MutationDispatcher::Mutate_EraseBytes, "EraseBytes"},
          {&MutationDispatcher::Mutate_InsertByte, "InsertByte"},
          {&MutationDispatcher::Mutate_InsertRepeatedBytes,
           "InsertRepeatedBytes"},
          {&MutationDispatcher::Mutate_ChangeByte, "ChangeByte"},
          {&MutationDispatcher::Mutate_ChangeBit, "ChangeBit"},
          {&MutationDispatcher::Mutate_ShuffleBytes, "ShuffleBytes"},
          {&MutationDispatcher::Mutate_ChangeASCIIInteger, "ChangeASCIIInt"},
          {&MutationDispatcher::Mutate_ChangeBinaryInteger, "ChangeBinInt"},
          {&MutationDispatcher::Mutate_CopyPart, "CopyPart"},
          {&MutationDispatcher::Mutate_CrossOver, "CrossOver"},
          {&MutationDispatcher::Mutate_AddWordFromManualDictionary,
           "ManualDict"},
          {&MutationDispatcher::Mutate_AddWordFromTemporaryAutoDictionary,
           "TempAutoDict"},
          {&MutationDispatcher::Mutate_AddWordFromPersistentAutoDictionary,
           "PersAutoDict"},
      });
  if(Options.UseCmp)
    DefaultMutators.push_back(
        {&MutationDispatcher::Mutate_AddWordFromTORC, "CMP"});

  if (EF->LLVMFuzzerCustomMutator)  // 如果存在用户自定义的数据变异方法,那就使用它
    Mutators.push_back({&MutationDispatcher::Mutate_Custom, "Custom"});
  else
    Mutators = DefaultMutators;

  if (EF->LLVMFuzzerCustomCrossOver)
    Mutators.push_back(
        {&MutationDispatcher::Mutate_CustomCrossOver, "CustomCrossOver"});
}
```



![img](libfuzzer.assets/libFuzzer-Mutate.png)







# ASAN实现

ASAN分为两部分,插桩(Instrumentation Pass)和运行时逻辑(Compiler-RT).

ASAN的实现代码在\llvm-project\llvm\lib\Transforms\Instrumentation\AddressSanitizer.cpp





遍历每个函数进行插桩的入口点在`AddressSanitizer::instrumentFunction()`函数.





获得筛选出来的指令后,接下来就进行插桩操作.下面的插桩核心原理,就是在**Load/Store**指令前面插入异常检测逻辑,如果没有异常才可以执行真实的数据读写操作.

# LibFuzzer源码(最新)

调用过程大致如下：

```
--------     --------------      ------------      --------------------      --------
| main | => | FuzzerDriver | => | RunOneTest | => | MutateAndTestOne() | => | RunOne |
--------     --------------     |  F->Loop() |     --------------------      --------
								 ------------		
```

主入口 FuzzerMain.cpp，用户定义的CallBack函数默认名称为`LLVMFuzzerTestOneInput`，在这里当作外部函数，作为参数传递给`FuzzerDriver`执行。

```cpp
extern "C" {
// This function should be defined by the user.
int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size);
}  // extern "C"

ATTRIBUTE_INTERFACE int main(int argc, char **argv) {
  return fuzzer::FuzzerDriver(&argc, &argv, LLVMFuzzerTestOneInput);
}
```

`int FuzzerDriver(int *argc, char ***argv, UserCallback Callback)`函数在`FuzzerDriver.cpp`中。思路类似AFL，初始化后，对语料库`RunOneTest`简单跑几遍，之后调用 `F->Loop(CorporaFiles)`进入Fuzzing主循环。

```cpp
int FuzzerDriver(int *argc, char ***argv, UserCallback Callback) {
  using namespace fuzzer;
  
  // 根据解析编译参数的Flags，对一些初始变量的赋值
  assert(argc && argv && "Argument pointers cannot be nullptr");
  std::string Argv0((*argv)[0]);
  EF = new ExternalFunctions();
  if (EF->LLVMFuzzerInitialize)
    EF->LLVMFuzzerInitialize(argc, argv);
  if (EF->__msan_scoped_disable_interceptor_checks)
    EF->__msan_scoped_disable_interceptor_checks();
  const std::vector<std::string> Args(*argv, *argv + *argc);
  assert(!Args.empty());
  ProgName = new std::string(Args[0]);
  ...
  
  // Initialize Seed. 初始化种子
  if (Seed == 0)
    Seed = static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count() + GetPid());
  if (Flags.verbosity)
    Printf("INFO: Seed: %u\n", Seed);

  // DTSan收集每个种子的数据流
  if (Flags.collect_data_flow && !Flags.fork &&
      !(Flags.merge || Flags.set_cover_merge)) {
    if (RunIndividualFiles)
      return CollectDataFlow(Flags.collect_data_flow, Flags.data_flow_trace,
                        ReadCorpora({}, *Inputs));
    else
      return CollectDataFlow(Flags.collect_data_flow, Flags.data_flow_trace,
                        ReadCorpora(*Inputs, {}));
  }

  // 创建组件对象：变异器、语料、Fuzzer
  Random Rand(Seed);
  auto *MD = new MutationDispatcher(Rand, Options);
  auto *Corpus = new InputCorpus(Options.OutputCorpus, Entropic);
  auto *F = new Fuzzer(Callback, *Corpus, *MD, Options);

  // 解析字典，添加到变异器中
  for (auto &U: Dictionary)
    if (U.size() <= Word::GetMaxSize())
      MD->AddWordToManualDictionary(Word(U.data(), U.size()));

  // 注册一些处理函数
  Options.HandleAbrt = Flags.handle_abrt;
  ...
  SetSignalHandler(Options);

  std::atexit(Fuzzer::StaticExitCallback);

  // 精简测试用例 并跑一遍
  if (Flags.minimize_crash)
    return MinimizeCrashInput(Args, Options);

  if (Flags.minimize_crash_internal_step)
    return MinimizeCrashInputInternalStep(F, Corpus);

  if (Flags.cleanse_crash)
    return CleanseCrashInput(Args, Options);

  if (RunIndividualFiles) {
    Options.SaveArtifacts = false;
    int Runs = std::max(1, Flags.runs);   // runs: 单个测试运行的次数
    Printf("%s: Running %zd inputs %d time(s) each.\n", ProgName->c_str(),
           Inputs->size(), Runs);
    for (auto &Path : *Inputs) {
      auto StartTime = system_clock::now();
      Printf("Running: %s\n", Path.c_str());
      for (int Iter = 0; Iter < Runs; Iter++)
        RunOneTest(F, Path.c_str(), Options.MaxLen);    // 跑Runs次测试
      auto StopTime = system_clock::now();
      auto MS = duration_cast<milliseconds>(StopTime - StartTime).count();
      Printf("Executed %s in %zd ms\n", Path.c_str(), (long)MS);
    }
    Printf("***\n"
           "*** NOTE: fuzzing was not performed, you have only\n"
           "***       executed the target code on a fixed set of inputs.\n"
           "***\n");
    F->PrintFinalStats();
    exit(0);
  }
  ...

  // 评估字典单元的一个过程，AnalyzeDictionary看字典是否有用
  if (Flags.analyze_dict) {
    size_t MaxLen = INT_MAX;  // Large max length.
    UnitVector InitialCorpus;
    for (auto &Inp : *Inputs) {
      Printf("Loading corpus dir: %s\n", Inp.c_str());
      ReadDirToVectorOfUnits(Inp.c_str(), &InitialCorpus, nullptr,
                             MaxLen, /*ExitOnError=*/false);
    }

    if (Dictionary.empty() || Inputs->empty()) {
      Printf("ERROR: can't analyze dict without dict and corpus provided\n");
      return 1;
    }
    if (AnalyzeDictionary(F, Dictionary, InitialCorpus)) {
      Printf("Dictionary analysis failed\n");
      exit(1);
    }
    Printf("Dictionary analysis succeeded\n");
    exit(0);
  }

  auto CorporaFiles = ReadCorpora(*Inputs, ParseSeedInuts(Flags.seed_inputs));  // 返回类型为 vector<SizedFile>
  
  // Fuzzing主循环
  F->Loop(CorporaFiles);

  if (Flags.verbosity)
    Printf("Done %zd runs in %zd second(s)\n", F->getTotalNumberOfRuns(),
           F->secondsSinceProcessStartUp());
  F->PrintFinalStats();

  exit(0);  // Don't let F destroy itself.
}
```

`Loop `位于 `FuzzerLoop.cpp`，主要功能是设置检测覆盖率的一些函数，在`while(true)`循环内调用`MutateAndTestOne()`

```cpp
void Fuzzer::Loop(std::vector<SizedFile> &CorporaFiles) {
  auto FocusFunctionOrAuto = Options.FocusFunction;
  DFT.Init(Options.DataFlowTrace, &FocusFunctionOrAuto, CorporaFiles,
           MD.GetRand());
  TPC.SetFocusFunction(FocusFunctionOrAuto);
  ReadAndExecuteSeedCorpora(CorporaFiles);
  DFT.Clear();  // No need for DFT any more. 清除数据流插桩
  TPC.SetPrintNewPCs(Options.PrintNewCovPcs);
  TPC.SetPrintNewFuncs(Options.PrintNewCovFuncs);
  system_clock::time_point LastCorpusReload = system_clock::now();

  TmpMaxMutationLen =
      Min(MaxMutationLen, Max(size_t(4), Corpus.MaxInputSize()));

  while (true) {
    ...
    // Perform several mutations and runs.
    MutateAndTestOne();

    PurgeAllocator();
  }

  PrintStats("DONE  ", "\n");
  MD.PrintRecommendedDictionary();
}
```

`MutateAndTestOne()`，选择种子变异后调用RunOne()，RunOne返回覆盖率信息。

```cpp
void Fuzzer::MutateAndTestOne() {
  MD.StartMutationSequence();
  
  // 选取
  auto &II = Corpus.ChooseUnitToMutate(MD.GetRand());     // MD随机选择一个Unit进行变异
  if (Options.DoCrossOver) {  // 交叉输入
    auto &CrossOverII = Corpus.ChooseUnitToCrossOverWith(
        MD.GetRand(), Options.CrossOverUniformDist);
    MD.SetCrossOverWith(&CrossOverII.U);
  }
  // 检查测试用例长度等信息
  const auto &U = II.U;
  memcpy(BaseSha1, II.Sha1, sizeof(BaseSha1));
  assert(CurrentUnitData);
  size_t Size = U.size();
  assert(Size <= MaxInputLen && "Oversized Unit");
  memcpy(CurrentUnitData, U.data(), Size);

  assert(MaxMutationLen > 0);

  size_t CurrentMaxMutationLen =
      Min(MaxMutationLen, Max(U.size(), TmpMaxMutationLen));
  assert(CurrentMaxMutationLen > 0);

  // 开始变异，并更新一些全局变量
  for (int i = 0; i < Options.MutateDepth; i++) {       // MutateDepth 每个输入连续突变的数量
    if (TotalNumberOfRuns >= Options.MaxNumberOfRuns)
      break;
    MaybeExitGracefully();
    size_t NewSize = 0;
    if (II.HasFocusFunction && !II.DataFlowTraceForFocusFunction.empty() &&
        Size <= CurrentMaxMutationLen)
      NewSize = MD.MutateWithMask(CurrentUnitData, Size, Size,
                                  II.DataFlowTraceForFocusFunction);

    // If MutateWithMask either failed or wasn't called, call default Mutate.
    if (!NewSize)
      NewSize = MD.Mutate(CurrentUnitData, Size, CurrentMaxMutationLen);
    assert(NewSize > 0 && "Mutator returned empty unit");
    assert(NewSize <= CurrentMaxMutationLen && "Mutator return oversized unit");
    Size = NewSize;
    II.NumExecutedMutations++;
    Corpus.IncrementNumExecutedMutations();

    // RunOne
    bool FoundUniqFeatures = false;
    bool NewCov = RunOne(CurrentUnitData, Size, /*MayDeleteFile=*/true, &II,
                         /*ForceAddToCorpus*/ false, &FoundUniqFeatures);
    TryDetectingAMemoryLeak(CurrentUnitData, Size,
                            /*DuringInitialCorpusExecution*/ false);
    if (NewCov) {
      ReportNewCoverage(&II, {CurrentUnitData, CurrentUnitData + Size});
      break;  // We will mutate this input more in the next rounds.
    }
    if (Options.ReduceDepth && !FoundUniqFeatures)
      break;
  }

  II.NeedsEnergyUpdate = true;
}
```

