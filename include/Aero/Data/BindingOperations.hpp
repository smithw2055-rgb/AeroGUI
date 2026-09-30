#pragma once

#include <Aero/Data/Binding.hpp>
#include <Aero/Data/BindingExpression.hpp>
#include <Aero/Data/MultiBindingExpression.hpp>
#include <Aero/Data/TemplateBindingExpression.hpp>
#include <Aero/DependencyObject.hpp>

namespace Aero::Data {

class AERO_GUI_API BindingOperations {
public:
    BindingOperations() = delete;

    // Code-side attach matching XAML {Binding}: builds the same
    // MetadataBindingDescriptor path BindingExtension / style setters use and
    // routes through BindingEngine::Attach without exposing BindingEngine.
    static Result<BindingExpression> SetBinding(
        DependencyObject* target,
        DependencyPropertyHandle property,
        const Binding& binding) noexcept;
    template<class TOwner, class TValue>
    static Result<BindingExpression> SetBinding(
        DependencyObject* target,
        const DependencyPropertyRef<TOwner, TValue>& property,
        const Binding& binding) noexcept {
        return SetBinding(target, property.Handle(), binding);
    }

    static void ClearBinding(
        DependencyObject* target,
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    static void ClearBinding(
        DependencyObject* target,
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        ClearBinding(target, property.Handle());
    }

    static void ClearAllBindings(DependencyObject* target) noexcept;

    static bool IsDataBound(
        DependencyObject* target,
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    static bool IsDataBound(
        DependencyObject* target,
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        return IsDataBound(target, property.Handle());
    }

    static BindingExpression GetBindingExpression(DependencyObject* target, DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue> static BindingExpression GetBindingExpression(
        DependencyObject* target, const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        return GetBindingExpression(target, property.Handle());
    }
    static MultiBindingExpression GetMultiBindingExpression(DependencyObject* target,
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue> static MultiBindingExpression GetMultiBindingExpression(
        DependencyObject* target, const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        return GetMultiBindingExpression(target, property.Handle());
    }
    static TemplateBindingExpression GetTemplateBindingExpression(DependencyObject* target,
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue> static TemplateBindingExpression GetTemplateBindingExpression(
        DependencyObject* target, const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        return GetTemplateBindingExpression(target, property.Handle());
    }
};

} // namespace Aero::Data
