// Compile the standard object writer through the activation-aware schema seam.
// This keeps the established writer implementation and transaction semantics
// while routing type/member/value operations through sealed metadata runtime
// descriptors and facets when the schema was created from MetadataDomain.
#include <Aero/Markup/XamlObjectWriter.hpp>

#include "XamlRuntimeSchema.cpp"

#define ResolveType ResolveTypeRuntime
#define ResolveMember ResolveMemberRuntime
#define ResolveContentMember ResolveContentMemberRuntime
#define ResolveMemberWritePolicy ResolveMemberWritePolicyRuntime
#define ConvertText ConvertTextRuntime
#define SetMember SetMemberRuntime
#define CreateObject CreateObjectActivated
#include "XamlObjectWriter.cpp"
#undef CreateObject
#undef SetMember
#undef ConvertText
#undef ResolveMemberWritePolicy
#undef ResolveContentMember
#undef ResolveMember
#undef ResolveType
