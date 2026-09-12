#pragma once

// Private metadata registration records for Registry / BehaviorTable /
// MetadataAuthoringSession. Lives under src/gui/meta (not include/Aero).

namespace Aero::Meta {

struct EnumValueRegistration {
    Base::StringView name;
    std::uint64_t rawValue = 0U;
};

struct EventRegistration {
    Base::StringView name;
    TypeId eventArgsType = InvalidTypeId;
    EventFlags flags = EventFlags::None;
};

struct MethodParameterRegistration {
    Base::StringView name;
    TypeId type = InvalidTypeId;
};

struct MethodRegistration {
    Base::StringView name;
    TypeId returnType = InvalidTypeId;
    Base::Span<const MethodParameterRegistration> parameters;
    MethodFlags flags = MethodFlags::None;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

struct TypeFactoryRegistration {
    TypeId type = InvalidTypeId;
    ObjectFactory factory = nullptr;
};

struct ContentAccessorRegistration {
    TypeId type = InvalidTypeId;
    MemberId member = InvalidMemberId;
    ContentKind kind = ContentKind::Single;
    ContentFlags flags = ContentFlags::None;
    ContentWriteCallback write = nullptr;
    ContentClearCallback clear = nullptr;
    void* context = nullptr;
};

struct PropertyAccessorRegistration {
    MemberId member = InvalidMemberId;
    PropertyAccessKind access = PropertyAccessKind::External;
    PropertyGetCallback get = nullptr;
    PropertySetCallback set = nullptr;
    PropertyProviderId provider = InvalidPropertyProviderId;
    void* context = nullptr;
};

struct ValueMemberAccessorRegistration {
    MemberId member = InvalidMemberId;
    ValueMemberGetCallback get = nullptr;
    ValueMemberSetCallback set = nullptr;
    void* context = nullptr;
};

struct MethodInvokerRegistration {
    MemberId member = InvalidMemberId;
    MethodInvokeCallback invoke = nullptr;
    void* context = nullptr;
};

struct PropertyChangeNotificationRegistration {
    TypeId type = InvalidTypeId;
    PropertyChangeSubscribeCallback subscribe = nullptr;
    PropertyChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};

struct CollectionChangeNotificationRegistration {
    TypeId type = InvalidTypeId;
    CollectionChangeSubscribeCallback subscribe = nullptr;
    CollectionChangeUnsubscribeCallback unsubscribe = nullptr;
    void* context = nullptr;
};


} // namespace Aero::Meta
