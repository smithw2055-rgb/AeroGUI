# ADR-0005：SDK 精简重构（对标 Noesis 单核形态）

- **状态**：Accepted
- **日期**：2026-09-05
- **决策者**：AeroGUI maintainers
- **修订关系**：收敛 `docs/spec/PUBLIC_HEADER_MODEL.md` 与 `docs/SDK_PACKAGING.md` 的 0.3 边界

## 背景

SDK 公共头膨胀到 464 个，6 个并列包（Base/Gui/Render/App/Audio/Meta）让
Noesis/WPF 用户难以找到最小入口。已确认的冗余：双 Run 入口、空伞
`AeroRender/Render.hpp`、Controls 扁平转发头、`DefineComponentModule` 放错头、
双后端 `StatePreservationPolicy` 各自定义。触发器三体系与双 Interop 经核查
实为 WPF 语义分工，不宜硬删，改为归属澄清。

## 决策

1. 单入口：只留 `Application::Run()`，删除 `Aero::App::Run()` 自由函数与
   `DesktopHost` 单参构造；`App.xaml` bootstrap 经 `App::RunOptions.applicationFile`
   进入同一 host。
2. 删空伞 `AeroRender/Render.hpp`；消费者直含 `RenderDevice.hpp` /
   `RenderTarget.hpp` + `Aero/IRenderer.hpp`。
3. Primitives 拥有 ButtonBase/RepeatButton/ToggleButton/RangeBase 声明权，
   删除 Controls 下 4 个扁平转发头；门禁由兼容伞检查改为 `aero_forbid_file`。
4. `DefineComponentModule` 归位 `Aero/Module.hpp`（轻量组合头），`Meta.hpp`
   只保留类型作者 API；对外仍以 `<Aero/Meta.hpp>` 为单一作者入口。
5. 抽取 `<AeroRender/BackendCommon.hpp>` 拥有共享 `StatePreservationPolicy`；
   新增 `<AeroPCH.hpp>` 单一伞头对标 `NoesisPCH.h` 分组。
6. 触发器归属写入 `PUBLIC_HEADER_MODEL.md`；IRenderer/ViewRenderer 与
   双 Interop 以注释澄清职责，不做重命名/删除。

## 后果

- 破坏性变更（无 shim）：`App::Run` 调用方改为 `Application::Run`；
  扁平 Controls 头与 `Render.hpp` 引用改为 Primitives/直含路径。
- DLL 合并（Base 并入 Gui、Audio 回调化）留待构建验证后跟进，不在本 ADR。
- 门禁：`AeroArchitectureCheck` 全过；`AeroConventionsCheck` 的 src 重复
  include 失败为工作树既有修改，与本重构无关。
