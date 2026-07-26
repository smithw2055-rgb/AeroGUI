#include <Aero/Base/ResourceUri.hpp>
#include <Aero/Core/Metadata/MetadataId.hpp>
#include <Aero/Core/Metadata/Value.hpp>
#include <Aero/Presentation/Resources.hpp>

#include <cstdio>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;
using namespace Aero::Presentation;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

constexpr TypeId IntegerType =
    MakeTypeId("urn:resource-tests", "Integer");
constexpr TypeId ButtonType =
    MakeTypeId("urn:resource-tests", "Button");

Value Integer(std::int64_t value) noexcept {
    return Value::FromSignedInteger(
        IntegerType, value);
}

struct ChangeProbe final {
    std::uint32_t count = 0U;
    ResourceChangeKind last =
        ResourceChangeKind::Added;
    std::uint64_t generation = 0U;
};

void OnChanged(
    void* context,
    StringView,
    ResourceChangeKind kind,
    std::uint64_t generation) noexcept {
    auto* probe = static_cast<ChangeProbe*>(context);
    ++probe->count;
    probe->last = kind;
    probe->generation = generation;
}

bool TestKeysAndMergedPriority() {
    ResourceDictionary first;
    ResourceDictionary second;
    ResourceDictionary root;
    CHECK(first.TryAdd(
        StringView("Accent"), Integer(2)));
    CHECK(second.TryAdd(
        StringView("Accent"), Integer(3)));
    CHECK(second.TryAdd(
        ButtonType, Integer(30)));
    CHECK(root.TryAddMerged(first));
    CHECK(root.TryAddMerged(second));

    Result<Value> merged =
        root.Lookup(StringView("Accent"));
    CHECK(merged);
    CHECK(merged.Value().AsSignedInteger() == 3);
    Result<Value> implicit = root.Lookup(ButtonType);
    CHECK(implicit);
    CHECK(implicit.Value().AsSignedInteger() == 30);

    CHECK(root.TryAdd(
        StringView("Accent"), Integer(1)));
    Result<Value> local =
        root.Lookup(StringView("Accent"));
    CHECK(local);
    CHECK(local.Value().AsSignedInteger() == 1);
    CHECK(root.Remove(StringView("Accent")).Value());
    CHECK(root.Lookup(StringView("Accent"))
        .Value().AsSignedInteger() == 3);
    return true;
}

bool TestMergeCyclesSealAndSource() {
    ResourceDictionary parent;
    ResourceDictionary child;
    CHECK(parent.TryAddMerged(child));
    Result<void> cycle = child.TryAddMerged(parent);
    CHECK(!cycle);
    CHECK(cycle.GetStatus().code ==
        ErrorCode::CycleDetected);

    Result<ResourceUri> source = ResourceUri::Parse(
        StringView(
            "pack://application:,,,/Aero.Controls;component/Themes/Light.xaml"));
    CHECK(source);
    CHECK(parent.SetSource(source.Value()));
    CHECK(parent.Source() == source.Value());
    CHECK(parent.Seal());
    CHECK(parent.IsSealed());
    Result<void> mutation =
        parent.TryAdd(StringView("Late"), Integer(4));
    CHECK(!mutation);
    CHECK(mutation.GetStatus().code ==
        ErrorCode::ReadOnly);
    CHECK(!parent.Clear());
    return true;
}

bool TestMergedGenerationSurvivesMove() {
    ResourceDictionary child;
    ResourceDictionary parent;
    CHECK(parent.TryAddMerged(child));
    ChangeProbe probe;
    Result<ResourceChangeSubscription> subscription =
        parent.SubscribeChanged(&OnChanged, &probe);
    CHECK(subscription);

    ResourceDictionary moved(std::move(parent));
    CHECK(child.TrySet(
        StringView("Live"), Integer(9)));
    CHECK(probe.count == 1U);
    CHECK(probe.last ==
        ResourceChangeKind::MergedDictionaryChanged);
    CHECK(probe.generation > 0U);
    CHECK(moved.Lookup(StringView("Live"))
        .Value().AsSignedInteger() == 9);
    CHECK(moved.Unsubscribe(subscription.Value()));
    return true;
}

} // namespace

int main() {
    if (!TestKeysAndMergedPriority()) return 1;
    if (!TestMergeCyclesSealAndSource()) return 1;
    if (!TestMergedGenerationSurvivesMove()) return 1;
    std::puts("Aero resource tests passed");
    return 0;
}
