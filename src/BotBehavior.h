// BotBehavior.h
// Base interface for class-specific bot behavior profiles.
// Each class (Warlock, Paladin, etc.) implements its own BotClassProfile
// in a dedicated .cpp file, keeping behavior logic per-class.

#pragma once

#include "Player.h"
#include "SharedDefines.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// ─── Role Tags ─────────────────────────────────────────────────────────────────
// A bot's current role determines its priorities in combat, positioning, and
// target selection. A single class can support multiple roles (e.g. Paladin
// can tank, heal, or melee DPS depending on spec).
enum class BotRole : uint8
{
    ROLE_TANK       = 0,  // Hold threat, position boss, use defensives
    ROLE_HEALER     = 1,  // Keep party alive, manage mana, triage
    ROLE_MELEE_DPS  = 2,  // Stick to target, maximize melee damage
    ROLE_RANGED_DPS = 3,  // Maintain range, maximize spell/ranged damage
};

inline const char* BotRoleName(BotRole role)
{
    switch (role)
    {
        case BotRole::ROLE_TANK:       return "Tank";
        case BotRole::ROLE_HEALER:     return "Healer";
        case BotRole::ROLE_MELEE_DPS:  return "Melee DPS";
        case BotRole::ROLE_RANGED_DPS: return "Ranged DPS";
        default:                       return "Unknown";
    }
}

// ─── Spec Profile ──────────────────────────────────────────────────────────────
// Defines a talent specialization within a class: its name, role, key spell
// IDs, and behavior configuration.
struct BotSpecProfile
{
    std::string specName;       // e.g. "Affliction", "Protection", "Holy"
    BotRole     role;           // What role this spec fulfills

    // Priority-ordered spell IDs the bot should attempt to use.
    // The bot AI will iterate this list and cast the first available/ready spell.
    std::vector<uint32> spellPriority;

    // Spell IDs to keep active on self (buffs, stances, auras)
    std::vector<uint32> selfBuffs;

    // Spell IDs to use on party members (heals, buffs)
    std::vector<uint32> partyBuffs;

    // Preferred combat range in yards (0 = melee)
    float preferredRange = 0.0f;

    // Description of this spec's AI behavior
    std::string behaviorDescription;
};

// ─── Class Profile ─────────────────────────────────────────────────────────────
// Each class .cpp file creates one of these and registers it.
// Contains all specs the class supports and utility methods.
struct BotClassProfile
{
    uint8       classId;        // CLASS_WARLOCK, CLASS_PALADIN, etc.
    std::string className;      // Human-readable name
    std::vector<BotSpecProfile> specs;

    // Get the spec profile for a given spec index (0, 1, 2)
    const BotSpecProfile* GetSpec(uint8 specIndex) const
    {
        if (specIndex < specs.size())
            return &specs[specIndex];
        return nullptr;
    }

    // Get the first spec that matches a given role
    const BotSpecProfile* GetSpecForRole(BotRole role) const
    {
        for (auto& spec : specs)
            if (spec.role == role)
                return &spec;
        return nullptr;
    }
};

// ─── Global Registry ───────────────────────────────────────────────────────────
// Class profiles register themselves here at startup. The bot AI looks up
// a player's class to find the right behavior profile.
class BotProfileRegistry
{
public:
    static BotProfileRegistry& Instance()
    {
        static BotProfileRegistry instance;
        return instance;
    }

    void Register(const BotClassProfile& profile)
    {
        _profiles[profile.classId] = profile;
    }

    const BotClassProfile* GetProfile(uint8 classId) const
    {
        auto it = _profiles.find(classId);
        if (it != _profiles.end())
            return &it->second;
        return nullptr;
    }

    const std::unordered_map<uint8, BotClassProfile>& GetAll() const
    {
        return _profiles;
    }

private:
    BotProfileRegistry() = default;
    std::unordered_map<uint8, BotClassProfile> _profiles;
};

#define sBotProfiles BotProfileRegistry::Instance()
