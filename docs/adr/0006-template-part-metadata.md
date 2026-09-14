# ADR-0006：TemplatePart 模板部件元数据（对标 Noesis TypeMetaData）

- **状态**：Accepted
- **日期**：2026-09-07
- **决策者**：AeroGUI maintainers
- **关联**：Noesis 对照（`TemplatePartMetaData`）、`docs/SOURCE_MAP.md` M2 缺口

## 背景

Noesis 以 `TypeMetaData` 派生小件（`ContentProperty`、`DependsOn`、
`TemplatePart`、`TypeConverter`、`NameScopeProperty`）描述类型级作者信息。
Aero 的 `TypeBuilder` 已有 `Content`/`Collection`/`TextConverter`/
`EventHandler`，但模板部件声明完全缺失：`Control::GetTemplateChild(name)`
与 `PART_*` 约定（`ScrollViewer` 等已在使用）没有任何声明式元数据支撑，
模板工具与诊断无法校验部件完备性。

候选存储有两条路：

1. **MetaTable facet**：`TypeRecord` 有空闲 `reserved` 字段、`MetadataFacetMask`
   有空闲位。但 `FacetDraft::facets[11]` 是单索引模型，而 PART 是一对多；
   range 编码（首索引+计数打包）或 side-table 都要改 seal/指纹链，且
   `MetaTable` 只服务冻结运行时 facet，与作者期描述不在一层。
2. **TypeRegistry 描述子列表**：`EventHandlerDescriptor` 已是同构先例——
   `TypeInfo::eventHandlers_`（`Base::String` 自有存储）+ `RegisterEventHandler`
  （冻结/空名/判重）+ `FindEventHandler`（含基类遍历）+ `Registry.inl` 组合并。
   模板部件是同样的“每类型多条具名记录”，零 facet 体系改动。

## 决策

采用方案 2，镜像 `EventHandler` 全链路：

- `TemplatePartDescriptor { Base::String name; TypeId partType; }` 存于
  `TypeInfo::templateParts_`，`TemplateParts()` span 只读暴露。
- `TypeRegistry::RegisterTemplatePart`（冻结/空名/非法 partType/同名判重）
  与 `FindTemplatePart(owner, name, includeBaseTypes)`（基类遍历）。
- `Registry.inl` 组合并循环镜像 `EventHandlers`。
- `MetadataAuthoringSession::TemplatePart` + `TypeBuilder::TemplatePart`
 （`StringView`/`TypeId` 拼写，符合 Meta facade 无 `Base::` 约束）。
- `MetaConsumer.cpp` 作者覆盖；诊断 `TemplatePartCount()` 对称补充。

`DependsOn` 本次不做：它需要失效/重算引擎语义配合，不只是记录；
待属性引擎设计时另立项。

## 后果

- 纯加法：无现有 facet/冻结格式改动，无破坏性变更；模板引擎消费
  （`GetTemplateChild` 校验、工具完备性检查）可后续增量接入查询 API。
- `TypeInfo` 增大一个 vector（空类型零开销外仅 24B 控制块，接受）。
