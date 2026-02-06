// RPGbotsCommands.cpp
// Adds commands to give a random temperament or psychology aura from the DB

#include "Chat.h"
#include "CommandScript.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "DatabaseEnv.h"
#include "RotationEngine.h"
#include <vector>
#include <random>

using namespace Acore::ChatCommands;

namespace
{
    // Helper to get a random aura_id from a table
    uint32 GetRandomAuraIdFromTable(const char* table)
    {
        QueryResult result = CharacterDatabase.Query("SELECT spell FROM {}", table);
        std::vector<uint32> auraIds;
        if (result)
        {
            do {
                auraIds.push_back((*result)[0].Get<uint32>());
            } while (result->NextRow());
        }
        if (auraIds.empty())
            return 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, auraIds.size() - 1);
        return auraIds[dis(gen)];
    }

    // Remove all auras from a list of aura_ids
    void RemoveAuras(Player* player, const char* table)
    {
        QueryResult result = CharacterDatabase.Query("SELECT spell FROM {}", table);
        if (result)
        {
            do {
                uint32 auraId = (*result)[0].Get<uint32>();
                player->RemoveAurasDueToSpell(auraId);
            } while (result->NextRow());
        }
    }
} // anonymous namespace

class RPGbotsCommands : public CommandScript
{
public:
    RPGbotsCommands() : CommandScript("RPGbotsCommands") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable rpgTable =
        {
            { "temperament", HandleRandomTemperamentCommand, SEC_PLAYER,     Console::No },
            { "psych",       HandleRandomPsychCommand,       SEC_PLAYER,     Console::No },
            { "reload",      HandleReloadRotationsCommand,   SEC_GAMEMASTER, Console::No },
            { "rotation",    HandleShowRotationCommand,      SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "rpg", rpgTable },
        };
        return commandTable;
    }

    static bool HandleRandomTemperamentCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;
        RemoveAuras(player, "rpgbots.rpg_temperaments");
        uint32 auraId = GetRandomAuraIdFromTable("rpgbots.rpg_temperaments");
        if (auraId)
        {
            player->AddAura(auraId, player);
            handler->PSendSysMessage("You have been given a new random temperament.");
        }
        else
        {
            handler->PSendSysMessage("No temperaments found in the database.");
        }
        return true;
    }

    static bool HandleRandomPsychCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;
        RemoveAuras(player, "rpgbots.rpg_psychology");
        uint32 auraId = GetRandomAuraIdFromTable("rpgbots.rpg_psychology");
        if (auraId)
        {
            player->AddAura(auraId, player);
            handler->PSendSysMessage("You have been given a new random psychology.");
        }
        else
        {
            handler->PSendSysMessage("No psychologies found in the database.");
        }
        return true;
    }

    // .rpg reload — hot-reload all rotation data from SQL without restart
    static bool HandleReloadRotationsCommand(ChatHandler* handler)
    {
        uint32 specs = sRotationEngine.LoadFromDB();
        handler->PSendSysMessage("|cff00ff00RPGBots: Reloaded {} spec(s) from bot_rotations.|r", specs);
        return true;
    }

    // .rpg rotation [class_id] [spec_index] — show what's loaded for a spec
    static bool HandleShowRotationCommand(ChatHandler* handler,
                                          Optional<uint8> classArg,
                                          Optional<uint8> specArg)
    {
        if (!classArg || !specArg)
        {
            handler->PSendSysMessage("|cff00ff00RPGBots: {} specs loaded.|r",
                sRotationEngine.GetSpecCount());
            handler->PSendSysMessage("Usage: .rpg rotation <class_id> <spec_index>");
            return true;
        }

        const SpecRotation* rot = sRotationEngine.GetRotation(*classArg, *specArg);
        if (!rot)
        {
            handler->PSendSysMessage("|cffff0000No rotation for class {} spec {}.|r",
                *classArg, *specArg);
            return true;
        }

        handler->PSendSysMessage("|cff00ff00=== {} ({}) — range {} yd ===|r",
            rot->specName, BotRoleName(rot->role), rot->preferredRange);

        auto showSlots = [&](const char* label, const std::array<uint32, SPELLS_PER_BUCKET>& arr)
        {
            std::string line = std::string(label) + ":";
            for (uint32 id : arr)
            {
                if (id == 0) continue;
                line += " " + std::to_string(id);
            }
            handler->PSendSysMessage("{}", line);
        };

        showSlots("Abilities",  rot->abilities);
        showSlots("Buffs",      rot->buffs);
        showSlots("Defensives", rot->defensives);
        showSlots("DoTs",       rot->dots);
        showSlots("HoTs",       rot->hots);
        showSlots("Mobility",   rot->mobility);

        return true;
    }
};

void AddRPGbotsCommands()
{
    new RPGbotsCommands();
}
