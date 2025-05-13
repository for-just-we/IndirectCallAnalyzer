
# 1.teeworlds

主要来自teeworlds-0.7.5版本，由于C++带来的语法问题。


下面示例 `this->*pfnCallback` 是个奇怪的表达式，看起来又像虚函数调用又像通过 `pfnCallback` 进行的间接调用。但是 `CMenus` 及其父类定义都没有 `pfnCallback` 这个field。
推测是这个 `pfnCallback` 指向的是某个虚函数。

```c++
float CMenus::DoIndependentDropdownMenu(void *pID, const CUIRect *pRect, const char *pStr, float HeaderHeight, FDropdownCallback pfnCallback, bool* pActive)
{
    if(Active)
		return HeaderHeight + (this->*pfnCallback)(View);
}
```

编译成LLVM IR后如下，除了第1个参数为 `this` 指针外，源代码第2个参数的类型由 `const CUIRect *` 变成 `%"struct.CCommandBuffer::CColor"*`，这个我也没找到它们之间的关联。
除此之外，`pfnCallback` 被编译成了 `i64 %pfnCallback.coerce0, i64 %pfnCallback.coerce1`。
个人推测大概传入进来的 `pfnCallback` 可能是个成员函数，而成员函数存在是否为虚函数2种情况，两个参数配合处理2种情况。
LLVM IR中这个间接调用的参数类型为 `%class.CMenus*,  <2 x float>,  <2 x float>`

```asm
define dso_local noundef float @_ZN6CMenus25DoIndependentDropdownMenuEPvPK7CUIRectPKcfMS_FfS1_EPb(%class.CMenus* noundef nonnull align 8 dereferenceable(8432) %this, i8* noundef %pID, %"struct.CCommandBuffer::CColor"* nocapture noundef readonly %pRect, i8* noundef %pStr, float noundef %HeaderHeight, i64 %pfnCallback.coerce0, i64 %pfnCallback.coerce1, i8* nocapture noundef %pActive) local_unnamed_addr #23 align 2 !dbg !76767 {
    %37 = getelementptr inbounds i8, i8* %36, i64 %pfnCallback.coerce1
    %this.adjusted = bitcast i8* %37 to %class.CMenus*, 
    %38 = and i64 %pfnCallback.coerce0, 1, 
    %39 = bitcast i8* %37 to i8**, !dbg !76845
    %vtable47 = load i8*, i8** %39, align 8, !dbg !76845, !tbaa !34470
    %40 = add i64 %pfnCallback.coerce0, -1, !dbg !76845
    %41 = getelementptr i8, i8* %vtable47, i64 %40, !dbg !76845, !nosanitize !3818
    %42 = bitcast i8* %41 to float (%class.CMenus*, <2 x float>, <2 x float>)**, !dbg !76845, !nosanitize !3818
    %memptr.virtualfn = load float (%class.CMenus*, <2 x float>, <2 x float>)*, float (%class.CMenus*, <2 x float>, <2 x float>)** %42, align 8, !dbg !76845, !nosanitize !3818
  
    %memptr.nonvirtualfn = inttoptr i64 %pfnCallback.coerce0 to float (%class.CMenus*, <2 x float>, <2 x float>)*
  
    %43 = phi float (%class.CMenus*, <2 x float>, <2 x float>)* [ %memptr.virtualfn, %memptr.virtual ], [ %memptr.nonvirtualfn, %memptr.nonvirtual ]
    %call49 = call noundef float %43(%class.CMenus* noundef nonnull align 8 %this.adjusted, <2 x float> %agg.tmp48.sroa.0.0.copyload, <2 x float> %agg.tmp48.sroa.2.0.copyload), !dbg !76845
```



