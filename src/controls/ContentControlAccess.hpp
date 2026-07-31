#pragma once

#include <Aero/Controls/Base.hpp>

namespace Aero::Controls::Detail {

class ContentControlAccess final {
public:
    static UIElement* ContentElement(const ContentControl& control) noexcept { return control.content_; }
    static const Base::Ref<Base::Object>& OwnedContent(const ContentControl& control) noexcept { return control.ownedContent_; }
    static const Base::Ref<Base::Object>& ContentValue(const ContentControl& control) noexcept { return control.contentValue_; }
    static Base::Result<void> SetOwnedContent(ContentControl& control, const Base::Ref<Base::Object>& owner, UIElement& content) noexcept { return control.SetOwnedContent(owner, content); }
    static Base::Result<void> SetContentValue(ContentControl& control, Base::Ref<Base::Object> value) noexcept { return control.SetContentValue(std::move(value)); }
    static Base::Result<void> SetContentValue(ContentControl& control, Core::Value value) noexcept { return control.SetContentValue(std::move(value)); }
    static void OnContentPropertyChanged(Core::DependencyObject& object, const Core::DependencyPropertyChangedEventArgs& change) noexcept { ContentControl::OnContentPropertyChanged(object, change); }
    static Base::Result<Base::Ref<Base::Object>> CreateTemplatedContent(const ContentControl& control) noexcept { return control.TryCreateTemplatedContent(); }
};

} // namespace Aero::Controls::Detail
