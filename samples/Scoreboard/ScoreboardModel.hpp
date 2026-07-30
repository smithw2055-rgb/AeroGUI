#pragma once

#include <Aero/Controls/Items.hpp>
#include <Aero/Metadata.hpp>
#include <Aero/Module.hpp>

namespace Aero::Samples::Scoreboard {

inline constexpr Base::StringView
ScoreboardNamespace() noexcept {
    return "clr-namespace:Scoreboard";
}

enum class Team {
    Alliance,
    Horde,
};

enum class Class {
    Fighter,
    Rogue,
    Hunter,
    Mage,
    Cleric,
};

struct Player final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        Player,
        Base::Object,
        "clr-namespace:Scoreboard",
        "Player")

    Player(
        Class klassValue,
        int deathsValue,
        int damageValue,
        int healValue,
        int killsValue,
        Base::String nameValue,
        int scoreValue,
        Team teamValue) noexcept
        : klass(klassValue),
          deaths(deathsValue),
          damage(damageValue),
          heal(healValue),
          kills(killsValue),
          name(std::move(nameValue)),
          score(scoreValue),
          team(teamValue) {}

    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Class GetClass() const noexcept { return klass; }
    void SetClass(Class value) noexcept { klass = value; }
    int GetDeaths() const noexcept { return deaths; }
    void SetDeaths(int value) noexcept { deaths = value; }
    int GetDamage() const noexcept { return damage; }
    void SetDamage(int value) noexcept { damage = value; }
    int GetHeal() const noexcept { return heal; }
    void SetHeal(int value) noexcept { heal = value; }
    int GetKills() const noexcept { return kills; }
    void SetKills(int value) noexcept { kills = value; }
    Base::StringView GetName() const noexcept {
        return name.View();
    }
    Base::Result<void> SetName(
        Base::StringView value) noexcept {
        return name.TryAssign(value);
    }
    int GetScore() const noexcept { return score; }
    void SetScore(int value) noexcept { score = value; }
    Team GetTeam() const noexcept { return team; }
    void SetTeam(Team value) noexcept { team = value; }

    Class klass;
    int deaths;
    int damage;
    int heal;
    int kills;
    Base::String name;
    int score;
    Team team;
};

class Game final : public Base::Object {
    AERO_DECLARE_TYPE_NAMED(
        Game,
        Base::Object,
        "clr-namespace:Scoreboard",
        "Game")
public:
    Core::TypeId RuntimeType() const noexcept override {
        return StaticTypeId();
    }

    Base::StringView Name() const noexcept {
        return name_.View();
    }
    Base::Result<void> SetName(
        Base::StringView value) noexcept {
        return name_.TryAssign(value);
    }
    int ElapsedTime() const noexcept {
        return elapsedTime_;
    }
    void SetElapsedTime(int value) noexcept {
        elapsedTime_ = value;
    }
    int SelectedTeam() const noexcept {
        return selectedTeam_;
    }
    void SetSelectedTeam(int value) noexcept {
        selectedTeam_ = value;
    }
    Base::Ref<Controls::ObjectItemsSource>
    Players() const noexcept {
        return players_;
    }
    void SetPlayers(
        Base::Ref<Controls::ObjectItemsSource> value) noexcept {
        players_ = std::move(value);
    }
    int AllianceScore() const noexcept;
    void SetAllianceScore(int) noexcept {}
    int HordeScore() const noexcept;
    void SetHordeScore(int) noexcept {}
    Base::Ref<Controls::ObjectItemsSource>
    VisibleTeams() const noexcept {
        return visibleTeams_;
    }
    void SetVisibleTeams(
        Base::Ref<Controls::ObjectItemsSource> value) noexcept {
        visibleTeams_ = std::move(value);
    }

private:
    Base::String name_;
    int elapsedTime_ = 0;
    int selectedTeam_ = 0;
    Base::Ref<Controls::ObjectItemsSource> players_;
    Base::Ref<Controls::ObjectItemsSource> visibleTeams_;
};

Base::Result<Base::Ref<Game>>
CreateScoreboardGame() noexcept;
ModuleRegistration MakeScoreboardModuleManifest() noexcept;

} // namespace Aero::Samples::Scoreboard

namespace Aero::Core {

template<>
struct MetaTypeTraits<Samples::Scoreboard::Team> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId(
            "clr-namespace:Scoreboard", "Team");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return "clr-namespace:Scoreboard";
    }
    static constexpr Base::StringView Name() noexcept {
        return "Team";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

template<>
struct MetaTypeTraits<Samples::Scoreboard::Class> {
    static constexpr TypeId Id() noexcept {
        return MakeTypeId(
            "clr-namespace:Scoreboard", "Class");
    }
    static constexpr Base::StringView Namespace() noexcept {
        return "clr-namespace:Scoreboard";
    }
    static constexpr Base::StringView Name() noexcept {
        return "Class";
    }
    static constexpr TypeId BaseType() noexcept {
        return InvalidTypeId;
    }
};

} // namespace Aero::Core
