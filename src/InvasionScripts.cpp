#include "InvasionMgr.h"
#include "ScriptObjects.h"
#include "Objects/Player.h"
#include "Spells/Spell.h"
#include "Config/Config.h"
#include "Chat.h"

// ============================================================================
// 1. Player Lifecycle & Spell/Combat Hooks
// ============================================================================
class InvasionPlayerScript : public PlayerScript
{
public:
    InvasionPlayerScript() : PlayerScript("InvasionPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LEVEL_CHANGED,
        PLAYERHOOK_ON_SPELL_CAST,
        PLAYERHOOK_ON_BEFORE_LOGOUT,
        PLAYERHOOK_ON_PLAYER_RELEASED_GHOST,
        PLAYERHOOK_ON_UPDATE_AREA
    }) {}

    void CheckAndLearnInvasionSpell(Player* player)
    {
        if (!player || !sConfig.GetBoolDefault("Invasion.Enable", true))
            return;

        uint32 const mode = sConfig.GetIntDefault("Invasion.AccessMode", 1);
        // Mode 1 (Spell) and Mode 3 (Hybrid) autolearn the spell
        if (mode != 1 && mode != 3)
            return;

        uint32 const minLevel = sConfig.GetIntDefault("Invasion.MinLevel", 10);
        uint32 const spellId = sConfig.GetIntDefault("Invasion.SpellId", 51942);

        if (player->GetLevel() >= minLevel && !player->HasSpell(spellId))
        {
            player->LearnSpell(spellId, false);

            if (player->GetSession() && !player->GetSession()->IsHeadless())
                ChatHandler(player).SendSysMessage("You have unlocked the Invader's Rite in your Spellbook.");
        }
    }

    void OnLogin(Player* player) override
    {
        CheckAndLearnInvasionSpell(player);
    }

    void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        CheckAndLearnInvasionSpell(player);
    }

    void OnSpellCast(Player* player, Spell* spell, bool /*skipCheck*/) override
    {
        if (!player || !spell || !spell->m_spellInfo)
            return;

        uint32 const spellId = sConfig.GetIntDefault("Invasion.SpellId", 51942);
        if (spell->m_spellInfo->Id != spellId)
            return;

        // If the player is already invading, casting again triggers withdrawal
        if (sInvasionMgr.IsInvading(player->GetObjectGuid()))
        {
            sInvasionMgr.Withdraw(player);
            return;
        }

        // Channeling completed: begin invasion
        sInvasionMgr.BeginInvasion(player);
    }

    void OnBeforeLogout(Player* player) override
    {
        sInvasionMgr.OnPlayerLogout(player);
    }

    void OnPlayerReleasedGhost(Player* player) override
    {
        sInvasionMgr.OnReleaseSpirit(player);
    }

    void OnUpdateArea(Player* player, uint32 /*oldArea*/, uint32 /*newArea*/) override
    {
        if (player && sInvasionMgr.IsFighting(player->GetObjectGuid()))
            sInvasionMgr.ApplyCombatantPvPFlags(player);
    }
};

// ============================================================================
// 2. World Server Tick Hook (Drive match states & bot invasions)
// ============================================================================
class InvasionWorldScript : public WorldScript
{
public:
    InvasionWorldScript() : WorldScript("InvasionWorldScript", {
        WORLDHOOK_ON_UPDATE
    }) {}

    void OnUpdate(uint32 diff) override
    {
        sInvasionMgr.Update(diff);
    }
};

// ============================================================================
// 3. Combat Death Hook (Intercept player kills inside invasions)
// ============================================================================
class InvasionUnitScript : public UnitScript
{
public:
    InvasionUnitScript() : UnitScript("InvasionUnitScript", {
        UNITHOOK_ON_UNIT_DEATH
    }) {}

    void OnUnitDeath(Unit* unit, Unit* killer) override
    {
        if (unit && unit->IsPlayer())
            sInvasionMgr.OnPlayerDeath(unit->ToPlayer(), killer);
    }
};

// ============================================================================
// 4. Item Script Hook (Invader's Sigil - Item 100035)
// ============================================================================
class InvasionItemScript : public ItemScript
{
public:
    InvasionItemScript() : ItemScript("item_invasion_sigil") {}

    bool OnUse(Player* player, Item* item, SpellCastTargets& /*targets*/) override
    {
        if (!player || !item)
            return true;

        if (!sConfig.GetBoolDefault("Invasion.Enable", true))
        {
            ChatHandler(player).SendSysMessage("The sigil is cold. Invasions are disabled.");
            return true;
        }

        uint32 const mode = sConfig.GetIntDefault("Invasion.AccessMode", 1);
        bool const talismanEnabled = sConfig.GetBoolDefault("Invasion.EnableTalisman", false);
        if (mode == 1 && !talismanEnabled)
        {
            ChatHandler(player).SendSysMessage("The talisman is dormant. Use your Spellbook to invoke the rite.");
            return true;
        }

        // Re-using the item mid-invasion triggers withdrawal
        if (sInvasionMgr.IsInvading(player->GetObjectGuid()))
        {
            sInvasionMgr.Withdraw(player);
            return true;
        }

        // Pre-checks before starting the 6s channeling bar
        if (!sInvasionMgr.CanBeginInvasion(player))
            return true;

        // Returning false allows the item's ON_USE spell (51942) to cast for 6 seconds
        return false;
    }
};

// Auto-register scripts into ScriptRegistry
void RegisterInvasionScripts()
{
    new InvasionPlayerScript();
    new InvasionWorldScript();
    new InvasionUnitScript();
    new InvasionItemScript();
}
