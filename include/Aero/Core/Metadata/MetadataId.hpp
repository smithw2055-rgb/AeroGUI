#pragma once

#include <Aero/Base/MetadataId.hpp>

namespace Aero::Core {

using TypeId = Base::MetaTypeId;
using MemberId = Base::MetaMemberId;

inline constexpr TypeId InvalidTypeId = Base::InvalidMetaTypeId;
inline constexpr MemberId InvalidMemberId = Base::InvalidMetaMemberId;

} // namespace Aero::Core
