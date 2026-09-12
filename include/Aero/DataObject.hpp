#ifndef __AERO_DATAOBJECT_HPP__
#define __AERO_DATAOBJECT_HPP__

#include <Aero/Base/Config.hpp>
#include <Aero/Base/Object.hpp>
#include <Aero/Base/Ref.hpp>
#include <Aero/Base/String.hpp>
#include <Aero/Base/Vector.hpp>
#include <Aero/DependencyProperty.hpp>
#include <Aero/Value.hpp>

namespace Aero {

/// Identifies the standard clipboard / drag-and-drop data formats used by
/// DataObject. Mirrors the well-known names from the reference framework.
struct AERO_GUI_API DataFormats {
    static constexpr Base::StringView Text() noexcept {
        return Base::StringView("Text");
    }
    static constexpr Base::StringView UnicodeText() noexcept {
        return Base::StringView("UnicodeText");
    }
    static constexpr Base::StringView FileDropList() noexcept {
        return Base::StringView("FileDropList");
    }
    static constexpr Base::StringView Bitmap() noexcept {
        return Base::StringView("Bitmap");
    }
    static constexpr Base::StringView Html() noexcept {
        return Base::StringView("Html");
    }
    static constexpr Base::StringView Xaml() noexcept {
        return Base::StringView("Xaml");
    }
    static constexpr Base::StringView Serializable() noexcept {
        return Base::StringView("Serializable");
    }
};

/// Defines a format-independent mechanism for transferring data. Stores a
/// mapping of format name to payload value.
///
/// Reference: System.Windows.DataObject
class AERO_GUI_API DataObject : public Base::Object {
    AERO_DECLARE_TYPE(DataObject, Base::Object)
public:
    DataObject() noexcept = default;

    void SetData(Base::StringView format, const Meta::Value& data) noexcept;
    void SetData(Base::StringView format, const Base::Ref<Base::Object>& object) noexcept;

    bool ContainsData(Base::StringView format) const noexcept;
    Meta::Value GetData(Base::StringView format) const noexcept;
    Base::Vector<Base::String> GetFormats() const noexcept;

    bool ContainsFileDropList() const noexcept;
    Base::Vector<Base::String> GetFileDropList() const noexcept;
    void SetFileDropList(const Base::Vector<Base::String>& files) noexcept;

private:
    struct Entry {
        Base::String format;
        Meta::Value value;
    };
    Base::Vector<Entry> entries_;
};

} // namespace Aero

#endif
