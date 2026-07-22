#include <Aero/Markup/XamlPanelLayout.hpp>

#include <cmath>
#include <limits>

namespace Aero::Markup {
namespace {

Base::Status InvalidPanelLayoutXaml(const char* message) noexcept {
    return Base::Status::Failure(Base::ErrorCode::InvalidArgument, message);
}

bool IsAttached(const Core::PropertyInfo& property) noexcept {
    return (static_cast<std::uint32_t>(property.Flags()) &
        static_cast<std::uint32_t>(Core::PropertyFlags::Attached)) != 0U;
}

Base::Result<double> Number(const XamlValue& value) noexcept {
    double result = 0.0;
    switch (value.Kind()) {
    case XamlValueKind::SignedInteger: result = static_cast<double>(value.AsSignedInteger()); break;
    case XamlValueKind::UnsignedInteger: result = static_cast<double>(value.AsUnsignedInteger()); break;
    case XamlValueKind::Double: result = value.AsDouble(); break;
    default: return InvalidPanelLayoutXaml("Attached layout property requires a number");
    }
    return std::isfinite(result)
        ? Base::Result<double>(result)
        : Base::Result<double>(InvalidPanelLayoutXaml("Attached layout value must be finite"));
}

} // namespace

XamlPanelLayoutExtension::XamlPanelLayoutExtension(
    Core::TypeId canvasType, Core::TypeId gridType,
    Base::IAllocator* allocator) noexcept
    : allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      types_(allocator_), values_(allocator_), canvasType_(canvasType), gridType_(gridType) {}

Base::Result<void> XamlPanelLayoutExtension::TryRegisterType(
    const XamlPanelLayoutTypeRegistration& registration) noexcept {
    if (schema_ != nullptr || registration.type == Core::InvalidTypeId ||
        registration.cast == nullptr) {
        return InvalidPanelLayoutXaml("XAML panel-layout type registration is invalid");
    }
    for (const XamlPanelLayoutTypeRegistration& current : types_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "XAML panel-layout type is already registered");
        }
    }
    return types_.TryPushBack(registration);
}

Base::Result<std::uint32_t> XamlPanelLayoutExtension::Register(
    XamlSchemaContext& schema) noexcept {
    if (schema_ != nullptr || schema.IsFrozen() || types_.Empty() ||
        canvasType_ == Core::InvalidTypeId || gridType_ == Core::InvalidTypeId ||
        !schema.Types().IsFrozen()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "XAML panel-layout registries are not ready");
    }
    const Core::PropertyInfo* left = schema.Types().FindProperty(
        canvasType_, Base::StringView("Left"), false);
    const Core::PropertyInfo* top = schema.Types().FindProperty(
        canvasType_, Base::StringView("Top"), false);
    const Core::PropertyInfo* row = schema.Types().FindProperty(
        gridType_, Base::StringView("Row"), false);
    const Core::PropertyInfo* column = schema.Types().FindProperty(
        gridType_, Base::StringView("Column"), false);
    if (left == nullptr || top == nullptr || row == nullptr || column == nullptr ||
        !IsAttached(*left) || !IsAttached(*top) || !IsAttached(*row) ||
        !IsAttached(*column)) {
        return InvalidPanelLayoutXaml("XAML attached panel-layout metadata is invalid");
    }
    const Core::MemberId members[] = {left->Id(), top->Id(), row->Id(), column->Id()};
    for (Core::MemberId member : members) {
        if (schema.FindMemberAdapter(member) != nullptr) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "XAML attached panel-layout member already has an adapter");
        }
        Base::Result<void> registered = schema.TryRegisterMemberAdapter({
            member, XamlMemberWriteMode::SetOnce, nullptr, this,
            &SetAttachedMember, true});
        if (!registered) return registered.GetStatus();
    }
    canvasLeftMember_ = left->Id(); canvasTopMember_ = top->Id();
    gridRowMember_ = row->Id(); gridColumnMember_ = column->Id();
    schema_ = &schema;
    return 4U;
}

const XamlPanelLayoutTypeRegistration* XamlPanelLayoutExtension::FindType(
    Core::TypeId type) const noexcept {
    Core::TypeId current = type;
    while (current != Core::InvalidTypeId && schema_ != nullptr) {
        for (const XamlPanelLayoutTypeRegistration& registration : types_) {
            if (registration.type == current) return &registration;
        }
        const Core::TypeInfo* info = schema_->Types().FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

Base::Result<Core::LayoutElement*> XamlPanelLayoutExtension::ResolveElement(
    Base::Object& object, const XamlServiceProvider& services) const noexcept {
    const XamlPanelLayoutTypeRegistration* registration = FindType(
        services.targetObjectType);
    if (schema_ == nullptr || services.targetObject != &object ||
        registration == nullptr) {
        return InvalidPanelLayoutXaml("XAML attached panel-layout target is invalid");
    }
    Core::LayoutElement* element = registration->cast(object, registration->context);
    return element != nullptr && element->RuntimeType() == services.targetObjectType
        ? Base::Result<Core::LayoutElement*>(element)
        : Base::Result<Core::LayoutElement*>(InvalidPanelLayoutXaml(
            "XAML attached panel-layout runtime type does not match metadata"));
}

Base::Result<XamlPanelLayoutExtension::Values*> XamlPanelLayoutExtension::FindOrCreate(
    Core::LayoutElement& child) noexcept {
    Values* found = FindValues(child);
    if (found != nullptr) return found;
    Base::Result<void> added = values_.TryPushBack({&child});
    return added ? Base::Result<Values*>(&values_.Back()) :
        Base::Result<Values*>(added.GetStatus());
}

XamlPanelLayoutExtension::Values* XamlPanelLayoutExtension::FindValues(
    Core::LayoutElement& child) noexcept {
    for (Values& current : values_) if (current.child == &child) return &current;
    return nullptr;
}

void XamlPanelLayoutExtension::RemoveValues(Core::LayoutElement& child) noexcept {
    for (std::uint32_t index = 0U; index < values_.Size(); ++index) {
        if (values_[index].child != &child) continue;
        if (index + 1U < values_.Size()) values_[index] = values_.Back();
        values_.PopBack();
        return;
    }
}

Base::Result<void> XamlPanelLayoutExtension::SetAttachedMember(
    Base::Object& object, const XamlValue& value,
    const XamlServiceProvider& services, void* context) noexcept {
    auto* extension = static_cast<XamlPanelLayoutExtension*>(context);
    if (extension == nullptr) return InvalidPanelLayoutXaml("XAML panel-layout extension is unavailable");
    Base::Result<Core::LayoutElement*> child = extension->ResolveElement(object, services);
    if (!child) return child.GetStatus();
    Base::Result<Values*> stored = extension->FindOrCreate(*child.Value());
    if (!stored) return stored.GetStatus();
    Values& values = *stored.Value();
    Base::Result<double> number = Number(value);
    if (!number) return number.GetStatus();
    if (services.targetMember == extension->canvasLeftMember_) { values.left = number.Value(); values.hasLeft = true; return {}; }
    if (services.targetMember == extension->canvasTopMember_) { values.top = number.Value(); values.hasTop = true; return {}; }
    if (services.targetMember == extension->gridRowMember_ || services.targetMember == extension->gridColumnMember_) {
        if (number.Value() < 0.0 || number.Value() > static_cast<double>(UINT32_MAX) ||
            std::floor(number.Value()) != number.Value()) return InvalidPanelLayoutXaml("Grid.Row and Grid.Column require a non-negative integer");
        if (services.targetMember == extension->gridRowMember_) { values.row = static_cast<std::uint32_t>(number.Value()); values.hasRow = true; }
        else { values.column = static_cast<std::uint32_t>(number.Value()); values.hasColumn = true; }
        return {};
    }
    return Base::Status::Failure(Base::ErrorCode::NotFound, "XAML panel-layout member is not registered");
}

Base::Result<void> XamlPanelLayoutExtension::ConfigureCanvasChild(
    Base::Object&, Core::LayoutElement& parent, Core::LayoutElement& child,
    void* context) noexcept {
    auto* extension = static_cast<XamlPanelLayoutExtension*>(context);
    auto* canvas = static_cast<Core::Canvas*>(&parent);
    if (extension == nullptr || canvas == nullptr) return InvalidPanelLayoutXaml("Canvas layout configurator is invalid");
    Values* values = extension->FindValues(child);
    if (values == nullptr || (!values->hasLeft && !values->hasTop)) return {};
    Base::Result<void> applied = canvas->SetChildPosition(child, {values->left, values->top});
    if (applied) extension->RemoveValues(child);
    return applied;
}

Base::Result<void> XamlPanelLayoutExtension::ConfigureGridChild(
    Base::Object&, Core::LayoutElement& parent, Core::LayoutElement& child,
    void* context) noexcept {
    auto* extension = static_cast<XamlPanelLayoutExtension*>(context);
    auto* grid = static_cast<Core::Grid*>(&parent);
    if (extension == nullptr || grid == nullptr) return InvalidPanelLayoutXaml("Grid layout configurator is invalid");
    Values* values = extension->FindValues(child);
    if (values == nullptr || (!values->hasRow && !values->hasColumn)) return {};
    Base::Result<void> applied = grid->SetChildCell(child, values->row, values->column);
    if (applied) extension->RemoveValues(child);
    return applied;
}

} // namespace Aero::Markup
