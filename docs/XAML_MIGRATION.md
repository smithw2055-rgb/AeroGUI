# XAML Runtime Migration Guide

本文面向从旧 AeroGUI-R XAML/主题接口迁移的应用与模块。

## 入口迁移

| 旧调用 | 新调用 |
| --- | --- |
| `RuntimeHost::LoadXaml(text)` | `RuntimeHost::ParseXaml(text, baseUri)` |
| 手工读文件后 parse | 注册 provider 后 `RuntimeHost::LoadXaml(uri)` |
| 自建 compiled object-writer 路径 | `LoadCompiledXaml(bytes, originUri)` |
| XAML activation wrapper | `Core::ActivationProviderRegistry` |
| `RuntimeHost::Load(reader/document)` | `ParseXaml` / `LoadCompiledXaml` |
| root-only object-writer load | 保留完整 `XamlLoadResult` |
| 独立注册 Style/Template XAML extension | `XamlPresentationObjectModel::Register` |

`LoadXaml` 的参数现在是 URI，不再猜测字符串是路径还是 XAML 内容。相对 URI
需要有效 base URI；网络 scheme 默认失败。

## 资源与主题迁移

旧代码中的 theme token map、私有 theme DOM 和 `XamlTheme::Apply(control)`
应改为普通资源：

```cpp
host.LoadBuiltInTheme(Aero::BuiltInTheme::Dark);
```

主题 XAML 使用普通 `ResourceDictionary`。控件默认外观使用类型键隐式 Style：

```xml
<Style xmlns="urn:aero" TargetType="Button">
  <Setter Property="Template">
    <Setter.Value>
      <ControlTemplate TargetType="Button">
        <ControlTemplate.VisualTree>
          <Border>
            <ContentPresenter />
          </Border>
        </ControlTemplate.VisualTree>
      </ControlTemplate>
    </Setter.Value>
  </Setter>
</Style>
```

应用资源、主题资源和系统资源不再使用不同对象模型。

## 控件迁移

- `FrameworkElement.Resources` 放元素本地资源。
- `FrameworkElement.Style` 放显式 Style。
- `Control.Template` 放显式 ControlTemplate。
- 默认样式用 `ResourceKey::FromType(controlType)`。
- 不要在控件构造函数或 Gallery glue code 中手工应用主题属性。

本地 DP 值优先于 Style；卸载前无需手工清理 Style/Template provider，
RuntimeHost 会统一 detach。

## DataTemplate 与 ItemsPanelTemplate

两者现在共享 `DeferredObjectProgram`。旧 `ItemsPanelFactory(void*)` 需要改为
统一签名：

```cpp
Aero::Base::Result<Aero::Base::Ref<Aero::Base::Object>>
CreatePanel(
    const Aero::Base::Ref<Aero::Base::Object>&,
    void* context) noexcept;
```

DataTemplate 的第一个参数仍是数据项；ItemsPanelTemplate 收到空引用。

## Compiled XAML

重新用当前 `aero-xamlc` 生成 cache format 6 文件。document 现在包含 origin
URI、依赖和统一 node IR。

需要稳定 origin 的离线编译使用：

```text
aero-xamlc --origin pack://application:,,,/My.App;component/Main.xaml \
  Main.xaml Main.axir
```

内置 Light/Dark/Generic 由 `AeroCompiledThemes` 在构建时以同一命令生成并嵌入
Runtime；应用代码不再自行读取或编译这些主题。

若旧 cache 与 runtime 不兼容：

- 提供可加载 origin URI：loader 回退到 source/provider 路径；
- 没有 source：返回明确失败，不尝试猜测旧格式。

## 构建配置

推荐：

```text
-DAERO_WITH_EXPAT=ON
```

`OFF` 保留内置 tokenizer，用于无第三方环境和 conformance 对照。两种配置都
禁止 DTD、外部实体和网络实体。

## 已删除的旧实现

以下旧实现已获得单独授权并从仓库删除：

- `include/Aero/Markup/XamlTheme.hpp`
- `src/markup/XamlTheme.cpp`
- `src/markup/XamlThemeObjectModel.cpp`
- `src/markup/XamlThemeObjectModel.hpp`
- `src/markup/XamlNamesResources.cpp`
- `tests/markup/XamlThemeTests.cpp`

普通 XAML、主题和资源加载路径不再依赖这些专用实现。
