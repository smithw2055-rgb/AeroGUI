#pragma once

#include <Aero/Markup/LoadOptions.hpp>

#include "LoadContext.hpp"

namespace Aero::Markup::Detail {

class LoadOptionsAccess final {
public:
    static void SetContext(
        LoadOptions& options,
        const LoadContext* context) noexcept {
        options.context_ = context;
    }

    static const LoadContext& Context(
        const LoadOptions& options) noexcept {
        static const LoadContext empty;
        return options.context_ != nullptr
            ? *static_cast<const LoadContext*>(options.context_)
            : empty;
    }
};

} // namespace Aero::Markup::Detail
