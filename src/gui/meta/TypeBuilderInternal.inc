// Implementation-only TypeBuilder fluent overloads (AERO_GUI_IMPLEMENTATION).
// Included inside class TypeBuilder in <Aero/Meta.hpp>.

    // P2.5: consolidated implementation-only overloads. Raw factory,
    // MemberId-addressed content, and raw value-semantics/text-converter
    // forms are reserved for in-tree schema authors; SDK consumers use the
    // typed fluent entries above and never see this section.
    TypeBuilder& Factory(ObjectFactory factory) noexcept {
        builder_.Factory(factory);
        return *this;
    }

    TypeBuilder& Content(MemberId member) noexcept {
        builder_.Content(member);
        return *this;
    }

    TypeBuilder& ContentAccessor(
        MemberId member,
        ContentKind kind,
        ContentWriteCallback write,
        ContentClearCallback clear,
        ContentFlags flags = ContentFlags::None,
        void* callbackContext = nullptr) noexcept {
        builder_.ContentAccessor(
            member, kind, write, clear,
            flags, callbackContext);
        return *this;
    }

    TypeBuilder& ValueSemantics(
        const ValueTypeRegistration& registration) noexcept {
        builder_.ValueSemantics(registration);
        return *this;
    }

    TypeBuilder& TextConverter(
        TextValueConverterCallback converter) noexcept {
        builder_.TextConverter(converter);
        return *this;
    }
