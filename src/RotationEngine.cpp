// RotationEngine.cpp
// Loads the flat rpgbots.bot_rotations table at startup and on `.rpg reload`.

#include "RotationEngine.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "Log.h"

// ─── String → Enum ─────────────────────────────────────────────────────────────

static BotRole RoleFromString(std::string_view s)
{
    if (s == "tank")       return BotRole::ROLE_TANK;
    if (s == "healer")     return BotRole::ROLE_HEALER;
    if (s == "melee_dps")  return BotRole::ROLE_MELEE_DPS;
    if (s == "ranged_dps") return BotRole::ROLE_RANGED_DPS;
    return BotRole::ROLE_MELEE_DPS;
}

// ─── Load From DB ──────────────────────────────────────────────────────────────

uint32 RotationEngine::LoadFromDB()
{
    _rotations.clear();

    //  SELECT mirrors the column order in the CREATE TABLE
    QueryResult result = CharacterDatabase.Query(
        "SELECT class_id, spec_index, spec_name, role, preferred_range, "
        "       ability_1, ability_2, ability_3, ability_4, ability_5, "
        "       buff_1, buff_2, buff_3, buff_4, buff_5, "
        "       defensive_1, defensive_2, defensive_3, defensive_4, defensive_5, "
        "       dot_1, dot_2, dot_3, dot_4, dot_5, "
        "       hot_1, hot_2, hot_3, hot_4, hot_5, "
        "       mobility_1, mobility_2, mobility_3, mobility_4, mobility_5 "
        "FROM rpgbots.bot_rotations");

    if (!result)
    {
        LOG_WARN("module", "RPGBots RotationEngine: rpgbots.bot_rotations is empty — "
                           "bots will auto-attack only.");
        return 0;
    }

    do {
        Field* f = result->Fetch();
        SpecRotation rot;
        rot.classId        = f[0].Get<uint8>();
        rot.specIndex      = f[1].Get<uint8>();
        rot.specName       = f[2].Get<std::string>();
        rot.role           = RoleFromString(f[3].Get<std::string>());
        rot.preferredRange = f[4].Get<float>();

        // 5 abilities  (columns 5-9)
        for (uint8 i = 0; i < SPELLS_PER_BUCKET; ++i)
            rot.abilities[i] = f[5 + i].Get<uint32>();

        // 5 buffs      (columns 10-14)
        for (uint8 i = 0; i < SPELLS_PER_BUCKET; ++i)
            rot.buffs[i] = f[10 + i].Get<uint32>();

        // 5 defensives (columns 15-19)
        for (uint8 i = 0; i < SPELLS_PER_BUCKET; ++i)
            rot.defensives[i] = f[15 + i].Get<uint32>();

        // 5 dots       (columns 20-24)
        for (uint8 i = 0; i < SPELLS_PER_BUCKET; ++i)
            rot.dots[i] = f[20 + i].Get<uint32>();

        // 5 hots       (columns 25-29)
        for (uint8 i = 0; i < SPELLS_PER_BUCKET; ++i)
            rot.hots[i] = f[25 + i].Get<uint32>();

        // 5 mobility   (columns 30-34)
        for (uint8 i = 0; i < SPELLS_PER_BUCKET; ++i)
            rot.mobility[i] = f[30 + i].Get<uint32>();

        SpecKey key = MakeSpecKey(rot.classId, rot.specIndex);
        _rotations[key] = std::move(rot);

    } while (result->NextRow());

    LOG_INFO("module", "RPGBots RotationEngine: Loaded {} specs from bot_rotations",
             _rotations.size());

    return uint32(_rotations.size());
}

// ─── World Script: load at startup ─────────────────────────────────────────────
class RotationEngineWorldScript : public WorldScript
{
public:
    RotationEngineWorldScript() : WorldScript("RotationEngineWorldScript") {}

    void OnStartup() override
    {
        uint32 count = sRotationEngine.LoadFromDB();
        if (count == 0)
            LOG_WARN("module", "RPGBots RotationEngine: No specs loaded — "
                               "bots will auto-attack only.");
    }
};

void AddRotationEngine()
{
    new RotationEngineWorldScript();
}
