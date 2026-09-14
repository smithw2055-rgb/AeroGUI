#pragma once

#include <Aero/Data/BindingExpression.hpp>
#include <Aero/Data/MultiBindingExpression.hpp>
#include <Aero/Data/TemplateBindingExpression.hpp>
#include <Aero/DependencyObject.hpp>

namespace Aero::Data {

class AERO_GUI_API BindingOperations {
public:
    BindingOperations() = delete;

    static BindingExpression GetBindingExpression(
        DependencyObject* target,
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    static BindingExpression GetBindingExpression(
        DependencyObject* target,
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        return GetBindingExpression(
            target, property.Handle());
    }
    static MultiBindingExpression GetMultiBindingExpression(
        DependencyObject* target,
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    static MultiBindingExpression GetMultiBindingExpression(
        DependencyObject* target,
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        return GetMultiBindingExpression(
            target, property.Handle());
    }
    static TemplateBindingExpression GetTemplateBindingExpression(
        DependencyObject* target,
        DependencyPropertyHandle property) noexcept;
    template<class TOwner, class TValue>
    static TemplateBindingExpression GetTemplateBindingExpression(
        DependencyObject* target,
        const DependencyPropertyRef<TOwner, TValue>& property) noexcept {
        return GetTemplateBindingExpression(
            target, property.Handle());
    }
};

} // namespace Aero::Data
