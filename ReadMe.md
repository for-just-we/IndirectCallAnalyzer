
重构了下[mlta](https://github.com/umnsec/mlta/tree/main)，借助mlta熟悉下LLVM中的类型系统

# 1.Basic Usage

编译benchmark： 确保安装LLVM，我们采用的是LLVM 15，编译benchmark时最好添加 `-g -Xclang -no-opaque-pointers -Xclang -disable-O0-optnone`。
一个是保留debug信息，一个是需要类型指针进行结构体分析，一个是 `mem2reg` 优化需要。

编译该project

```bash
mkdir build && cd build
cmake ..
cmake --build . -j 16
```

运行：`ica -analysis-type=2 xxx.bc`

| 选项 | 说明 |
| ---- | ---- |
| `analysis-type` | 采用的分析算法，`1` 表示用 `FLTA`、`2` 表示用 `MLTA`、`3` 表示加强版 `MLTA`、`4` 表示 `Kelp` |
| `debug` | 是否输出运行时的debug信息 |
| `max-type-layer` | MLTA最大的类型匹配层数，默认 `10` |


# 2.LLVM Basis


## 2.1.basic

与source code不同的是，结构体initializer在global域和local域存在不同，以[test9](testcases/mlta/test9/test.c)为例。

其编译后生成的IR如下：

```asm
%struct.S = type { i32 (i32, i32)*, i32, i32 }

@s1 = global %struct.S { i32 (i32, i32)* @func1, i32 0, i32 1 }, align 8
@__const.main.s_local_1 = private unnamed_addr constant %struct.S { i32 (i32, i32)* @func1, i32 0, i32 1 }, align 8
@__const.main.s_local_2 = private unnamed_addr constant %struct.S { i32 (i32, i32)* @func2, i32 0, i32 2 }, align 8

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @func1(i32 noundef %a, i32 noundef %b) #0 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, i32* %a.addr, align 4
  store i32 %b, i32* %b.addr, align 4
  %0 = load i32, i32* %a.addr, align 4
  %1 = load i32, i32* %b.addr, align 4
  %add = add nsw i32 %0, %1
  ret i32 %add
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @func2(i32 noundef %a, i32 noundef %b) #0 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, i32* %a.addr, align 4
  store i32 %b, i32* %b.addr, align 4
  %0 = load i32, i32* %a.addr, align 4
  %1 = load i32, i32* %b.addr, align 4
  %sub = sub nsw i32 %0, %1
  ret i32 %sub
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @func3(i32 noundef %a, i32 noundef %b) #0 {
entry:
  %a.addr = alloca i32, align 4
  %b.addr = alloca i32, align 4
  store i32 %a, i32* %a.addr, align 4
  store i32 %b, i32* %b.addr, align 4
  %0 = load i32, i32* %a.addr, align 4
  %1 = load i32, i32* %b.addr, align 4
  %mul = mul nsw i32 %0, %1
  ret i32 %mul
}

; Function Attrs: noinline nounwind optnone ssp uwtable
define i32 @main() #0 {
entry:
  %retval = alloca i32, align 4
  %s_local_1 = alloca %struct.S, align 8
  %s_local_2 = alloca %struct.S, align 8
  %a = alloca i32, align 4
  %s_local4 = alloca %struct.S, align 8
  %s_local_3 = alloca %struct.S, align 8
  store i32 0, i32* %retval, align 4
  %0 = bitcast %struct.S* %s_local_1 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %0, i8* align 8 bitcast (%struct.S* @__const.main.s_local_1 to i8*), i64 16, i1 false)
  %1 = bitcast %struct.S* %s_local_2 to i8*
  call void @llvm.memcpy.p0i8.p0i8.i64(i8* align 8 %1, i8* align 8 bitcast (%struct.S* @__const.main.s_local_2 to i8*), i64 16, i1 false)
  store i32 2, i32* %a, align 4
  %f = getelementptr inbounds %struct.S, %struct.S* %s_local4, i32 0, i32 0
  store i32 (i32, i32)* @func3, i32 (i32, i32)** %f, align 8
  %num = getelementptr inbounds %struct.S, %struct.S* %s_local4, i32 0, i32 1
  store i32 0, i32* %num, align 8
  %id = getelementptr inbounds %struct.S, %struct.S* %s_local4, i32 0, i32 2
  %2 = load i32, i32* %a, align 4
  store i32 %2, i32* %id, align 4
  %num1 = getelementptr inbounds %struct.S, %struct.S* %s_local_3, i32 0, i32 1
  store i32 ptrtoint (i32 (i32, i32)* @func3 to i32), i32* %num1, align 8
  ret i32 0
}
```

可以看到

- `main` 函数中的两个结构体变量 `s_local_1` 和 `s_local_2` 对应的initializer常量被转移到global域，因此MLTA分析时实际是在全局段进行分析，在local域，通过 `llvm.memcpy.p0i8.p0i8.i64` 函数将值转移到 `s_local_1, s_local_2`。

- 对于 `s_local4`，由于其initializer出现了局部变量，因此其field的初始化被拆分成一个个 `store` 指令。

    * `store i32 (i32, i32)* @func3, i32 (i32, i32)** %f, align 8` 对应 `s_local4.f = func3`。

因此在分析local域的type confinement时候不需要考虑initializer，主要考虑 `store` 指令。

同时可以看到 `Stu s_local4 = {func3, 0, a};` 中赋值操作是通过 `llvm.memcpy.p0i8.p0i8.i64` 完成的。
这个函数可以用来完成结构体赋值。
因此分析 `store` 的时候需要考虑这个函数。


在处理结构体层次时需要处理 `getelementptr` 指令，
代码里会跳过第一个index，其介绍在[what-is-the-first-index-of-the-gep-instruction](https://llvm.org/docs/GetElementPtr.html#what-is-the-first-index-of-the-gep-instruction)中，
所以field访问通常从第2个index开始。这里通过 `getElementType` API可访问子field的类型。



这里用MLTA分析中有非常多trick

- recover type: 在某些function中进行

## 2.2.类型系统


同时LLVM IR中判断类型相等也不是件容易的事，参考[cn-blog](https://blog.csdn.net/fcsfcsfcs/article/details/119062032)，[原版blog](https://lowlevelbits.org/type-equality-in-llvm/)。

比如，编译完的nginx中发现同一个结构体可能链接后有多个不同的类型

```asm
%struct.ngx_http_phase_handler_s = type { i64 (%struct.ngx_http_request_s.1418*, %struct.ngx_http_phase_handler_s*)*, i64 (%struct.ngx_http_request_s.1418*)*, i64 }
%struct.ngx_http_phase_handler_s.2192 = type { i64 (%struct.ngx_http_request_s.2187*, %struct.ngx_http_phase_handler_s.2192*)*, i64 (%struct.ngx_http_request_s.2187*)*, i64 }
```

这些别名类型的存在严重影响mlta的效率。
对于这类同名类型我们分为两类：

- 1.在source code中只有1种实现

- 2.在source code中有多种实现

我们目前只考虑第一类情况。对于一个类型，如果它们去掉数字后缀后名称一样并且参数数量一样，我们认为是相同类型，并以此为基础进行hash。

## 2.3.编译后结构体变化

参考[TFA paper](https://www.usenix.org/system/files/usenixsecurity24-liu-dinghao-improving.pdf)、[why-a-function-pointer-field-in-a-llvm-ir-struct-is-replaced-by](https://stackoverflow.com/questions/18730620/why-a-function-pointer-field-in-a-llvm-ir-struct-is-replaced-by)、[Function pointer type becomes empty struct](https://lists.llvm.org/pipermail/cfe-dev/2016-November/051601.html)、[Function pointer type becomes empty struct](https://lists.llvm.org/pipermail/cfe-dev/2016-November/051633.html)、[Function pointer type becomes empty struct](https://lists.llvm.org/pipermail/cfe-dev/2016-November/051635.html)。
编译后结构体field访问和源代码可能对应不上导致MLTA发挥不了作用，TFA的解决思路是通过debug信息恢复原始类型信息，不过这一步我们这里并没有实现。

```cpp
/* 源代码 */
struct A {
 int i;
 int (*f)(int, struct A*);
 int (*g)(char, struct A*);
};

/* 预期对应IR */
%struct.A = type {i32, i32 (i32, %struct.A*), i32 (i8, %struct.A*)}

/* 实际IR */
%struct.A = type {i32, {}*, i32 (i8, %struct.A*)}
```


# 3.Implementations


## 3.1.MLTA

相对[原版MLTA](https://github.com/umnsec/mlta)，我们在实现上做了一些调整。 motivation参考[case study](docs/mlta_case_analysis.md)。

- 1.我们默认采用逐个比对类型的方式 (`MLTA::findCalleesWithType` 函数) 而不是比对函数签名的方式进行类型比对。

- 2.我们不将函数类型添加进 `typeCapSet` (`MLTAPass::confineTargetFunction` 函数最后一行)。

- 3.当 `store VO, PO` (`*PO = VO`) 时，假如 `VO` 不是常量，我们将 `PO` 涉及到的结构体类型层次添加进 `typeEscapedSet`。

除了上述3点，我们还注意到原版mlta以下问题：

- 1.当结构体类型被cast到 `void*` 或者整数类型时没有考虑


针对上面问题，我们的调整如下：

- 在 `MLTA::typePropInFunction` 函数的实现中，分析cast指令时如果识别到 `void` 或者整形指针与 `struct` 指针互相转换，将 `struct` 类型标记为escaped type。

此外，我们添加了一个 `MLTADFPass`，这个简单实现了TFA paper中的数据流分析策略，不过没有实现结构体信息恢复部分。
