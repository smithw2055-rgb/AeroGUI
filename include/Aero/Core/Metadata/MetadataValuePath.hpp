#pragma once

#include <Aero/Base/Span.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Core/Metadata/MetadataRuntime.hpp>

namespace Aero::Core {

// Resolves metadata paths across object properties and nested value-object
// fields. Value-object writes are copy-on-write and are propagated back through
// each containing field/property, while object children are mutated in place.
class MetadataValuePath final {
public:
    explicit MetadataValuePath(MetadataRuntime& runtime) noexcept
        : runtime_(&runtime) {}

    Base::Result<Value> Get(
        const Base::Object& root,
        Base::Span<const Base::StringView> path) const noexcept {
        if (!Ready() || path.Empty()) return InvalidPath();
        return GetObject(root, path);
    }

    Base::Result<Value> Get(
        const Value& root,
        Base::Span<const Base::StringView> path) const noexcept {
        if (!Ready() || path.Empty()) return InvalidPath();
        return GetValue(root, path);
    }

    Base::Result<void> Set(
        Base::Object& root,
        Base::Span<const Base::StringView> path,
        const Value& value) const noexcept {
        if (!Ready() || path.Empty() || value.IsUnset()) return InvalidPath();
        return SetObject(root, path, value);
    }

    Base::Result<void> Set(
        Value& root,
        Base::Span<const Base::StringView> path,
        const Value& value) const noexcept {
        if (!Ready() || path.Empty() || value.IsUnset()) return InvalidPath();
        Base::Result<bool> changed = SetValue(root, path, value);
        return changed ? Base::Result<void>()
                       : Base::Result<void>(changed.GetStatus());
    }

private:
    MetadataRuntime* runtime_ = nullptr;

    bool Ready() const noexcept {
        return runtime_ != nullptr && runtime_->IsFrozen();
    }

    static Base::Status InvalidPath() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::InvalidArgument,
            "Metadata value path is empty, unset, or runtime is not frozen");
    }

    static Base::Status UnsupportedNode() noexcept {
        return Base::Status::Failure(
            Base::ErrorCode::Unsupported,
            "Metadata value path cannot traverse this value kind");
    }

    static Base::Span<const Base::StringView> Tail(
        Base::Span<const Base::StringView> path) noexcept {
        return path.Subspan(1U, path.Size() - 1U);
    }

    Base::Result<Value> GetObject(
        const Base::Object& object,
        Base::Span<const Base::StringView> path) const noexcept {
        const PropertyInfo* property =
            runtime_->Types().FindProperty(
                object.RuntimeType(), path[0], true);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata object property in path was not found");
        }
        Base::Result<Value> current =
            runtime_->GetProperty(object, property->Id());
        if (!current || path.Size() == 1U) return current;
        return GetValue(current.Value(), Tail(path));
    }

    Base::Result<Value> GetValue(
        const Value& value,
        Base::Span<const Base::StringView> path) const noexcept {
        if (value.Kind() == ValueKind::Object) {
            if (value.IsNullObject()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Metadata value path reached a null object");
            }
            return GetObject(*value.AsObject(), path);
        }
        if (value.Kind() != ValueKind::Custom) return UnsupportedNode();
        const FieldInfo* field =
            runtime_->Types().FindField(value.Type(), path[0]);
        if (field == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata value field in path was not found");
        }
        Base::Result<Value> current =
            runtime_->GetValueMember(value, field->Id());
        if (!current || path.Size() == 1U) return current;
        return GetValue(current.Value(), Tail(path));
    }

    Base::Result<void> SetObject(
        Base::Object& object,
        Base::Span<const Base::StringView> path,
        const Value& value) const noexcept {
        const PropertyInfo* property =
            runtime_->Types().FindProperty(
                object.RuntimeType(), path[0], true);
        if (property == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata object property in path was not found");
        }
        if (path.Size() == 1U) {
            return runtime_->SetProperty(object, property->Id(), value);
        }
        Base::Result<Value> child =
            runtime_->GetProperty(object, property->Id());
        if (!child) return child.GetStatus();
        Base::Result<bool> changed =
            SetValue(child.Value(), Tail(path), value);
        if (!changed) return changed.GetStatus();
        return changed.Value()
            ? runtime_->SetProperty(object, property->Id(), child.Value())
            : Base::Result<void>();
    }

    Base::Result<bool> SetValue(
        Value& owner,
        Base::Span<const Base::StringView> path,
        const Value& value) const noexcept {
        if (owner.Kind() == ValueKind::Object) {
            if (owner.IsNullObject()) {
                return Base::Status::Failure(
                    Base::ErrorCode::NotFound,
                    "Metadata value path reached a null object");
            }
            Base::Result<void> result =
                SetObject(*owner.AsObject(), path, value);
            return result ? Base::Result<bool>(false)
                          : Base::Result<bool>(result.GetStatus());
        }
        if (owner.Kind() != ValueKind::Custom) return UnsupportedNode();
        const FieldInfo* field =
            runtime_->Types().FindField(owner.Type(), path[0]);
        if (field == nullptr) {
            return Base::Status::Failure(
                Base::ErrorCode::NotFound,
                "Metadata value field in path was not found");
        }
        if (path.Size() == 1U) {
            Base::Result<void> result =
                runtime_->SetValueMember(owner, field->Id(), value);
            return result ? Base::Result<bool>(true)
                          : Base::Result<bool>(result.GetStatus());
        }
        Base::Result<Value> child =
            runtime_->GetValueMember(owner, field->Id());
        if (!child) return child.GetStatus();
        Base::Result<bool> changed =
            SetValue(child.Value(), Tail(path), value);
        if (!changed) return changed.GetStatus();
        if (!changed.Value()) return false;
        Base::Result<void> stored =
            runtime_->SetValueMember(owner, field->Id(), child.Value());
        return stored ? Base::Result<bool>(true)
                      : Base::Result<bool>(stored.GetStatus());
    }
};

} // namespace Aero::Core
