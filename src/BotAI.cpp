// BotAI.cpp
// Priority-Queue AI: We don't script fights; we give the bot a list of
// priorities divided into Maintenance, Defense, Offense, and Utility.
// The bot simply "fills the gaps" by casting the highest-priority spell
// available in its table.
//
// The four-bucket waterfall runs every AI tick:
//   1. MAINTENANCE — "Is my tax paid?" (buffs, DoTs)
//   2. DEFENSIVE   — "Am I dying?"     (emergency CDs, self-heals)
//   3. ROTATION    — "What do I press?" (core DPS/heal priority list)
//   4. UTILITY     — "Where should I be?" (gap closers, CC, dispels)
//
// All spell data comes from rpgbots.bot_rotation_entries (SQL).
// No recompile needed to change rotations — `.rpg reload` in-game.

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
#include "Pet.h"
#include <cmath>

// ─── Constants ─────────────────────────────────────────────────────────────────
static constexpr uint32  AI_UPDATE_INTERVAL_MS = 1000;
static constexpr float   FOLLOW_DISTANCE       = 4.0f;
static constexpr float   FOLLOW_ANGLE          = float(M_PI);
static constexpr float   MAX_FOLLOW_DISTANCE   = 40.0f;
static constexpr float   COMBAT_CHASE_DIST     = 5.0f;
static constexpr float   RANGED_STOP_DIST      = 25.0f;
static constexpr float   BOT_MELEE_RANGE       = 8.0f;

// ─── Role Auto-Detection ───────────────────────────────────────────────────────
// If the rotation engine has metadata for this class/spec, use its role.
// Otherwise fall back to AzerothCore's built-in spec detection.

BotRole DetectBotRole(Player* bot)
{
    if (!bot)
        return BotRole::ROLE_MELEE_DPS;

    // Prefer SQL-defined role
    uint8 specIdx = bot->GetMostPointsTalentTree();
    const SpecRotation* rot = sRotationEngine.GetRotation(bot->getClass(), specIdx);
    if (rot)
        return rot->role;

    // Fallback: core helpers
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
    if (!bot) return 0;
    return bot->GetMostPointsTalentTree();
}

// ─── Geometry Helpers ──────────────────────────────────────────────────────────

static float GetDistance2D(Player* a, Unit* b)
{
    if (!a || !b || a->GetMapId() != b->GetMapId())
        return 99999.0f;
    float dx = a->GetPositionX() - b->GetPositionX();
    float dy = a->GetPositionY() - b->GetPositionY();
    return std::sqrt(dx * dx + dy * dy);
}

static void TeleportBotToMaster(Player* bot, Player* master)
{
    if (!bot || !master) return;

    float angle = frand(0.0f, 2.0f * float(M_PI));
    float dist  = frand(2.0f, 4.0f);
    float x = master->GetPositionX() + dist * std::cos(angle);
    float y = master->GetPositionY() + dist * std::sin(angle);
    float z = master->GetPositionZ();

    if (bot->GetMapId() != master->GetMapId())
        bot->TeleportTo(master->GetMapId(), x, y, z, master->GetOrientation());
    else
        bot->NearTeleportTo(x, y, z, master->GetOrientation());
}

// ─── Party Helpers ─────────────────────────────────────────────────────────────

static Player* FindLowestHPMember(Player* bot, Player* master)
{
    Group* group = master->GetGroup();
    if (!group) return nullptr;

    Player* lowest = nullptr;
    float lowestPct = 100.0f;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive() || !member->IsInWorld())
            continue;
        if (member->GetMapId() != bot->GetMapId())
            continue;
        float pct = member->GetHealthPct();
        if (pct < lowestPct)
        {
            lowestPct = pct;
            lowest = member;
        }
    }
    return lowest;
}

static Player* FindPartyTank(Player* bot, Player* master)
{
    Group* group = master->GetGroup();
    if (!group) return master;

    for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
    {
        Player* member = ref->GetSource();
        if (!member || !member->IsAlive()) continue;

        BotInfo* info = sBotMgr.FindBot(member->GetGUID());
        if (info && info->role == BotRole::ROLE_TANK)
            return member;
    }
    return master;
}

// ─── Condition Evaluation ──────────────────────────────────────────────────────
// The heart of the data-driven system.  Each RotationEntry has a
// condition_type + condition_value.  Returns true when the condition is met.

static bool EvaluateCondition(Player* bot, Unit* target,
                              RotationCondition cond, int32 condValue)
{
    switch (cond)
    {
        case RotationCondition::NONE:
            return true;

        case RotationCondition::HEALTH_BELOW:
            return bot->GetHealthPct() < float(condValue);

        case RotationCondition::HEALTH_ABOVE:
            return bot->GetHealthPct() > float(condValue);

        case RotationCondition::MANA_BELOW:
            return bot->GetPower(POWER_MANA) * 100 /
                   std::max(bot->GetMaxPower(POWER_MANA), 1u) < uint32(condValue);

        case RotationCondition::MANA_ABOVE:
            return bot->GetPower(POWER_MANA) * 100 /
                   std::max(bot->GetMaxPower(POWER_MANA), 1u) > uint32(condValue);

        case RotationCondition::TARGET_HEALTH_BELOW:
            return target && target->GetHealthPct() < float(condValue);

        case RotationCondition::TARGET_HEALTH_ABOVE:
            return target && target->GetHealthPct() > float(condValue);

        case RotationCondition::HAS_AURA:
            return bot->HasAura(uint32(condValue));

        case RotationCondition::MISSING_AURA:
            return !bot->HasAura(uint32(condValue));

        case RotationCondition::TARGET_HAS_AURA:
            return target && target->HasAura(uint32(condValue));

        case RotationCondition::TARGET_MISSING_AURA:
            return target && !target->HasAura(uint32(condValue));

        case RotationCondition::IN_MELEE_RANGE:
            return target && GetDistance2D(bot, target) <= BOT_MELEE_RANGE;

        case RotationCondition::NOT_IN_MELEE_RANGE:
            return target && GetDistance2D(bot, target) > BOT_MELEE_RANGE;
    }
    return true;
}

// ─── Resolve Target ────────────────────────────────────────────────────────────
// Translates a RotationEntry's target_type enum into an actual Unit*.

static Unit* ResolveTarget(Player* bot, Player* master, Unit* enemy,
                           RotationTarget targetType)
{
    switch (targetType)
    {
        case RotationTarget::ENEMY:
            return enemy;

        case RotationTarget::SELF:
            return bot;

        case RotationTarget::FRIENDLY_LOWEST_HP:
            return FindLowestHPMember(bot, master);

        case RotationTarget::FRIENDLY_TANK:
            return FindPartyTank(bot, master);

        case RotationTarget::PET:
        {
            Pet* pet = bot->GetPet();
            return pet ? static_cast<Unit*>(pet) : static_cast<Unit*>(bot);
        }
    }
    return enemy;
}

// ─── Try One Entry ─────────────────────────────────────────────────────────────
// Checks: spell known? off cooldown? condition met? target valid?
// If all pass → cast → return true.

static bool TryEntry(Player* bot, Player* master, Unit* enemy,
                     const RotationEntry& entry)
{
    if (!bot->HasSpell(entry.spellId))
        return false;

    if (bot->HasSpellCooldown(entry.spellId))
        return false;

    Unit* target = ResolveTarget(bot, master, enemy, entry.targetType);
    if (!target)
        return false;

    if (!EvaluateCondition(bot, target, entry.conditionType, entry.conditionValue))
        return false;

    SpellCastResult result = bot->CastSpell(target, entry.spellId, false);
    return result == SPELL_CAST_OK;
}

// ─── Run One Bucket ────────────────────────────────────────────────────────────
// Scans top-to-bottom, returns true on first successful cast.

static bool RunBucket(Player* bot, Player* master, Unit* enemy,
                      const std::vector<RotationEntry>& entries)
{
    for (const auto& entry : entries)
    {
        if (TryEntry(bot, master, enemy, entry))
            return true;
    }
    return false;
}

// ─── The Waterfall ─────────────────────────────────────────────────────────────
//   1. Maintenance — buffs/dots: "Is my tax paid?"
//   2. Defensive   — emergencies: "Am I dying?"
//   3. Rotation    — core spells: "What do I press?"
//   4. Utility     — CC/misc:     "Anything else useful?"
//
// One cast per tick.  If bucket 1 fires, we skip the rest.

static void RunWaterfall(Player* bot, Player* master, Unit* enemy,
                         const SpecRotation* rot)
{
    if (bot->HasUnitState(UNIT_STATE_CASTING))
        return;

    if (RunBucket(bot, master, enemy, rot->maintenance))
        return;

    if (RunBucket(bot, master, enemy, rot->defensive))
        return;

    if (RunBucket(bot, master, enemy, rot->rotation))
        return;

    RunBucket(bot, master, enemy, rot->utility);
}

// ─── Per-Bot AI Update ─────────────────────────────────────────────────────────

static void UpdateBotAI(BotInfo& info, Player* master)
{
    Player* bot = info.player;
    if (!bot || !bot->IsInWorld() || !bot->IsAlive())
        return;
    if (!master || !master->IsInWorld())
        return;

    // SQL rotation for this bot's class/spec
    const SpecRotation* rot = sRotationEngine.GetRotation(
        bot->getClass(), info.specIndex);

    // ── Combat ─────────────────────────────────────────────────────────────
    Unit* masterTarget = master->GetVictim();
    if (!masterTarget)
        masterTarget = master->GetSelectedUnit();

    bool masterInCombat = master->IsInCombat();

    if (masterInCombat && masterTarget && masterTarget->IsAlive() &&
        masterTarget->IsInWorld() && !masterTarget->IsPlayer())
    {
        // Engage
        if (!info.isInCombat || bot->GetVictim() != masterTarget)
        {
            info.isInCombat  = true;
            info.isFollowing = false;

            bool melee = (info.role == BotRole::ROLE_MELEE_DPS ||
                          info.role == BotRole::ROLE_TANK);

            bot->Attack(masterTarget, melee);

            float chaseDist = melee ? COMBAT_CHASE_DIST : RANGED_STOP_DIST;
            if (rot && rot->preferredRange > 0)
                chaseDist = rot->preferredRange;

            bot->GetMotionMaster()->Clear();
            bot->GetMotionMaster()->MoveChase(masterTarget, chaseDist);
        }

        // Run the waterfall
        if (rot)
            RunWaterfall(bot, master, masterTarget, rot);

        return;
    }

    // ── Out-of-combat: still run maintenance for self-buffs ────────────────
    if (rot && !bot->HasUnitState(UNIT_STATE_CASTING))
        RunBucket(bot, master, nullptr, rot->maintenance);

    // ── Leave-combat transition ────────────────────────────────────────────
    if (info.isInCombat)
    {
        info.isInCombat = false;
        bot->AttackStop();
        bot->GetMotionMaster()->Clear();
    }

    // ── Follow master ──────────────────────────────────────────────────────
    float dist = GetDistance2D(bot, master);

    if (dist > MAX_FOLLOW_DISTANCE || bot->GetMapId() != master->GetMapId())
    {
        TeleportBotToMaster(bot, master);
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

// ─── World Script: AI Update Loop ─────────────────────────────────────────────
class BotAIWorldScript : public WorldScript
{
public:
    BotAIWorldScript() : WorldScript("BotAIWorldScript") {}

    void OnUpdate(uint32 diff) override
    {
        _timer += diff;
        if (_timer < AI_UPDATE_INTERVAL_MS)
            return;
        _timer = 0;

        auto& allBots = sBotMgr.GetAll();
        if (allBots.empty())
            return;

        for (auto& [masterLow, bots] : allBots)
        {
            ObjectGuid masterGuid = ObjectGuid::Create<HighGuid::Player>(masterLow);
            Player* master = ObjectAccessor::FindPlayer(masterGuid);
            if (!master || !master->IsInWorld())
                continue;

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
