#pragma once

#include <Aero/Markup/LoadOptions.hpp>

#include "LoadState.hpp"

namespace Aero::Markup::Detail {

class LoadOptionsPrivate final {
public:
    static void SetContext(
        LoadOptions& options,
        const LoadState* context) noexcept {
        options.context_ = context;
    }

    static const LoadState& Context(
        const LoadOptions& options) noexcept {
        static const LoadState empty;
        return options.context_ != nullptr
            ? *static_cast<const LoadState*>(options.context_)
            : empty;
    }
};

} // namespace Aero::Markup::Detail
