#ifndef MOD_TWOW_INVASIONS_MGR_H
#define MOD_TWOW_INVASIONS_MGR_H

#include "Common.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include <unordered_map>
#include <vector>

class Player;
class Unit;

enum InvasionPhase
{
    PHASE_STALKING,     // The defender has been warned; the invader has not arrived
    PHASE_FIGHTING,     // The invader is there and the combat clock is running
    PHASE_LEAVING,      // Match is over; the invader is on their way home
    PHASE_REVIVING      // A fallen invader has been carried home dead and is about to wake
};

struct Invasion
{
    ObjectGuid invader;
    ObjectGuid defender;
    WorldLocation origin;       // Spot where the invasion rite was started (home)
    InvasionPhase phase = PHASE_STALKING;
    uint32 timer = 0;           // ms left in current phase
    WorldLocation arena;        // Coordinates where the invader landed (arena anchor)
    uint32 outInvader = 0;      // ms out of arena bounds
    uint32 outDefender = 0;     // ms out of arena bounds
    uint32 radarMs = 0;         // ms until next minimap radar update
    bool arrivalFx = false;     // arrival effect waits for teleport to finish
    bool arrivalLanded = false; // teleport has fully landed in world
    bool keepStealth = false;   // stealth preservation on landing
    uint32 arrivalFxMs = 0;     // delay after landing before effects play
    uint32 leaveKit = 0;        // departure visual effect
    bool isBotInvader = false;  // invader is a headless bot session
    bool isBotDefender = false; // defender is a headless bot session
};

struct PendingRevive
{
    ObjectGuid guid;
    WorldLocation where;        // coordinates where defender died
};

class InvasionManager
{
public:
    static InvasionManager& instance()
    {
        static InvasionManager inst;
        return inst;
    }

    void Update(uint32 diff);

    bool CanBeginInvasion(Player* invader);
    bool BeginInvasion(Player* invader, Player* designatedDefender = nullptr);
    void OnPlayerDeath(Player* victim, Unit* killer);
    bool OnPlayerLogout(Player* player);
    bool OnReleaseSpirit(Player* player);
    bool Withdraw(Player* invader);

    bool IsInvolved(ObjectGuid guid) const;
    bool IsInvading(ObjectGuid guid) const;
    bool IsFighting(ObjectGuid guid) const;
    bool AreFoes(ObjectGuid a, ObjectGuid b) const;

    void GrantRespite(ObjectGuid defender);
    bool HasRespite(ObjectGuid defender) const;

    void GrantBotCooldown(ObjectGuid botGuid);
    bool HasBotCooldown(ObjectGuid botGuid) const;

    uint32 GetActiveBotInvasionsCount() const;
    void CheckAutonomousBotInvasions(uint32 diff);

private:
    InvasionManager() = default;

    bool CanInvadeFrom(Player* invader) const;
    bool IsAvailableForInvasion(Player* player) const;
    bool IsValidDefender(Player* invader, Player* target) const;
    Player* PickDefender(Player* invader) const;

    void Arrive(Invasion& inv, Player* invader, Player* defender);
    bool ArenaForfeit(Invasion& inv, Player* invader, Player* defender, uint32 diff);
    void SendHome(Invasion const& inv, Player* invader);
    void Conclude(Invasion& inv, char const* invaderLine, char const* defenderLine,
                  uint32 delayMs = 0, ObjectGuid winner = ObjectGuid());
    void RecordMatch(Invasion const& inv, ObjectGuid winner);
    void AwardRewards(Player* winner, Player* loser);

    std::vector<Invasion> m_invasions;
    std::vector<PendingRevive> m_revives;
    std::unordered_map<ObjectGuid, time_t> m_respite;
    std::unordered_map<ObjectGuid, time_t> m_botCooldowns;
    uint32 m_botCheckTimer = 0;
};

#define sInvasionMgr InvasionManager::instance()

#endif // MOD_TWOW_INVASIONS_MGR_H
