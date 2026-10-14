#pragma once

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Result.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/StringView.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/Value.hpp>
#include <Aero/DependencyProperty.hpp>

#include <cstdint>

namespace Aero {

struct PropertyPathToken {
    Base::StringView raw;
    Base::StringView typeQualifier;
    Base::StringView propertyName;
    std::uint32_t collectionIndex = UINT32_MAX;
    bool isTypeQualified = false;
    bool hasIndex = false;
};

// WPF-compatible PropertyPath representing a path to a dependency property or CLR property.
// Used across Data Binding, Storyboard/Animation targets, and Setter targets.
class AERO_GUI_API PropertyPath : public Base::Object {
    AERO_DECLARE_TYPE(PropertyPath, Base::Object)

public:
    PropertyPath() noexcept = default;
    explicit PropertyPath(StringView path) noexcept { (void)path_.Assign(path); }
    explicit PropertyPath(Base::String path) noexcept : path_(std::move(path)) {}

    PropertyPath(const PropertyPath& other) noexcept : Base::Object() {
        (void)path_.Assign(other.path_.View());
    }
    PropertyPath(PropertyPath&& other) noexcept : Base::Object(), path_(std::move(other.path_)) {}

    PropertyPath& operator=(const PropertyPath& other) noexcept {
        if (this != &other) {
            (void)path_.Assign(other.path_.View());
        }
        return *this;
    }
    PropertyPath& operator=(PropertyPath&& other) noexcept {
        if (this != &other) {
            path_ = std::move(other.path_);
        }
        return *this;
    }

    ~PropertyPath() override = default;

    StringView Path() const noexcept { return path_.View(); }
    StringView GetPath() const noexcept { return path_.View(); }
    void SetPath(StringView path) noexcept { (void)path_.Assign(path); }

    bool Empty() const noexcept { return path_.Empty(); }
    bool GetIsEmpty() const noexcept { return path_.Empty(); }

    static Base::Result<void> ParseTokens(
        Base::StringView path,
        Base::Vector<PropertyPathToken>& outTokens) noexcept;

    Base::Result<void> ParseTokens(
        Base::Vector<PropertyPathToken>& outTokens) const noexcept {
        return ParseTokens(path_.View(), outTokens);
    }

private:
    Base::String path_;
};

} // namespace Aero

