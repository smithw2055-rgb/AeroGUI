#include <Aero/Core/Input.hpp>

#include <cmath>

namespace Aero::Core {
namespace {

bool Contains(Size size, Point point) noexcept {
    return point.x >= 0.0 && point.y >= 0.0 &&
        point.x < size.width && point.y < size.height;
}

} // namespace

HitTestManager::HitTestManager(TypeRegistry& types, Base::IAllocator* allocator) noexcept
    : types_(&types),
      allocator_(allocator != nullptr ? allocator : &Base::GetDefaultAllocator()),
      typesByRuntimeType_(allocator_) {}

Base::Result<void> HitTestManager::TryRegisterType(
    const HitTestTypeRegistration& registration) noexcept {
    if (registration.type == InvalidTypeId || registration.cast == nullptr ||
        types_->FindType(registration.type) == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test type registration is invalid");
    }
    for (const HitTestTypeRegistration& current : typesByRuntimeType_) {
        if (current.type == registration.type) {
            return Base::Status::Failure(Base::ErrorCode::AlreadyExists,
                "Hit-test type is already registered");
        }
    }
    return typesByRuntimeType_.TryPushBack(registration);
}

const HitTestTypeRegistration* HitTestManager::FindRegistration(TypeId type) const noexcept {
    TypeId current = type;
    while (current != InvalidTypeId) {
        for (const HitTestTypeRegistration& registration : typesByRuntimeType_) {
            if (registration.type == current) return &registration;
        }
        const TypeInfo* info = types_->FindType(current);
        if (info == nullptr) break;
        current = info->BaseType();
    }
    return nullptr;
}

LayoutElement* HitTestManager::AsLayoutElement(TreeNode& node) const noexcept {
    const HitTestTypeRegistration* registration = FindRegistration(node.RuntimeType());
    if (registration == nullptr || registration->cast == nullptr) return nullptr;
    LayoutElement* element = registration->cast(node, registration->context);
    return element != nullptr && element->RuntimeType() == node.RuntimeType() ? element : nullptr;
}

Base::Result<HitTestResult> HitTestManager::HitTest(
    TreeNode& root, Point position) const noexcept {
    if (!IsFinite(position)) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test position must be finite");
    }
    LayoutElement* rootElement = AsLayoutElement(root);
    if (rootElement == nullptr) {
        return Base::Status::Failure(Base::ErrorCode::InvalidArgument,
            "Hit-test root is not a registered LayoutElement");
    }
    return HitTestElement(*rootElement, position);
}

Base::Result<HitTestResult> HitTestManager::HitTestElement(
    LayoutElement& element, Point position) const noexcept {
    if (!element.IsArrangeValid()) {
        return Base::Status::Failure(Base::ErrorCode::InvalidState,
            "Hit-test requires an arranged visual tree");
    }
    if (!Contains(element.RenderSize(), position)) return HitTestResult{};

    const Base::Span<TreeNode* const> children = element.VisualChildren();
    for (std::uint32_t index = children.Size(); index > 0U; --index) {
        TreeNode* childNode = children[index - 1U];
        if (childNode == nullptr) continue;
        LayoutElement* child = AsLayoutElement(*childNode);
        if (child == nullptr) continue;
        const Rect slot = child->LayoutSlot();
        Base::Result<HitTestResult> nested = HitTestElement(*child,
            {position.x - slot.x, position.y - slot.y});
        if (!nested) return nested.GetStatus();
        if (nested.Value().HasTarget()) return nested;
    }
    return HitTestResult{&element, position};
}

} // namespace Aero::Core
