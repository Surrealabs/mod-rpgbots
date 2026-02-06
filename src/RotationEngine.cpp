// RotationEngine.cpp
// Loads rotation data from rpgbots.bot_rotation_specs and
// rpgbots.bot_rotation_entries at server startup (and on `.rpg reload`).
// Provides the four-bucket waterfall to BotAI.

#include "RotationEngine.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"

// ─── String → Enum Helpers ─────────────────────────────────────────────────────

static BotRole RoleFromString(std::string_view s)
{
    if (s == "tank")       return BotRole::ROLE_TANK;
    if (s == "healer")     return BotRole::ROLE_HEALER;
    if (s == "melee_dps")  return BotRole::ROLE_MELEE_DPS;
    if (s == "ranged_dps") return BotRole::ROLE_RANGED_DPS;
    return BotRole::ROLE_MELEE_DPS;
}

static RotationCategory CategoryFromString(std::string_view s)
{
    if (s == "maintenance") return RotationCategory::MAINTENANCE;
    if (s == "defensive")   return RotationCategory::DEFENSIVE;
    if (s == "rotation")    return RotationCategory::ROTATION;
    if (s == "utility")     return RotationCategory::UTILITY;
    return RotationCategory::ROTATION;
}

static RotationTarget TargetFromString(std::string_view s)
{
    if (s == "enemy")              return RotationTarget::ENEMY;
    if (s == "self")               return RotationTarget::SELF;
    if (s == "friendly_lowest_hp") return RotationTarget::FRIENDLY_LOWEST_HP;
    if (s == "friendly_tank")      return RotationTarget::FRIENDLY_TANK;
    if (s == "pet")                return RotationTarget::PET;
    return RotationTarget::ENEMY;
}

static RotationCondition ConditionFromString(std::string_view s)
{
    if (s == "none")                return RotationCondition::NONE;
    if (s == "health_below")       return RotationCondition::HEALTH_BELOW;
    if (s == "health_above")       return RotationCondition::HEALTH_ABOVE;
    if (s == "mana_below")         return RotationCondition::MANA_BELOW;
    if (s == "mana_above")         return RotationCondition::MANA_ABOVE;
    if (s == "target_health_below") return RotationCondition::TARGET_HEALTH_BELOW;
    if (s == "target_health_above") return RotationCondition::TARGET_HEALTH_ABOVE;
    if (s == "has_aura")           return RotationCondition::HAS_AURA;
    if (s == "missing_aura")       return RotationCondition::MISSING_AURA;
    if (s == "target_has_aura")    return RotationCondition::TARGET_HAS_AURA;
    if (s == "target_missing_aura") return RotationCondition::TARGET_MISSING_AURA;
    if (s == "in_melee_range")     return RotationCondition::IN_MELEE_RANGE;
    if (s == "not_in_melee_range") return RotationCondition::NOT_IN_MELEE_RANGE;
    return RotationCondition::NONE;
}

// ─── Load From Database ────────────────────────────────────────────────────────

uint32 RotationEngine::LoadFromDB()
{
    _rotations.clear();
    _totalEntries = 0;

    // ── 1. Load spec metadata ──────────────────────────────────────────────
    QueryResult specResult = CharacterDatabase.Query(
        "SELECT class_id, spec_index, spec_name, role, preferred_range, description "
        "FROM rpgbots.bot_rotation_specs");

    if (!specResult)
    {
        LOG_WARN("module", "RPGBots RotationEngine: No rows in bot_rotation_specs — "
                           "bots will have no rotations.");
        return 0;
    }

    do {
        Field* f = specResult->Fetch();
        uint8       classId    = f[0].Get<uint8>();
        uint8       specIndex  = f[1].Get<uint8>();
        std::string specName   = f[2].Get<std::string>();
        std::string roleStr    = f[3].Get<std::string>();
        float       range      = f[4].Get<float>();
        std::string desc       = f[5].Get<std::string>();

        SpecKey key = MakeSpecKey(classId, specIndex);
        SpecRotation& rot = _rotations[key];
        rot.classId       = classId;
        rot.specIndex     = specIndex;
        rot.specName      = specName;
        rot.role          = RoleFromString(roleStr);
        rot.preferredRange = range;
        rot.description   = desc;
    } while (specResult->NextRow());

    // ── 2. Load rotation entries ───────────────────────────────────────────
    QueryResult entryResult = CharacterDatabase.Query(
        "SELECT class_id, spec_index, category, priority_order, spell_id, spell_name, "
        "       target_type, condition_type, condition_value "
        "FROM rpgbots.bot_rotation_entries "
        "WHERE enabled = 1 "
        "ORDER BY class_id, spec_index, category, priority_order");

    if (!entryResult)
    {
        LOG_WARN("module", "RPGBots RotationEngine: No entries in bot_rotation_entries.");
        return static_cast<uint32>(_rotations.size());
    }

    do {
        Field* f = entryResult->Fetch();
        uint8       classId    = f[0].Get<uint8>();
        uint8       specIndex  = f[1].Get<uint8>();
        std::string catStr     = f[2].Get<std::string>();
        uint16      priority   = f[3].Get<uint16>();
        uint32      spellId    = f[4].Get<uint32>();
        std::string spellName  = f[5].Get<std::string>();
        std::string targetStr  = f[6].Get<std::string>();
        std::string condStr    = f[7].Get<std::string>();
        int32       condVal    = f[8].Get<int32>();

        SpecKey key = MakeSpecKey(classId, specIndex);
        auto it = _rotations.find(key);
        if (it == _rotations.end())
        {
            // Entry references a spec that wasn't in bot_rotation_specs — skip
            LOG_WARN("module", "RPGBots RotationEngine: Entry for class {} spec {} "
                               "has no spec metadata — skipping.", classId, specIndex);
            continue;
        }

        RotationEntry entry;
        entry.spellId       = spellId;
        entry.spellName     = spellName;
        entry.category      = CategoryFromString(catStr);
        entry.priorityOrder = priority;
        entry.targetType    = TargetFromString(targetStr);
        entry.conditionType = ConditionFromString(condStr);
        entry.conditionValue = condVal;

        // File into the right bucket
        switch (entry.category)
        {
            case RotationCategory::MAINTENANCE: it->second.maintenance.push_back(entry); break;
            case RotationCategory::DEFENSIVE:   it->second.defensive.push_back(entry);   break;
            case RotationCategory::ROTATION:    it->second.rotation.push_back(entry);    break;
            case RotationCategory::UTILITY:     it->second.utility.push_back(entry);     break;
        }
        ++_totalEntries;

    } while (entryResult->NextRow());

    LOG_INFO("module", "RPGBots RotationEngine: Loaded {} specs, {} total entries",
             _rotations.size(), _totalEntries);

    return static_cast<uint32>(_rotations.size());
}

// ─── World Script: load rotations on server startup ────────────────────────────
class RotationEngineWorldScript : public WorldScript
{
public:
    RotationEngineWorldScript() : WorldScript("RotationEngineWorldScript") {}

    void OnStartup() override
    {
        uint32 count = sRotationEngine.LoadFromDB();
        if (count == 0)
            LOG_WARN("module", "RPGBots RotationEngine: No rotation specs loaded. "
                               "Bots will auto-attack only.");
    }
};

void AddRotationEngine()
{
    new RotationEngineWorldScript();
}
