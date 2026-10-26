#include "InvasionMgr.h"

#include "Chat.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "Util.h"
#include "DBCStores.h"
#include "Group.h"
#include "Config/Config.h"

#if defined(HAVE_TORTOISEBOTS)
#include "runtime/PlayerbotAIStorage.h"
#include "ai/playerbot/PlayerbotAI.h"
#include "ai/playerbot/BotState.h"
#endif

#include <cmath>
#include <cstdio>
#include <algorithm>

extern DBCStorage<WorldMapAreaEntry> sWorldMapAreaStore;

namespace
{
    float const ARRIVAL_BOUNDING_RADIUS = 1.0f;
    uint32 const ARRIVE_KIT = 6151;             // Vanish Cast Base: puff at feet
    uint32 const MARK_KIT = 225;                // Shadow ImpactBuff Base on defender
    uint32 const LEAVE_KIT = 117;               // Shadow ImpactDD on chest
    uint32 const ESCAPE_KIT = 6379;             // Banish red
    uint32 const DEATH_KIT = 6448;              // Moonbeam on defender who dies
    uint32 const NO_KIT = 0xFFFFFFFF;
    uint32 const WITHDRAW_PULL_MS = 1000;
    uint32 const ARRIVE_FX_DELAY_MS = 1500;
    uint32 const RADAR_MS = 250;
    float const ARENA_OUT_YARDS = 100.0f;
    float const ARENA_BACK_YARDS = 95.0f;
    uint32 const ARENA_FORFEIT_MS = 10 * IN_MILLISECONDS;

    inline bool IsDebugEnabled()
    {
        return sConfig.GetBoolDefault("Invasion.Debug", false);
    }

    inline void DebugLog(char const* fmt, ...)
    {
        if (!IsDebugEnabled())
            return;

        char buffer[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, args);
        va_end(args);

        sLog.outString("Invasion [DEBUG]: %s", buffer);
    }

    inline bool IsHeadlessBot(Player const* player)
    {
        return player && player->GetSession() && player->GetSession()->IsHeadless();
    }

    void ResendPlayerFlags(Player* player)
    {
        if (player && player->IsInWorld())
            player->ForceValuesUpdateAtIndex(PLAYER_FLAGS);
    }

    void Tell(Player* player, char const* text)
    {
        if (!player || !player->GetSession())
            return;

        // Suppress sending chat frames to headless bot sessions
        if (!player->GetSession()->IsHeadless())
            ChatHandler(player).SendSysMessage(text);

        DebugLog("-> %s: %s", player->GetName(), text);
    }

    void TellFmt(Player* player, char const* fmt, char const* arg)
    {
        if (!player || !player->GetSession())
            return;

        if (!player->GetSession()->IsHeadless())
            ChatHandler(player).PSendSysMessage(fmt, arg);

        DebugLog("-> %s: (fmt) %s", player->GetName(), arg);
    }

    void SendRadar(Player* to, Player* other)
    {
        if (!to || !to->GetSession() || to->GetSession()->IsHeadless())
            return;

        if (!other || other->GetMapId() != to->GetMapId())
        {
            to->SendAddonMessage("CINV", "H");
            return;
        }

        uint32 const zone = to->GetZoneId();
        WorldMapAreaEntry const* area = sWorldMapAreaStore.LookupEntry(zone);
        float mx = other->GetPositionX();
        float my = other->GetPositionY();
        if (area && Map2ZoneCoordinates(mx, my, zone))
        {
            char buf[96];
            snprintf(buf, sizeof(buf), "Z %.5f %.5f %.1f %.1f", mx / 100.0f, my / 100.0f,
                     std::fabs(area->y2 - area->y1), std::fabs(area->x2 - area->x1));
            to->SendAddonMessage("CINV", buf);
            return;
        }

        int const north = int(other->GetPositionX() - to->GetPositionX());
        int const west = int(other->GetPositionY() - to->GetPositionY());
        to->SendAddonMessage("CINV", "P " + std::to_string(north) + " " + std::to_string(west));
    }

    void HideRadar(Player* to)
    {
        if (to && to->GetSession() && !to->GetSession()->IsHeadless())
            to->SendAddonMessage("CINV", "H");
    }
}

bool InvasionManager::IsAvailableForInvasion(Player* player) const
{
    if (!player || !player->IsInWorld() || !player->GetSession())
        return false;

    if (!player->IsAlive() || player->IsBeingTeleported() || player->IsTaxiFlying())
        return false;

    if (player->GetTransport())
        return false;

    if (player->IsGameMaster())
        return false;

    if (player->IsHardcore())
        return false;

    if (player->InBattleGround() || !player->GetMap() || player->GetMap()->Instanceable())
        return false;

    // Check City and Tavern resting protection
    if (!sConfig.GetBoolDefault("Invasion.AllowInCities", false))
    {
        if (player->GetRestType() == REST_TYPE_IN_CITY || player->GetRestType() == REST_TYPE_IN_TAVERN)
            return false;
    }

    // Check Sanctuary zone protection
    if (!sConfig.GetBoolDefault("Invasion.AllowInSanctuaries", false))
    {
        if (player->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_SANCTUARY))
            return false;
    }

    return true;
}

bool InvasionManager::CanInvadeFrom(Player* invader) const
{
    if (!sConfig.GetBoolDefault("Invasion.Enable", true))
    {
        Tell(invader, "The sigil is cold. Invasions are disabled.");
        return false;
    }

    uint32 const minLevel = sConfig.GetIntDefault("Invasion.MinLevel", 10);
    if (invader->GetLevel() < minLevel)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "You must be level %u to invade.", minLevel);
        Tell(invader, msg);
        return false;
    }

    if (IsInvolved(invader->GetObjectGuid()))
    {
        Tell(invader, "You are already involved in an invasion.");
        return false;
    }

    if (HasInvaderCooldown(invader->GetObjectGuid()))
    {
        uint32 const remaining = GetInvaderCooldownRemaining(invader->GetObjectGuid());
        char msg[128];
        snprintf(msg, sizeof(msg), "You must rest before seeking another soul (%u seconds remaining).", remaining);
        Tell(invader, msg);
        return false;
    }

    if (invader->IsInCombat())
    {
        Tell(invader, "Not while you are in combat.");
        return false;
    }

    if (invader->IsHardcore())
    {
        Tell(invader, "Hardcore characters cannot invade. Losing would be permanent.");
        return false;
    }

    if (invader->IsGameMaster())
    {
        Tell(invader, "Not in GM mode. Type .gm off first.");
        return false;
    }

    if (!IsAvailableForInvasion(invader))
    {
        Tell(invader, "You cannot invade from here.");
        return false;
    }

    // Party companions check: 1v1 rule
    if (Group* group = invader->GetGroup())
    {
        for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
        {
            Player* member = ref->getSource();
            if (member && member != invader && IsHeadlessBot(member))
            {
                Tell(invader, "Send your party companions away first.");
                return false;
            }
        }
    }

    return true;
}

bool InvasionManager::IsValidDefender(Player* invader, Player* target) const
{
    if (!target || target == invader)
        return false;

    if (!target->IsPvP())
        return false;

    uint32 const minLevel = sConfig.GetIntDefault("Invasion.MinLevel", 10);
    uint32 const range = sConfig.GetIntDefault("Invasion.LevelRange", 3);
    uint32 const mine = invader->GetLevel();
    uint32 const theirs = target->GetLevel();

    // Strict minimum level protection for defender
    if (theirs < minLevel)
        return false;

    // Bracket check: target must be within [mine - range, mine + range]
    if (theirs + range < mine || mine + range < theirs)
        return false;

    // Same-faction invasion check
    bool const allowSameFaction = sConfig.GetBoolDefault("Invasion.AllowSameFaction", true);
    if (!allowSameFaction && invader->GetTeam() == target->GetTeam())
        return false;

    if (invader->GetGroup() && invader->GetGroup() == target->GetGroup())
        return false;

    if (invader->GetSession() && target->GetSession() &&
        invader->GetSession()->GetAccountId() == target->GetSession()->GetAccountId())
        return false;

    bool const invaderIsBot = IsHeadlessBot(invader);
    bool const targetIsBot = IsHeadlessBot(target);

    // Matrix checks based on participant types
    if (!invaderIsBot && targetIsBot)
    {
        if (!sConfig.GetBoolDefault("Invasion.AllowPlayerToBot", true))
            return false;
    }
    else if (invaderIsBot && !targetIsBot)
    {
        if (!sConfig.GetBoolDefault("Invasion.AllowBotToPlayer", true))
            return false;
    }
    else if (invaderIsBot && targetIsBot)
    {
        if (!sConfig.GetBoolDefault("Invasion.AllowBotToBot", true))
            return false;
    }

    if (targetIsBot && HasBotCooldown(target->GetObjectGuid()))
        return false;

    return IsAvailableForInvasion(target);
}

void InvasionManager::GrantRespite(ObjectGuid defender)
{
    uint32 const seconds = sConfig.GetIntDefault("Invasion.RespiteSeconds", 600);
    if (!seconds)
        return;

    m_respite[defender] = time(nullptr) + seconds;
    DebugLog("Granted respite to defender %u until %llu", defender.GetCounter(), (unsigned long long)m_respite[defender]);
}

bool InvasionManager::HasRespite(ObjectGuid defender) const
{
    auto const itr = m_respite.find(defender);
    return itr != m_respite.end() && itr->second > time(nullptr);
}

void InvasionManager::GrantBotCooldown(ObjectGuid botGuid)
{
    uint32 const seconds = sConfig.GetIntDefault("Invasion.BotCooldownSeconds", 600);
    if (!seconds)
        return;

    m_botCooldowns[botGuid] = time(nullptr) + seconds;
}

bool InvasionManager::HasBotCooldown(ObjectGuid botGuid) const
{
    auto const itr = m_botCooldowns.find(botGuid);
    return itr != m_botCooldowns.end() && itr->second > time(nullptr);
}

void InvasionManager::GrantInvaderCooldown(Player* invader)
{
    if (!invader)
        return;

    uint32 const seconds = sConfig.GetIntDefault("Invasion.InvaderCooldownSeconds", 60);
    if (!seconds)
        return;

    time_t const now = time(nullptr);
    m_invaderCooldowns[invader->GetObjectGuid()] = now + seconds;

    uint32 const spellId = sConfig.GetIntDefault("Invasion.SpellId", 51942);
    time_t const endTime = now + seconds;

    invader->AddSpellCooldown(spellId, 0, endTime);
    invader->SendSpellCooldown(spellId, seconds * IN_MILLISECONDS, invader->GetObjectGuid());

    DebugLog("Granted invader cooldown to %s for %u seconds", invader->GetName(), seconds);
}

bool InvasionManager::HasInvaderCooldown(ObjectGuid invaderGuid) const
{
    auto const itr = m_invaderCooldowns.find(invaderGuid);
    return itr != m_invaderCooldowns.end() && itr->second > time(nullptr);
}

uint32 InvasionManager::GetInvaderCooldownRemaining(ObjectGuid invaderGuid) const
{
    auto const itr = m_invaderCooldowns.find(invaderGuid);
    if (itr == m_invaderCooldowns.end())
        return 0;

    time_t const now = time(nullptr);
    return (itr->second > now) ? uint32(itr->second - now) : 0;
}

void InvasionManager::ApplyCombatantPvPFlags(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    player->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
    player->SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
    ResendPlayerFlags(player);
}

void InvasionManager::RemoveCombatantPvPFlags(Player* player)
{
    if (!player || !player->IsInWorld())
        return;

    player->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
    player->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
    ResendPlayerFlags(player);
}

void InvasionManager::EngageBotInCombat(Player* botPlayer, Player* targetPlayer)
{
    if (!botPlayer || !targetPlayer || !IsHeadlessBot(botPlayer))
        return;

    if (!sConfig.GetBoolDefault("Invasion.BotEngagePvP", true))
        return;

    // 1. Core orientation & facing
    botPlayer->SetFacingToObject(targetPlayer);

    // 2. Clear any lingering passive movement generators
    botPlayer->GetMotionMaster()->Clear(false);

    // 3. Target selection & arm auto-attack
    botPlayer->SetSelectionGuid(targetPlayer->GetObjectGuid());
    botPlayer->Attack(targetPlayer, true);

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = GET_PLAYERBOT_AI(botPlayer);
    if (ai)
    {
        // Switch to combat engine immediately
        ai->ChangeEngine(BotState::BOT_STATE_COMBAT);

        // Apply configured PvP strategies
        std::string pvpStrats = sConfig.GetStringDefault("Invasion.BotPvPStrategies", "+pvp,+world pvp");
        if (!pvpStrats.empty())
            ai->ChangeStrategy(pvpStrats, BotState::BOT_STATE_ALL);

        // Set explicit AI target values
        if (ai->GetAiObjectContext())
        {
            if (auto currentTarget = ai->GetAiObjectContext()->GetValue<Unit*>("current target"))
                currentTarget->Set(targetPlayer);

            if (auto attackTarget = ai->GetAiObjectContext()->GetValue<ObjectGuid>("attack target"))
                attackTarget->Set(targetPlayer->GetObjectGuid());
        }

        // Force bot to act immediately without waiting for normal tick throttle
        ai->DoNextAction();

        DebugLog("Engaged TortoiseBots AI for bot %s against %s (Strats: %s)",
                 botPlayer->GetName(), targetPlayer->GetName(), pvpStrats.c_str());
    }
#endif
}

void InvasionManager::ResetBotAI(Player* botPlayer)
{
    if (!botPlayer || !IsHeadlessBot(botPlayer))
        return;

    botPlayer->CombatStop(true);
    botPlayer->SetSelectionGuid(ObjectGuid());
    botPlayer->GetMotionMaster()->Clear(false);

    if (!sConfig.GetBoolDefault("Invasion.BotResetAI", true))
        return;

#if defined(HAVE_TORTOISEBOTS)
    PlayerbotAI* ai = GET_PLAYERBOT_AI(botPlayer);
    if (ai)
    {
        std::string pvpStrats = sConfig.GetStringDefault("Invasion.BotPvPStrategies", "+pvp,+world pvp");
        std::string removeStrats = pvpStrats;
        for (char& c : removeStrats)
            if (c == '+')
                c = '-';
        if (!removeStrats.empty())
            ai->ChangeStrategy(removeStrats, BotState::BOT_STATE_ALL);

        ai->Reset(true);
        ai->ResetStrategies(true);

        DebugLog("Reset TortoiseBots AI for bot %s after invasion", botPlayer->GetName());
    }
#endif
}

Player* InvasionManager::PickDefender(Player* invader) const
{
    bool const invaderIsBot = IsHeadlessBot(invader);
    std::vector<Player*> humanCandidates;
    std::vector<Player*> botCandidates;

    {
        HashMapHolder<Player>::ReadGuard guard(HashMapHolder<Player>::GetLock());
        for (auto const& itr : sObjectAccessor.GetPlayers())
        {
            Player* candidate = itr.second;
            if (!candidate)
                continue;

            if (IsValidDefender(invader, candidate) &&
                !IsInvolved(candidate->GetObjectGuid()) &&
                !HasRespite(candidate->GetObjectGuid()))
            {
                if (IsHeadlessBot(candidate))
                    botCandidates.push_back(candidate);
                else
                    humanCandidates.push_back(candidate);
            }
        }
    }

    DebugLog("PickDefender: invader %s (%s) found %u human candidates, %u bot candidates",
             invader->GetName(), invaderIsBot ? "Bot" : "Human",
             (uint32)humanCandidates.size(), (uint32)botCandidates.size());

    // When a human player initiates the invasion:
    // Prioritize real human players first!
    if (!invaderIsBot)
    {
        if (!humanCandidates.empty())
            return humanCandidates[urand(0, humanCandidates.size() - 1)];

        // Fallback to roaming bots if allowed
        if (sConfig.GetBoolDefault("Invasion.AllowPlayerToBot", true) && !botCandidates.empty())
            return botCandidates[urand(0, botCandidates.size() - 1)];

        return nullptr;
    }

    // When an autonomous bot initiates:
    // Check allowBotToPlayer vs allowBotToBot
    bool allowPlayer = sConfig.GetBoolDefault("Invasion.AllowBotToPlayer", true);
    bool allowBot = sConfig.GetBoolDefault("Invasion.AllowBotToBot", true);

    if (allowPlayer && !humanCandidates.empty())
    {
        // 70% preference to hunt human players if available
        if (!botCandidates.empty() && allowBot)
        {
            if (urand(1, 100) <= 70)
                return humanCandidates[urand(0, humanCandidates.size() - 1)];
            else
                return botCandidates[urand(0, botCandidates.size() - 1)];
        }
        return humanCandidates[urand(0, humanCandidates.size() - 1)];
    }

    if (allowBot && !botCandidates.empty())
        return botCandidates[urand(0, botCandidates.size() - 1)];

    return nullptr;
}

bool InvasionManager::CanBeginInvasion(Player* invader)
{
    if (!invader)
        return false;

    time_t const now = time(nullptr);
    for (auto itr = m_respite.begin(); itr != m_respite.end();)
        itr = (itr->second <= now) ? m_respite.erase(itr) : ++itr;

    for (auto itr = m_botCooldowns.begin(); itr != m_botCooldowns.end();)
        itr = (itr->second <= now) ? m_botCooldowns.erase(itr) : ++itr;

    for (auto itr = m_invaderCooldowns.begin(); itr != m_invaderCooldowns.end();)
        itr = (itr->second <= now) ? m_invaderCooldowns.erase(itr) : ++itr;

    return CanInvadeFrom(invader);
}

bool InvasionManager::BeginInvasion(Player* invader, Player* designatedDefender)
{
    if (!CanBeginInvasion(invader))
        return false;

    Player* defender = designatedDefender ? designatedDefender : PickDefender(invader);
    if (!defender)
    {
        Tell(invader, "No soul here is ready to be forgiven.");
        return false;
    }

    // Ensure PvP flag is locked on for both participants
    for (Player* side : { invader, defender })
    {
        if (!side->IsPvP())
            side->UpdatePvP(true);

        if (!side->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_DESIRED))
        {
            side->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_PVP_DESIRED);
            side->UpdatePvP(true);
        }
    }

    Invasion inv;
    inv.invader = invader->GetObjectGuid();
    inv.defender = defender->GetObjectGuid();
    inv.origin = WorldLocation(invader->GetMapId(), invader->GetPositionX(),
                               invader->GetPositionY(), invader->GetPositionZ(),
                               invader->GetOrientation());
    inv.phase = PHASE_STALKING;
    inv.timer = sConfig.GetIntDefault("Invasion.WarnSeconds", 5) * IN_MILLISECONDS;
    inv.isBotInvader = IsHeadlessBot(invader);
    inv.isBotDefender = IsHeadlessBot(defender);

    m_invasions.push_back(inv);

    Tell(defender, "Something has come to forgive you.");
    TellFmt(invader, "You go to forgive %s.", defender->GetName());

    sLog.outBasic("Invasion: %s (level %u, %s) is hunting %s (level %u, %s).",
        invader->GetName(), invader->GetLevel(), inv.isBotInvader ? "Bot" : "Human",
        defender->GetName(), defender->GetLevel(), inv.isBotDefender ? "Bot" : "Human");

    return true;
}

void InvasionManager::Arrive(Invasion& inv, Player* invader, Player* defender)
{
    float const arriveDistance = float(sConfig.GetIntDefault("Invasion.ArriveDistance", 10));
    float x, y, z;
    defender->GetClosePoint(x, y, z, ARRIVAL_BOUNDING_RADIUS, arriveDistance, M_PI_F);

    invader->TeleportTo(defender->GetMapId(), x, y, z, defender->GetOrientation());

    inv.arena = WorldLocation(defender->GetMapId(), x, y, z, defender->GetOrientation());
    inv.arrivalFx = true;
    inv.arrivalLanded = false;
    inv.arrivalFxMs = ARRIVE_FX_DELAY_MS;
    inv.outInvader = 0;
    inv.outDefender = 0;
    inv.phase = PHASE_FIGHTING;
    inv.timer = sConfig.GetIntDefault("Invasion.DurationSeconds", 300) * IN_MILLISECONDS;

    // Apply both server and client FFA PvP flags so same-faction combatants can attack each other
    ApplyCombatantPvPFlags(invader);
    ApplyCombatantPvPFlags(defender);

    GrantRespite(inv.defender);

    TellFmt(invader, "You stand behind %s. Forgive them.", defender->GetName());
    TellFmt(defender, "%s has come to forgive you.", invader->GetName());

    // Notify client UI addon
    if (!inv.isBotDefender)
        defender->SendAddonMessage("CINV", std::string("I ") + invader->GetName());

    // Bot instant aggression commands: ensure bots target each other and fight like normal PvP
    if (inv.isBotInvader)
        EngageBotInCombat(invader, defender);

    if (inv.isBotDefender)
        EngageBotInCombat(defender, invader);
}

bool InvasionManager::ArenaForfeit(Invasion& inv, Player* invader, Player* defender, uint32 diff)
{
    Player* const sides[2] = { invader, defender };
    uint32* const clocks[2] = { &inv.outInvader, &inv.outDefender };

    for (int i = 0; i < 2; ++i)
    {
        Player* const p = sides[i];
        uint32& left = *clocks[i];

        if (!p || p->IsBeingTeleported())
            continue;

        float const dist = (p->GetMapId() == inv.arena.mapId)
            ? p->GetDistance(inv.arena.x, inv.arena.y, inv.arena.z)
            : ARENA_OUT_YARDS * 100.0f;

        if (!left)
        {
            if (dist > ARENA_OUT_YARDS)
            {
                left = ARENA_FORFEIT_MS;
                if (!IsHeadlessBot(p))
                {
                    p->GetSession()->SendAreaTriggerMessage("Return within 10 seconds or forfeit.");
                    Tell(p, "You stray from the rite. Return within 10 seconds or forfeit.");
                }
            }
            continue;
        }

        if (dist < ARENA_BACK_YARDS)
        {
            left = 0;
            Tell(p, "You return to the rite.");
            continue;
        }

        if (left > diff)
        {
            left -= diff;
            continue;
        }

        // Forfeit occurred
        if (i == 0)
            Conclude(inv, "You abandoned the rite. You forfeit.", "Your forgiver fled. You remain.", 0, inv.defender);
        else
        {
            inv.leaveKit = ESCAPE_KIT;
            Conclude(inv, "They fled their forgiveness.", "You turned away. You forfeit.", 0, inv.invader);
        }
        return true;
    }
    return false;
}

void InvasionManager::SendHome(Invasion const& inv, Player* invader)
{
    if (!invader || !invader->IsInWorld())
        return;

    if (inv.leaveKit != NO_KIT)
        invader->SendPlaySpellVisual(inv.leaveKit ? inv.leaveKit : LEAVE_KIT);

    invader->TeleportTo(inv.origin.mapId, inv.origin.x, inv.origin.y, inv.origin.z, inv.origin.o);
}

void InvasionManager::RecordMatch(Invasion const& inv, ObjectGuid winner)
{
    if (inv.phase != PHASE_FIGHTING && inv.phase != PHASE_LEAVING)
        return;

    CharacterDatabase.PExecute("INSERT INTO character_invasion_match (invader, defender, winner) VALUES ('%u', '%u', '%u')",
                               inv.invader.GetCounter(), inv.defender.GetCounter(), winner.GetCounter());
}

void InvasionManager::AwardRewards(Player* winner, Player* loser)
{
    if (!winner || !loser)
        return;

    // Honor reward
    uint32 const honorReward = sConfig.GetIntDefault("Invasion.RewardHonor", 75);
    if (honorReward)
        winner->RewardHonor(loser, 1);

    // Gold reward
    uint32 const moneyReward = sConfig.GetIntDefault("Invasion.RewardMoney", 5000);
    if (moneyReward)
        winner->ModifyMoney(moneyReward);

    if (!IsHeadlessBot(winner))
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "Victory! Awarded %u Honor and %u silver.", honorReward, moneyReward / 100);
        Tell(winner, buf);
    }
}

void InvasionManager::Conclude(Invasion& inv, char const* invaderLine, char const* defenderLine,
                               uint32 delayMs, ObjectGuid winner)
{
    RecordMatch(inv, winner);

    Player* pInvader = sObjectAccessor.FindPlayer(inv.invader);
    Player* pDefender = sObjectAccessor.FindPlayer(inv.defender);

    // Revert FFA PvP flags upon match completion
    RemoveCombatantPvPFlags(pInvader);
    RemoveCombatantPvPFlags(pDefender);

    if (winner && pInvader && pDefender)
    {
        Player* winnerPlayer = (winner == inv.invader) ? pInvader : pDefender;
        Player* loserPlayer = (winner == inv.invader) ? pDefender : pInvader;
        AwardRewards(winnerPlayer, loserPlayer);
    }

    if (inv.phase == PHASE_FIGHTING)
    {
        GrantRespite(inv.defender);
        if (pInvader && !inv.isBotInvader)
            GrantInvaderCooldown(pInvader);
        if (inv.isBotInvader)
            GrantBotCooldown(inv.invader);
        if (inv.isBotDefender)
            GrantBotCooldown(inv.defender);
    }

    // Reset bot AI decision trees and restore default non-combat strategies
    if (inv.isBotInvader && pInvader)
        ResetBotAI(pInvader);

    if (inv.isBotDefender && pDefender)
        ResetBotAI(pDefender);

    inv.phase = PHASE_LEAVING;
    inv.timer = delayMs ? delayMs : (sConfig.GetIntDefault("Invasion.ReturnSeconds", 5) * IN_MILLISECONDS);

    ResendPlayerFlags(pInvader);
    ResendPlayerFlags(pDefender);

    HideRadar(pInvader);
    HideRadar(pDefender);

    if (invaderLine)
        Tell(pInvader, invaderLine);
    if (defenderLine)
        Tell(pDefender, defenderLine);
}

void InvasionManager::OnPlayerDeath(Player* victim, Unit* killer)
{
    if (!victim)
        return;

    for (auto& inv : m_invasions)
    {
        bool const victimIsInvader = (inv.invader == victim->GetObjectGuid());
        if (!victimIsInvader && inv.defender != victim->GetObjectGuid())
            continue;

        if (inv.phase != PHASE_FIGHTING)
        {
            if (inv.phase == PHASE_STALKING)
                Conclude(inv, "Something else forgave them first.", nullptr);
            return;
        }

        ObjectGuid const opponent = victimIsInvader ? inv.defender : inv.invader;
        Player* const killerPlayer = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
        Player* const opponentPlayer = sObjectAccessor.FindPlayer(opponent);
        ObjectGuid const winner = (killerPlayer && killerPlayer->GetObjectGuid() == opponent &&
                                   opponentPlayer && opponentPlayer->IsAlive()) ? opponent : ObjectGuid();

        if (victimIsInvader)
        {
            uint32 const lingerMs = sConfig.GetIntDefault("Invasion.DeathLingerSeconds", 10) * IN_MILLISECONDS;
            Conclude(inv, "You have fallen. Lie still. You will be carried back.",
                          "You refused forgiveness. You remain.",
                     lingerMs, winner);
        }
        else
        {
            Conclude(inv, "They are forgiven.", "You have been forgiven.", 0, winner);
            victim->SendPlaySpellVisual(DEATH_KIT);

            PendingRevive r;
            r.guid = victim->GetObjectGuid();
            r.where = WorldLocation(victim->GetMapId(), victim->GetPositionX(), victim->GetPositionY(),
                                    victim->GetPositionZ(), victim->GetOrientation());
            m_revives.push_back(r);
        }
        return;
    }
}

bool InvasionManager::OnPlayerLogout(Player* player)
{
    if (!player)
        return false;

    ObjectGuid const guid = player->GetObjectGuid();

    for (auto itr = m_invasions.begin(); itr != m_invasions.end(); ++itr)
    {
        if (itr->invader != guid && itr->defender != guid)
            continue;

        bool const wasInvader = (itr->invader == guid);

        if (wasInvader)
        {
            ResetBotAI(player);
            RemoveCombatantPvPFlags(player);
            player->TeleportTo(itr->origin.mapId, itr->origin.x, itr->origin.y,
                               itr->origin.z, itr->origin.o);

            Player* defender = sObjectAccessor.FindPlayer(itr->defender);
            Tell(defender, "Your forgiver has left this world.");
            HideRadar(defender);
            if (defender)
            {
                RemoveCombatantPvPFlags(defender);
                ResetBotAI(defender);
            }
            ResendPlayerFlags(defender);
            RecordMatch(*itr, ObjectGuid());
        }
        else
        {
            if (itr->phase != PHASE_LEAVING)
            {
                itr->leaveKit = ESCAPE_KIT;
                Conclude(*itr, "They left this world before you could forgive them.", nullptr);
            }
            return false;
        }

        m_invasions.erase(itr);
        return wasInvader;
    }

    return false;
}

void InvasionManager::UpdateRevives(uint32 /*diff*/)
{
    for (size_t i = 0; i < m_revives.size();)
    {
        Player* p = sObjectAccessor.FindPlayer(m_revives[i].guid);
        if (!p || !p->IsInWorld() || p->IsAlive())
            m_revives.erase(m_revives.begin() + i);
        else
            ++i;
    }
}

bool InvasionManager::OnReleaseSpirit(Player* player)
{
    if (!player)
        return false;

    for (size_t i = 0; i < m_revives.size(); ++i)
    {
        if (m_revives[i].guid != player->GetObjectGuid())
            continue;

        m_revives.erase(m_revives.begin() + i);
        Tell(player, "Your spirit lingers over your body.");
        return true;
    }
    return false;
}

bool InvasionManager::Withdraw(Player* invader)
{
    if (!invader)
        return false;

    for (auto itr = m_invasions.begin(); itr != m_invasions.end(); ++itr)
    {
        if (itr->invader != invader->GetObjectGuid())
            continue;

        Player* defender = sObjectAccessor.FindPlayer(itr->defender);
        switch (itr->phase)
        {
            case PHASE_STALKING:
                Tell(invader, "You let them go.");
                Tell(defender, "The cold passes.");
                m_invasions.erase(itr);
                return true;
            case PHASE_FIGHTING:
                invader->SendPlaySpellVisual(ESCAPE_KIT);
                itr->leaveKit = NO_KIT;
                Conclude(*itr, "You abandon the rite. You forfeit.", "Your forgiver withdrew. You remain.",
                         WITHDRAW_PULL_MS, itr->defender);
                return true;
            default:
                Tell(invader, "You are already being drawn back.");
                return true;
        }
    }
    return false;
}

bool InvasionManager::IsInvolved(ObjectGuid guid) const
{
    for (auto const& inv : m_invasions)
        if (inv.invader == guid || inv.defender == guid)
            return true;
    return false;
}

bool InvasionManager::IsInvading(ObjectGuid guid) const
{
    for (auto const& inv : m_invasions)
        if (inv.invader == guid)
            return true;
    return false;
}

bool InvasionManager::IsFighting(ObjectGuid guid) const
{
    for (auto const& inv : m_invasions)
        if (inv.phase == PHASE_FIGHTING && (inv.invader == guid || inv.defender == guid))
            return true;
    return false;
}

bool InvasionManager::AreFoes(ObjectGuid a, ObjectGuid b) const
{
    for (auto const& inv : m_invasions)
        if (inv.phase == PHASE_FIGHTING &&
            ((inv.invader == a && inv.defender == b) || (inv.invader == b && inv.defender == a)))
            return true;
    return false;
}

uint32 InvasionManager::GetActiveBotInvasionsCount() const
{
    uint32 count = 0;
    for (auto const& inv : m_invasions)
        if (inv.isBotInvader)
            count++;
    return count;
}

void InvasionManager::CheckAutonomousBotInvasions(uint32 diff)
{
    if (!sConfig.GetBoolDefault("Invasion.Enable", true))
        return;

    bool allowBotToPlayer = sConfig.GetBoolDefault("Invasion.AllowBotToPlayer", true);
    bool allowBotToBot = sConfig.GetBoolDefault("Invasion.AllowBotToBot", true);
    if (!allowBotToPlayer && !allowBotToBot)
        return;

    uint32 const intervalMs = sConfig.GetIntDefault("Invasion.BotInvasionInterval", 300) * IN_MILLISECONDS;
    m_botCheckTimer += diff;
    if (m_botCheckTimer < intervalMs)
        return;

    m_botCheckTimer = 0;

    uint32 const maxConcurrent = sConfig.GetIntDefault("Invasion.BotMaxActiveInvasions", 3);
    if (GetActiveBotInvasionsCount() >= maxConcurrent)
        return;

    uint32 const chance = sConfig.GetIntDefault("Invasion.BotInvasionChance", 30);
    if (urand(1, 100) > chance)
        return;

    uint32 const minLevel = sConfig.GetIntDefault("Invasion.MinLevel", 10);
    uint32 const spellId = sConfig.GetIntDefault("Invasion.SpellId", 51942);

    std::vector<Player*> botCandidates;
    {
        HashMapHolder<Player>::ReadGuard guard(HashMapHolder<Player>::GetLock());
        for (auto const& itr : sObjectAccessor.GetPlayers())
        {
            Player* p = itr.second;
            if (p && IsHeadlessBot(p) && p->GetLevel() >= minLevel &&
                !p->IsInCombat() && !IsInvolved(p->GetObjectGuid()) &&
                !HasBotCooldown(p->GetObjectGuid()) && IsAvailableForInvasion(p))
            {
                botCandidates.push_back(p);
            }
        }
    }

    if (botCandidates.empty())
        return;

    Player* botInvader = botCandidates[urand(0, botCandidates.size() - 1)];
    if (!botInvader)
        return;

    // Pick defender
    Player* defender = PickDefender(botInvader);
    if (!defender)
        return;

    DebugLog("Autonomous Bot Invader triggered: %s targeting %s", botInvader->GetName(), defender->GetName());

    // Bot begins channeling spell 51942 if in Mode 1 or hybrid
    if (botInvader->HasSpell(spellId))
        botInvader->CastSpell(botInvader, spellId, false);
    else
        BeginInvasion(botInvader, defender);
}

void InvasionManager::Update(uint32 diff)
{
    UpdateRevives(diff);
    CheckAutonomousBotInvasions(diff);

    for (size_t i = 0; i < m_invasions.size();)
    {
        Invasion& inv = m_invasions[i];

        Player* invader = sObjectAccessor.FindPlayerNotInWorld(inv.invader);
        Player* defender = sObjectAccessor.FindPlayer(inv.defender);

        if (!invader || (!invader->IsInWorld() && !invader->IsBeingTeleported()))
        {
            HideRadar(defender);
            if (defender)
                defender->SetFFAPvP(false);
            ResendPlayerFlags(defender);
            m_invasions.erase(m_invasions.begin() + i);
            continue;
        }

        if (!invader->IsInWorld())
        {
            ++i;
            continue;
        }

        inv.timer = (inv.timer > diff) ? inv.timer - diff : 0;

        switch (inv.phase)
        {
            case PHASE_STALKING:
            {
                if (inv.timer)
                    break;

                if (!defender || !IsValidDefender(invader, defender) || !IsAvailableForInvasion(invader))
                {
                    Tell(invader, "Their soul slips from your sight.");
                    Tell(defender, "The cold passes.");
                    m_invasions.erase(m_invasions.begin() + i);
                    continue;
                }

                Arrive(inv, invader, defender);
                break;
            }
            case PHASE_FIGHTING:
            {
                if (!defender || !defender->IsInWorld() ||
                    (defender->GetMap() && defender->GetMap()->Instanceable()))
                {
                    inv.leaveKit = ESCAPE_KIT;
                    Conclude(inv, "They are beyond forgiveness, for now.", nullptr);
                    break;
                }

                if (inv.arrivalFx && !invader->IsBeingTeleported())
                {
                    if (!inv.arrivalLanded)
                    {
                        inv.arrivalLanded = true;
                        inv.keepStealth = invader->HasAuraType(SPELL_AURA_MOD_STEALTH);
                    }

                    inv.arrivalFxMs = (inv.arrivalFxMs > diff) ? inv.arrivalFxMs - diff : 0;
                    if (!inv.arrivalFxMs)
                    {
                        if (!inv.keepStealth)
                            invader->SendPlaySpellVisual(ARRIVE_KIT);
                        if (defender)
                            defender->SendPlaySpellVisual(MARK_KIT);
                        inv.arrivalFx = false;
                    }
                }

                // Maintain both server and client FFA PvP hostility flags on both combatants
                ApplyCombatantPvPFlags(invader);
                if (defender)
                    ApplyCombatantPvPFlags(defender);

                // Ensure bots stay actively focused and do not stay static or drop target
                if (inv.isBotInvader && invader->IsAlive() && defender && defender->IsAlive())
                {
                    if (invader->GetSelectionGuid() != defender->GetObjectGuid() || !invader->GetVictim())
                        EngageBotInCombat(invader, defender);
                }

                if (inv.isBotDefender && defender && defender->IsAlive() && invader->IsAlive())
                {
                    if (defender->GetSelectionGuid() != invader->GetObjectGuid() || !defender->GetVictim())
                        EngageBotInCombat(defender, invader);
                }

                if (ArenaForfeit(inv, invader, defender, diff))
                    break;

                inv.radarMs = (inv.radarMs > diff) ? inv.radarMs - diff : 0;
                if (!inv.radarMs)
                {
                    inv.radarMs = RADAR_MS;
                    SendRadar(invader, defender);
                    SendRadar(defender, invader);
                }

                if (!inv.timer)
                    Conclude(inv, "The rite fades. They remain unforgiven.", "The presence behind you fades.");

                break;
            }
            case PHASE_LEAVING:
            {
                if (inv.timer)
                    break;

                SendHome(inv, invader);

                if (invader->IsAlive())
                {
                    Tell(invader, "The rite is over. You are drawn back.");
                    m_invasions.erase(m_invasions.begin() + i);
                    continue;
                }

                inv.phase = PHASE_REVIVING;
                inv.timer = sConfig.GetIntDefault("Invasion.ReviveSeconds", 3) * IN_MILLISECONDS;
                break;
            }
            case PHASE_REVIVING:
            {
                if (inv.timer || invader->IsBeingTeleported())
                    break;

                invader->ResurrectPlayer(1.0f, false);
                invader->SpawnCorpseBones();
                Tell(invader, "You wake where you began.");
                m_invasions.erase(m_invasions.begin() + i);
                continue;
            }
        }

        ++i;
    }
}
