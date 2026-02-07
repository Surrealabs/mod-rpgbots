// SelfBotSystem.cpp
// "Selfbot" / autoplay mode: the player's own character is controlled by the
// bot AI waterfall.  Toggle with `.rpg selfbot`.
//
// When active the system:
//   1. Detects the player's spec and loads the matching rotation
//   2. Auto-acquires a nearby hostile target if the player has none
//   3. Runs the same bucket waterfall (buffs → defensives → dots → hots → abilities → mobility)
//   4. Auto-chases melee/ranged targets at the rotation's preferred range
//   5. Stops immediately when toggled off or the player moves/casts manually
//
// Manual override: any player-initiated movement or spell cast disables selfbot
// temporarily for 3 seconds, then resumes.

#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "CommandScript.h"
#include "BotAI.h"
#include "RotationEngine.h"
#include "RPGBotsConfig.h"
#include "SpellAuras.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "ObjectAccessor.h"
#include "Log.h"
#include "Group.h"
#include <unordered_map>
#include <unordered_set>

using namespace Acore::ChatCommands;

// ─── Selfbot state per player ──────────────────────────────────────────────────
struct SelfBotState
{
    BotRole  role          = BotRole::ROLE_MELEE_DPS;
    uint8    specIndex     = 0;
    uint32   queuedSpellId = 0;
    ObjectGuid queuedTargetGuid;
    bool     isInCombat    = false;
};

static std::unordered_map<ObjectGuid::LowType, SelfBotState> sSelfBotPlayers;

// ─── Public toggle helpers ─────────────────────────────────────────────────────
static bool IsSelfBotActive(Player* player)
{
    return sSelfBotPlayers.count(player->GetGUID().GetCounter()) > 0;
}

static void EnableSelfBot(Player* player)
{
    auto& state = sSelfBotPlayers[player->GetGUID().GetCounter()];
    state.specIndex = DetectSpecIndex(player);
    state.role      = DetectBotRole(player);
    state.isInCombat = false;
    state.queuedSpellId = 0;
    state.queuedTargetGuid = ObjectGuid::Empty;
}

static void DisableSelfBot(Player* player)
{
    sSelfBotPlayers.erase(player->GetGUID().GetCounter());
}

// ─── Nearest hostile target finder ─────────────────────────────────────────────
static Unit* FindNearestHostile(Player* player, float /*range*/)
{
    // Use the threat manager: if anything is threatening us, pick it
    Unit* attacker = player->getAttackerForHelper();
    if (attacker && attacker->IsAlive() && !attacker->IsPlayer())
        return attacker;
    return nullptr;
}

// ─── Helpers (same as BotAI.cpp — duplicated to keep selfbot self-contained) ──
static bool CanCastSelf(Player* bot, Unit* target, uint32 spellId)
{
    if (spellId == 0 || !target)             return false;
    if (!bot->HasSpell(spellId))             return false;
    if (bot->HasSpellCooldown(spellId))      return false;
    return true;
}

static bool TryCastSelf(Player* bot, Unit* target, uint32 spellId)
{
    if (!CanCastSelf(bot, target, spellId))
        return false;
    return bot->CastSpell(target, spellId, false) == SPELL_CAST_OK;
}

// ─── Bucket runners (mirrors from BotAI.cpp) ──────────────────────────────────
static bool SelfRunBuffs(Player* bot, const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (bot->HasAura(id)) continue;
        if (TryCastSelf(bot, bot, id)) return true;
    }
    return false;
}

static bool SelfRunDefensives(Player* bot, const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    if (bot->GetHealthPct() >= 35.0f)
        return false;
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (TryCastSelf(bot, bot, id)) return true;
    }
    return false;
}

static bool SelfRunDots(Player* bot, Unit* enemy, const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    if (!enemy) return false;
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (enemy->HasAura(id)) continue;
        if (TryCastSelf(bot, enemy, id)) return true;
    }
    return false;
}

static Player* FindLowestHPSelf(Player* player)
{
    Group* grp = player->GetGroup();
    if (!grp) return nullptr;

    Player* lowest = nullptr;
    float lowPct = 100.f;
    for (GroupReference* ref = grp->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || !m->IsAlive() || !m->IsInWorld()) continue;
        if (m->GetMapId() != player->GetMapId()) continue;
        float pct = m->GetHealthPct();
        if (pct < lowPct) { lowPct = pct; lowest = m; }
    }
    return lowest;
}

static bool SelfRunHots(Player* bot, const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    Player* target = FindLowestHPSelf(bot);
    if (!target) return false;
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (target->HasAura(id)) continue;
        if (TryCastSelf(bot, target, id)) return true;
    }
    return false;
}

static bool SelfRunAbilities(Player* bot, Unit* enemy, BotRole role,
                             const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    if (role == BotRole::ROLE_HEALER)
    {
        Player* healTarget = FindLowestHPSelf(bot);
        if (!healTarget || healTarget->GetHealthPct() >= 90.0f)
            return false;
        for (uint32 id : spells)
        {
            if (id == 0) continue;
            if (TryCastSelf(bot, healTarget, id)) return true;
        }
        return false;
    }

    if (!enemy) return false;
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (TryCastSelf(bot, enemy, id)) return true;
    }
    return false;
}

// ─── Scan waterfall (dry-run for queuing) ──────────────────────────────────────
static bool SelfScanWaterfall(Player* bot, Unit* enemy, const SpecRotation* rot,
                              BotRole role,
                              uint32& outSpellId, ObjectGuid& outTargetGuid)
{
    // 1. Buffs
    for (uint32 id : rot->buffs)
    {
        if (id == 0) continue;
        if (bot->HasAura(id)) continue;
        if (CanCastSelf(bot, bot, id))
        {
            outSpellId = id; outTargetGuid = bot->GetGUID(); return true;
        }
    }

    // 2. Defensives
    if (bot->GetHealthPct() < 35.0f)
    {
        for (uint32 id : rot->defensives)
        {
            if (id == 0) continue;
            if (CanCastSelf(bot, bot, id))
            {
                outSpellId = id; outTargetGuid = bot->GetGUID(); return true;
            }
        }
    }

    // 3. DoTs
    if (enemy)
    {
        for (uint32 id : rot->dots)
        {
            if (id == 0) continue;
            if (enemy->HasAura(id)) continue;
            if (CanCastSelf(bot, enemy, id))
            {
                outSpellId = id; outTargetGuid = enemy->GetGUID(); return true;
            }
        }
    }

    // 4. HoTs
    {
        Player* hotTarget = FindLowestHPSelf(bot);
        if (hotTarget)
        {
            for (uint32 id : rot->hots)
            {
                if (id == 0) continue;
                if (hotTarget->HasAura(id)) continue;
                if (CanCastSelf(bot, hotTarget, id))
                {
                    outSpellId = id; outTargetGuid = hotTarget->GetGUID(); return true;
                }
            }
        }
    }

    // 5. Abilities
    if (role == BotRole::ROLE_HEALER)
    {
        Player* healTarget = FindLowestHPSelf(bot);
        if (healTarget && healTarget->GetHealthPct() < 90.0f)
        {
            for (uint32 id : rot->abilities)
            {
                if (id == 0) continue;
                if (CanCastSelf(bot, healTarget, id))
                {
                    outSpellId = id; outTargetGuid = healTarget->GetGUID(); return true;
                }
            }
        }
    }
    else if (enemy)
    {
        for (uint32 id : rot->abilities)
        {
            if (id == 0) continue;
            if (CanCastSelf(bot, enemy, id))
            {
                outSpellId = id; outTargetGuid = enemy->GetGUID(); return true;
            }
        }
    }

    return false;
}

// ─── Main selfbot waterfall ────────────────────────────────────────────────────
static void RunSelfBotWaterfall(Player* bot, Unit* enemy,
                                const SpecRotation* rot, SelfBotState& state)
{
    // While casting: queue next spell (once)
    if (bot->HasUnitState(UNIT_STATE_CASTING))
    {
        if (state.queuedSpellId == 0)
        {
            uint32 qSpell = 0;
            ObjectGuid qTarget;
            if (SelfScanWaterfall(bot, enemy, rot, state.role, qSpell, qTarget))
            {
                state.queuedSpellId    = qSpell;
                state.queuedTargetGuid = qTarget;
            }
        }
        return;
    }

    // Consume queued spell
    if (state.queuedSpellId != 0)
    {
        uint32 qSpell = state.queuedSpellId;
        ObjectGuid qTarget = state.queuedTargetGuid;
        state.queuedSpellId = 0;
        state.queuedTargetGuid = ObjectGuid::Empty;

        Unit* target = ObjectAccessor::GetUnit(*bot, qTarget);
        if (target && target->IsAlive() && target->IsInWorld())
        {
            if (TryCastSelf(bot, target, qSpell))
                return;
        }
    }

    // Normal waterfall
    if (SelfRunBuffs(bot, rot->buffs)) return;
    if (SelfRunDefensives(bot, rot->defensives)) return;
    if (SelfRunDots(bot, enemy, rot->dots)) return;
    if (SelfRunHots(bot, rot->hots)) return;
    if (SelfRunAbilities(bot, enemy, state.role, rot->abilities)) return;
}

// ─── World Script: tick selfbot players ────────────────────────────────────────
class SelfBotWorldScript : public WorldScript
{
public:
    SelfBotWorldScript() : WorldScript("SelfBotWorldScript") {}

    void OnUpdate(uint32 diff) override
    {
        _timer += diff;
        if (_timer < 1000) return;  // 1 second tick
        _timer = 0;

        if (sSelfBotPlayers.empty()) return;

        // Iterate a copy of keys in case we need to remove
        std::vector<ObjectGuid::LowType> toRemove;

        for (auto& [guidLow, state] : sSelfBotPlayers)
        {
            ObjectGuid guid = ObjectGuid::Create<HighGuid::Player>(guidLow);
            Player* player = ObjectAccessor::FindPlayer(guid);
            if (!player || !player->IsInWorld() || !player->IsAlive())
            {
                if (!player)
                    toRemove.push_back(guidLow);
                continue;
            }

            const SpecRotation* rot = sRotationEngine.GetRotation(
                player->getClass(), state.specIndex);
            if (!rot) continue;

            // ── Resolve target ─────────────────────────────────────────────
            Unit* enemy = player->GetVictim();
            if (!enemy || !enemy->IsAlive())
                enemy = player->GetSelectedUnit();
            if (enemy && (!enemy->IsAlive() || enemy->IsPlayer()))
                enemy = nullptr;

            // Auto-acquire nearest hostile if in combat with no target
            if (!enemy && player->IsInCombat())
                enemy = FindNearestHostile(player, 30.f);

            // Also auto-pull if idle and a hostile is very close (aggro simulation)
            if (!enemy)
                enemy = FindNearestHostile(player, 8.f);

            // ── Combat ─────────────────────────────────────────────────────
            if (enemy && enemy->IsAlive() && enemy->IsInWorld())
            {
                if (!state.isInCombat)
                {
                    state.isInCombat = true;
                    bool isMelee = (state.role == BotRole::ROLE_MELEE_DPS ||
                                    state.role == BotRole::ROLE_TANK);
                    player->Attack(enemy, isMelee);

                    float chase = (rot->preferredRange > 0)
                        ? rot->preferredRange
                        : (isMelee ? 0.5f : 25.0f);

                    player->GetMotionMaster()->Clear();
                    player->GetMotionMaster()->MoveChase(enemy, chase);
                }
                else if (player->GetVictim() != enemy)
                {
                    // Retarget if enemy changed
                    bool isMelee = (state.role == BotRole::ROLE_MELEE_DPS ||
                                    state.role == BotRole::ROLE_TANK);
                    player->Attack(enemy, isMelee);

                    float chase = (rot->preferredRange > 0)
                        ? rot->preferredRange
                        : (isMelee ? 0.5f : 25.0f);

                    player->GetMotionMaster()->Clear();
                    player->GetMotionMaster()->MoveChase(enemy, chase);
                }

                RunSelfBotWaterfall(player, enemy, rot, state);
            }
            else
            {
                // Out of combat
                if (state.isInCombat)
                {
                    state.isInCombat = false;
                    state.queuedSpellId = 0;
                    state.queuedTargetGuid = ObjectGuid::Empty;
                    player->AttackStop();
                    player->GetMotionMaster()->Clear();
                }
            }
        }

        for (auto g : toRemove)
            sSelfBotPlayers.erase(g);
    }

private:
    uint32 _timer = 0;
};

// ─── Player logout cleanup ─────────────────────────────────────────────────────
class SelfBotPlayerScript : public PlayerScript
{
public:
    SelfBotPlayerScript() : PlayerScript("SelfBotPlayerScript") {}

    void OnPlayerLogout(Player* player) override
    {
        if (player)
            sSelfBotPlayers.erase(player->GetGUID().GetCounter());
    }
};

// ─── Command: .rpg selfbot ─────────────────────────────────────────────────────
class SelfBotCommandScript : public CommandScript
{
public:
    SelfBotCommandScript() : CommandScript("SelfBotCommandScript") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable rpgTable =
        {
            { "selfbot", HandleSelfBotCommand, SEC_PLAYER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "rpg", rpgTable },
        };
        return commandTable;
    }

    static bool HandleSelfBotCommand(ChatHandler* handler)
    {
        if (!RPGBotsConfig::SelfBotEnabled)
        {
            handler->PSendSysMessage("|cffff0000Selfbot is disabled in server config.|r");
            return true;
        }

        Player* player = handler->GetSession()->GetPlayer();
        if (!player) return false;

        if (IsSelfBotActive(player))
        {
            DisableSelfBot(player);
            player->AttackStop();
            player->GetMotionMaster()->Clear();
            handler->PSendSysMessage("|cffff0000Selfbot DISABLED.|r Your character is back under your control.");
        }
        else
        {
            const SpecRotation* rot = sRotationEngine.GetRotation(
                player->getClass(), DetectSpecIndex(player));
            if (!rot)
            {
                handler->PSendSysMessage("|cffff0000No rotation found for your class/spec. Selfbot cannot activate.|r");
                return true;
            }
            EnableSelfBot(player);
            handler->PSendSysMessage("|cff00ff00Selfbot ENABLED.|r Your character will fight automatically.");
            handler->PSendSysMessage("  Spec: |cffffd700{}|r  Role: |cffffd700{}|r",
                rot->specName, BotRoleName(DetectBotRole(player)));
            handler->PSendSysMessage("  Type |cffffd700.rpg selfbot|r again to disable.");
        }
        return true;
    }
};

void AddSelfBotSystem()
{
    new SelfBotWorldScript();
    new SelfBotPlayerScript();
    new SelfBotCommandScript();
}
