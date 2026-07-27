# XAML Runtime Architecture

本文记录 AeroGUI-R 产品级 XAML 内核的当前架构契约。`C:\Projects\AeroGUI`
仅用于公开行为对照；AeroGUI-R 保持 clean-room、C++17、无异常、无 RTTI。
公开加载链保持统一的 node-pipeline 形态；当前 tokenizer 在进入 `XamlNodeReader`
前会物化当前文档的 token 序列，因此文档不把它描述为零缓冲流式解析。

## 唯一加载链路

所有入口最终收敛到同一条实例化链路：

```text
ResourceUri
  -> XamlSourceProviderRegistry
  -> UTF-8 XML tokenizer (Expat or built-in)
  -> XamlNodeReader
  -> XamlLoadSession
  -> XamlObjectWriter
  -> Resource / Style / Template plans
  -> RuntimeHost mount and presentation services
```

- URI XAML 由 provider 读取。
- 文本 XAML 只跳过 provider，仍从 tokenizer 进入同一 node pipeline。
- compiled XAML 反序列化为同一种 node document，再进入同一个 load session。
- 主题、应用资源和普通文档都以 metadata 可实例化的
  `ResourceDictionary` 为根对象。

`XamlObjectWriter` 保存冻结的 schema 与服务配置，不保存跨文档可变状态。
`XamlSchemaContext` 只负责 metadata 解析和行为调用；成员、类型、资源与
markup-extension 行为集中在一次冻结的 `XamlFacetStore` 中。每次加载创建一次性
`XamlLoadSession`。对象栈、事务、文档 NameScope、资源
作用域、base URI、templated parent 和 ambient resource chain 都属于 session。
任一对象创建、成员赋值、字典依赖或 `LoadComponent` 步骤失败时，session
撤销本次声明产生的内容和名称注册。

模板 prototype edge 同样属于 session 的 deferred-content plan。模板结束初始化
时编译并释放自己的 edge；文档提交前若仍有未完成 edge，整次加载失败。URI、
依赖清单和资源 `Source` 解析由 session 的 pre-commit finalizer 完成，因此不会
出现“对象已经提交、资源依赖随后失败”的半成功状态。

## URI 与 provider

`Base::ResourceUri` 负责解析、相对解析和规范化。首版支持相对 URI、`file`、
`pack/application` 以及 `assembly;component` 形式。安全策略由
`XamlLoadPolicy` 执行，默认拒绝网络 scheme。

`XamlSourceProviderRegistry` 的路由顺序固定为：

1. scheme + assembly
2. scheme
3. assembly
4. default

内置 provider 包括文件 provider 和可冻结的嵌入式 provider。规范 URI 同时
用于依赖列表与递归加载栈，因此 `Source`、merged dictionary 和间接依赖循环
具有稳定诊断。

## XML 安全边界

`AERO_WITH_EXPAT=ON` 使用 `ExpatXmlTokenizer`，关闭 DTD、外部实体与参数实体；
无 Expat 时使用内置 UTF-8 tokenizer。两个实现共享输入字节数、深度、属性数
和 token 长度限制，并通过 conformance 用例验证相同的 XAML node 序列。
provider 与 tokenizer 都不执行网络访问。

## 资源系统

`NameScope`、`ResourceKey`、`ResourceDictionary` 和 `ResourceResolver`
属于 Presentation。字典值统一为 `Core::Value`，键支持字符串和 `TypeId`。

字典内部查找顺序为：

1. 当前字典本地条目
2. `MergedDictionaries` 逆序

运行时环境查找顺序为：

1. 元素及逻辑祖先
2. 模板上下文
3. application
4. theme
5. system

字典支持 `Source`、merged dictionaries、seal/read-only、generation 和变更
订阅。`StaticResource` 在当前 load session 即时解析并拒绝前向引用。
`DynamicResource` 保存表达式并订阅完整字典链；字典 generation 或环境层
变化后会重新求值。

`Source` 产生的字典先以稳定共享句柄暂存，全部依赖解析成功后再统一合并；
提交中途失败会逆序撤销已合并项。根元素、视觉计划中的元素和嵌套资源对象
都通过 `XamlTypeFacet::resolveResourceScope` 遍历；隐式键通过
`resolveImplicitResourceKey` 生成。Loader 不再识别 Style、ControlTemplate、
DataTemplate 等具体资源类型。

## Style 与 Template

`Style`、`Setter` 和 property `Trigger` 是 metadata 对象。Style 首次应用前
验证 TargetType、BasedOn 类型与循环、setter 值和 trigger，并 seal 成不可变
plan。显式 Style 使用 Style provider；隐式 Style 使用 `TypeId` 资源键。
本地值仍高于 Style，现有 dependency-property 优先级不变。

`ControlTemplate` 保存不可变 `TemplateProgram`。XAML authored visual tree 只在
定义阶段编译为 prototype blueprint；每次应用都重新创建独立对象、NameScope
和模板资源环境。当前统一模板语义包括：

- templated parent
- TemplateBinding
- property trigger
- setter-only VisualState
- FindName
- ContentPresenter 内容投影
- 模板资源
- 安全换模板与卸载清理

`DataTemplate` 和 `ItemsPanelTemplate` 共用 `DeferredObjectProgram`，不再各自
保存不同签名的裸回调。data template 的 payload 是数据项，items-panel
template 使用空 payload；两者都携带规范 base URI 和资源字典。

`XamlPresentationObjectModel` 是 Style/Setter/Trigger/Template 的唯一公开
schema 注册入口。内部 Style 与 Template facet 只负责把 metadata 对象编译为
不可变 plan，不再作为 RuntimeHost 或 `aero-xamlc` 的独立产品调用层。Binding、
DynamicResource 与 Type 扩展统一返回 `XamlProvidedValue`：普通值由 writer 写入，
表达式由 writer 安装并纳入事务，已处理结果携带可选 rollback token。

## RuntimeHost

`RuntimeHost` 拥有 provider registry、application/theme/system 资源层、Core
activation facets、StyleManager、TemplateManager 和 DynamicResource 环境。
Style/Template 的解析、应用和卸载策略由内部 `RuntimePresentationServices` 承担；
`RuntimeHost` 只组合服务和管理 mount 生命周期。

Generic/Light/Dark 都是普通 ResourceDictionary。Light/Dark 提供调色板资源，
Generic 提供隐式 Style，ControlTemplate 由 Style 的 `Template` setter 提供。
ControlGallery 不再逐控件调用主题 apply，也不再包含程序化外观补丁。

本机构建默认通过 `AeroCompiledThemes` 调用 `aero-xamlc --origin`，将 Light、
Dark、Generic 编译为 AXIR 并嵌入 `AeroRuntime`。交叉编译可提供
`AERO_HOST_XAMLC_EXECUTABLE`；关闭 `AERO_PRECOMPILE_BUILTIN_THEMES` 时只嵌入
原始 XAML，并通过相同 pack URI/provider 路径加载。`LoadBuiltInTheme` 优先加载
compiled document，compiled payload 不存在或 schema identity 不兼容时确定性
回退到内嵌源 XAML。

## 构建边界

`AeroMarkupKernel` 包含 tokenizer、node reader 和基础 compiled document/cache，
只依赖 `AeroCore`。Schema 验证、object writer、资源、Binding、Style 与 Template
位于上层 `AeroMarkup`，后者才依赖 Controls。架构检查禁止 Kernel 反向包含
Presentation、Controls 或 Markup integration 目录。

## Compiled XAML

compiled cache format 当前为 6。document 只保存加载链实际消费的 origin URI、
依赖清单和 node IR；模板不再维护一份未使用的旁路 range 表。
runtime/compiled 使用相同 object writer。

当 cache identity 或 metadata schema 不兼容且调用方提供了可加载的 origin
URI 时，`XamlLoader::LoadCompiled` 回退到该源文档；没有源 URI 时返回明确的
`Unsupported` 或 `ValidationFailed`。

`aero-xamlc` 与 RuntimeHost 注册相同的 Resource、DynamicResource、Style、
Template 和 `XamlContentWriter` schema extension。值类型元素（例如
`<Color Value="..."/>`）在 compiled schema validation 与 object writer 中
使用相同帧语义，不再需要主题专用 object model。

## 本轮明确不包含

- DataTrigger、MultiTrigger、EventTrigger
- 动画式 VisualState transition
- HierarchicalDataTemplate
- 实时视觉树热重载
