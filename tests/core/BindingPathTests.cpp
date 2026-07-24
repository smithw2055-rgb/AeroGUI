#include <Aero/Base/Object.hpp>
#include <Aero/Core/Metadata/BindingPath.hpp>
#include <Aero/Core/Metadata/BuiltinTypeIds.hpp>
#include <Aero/Core/Metadata/CoreMetadata.hpp>
#include <Aero/Core/Metadata/MetadataDsl.hpp>

#include <cstdio>
#include <memory>

namespace {

using namespace Aero::Base;
using namespace Aero::Core;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expression); \
            return false; \
        } \
    } while (false)

constexpr StringView TestModuleName("Aero.Tests.BindingPath");

struct Location final {
    AERO_TYPED_META_NAMED(
        Location, NoMetadataBase, "urn:binding-path-tests", "Location")
    double x = 0.0;
};

struct Address final {
    AERO_TYPED_META_NAMED(
        Address, NoMetadataBase, "urn:binding-path-tests", "Address")
    Location location;
};

class UserModel final : public Object {
    AERO_TYPED_META_NAMED(
        UserModel, Object, "urn:binding-path-tests", "UserModel")
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    ~UserModel() override = default;

    Address address;

    Result<std::uint64_t> Subscribe(
        MetadataPropertyChangedCallback callback,
        void* context) noexcept {
        if (callback == nullptr || notification_ != nullptr) {
            return Status::Failure(
                ErrorCode::AlreadyExists,
                "UserModel test notification is already subscribed");
        }
        notification_ = callback;
        notificationContext_ = context;
        return 1U;
    }

    Result<bool> Unsubscribe(std::uint64_t token) noexcept {
        if (token != 1U || notification_ == nullptr) return false;
        notification_ = nullptr;
        notificationContext_ = nullptr;
        return true;
    }

    void Notify(MemberId property) noexcept {
        if (notification_ != nullptr) {
            notification_(*this, property, notificationContext_);
        }
    }

private:
    MetadataPropertyChangedCallback notification_ = nullptr;
    void* notificationContext_ = nullptr;
};

class ObservableItems final : public Object {
    AERO_TYPED_META_NAMED(
        ObservableItems, Object,
        "urn:binding-path-tests", "ObservableItems")
public:
    TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }
    ~ObservableItems() override = default;

    Result<std::uint64_t> Subscribe(
        MetadataCollectionChangedCallback callback,
        void* context) noexcept {
        if (callback == nullptr || notification_ != nullptr) {
            return Status::Failure(
                ErrorCode::AlreadyExists,
                "Collection test notification is already subscribed");
        }
        notification_ = callback;
        notificationContext_ = context;
        return 2U;
    }

    Result<bool> Unsubscribe(std::uint64_t token) noexcept {
        if (token != 2U || notification_ == nullptr) return false;
        notification_ = nullptr;
        notificationContext_ = nullptr;
        return true;
    }

    void Notify(const MetadataCollectionChangedEvent& event) noexcept {
        if (notification_ != nullptr) {
            notification_(*this, event, notificationContext_);
        }
    }

private:
    MetadataCollectionChangedCallback notification_ = nullptr;
    void* notificationContext_ = nullptr;
};

struct Fixture;

Result<Value> GetAddress(const Object& object, void* context) noexcept;
Result<void> SetAddress(
    Object& object, const Value& value, void* context) noexcept;
Result<std::uint64_t> SubscribeUser(
    Object& object,
    MetadataPropertyChangedCallback callback,
    void* callbackContext,
    void*) noexcept;
Result<bool> UnsubscribeUser(
    Object& object,
    std::uint64_t subscription,
    void*) noexcept;
Result<std::uint64_t> SubscribeItems(
    Object& object,
    MetadataCollectionChangedCallback callback,
    void* callbackContext,
    void*) noexcept;
Result<bool> UnsubscribeItems(
    Object& object,
    std::uint64_t subscription,
    void*) noexcept;

struct Fixture final {
    MetadataDomain metadata;
    std::unique_ptr<MetadataRuntime> runtime;
    std::uint32_t schemaVersion = 1U;

    static Result<void> Register(
        MetaRegistrationContext& context,
        void* fixtureContext) noexcept {
        auto* fixture = static_cast<Fixture*>(fixtureContext);

        MetaTypeBuilder<Location> location =
            MetaTypeBuilder<Location>::Struct(context);
        location.Field<&Location::x>("X").ValueSemantics();
        Result<void> status = location.Finish();
        if (!status) return status.GetStatus();

        MetaTypeBuilder<Address> address =
            MetaTypeBuilder<Address>::Struct(context);
        address.Field<&Address::location>("Location").ValueSemantics();
        status = address.Finish();
        if (!status) return status.GetStatus();

        MetaTypeBuilder<UserModel> user =
            MetaTypeBuilder<UserModel>::Object(context);
        user.PropertyChangeNotifications(
            &SubscribeUser, &UnsubscribeUser);
        PropertyRegistration addressProperty;
        addressProperty.name = "Address";
        addressProperty.valueType = TypeOf<Address>();
        addressProperty.access = PropertyAccessKind::Ordinary;
        addressProperty.get = &GetAddress;
        addressProperty.set = &SetAddress;
        addressProperty.context = fixture;
        user.Property(addressProperty);

        PropertyRegistration readOnlyAddress = addressProperty;
        readOnlyAddress.name = "ReadOnlyAddress";
        readOnlyAddress.flags = PropertyFlags::ReadOnly;
        readOnlyAddress.set = nullptr;
        user.Property(readOnlyAddress);
        status = user.Finish();
        if (!status) return status.GetStatus();

        MetaTypeBuilder<ObservableItems> items =
            MetaTypeBuilder<ObservableItems>::Object(
                context, TypeFlags::Collection);
        items.CollectionChangeNotifications(
            &SubscribeItems, &UnsubscribeItems);
        return items.Finish();
    }

    bool Build(std::uint32_t version = 1U) {
        schemaVersion = version;
        CHECK(TryRegisterCoreMetadata(metadata));
        CHECK(metadata.TryRegisterModule({
            MakeMetadataModuleId(TestModuleName),
            TestModuleName,
            schemaVersion,
            &Fixture::Register,
            this}));
        CHECK(metadata.Seal());
        runtime = std::make_unique<MetadataRuntime>(metadata);
        CHECK(runtime->Freeze());
        return true;
    }
};

Result<Value> GetAddress(const Object& object, void* context) noexcept {
    auto* fixture = static_cast<Fixture*>(context);
    if (fixture == nullptr || fixture->runtime == nullptr) {
        return Status::Failure(
            ErrorCode::InvalidState,
            "Binding path test runtime is unavailable");
    }
    return fixture->runtime->TryCreateValue(
        TypeOf<Address>(),
        &static_cast<const UserModel&>(object).address);
}

Result<void> SetAddress(
    Object& object,
    const Value& value,
    void*) noexcept {
    if (value.Type() != TypeOf<Address>() ||
        value.Kind() != ValueKind::Custom ||
        value.AsCustom() == nullptr) {
        return Status::Failure(
            ErrorCode::InvalidArgument,
            "Address value is incompatible");
    }
    static_cast<UserModel&>(object).address =
        *static_cast<const Address*>(value.AsCustom());
    static_cast<UserModel&>(object).Notify(MakeMemberId(
        UserModel::StaticTypeId(),
        MemberKind::Property,
        "Address"));
    return {};
}

Result<std::uint64_t> SubscribeUser(
    Object& object,
    MetadataPropertyChangedCallback callback,
    void* callbackContext,
    void*) noexcept {
    return static_cast<UserModel&>(object).Subscribe(
        callback, callbackContext);
}

Result<bool> UnsubscribeUser(
    Object& object,
    std::uint64_t subscription,
    void*) noexcept {
    return static_cast<UserModel&>(object).Unsubscribe(subscription);
}

Result<std::uint64_t> SubscribeItems(
    Object& object,
    MetadataCollectionChangedCallback callback,
    void* callbackContext,
    void*) noexcept {
    return static_cast<ObservableItems&>(object).Subscribe(
        callback, callbackContext);
}

Result<bool> UnsubscribeItems(
    Object& object,
    std::uint64_t subscription,
    void*) noexcept {
    return static_cast<ObservableItems&>(object).Unsubscribe(subscription);
}

struct NotificationProbe final {
    std::uint32_t count = 0U;
    MemberId property = InvalidMemberId;
};

void RecordNotification(
    Object&,
    MemberId property,
    void* context) noexcept {
    auto* probe = static_cast<NotificationProbe*>(context);
    ++probe->count;
    probe->property = property;
}

struct CollectionNotificationProbe final {
    std::uint32_t count = 0U;
    MetadataCollectionChangedEvent event;
};

void RecordCollectionNotification(
    Object&,
    const MetadataCollectionChangedEvent& event,
    void* context) noexcept {
    auto* probe = static_cast<CollectionNotificationProbe*>(context);
    ++probe->count;
    probe->event = event;
}

bool TestCompileAndExecuteNestedCopyOnWrite() {
    Fixture fixture;
    CHECK(fixture.Build());
    BindingPathCompileError error;
    Result<BindingPathPlan> compiled = BindingPathPlan::Compile(
        *fixture.runtime,
        UserModel::StaticTypeId(),
        "Address.Location.X",
        &error);
    CHECK(compiled);
    const BindingPathPlan& plan = compiled.Value();
    CHECK(plan.IsValid());
    CHECK(plan.RootType() == UserModel::StaticTypeId());
    CHECK(plan.ResultType() == BuiltinTypes::Double);
    CHECK(plan.SchemaHash() ==
        fixture.metadata.ComputeSchemaHash().Value());
    CHECK(plan.CanRead() && plan.CanWrite());
    CHECK(plan.Segments().Size() == 3U);
    CHECK(plan.Segments()[0].kind ==
        BindingPathSegmentKind::ObjectProperty);
    CHECK(plan.Segments()[0].copyOnWrite);
    CHECK(plan.Segments()[1].kind ==
        BindingPathSegmentKind::ValueField);
    CHECK(plan.Segments()[1].copyOnWrite);
    CHECK(plan.Segments()[2].kind ==
        BindingPathSegmentKind::ValueField);
    const PropertyChangeNotificationFacet* notifications =
        fixture.metadata.Facets().FindPropertyChangeNotification(
            UserModel::StaticTypeId());
    CHECK(notifications != nullptr);
    CHECK(fixture.metadata.Facets().HasTypeFacet(
        UserModel::StaticTypeId(),
        MetadataFacetKind::PropertyChangeNotification));

    UserModel user;
    user.address.location.x = 3.5;
    NotificationProbe probe;
    Result<std::uint64_t> subscription = notifications->subscribe(
        user,
        &RecordNotification,
        &probe,
        notifications->context);
    CHECK(subscription);
    Result<Value> initial = plan.Get(*fixture.runtime, user);
    CHECK(initial && initial.Value().AsDouble() == 3.5);
    CHECK(plan.Set(
        *fixture.runtime,
        user,
        Value::FromDouble(BuiltinTypes::Double, 8.25)));
    CHECK(user.address.location.x == 8.25);
    CHECK(probe.count == 1U);
    CHECK(probe.property == plan.Segments()[0].member);
    Result<Value> updated = plan.Get(*fixture.runtime, user);
    CHECK(updated && updated.Value().AsDouble() == 8.25);
    CHECK(notifications->unsubscribe(
        user, subscription.Value(), notifications->context).Value());
    return true;
}

bool TestCompileDiagnosticsAndReadOnlyPlan() {
    Fixture fixture;
    CHECK(fixture.Build());
    BindingPathCompileError error;
    Result<BindingPathPlan> missing = BindingPathPlan::Compile(
        *fixture.runtime,
        UserModel::StaticTypeId(),
        "Address.Missing.X",
        &error);
    CHECK(!missing);
    CHECK(missing.GetStatus().code == ErrorCode::NotFound);
    CHECK(error.segmentIndex == 1U);
    CHECK(error.inputType == TypeOf<Address>());
    CHECK(error.segment == StringView("Missing"));

    Result<BindingPathPlan> readOnly = BindingPathPlan::Compile(
        *fixture.runtime,
        UserModel::StaticTypeId(),
        "ReadOnlyAddress.Location.X");
    CHECK(readOnly);
    CHECK(readOnly.Value().CanRead());
    CHECK(!readOnly.Value().CanWrite());
    UserModel user;
    Result<void> stored = readOnly.Value().Set(
        *fixture.runtime,
        user,
        Value::FromDouble(BuiltinTypes::Double, 4.0));
    CHECK(!stored && stored.GetStatus().code == ErrorCode::ReadOnly);
    return true;
}

bool TestSchemaCompatibility() {
    Fixture source;
    CHECK(source.Build(1U));
    Result<BindingPathPlan> compiled = BindingPathPlan::Compile(
        *source.runtime,
        UserModel::StaticTypeId(),
        "Address.Location.X");
    CHECK(compiled);

    Fixture equivalent;
    CHECK(equivalent.Build(1U));
    UserModel sameSchema;
    sameSchema.address.location.x = 6.0;
    Result<Value> compatible =
        compiled.Value().Get(*equivalent.runtime, sameSchema);
    CHECK(compatible && compatible.Value().AsDouble() == 6.0);

    Fixture changed;
    CHECK(changed.Build(2U));
    UserModel changedSchema;
    Result<Value> incompatible =
        compiled.Value().Get(*changed.runtime, changedSchema);
    CHECK(!incompatible);
    CHECK(incompatible.GetStatus().code == ErrorCode::ValidationFailed);
    return true;
}

bool TestCollectionNotificationFacet() {
    Fixture fixture;
    CHECK(fixture.Build());
    const CollectionChangeNotificationFacet* notifications =
        fixture.metadata.Facets().FindCollectionChangeNotification(
            ObservableItems::StaticTypeId());
    CHECK(notifications != nullptr);
    CHECK(fixture.metadata.Facets().HasTypeFacet(
        ObservableItems::StaticTypeId(),
        MetadataFacetKind::CollectionChangeNotification));

    ObservableItems items;
    CollectionNotificationProbe probe;
    Result<std::uint64_t> subscription = notifications->subscribe(
        items,
        &RecordCollectionNotification,
        &probe,
        notifications->context);
    CHECK(subscription && subscription.Value() == 2U);
    items.Notify({
        MetadataCollectionChangeAction::Add,
        UINT32_MAX,
        3U,
        0U,
        2U});
    CHECK(probe.count == 1U);
    CHECK(probe.event.action == MetadataCollectionChangeAction::Add);
    CHECK(probe.event.newIndex == 3U);
    CHECK(probe.event.newCount == 2U);
    CHECK(notifications->unsubscribe(
        items, subscription.Value(), notifications->context).Value());
    return true;
}

} // namespace

int main() {
    if (!TestCompileAndExecuteNestedCopyOnWrite()) return 1;
    if (!TestCompileDiagnosticsAndReadOnlyPlan()) return 1;
    if (!TestSchemaCompatibility()) return 1;
    if (!TestCollectionNotificationFacet()) return 1;
    std::puts("Aero binding path tests passed");
    return 0;
}
