#include "ScoreboardModel.hpp"

#include <Aero/Metadata.hpp>

namespace Aero::Samples::Scoreboard {
namespace {

Base::Result<void> RegisterMetadata(
    Core::MetadataContext& context) noexcept {
    auto team = Describe<Team>(context);
    team
        .Value("Alliance", Team::Alliance)
        .Value("Horde", Team::Horde);
    Base::Result<void> status = team.Result();
    if (!status) return status.GetStatus();

    auto klass = Describe<Class>(context);
    klass
        .Value("Fighter", Class::Fighter)
        .Value("Rogue", Class::Rogue)
        .Value("Hunter", Class::Hunter)
        .Value("Mage", Class::Mage)
        .Value("Cleric", Class::Cleric);
    status = klass.Result();
    if (!status) return status.GetStatus();

    auto player = Describe<Player>(context);
    player
        .Property(
            "Class",
            &Player::GetClass,
            &Player::SetClass)
        .Property(
            "Deaths",
            &Player::GetDeaths,
            &Player::SetDeaths)
        .Property(
            "Damage",
            &Player::GetDamage,
            &Player::SetDamage)
        .Property(
            "Heal",
            &Player::GetHeal,
            &Player::SetHeal)
        .Property(
            "Kills",
            &Player::GetKills,
            &Player::SetKills)
        .Property(
            "Name",
            &Player::GetName,
            &Player::SetName)
        .Property(
            "Score",
            &Player::GetScore,
            &Player::SetScore)
        .Property(
            "Team",
            &Player::GetTeam,
            &Player::SetTeam);
    status = player.Result();
    if (!status) return status.GetStatus();

    auto game = Describe<Game>(context);
    game
        .Property(
            "Name",
            &Game::Name,
            &Game::SetName)
        .Property(
            "ElapsedTime",
            &Game::ElapsedTime,
            &Game::SetElapsedTime)
        .Property(
            "Players",
            &Game::Players,
            &Game::SetPlayers)
        .Property(
            "AllianceScore",
            &Game::AllianceScore,
            &Game::SetAllianceScore)
        .Property(
            "HordeScore",
            &Game::HordeScore,
            &Game::SetHordeScore)
        .Property(
            "SelectedTeam",
            &Game::SelectedTeam,
            &Game::SetSelectedTeam)
        .Property(
            "VisibleTeams",
            &Game::VisibleTeams,
            &Game::SetVisibleTeams);
    return game.Result();
}

Base::Result<Base::String> CopyText(
    Base::StringView value) noexcept {
    Base::String result;
    Base::Result<void> assigned =
        result.TryAssign(value);
    return assigned
        ? Base::Result<Base::String>(
              std::move(result))
        : Base::Result<Base::String>(
              assigned.GetStatus());
}

Base::Result<void> AddPlayer(
    Controls::ObjectItemsSource& players,
    Class klass,
    int deaths,
    int damage,
    int heal,
    int kills,
    Base::StringView name,
    int score,
    Team team) noexcept {
    Base::Result<Base::String> copied =
        CopyText(name);
    if (!copied) return copied.GetStatus();
    Base::Result<Base::Ref<Player>> player =
        Base::MakeRef<Player>(
            klass,
            deaths,
            damage,
            heal,
            kills,
            std::move(copied).Value(),
            score,
            team);
    if (!player) return player.GetStatus();
    return players.Add(
        Base::Ref<Base::Object>(
            std::move(player).Value()));
}

} // namespace

int Game::AllianceScore() const noexcept {
    int score = 0;
    if (!players_) return score;
    for (std::uint32_t index = 0U;
         index < players_->Count();
         ++index) {
        Base::Ref<Base::Object> item =
            players_->ItemAt(index);
        if (item &&
            item->RuntimeType() ==
                Player::StaticTypeId()) {
            const Player& player =
                static_cast<const Player&>(*item);
            if (player.team == Team::Alliance) {
                score += player.score;
            }
        }
    }
    return score;
}

int Game::HordeScore() const noexcept {
    int score = 0;
    if (!players_) return score;
    for (std::uint32_t index = 0U;
         index < players_->Count();
         ++index) {
        Base::Ref<Base::Object> item =
            players_->ItemAt(index);
        if (item &&
            item->RuntimeType() ==
                Player::StaticTypeId()) {
            const Player& player =
                static_cast<const Player&>(*item);
            if (player.team == Team::Horde) {
                score += player.score;
            }
        }
    }
    return score;
}

Base::Result<Base::Ref<Game>>
CreateScoreboardGame() noexcept {
    Base::Result<Base::Ref<Game>> madeGame =
        Base::MakeRef<Game>();
    if (!madeGame) return madeGame.GetStatus();
    Base::Ref<Game> game =
        std::move(madeGame).Value();

    Base::Result<void> status =
        game->SetName("Silvershard Mines");
    if (!status) return status.GetStatus();
    game->SetElapsedTime(16);
    game->SetSelectedTeam(0);

    Base::Result<Base::Ref<Controls::ObjectItemsSource>>
        madePlayers =
            Base::MakeRef<Controls::ObjectItemsSource>();
    if (!madePlayers) {
        return madePlayers.GetStatus();
    }
    Base::Ref<Controls::ObjectItemsSource> players =
        std::move(madePlayers).Value();

    status = AddPlayer(
        *players, Class::Mage, 96, 8134124, 1831, 43,
        "Nam cras aenean", 476, Team::Alliance);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Rogue, 98, 8324715, 2954, 79,
        "Sed class vestibulum", 414, Team::Horde);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Hunter, 45, 797117, 2615, 99,
        "Curae praesent", 383, Team::Horde);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Hunter, 93, 481757, 6353, 34,
        "Adipiscing dis quisque", 327, Team::Alliance);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Fighter, 82, 743715, 37415, 80,
        "Est donec vivamus", 289, Team::Horde);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Rogue, 21, 383571, 82114, 90,
        "Duis leo curabitur", 265, Team::Alliance);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Cleric, 86, 441751, 255131, 37,
        "Mus etiam aliquam", 259, Team::Alliance);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Mage, 60, 201175, 4915, 63,
        "Nunc mauris accumsan", 225, Team::Horde);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Fighter, 30, 271735, 6715, 20,
        "Phasellus nullam", 195, Team::Alliance);
    if (!status) return status.GetStatus();
    status = AddPlayer(
        *players, Class::Cleric, 18, 87537, 95717, 54,
        "Consequat bibendum", 180, Team::Horde);
    if (!status) return status.GetStatus();
    game->SetPlayers(players);

    Base::Result<Base::Ref<Controls::ObjectItemsSource>>
        madeTeams =
            Base::MakeRef<Controls::ObjectItemsSource>();
    if (!madeTeams) return madeTeams.GetStatus();
    Base::Ref<Controls::ObjectItemsSource> teams =
        std::move(madeTeams).Value();
    status = Controls::AddBoxedStringItem(
        *teams, "Overall");
    if (!status) return status.GetStatus();
    status = Controls::AddBoxedStringItem(
        *teams, "Alliance");
    if (!status) return status.GetStatus();
    status = Controls::AddBoxedStringItem(
        *teams, "Horde");
    if (!status) return status.GetStatus();
    game->SetVisibleTeams(std::move(teams));

    return game;
}

ModuleRegistration MakeScoreboardModuleManifest() noexcept {
    return DefineModule(
        "Aero.Samples.Scoreboard",
        &RegisterMetadata);
}

} // namespace Aero::Samples::Scoreboard
