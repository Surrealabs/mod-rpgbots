// BotAI.cpp
// Dead-simple priority-queue AI.  One SQL row per spec, 20 spells, 4 buckets.
//
// The waterfall every tick:
//   1. BUFFS       — cast on self if aura missing
//   2. DEFENSIVES  — cast on self if HP < 35%
//   3. ABILITIES   — role decides the target:
//                      healer  → lowest-HP ally under 90%
//                      dps     → master's enemy target
//                      tank    → master's enemy target
//   4. MOBILITY    — cast on self if out of preferred range
//
// One cast per tick.  First valid spell wins.  No branching spaghetti.

#include "BotAI.h"
#include "BotBehavior.h"
#include "RotationEngine.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Group.h"
#include "Log.h"
#include "Chat.h"
#include <cmath>

// ─── Constants ─────────────────────────────────────────────────────────────────
static constexpr uint32 AI_UPDATE_INTERVAL_MS = 1000;
static constexpr float  FOLLOW_DISTANCE       = 4.0f;
static constexpr float  FOLLOW_ANGLE          = float(M_PI);
static constexpr float  MAX_FOLLOW_DISTANCE   = 40.0f;
static constexpr float  COMBAT_CHASE_MELEE    = 5.0f;
static constexpr float  COMBAT_CHASE_RANGED   = 25.0f;
static constexpr float  HEAL_THRESHOLD_PCT    = 90.0f;
static constexpr float  DEFENSIVE_HP_PCT      = 35.0f;

// ─── Role Auto-Detection ───────────────────────────────────────────────────────

BotRole DetectBotRole(Player* bot)
{
    if (!bot) return BotRole::ROLE_MELEE_DPS;

    uint8 specIdx = bot->GetMostPointsTalentTree();
    const SpecRotation* rot = sRotationEngine.GetRotation(bot->getClass(), specIdx);
    if (rot) return rot->role;

    if (bot->HasTankSpec())   return BotRole::ROLE_TANK;
    if (bot->HasHealSpec())   return BotRole::ROLE_HEALER;
    if (bot->HasCasterSpec()) return BotRole::ROLE_RANGED_DPS;
    if (bot->HasMeleeSpec())  return BotRole::ROLE_MELEE_DPS;

    switch (bot->getClass())
    {
        case CLASS_WARLOCK: case CLASS_MAGE: case CLASS_PRIEST:
            return BotRole::ROLE_RANGED_DPS;
        default:
            return BotRole::ROLE_MELEE_DPS;
    }
}

uint8 DetectSpecIndex(Player* bot)
{
    return bot ? bot->GetMostPointsTalentTree() : 0;
}

// ─── Helpers ───────────────────────────────────────────────────────────────────

static float Dist2D(Unit* a, Unit* b)
{
    if (!a || !b || a->GetMapId() != b->GetMapId()) return 99999.f;
    float dx = a->GetPositionX() - b->GetPositionX();
    float dy = a->GetPositionY() - b->GetPositionY();
    return std::sqrt(dx * dx + dy * dy);
}

static void TeleportToMaster(Player* bot, Player* master)
{
    float ang = frand(0.f, 2.f * float(M_PI));
    float d   = frand(2.f, 4.f);
    float x = master->GetPositionX() + d * std::cos(ang);
    float y = master->GetPositionY() + d * std::sin(ang);
    float z = master->GetPositionZ();
    if (bot->GetMapId() != master->GetMapId())
        bot->TeleportTo(master->GetMapId(), x, y, z, master->GetOrientation());
    else
        bot->NearTeleportTo(x, y, z, master->GetOrientation());
}

// Find party member with lowest HP% (same map, alive)
static Player* FindLowestHP(Player* bot, Player* master)
{
    Group* grp = master->GetGroup();
    if (!grp) return nullptr;

    Player* lowest   = nullptr;
    float   lowPct   = 100.f;
    for (GroupReference* ref = grp->GetFirstMember(); ref; ref = ref->next())
    {
        Player* m = ref->GetSource();
        if (!m || !m->IsAlive() || !m->IsInWorld()) continue;
        if (m->GetMapId() != bot->GetMapId()) continue;
        float pct = m->GetHealthPct();
        if (pct < lowPct) { lowPct = pct; lowest = m; }
    }
    return lowest;
}

// ─── Try to cast one spell ─────────────────────────────────────────────────────
// Returns true if the spell was successfully cast.

static bool TryCast(Player* bot, Unit* target, uint32 spellId)
{
    if (spellId == 0)          return false;
    if (!target)               return false;
    if (!bot->HasSpell(spellId))       return false;
    if (bot->HasSpellCooldown(spellId)) return false;

    return bot->CastSpell(target, spellId, false) == SPELL_CAST_OK;
}

// ─── Bucket Runners ────────────────────────────────────────────────────────────
// Each bucket scans its 5 slots. First valid cast wins → return true.

// Warlock Metamorphosis spell ID (Demonology buff_1)
static constexpr uint32 WARLOCK_METAMORPHOSIS = 47241;
static constexpr float  META_MANA_THRESHOLD   = 80.0f;

// Buffs: cast on SELF if the aura is missing
static bool RunBuffs(Player* bot, const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (bot->HasAura(id))  continue;                 // already have it

        // Warlock Metamorphosis: only pop Meta when mana > 80%
        if (id == WARLOCK_METAMORPHOSIS)
        {
            if (bot->GetPower(POWER_MANA) * 100 / std::max(bot->GetMaxPower(POWER_MANA), 1u) < META_MANA_THRESHOLD)
                continue;
        }

        if (TryCast(bot, bot, id)) return true;
    }
    return false;
}

// Defensives: cast on SELF only when HP < threshold
static bool RunDefensives(Player* bot, const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    if (bot->GetHealthPct() >= DEFENSIVE_HP_PCT)
        return false; // not in danger, skip entire bucket

    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (TryCast(bot, bot, id)) return true;
    }
    return false;
}

// Abilities: target depends on role
//   healer  → lowest-HP party member below HEAL_THRESHOLD_PCT
//   others  → enemy (master's target)
static bool RunAbilities(Player* bot, Player* master, Unit* enemy,
                         BotRole role,
                         const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    if (role == BotRole::ROLE_HEALER)
    {
        Player* healTarget = FindLowestHP(bot, master);
        if (!healTarget || healTarget->GetHealthPct() >= HEAL_THRESHOLD_PCT)
            return false; // nobody needs healing

        for (uint32 id : spells)
        {
            if (id == 0) continue;
            if (TryCast(bot, healTarget, id)) return true;
        }
        return false;
    }

    // DPS / Tank: cast on enemy
    if (!enemy) return false;
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (TryCast(bot, enemy, id)) return true;
    }
    return false;
}

// DoTs: cast on ENEMY if the aura is missing on the target
static bool RunDots(Player* bot, Unit* enemy,
                    const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    if (!enemy) return false;
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (enemy->HasAura(id))  continue;               // already ticking
        if (TryCast(bot, enemy, id)) return true;
    }
    return false;
}

// HoTs: cast on lowest-HP ally if the aura is missing
static bool RunHots(Player* bot, Player* master,
                    const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    Player* target = FindLowestHP(bot, master);
    if (!target) return false;
    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (target->HasAura(id))  continue;              // already ticking
        if (TryCast(bot, target, id)) return true;
    }
    return false;
}

// Mobility: cast on SELF if we're out of preferred range of the enemy
static bool RunMobility(Player* bot, Unit* enemy, float preferredRange,
                        const std::array<uint32, SPELLS_PER_BUCKET>& spells)
{
    if (!enemy) return false;
    // Only trigger if we're significantly farther than preferred range
    float dist = Dist2D(bot, enemy);
    if (dist <= preferredRange + 5.f) return false; // close enough

    for (uint32 id : spells)
    {
        if (id == 0) continue;
        if (TryCast(bot, bot, id)) return true;
    }
    return false;
}

// ─── The Waterfall ─────────────────────────────────────────────────────────────
// One cast per tick.  Buffs → Defensives → Abilities → Mobility.

static void RunWaterfall(Player* bot, Player* master, Unit* enemy,
                         const SpecRotation* rot)
{
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return;

    // 1. Buffs — "Is my tax paid?"
    if (RunBuffs(bot, rot->buffs))
        return;

    // 2. Defensives — "Am I dying?"
    if (RunDefensives(bot, rot->defensives))
        return;

    // 3. DoTs — "Are my DoTs ticking?"
    if (RunDots(bot, enemy, rot->dots))
        return;

    // 4. HoTs — "Are my HoTs rolling?"
    if (RunHots(bot, master, rot->hots))
        return;

    // 5. Abilities — "What do I press?"
    if (RunAbilities(bot, master, enemy, rot->role, rot->abilities))
        return;

    // 6. Mobility — "Can I get in range?"
    RunMobility(bot, enemy, rot->preferredRange, rot->mobility);
}

// ─── Per-Bot Update ────────────────────────────────────────────────────────────

static void UpdateBotAI(BotInfo& info, Player* master)
{
    Player* bot = info.player;
    if (!bot || !bot->IsInWorld() || !bot->IsAlive()) return;
    if (!master || !master->IsInWorld()) return;

    const SpecRotation* rot = sRotationEngine.GetRotation(
        bot->getClass(), info.specIndex);

    // ── Resolve enemy target ───────────────────────────────────────────────
    Unit* enemy = master->GetVictim();
    if (!enemy) enemy = master->GetSelectedUnit();

    bool masterInCombat = master->IsInCombat();

    // ── Combat ─────────────────────────────────────────────────────────────
    if (masterInCombat && enemy && enemy->IsAlive() &&
        enemy->IsInWorld() && !enemy->IsPlayer())
    {
        if (!info.isInCombat || bot->GetVictim() != enemy)
        {
            info.isInCombat  = true;
            info.isFollowing = false;

            bool melee = (info.role == BotRole::ROLE_MELEE_DPS ||
                          info.role == BotRole::ROLE_TANK);
            bot->Attack(enemy, melee);

            float chase = melee ? COMBAT_CHASE_MELEE : COMBAT_CHASE_RANGED;
            if (rot && rot->preferredRange > 0)
                chase = rot->preferredRange;

            bot->GetMotionMaster()->Clear();
            bot->GetMotionMaster()->MoveChase(enemy, chase);
        }

        // Run the waterfall
        if (rot)
            RunWaterfall(bot, master, enemy, rot);

        return;
    }

    // ── Out of combat: keep buffs rolling ──────────────────────────────────
    if (rot && !bot->HasUnitState(UNIT_STATE_CASTING))
        RunBuffs(bot, rot->buffs);

    // ── Leave-combat transition ────────────────────────────────────────────
    if (info.isInCombat)
    {
        info.isInCombat = false;
        bot->AttackStop();
        bot->GetMotionMaster()->Clear();
    }

    // ── Follow master ──────────────────────────────────────────────────────
    float dist = Dist2D(bot, master);

    if (dist > MAX_FOLLOW_DISTANCE || bot->GetMapId() != master->GetMapId())
    {
        TeleportToMaster(bot, master);
        info.isFollowing = false;
        return;
    }

    if (!info.isFollowing)
    {
        info.isFollowing = true;
        bot->GetMotionMaster()->Clear();
        bot->GetMotionMaster()->MoveFollow(master, FOLLOW_DISTANCE, FOLLOW_ANGLE);
    }
}

// ─── World Script: tick loop ───────────────────────────────────────────────────
class BotAIWorldScript : public WorldScript
{
public:
    BotAIWorldScript() : WorldScript("BotAIWorldScript") {}

    void OnUpdate(uint32 diff) override
    {
        _timer += diff;
        if (_timer < AI_UPDATE_INTERVAL_MS) return;
        _timer = 0;

        auto& all = sBotMgr.GetAll();
        if (all.empty()) return;

        for (auto& [masterLow, bots] : all)
        {
            ObjectGuid mg = ObjectGuid::Create<HighGuid::Player>(masterLow);
            Player* master = ObjectAccessor::FindPlayer(mg);
            if (!master || !master->IsInWorld()) continue;

            for (auto& info : bots)
                UpdateBotAI(info, master);
        }
    }

private:
    uint32 _timer = 0;
};

void AddBotAI()
{
    new BotAIWorldScript();
}
