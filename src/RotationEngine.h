// RotationEngine.h
// Data-driven rotation system. All rotation logic comes from SQL tables,
// not hardcoded C++. Server owners can edit rotations and `.rpg reload`
// without recompiling.
//
// Tables:
//   rpgbots.bot_rotation_specs   — one row per class/spec (metadata + role)
//   rpgbots.bot_rotation_entries — priority-ordered spell list per spec

#pragma once

#include "BotBehavior.h"
#include "Player.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

// ─── Spell Entry Category ──────────────────────────────────────────────────────
enum class RotationCategory : uint8
{
    MAINTENANCE = 0,   // Buffs/DoTs to keep rolling
    DEFENSIVE   = 1,   // Emergency / health/mana triggers
    ROTATION    = 2,   // Core DPS/heal rotation
    UTILITY     = 3,   // CC, dispels, misc
};

// ─── Target Type ───────────────────────────────────────────────────────────────
enum class RotationTarget : uint8
{
    ENEMY              = 0,
    SELF               = 1,
    FRIENDLY_LOWEST_HP = 2,
    FRIENDLY_TANK      = 3,
    PET                = 4,
};

// ─── Condition Type ────────────────────────────────────────────────────────────
enum class RotationCondition : uint8
{
    NONE                = 0,
    HEALTH_BELOW        = 1,
    HEALTH_ABOVE        = 2,
    MANA_BELOW          = 3,
    MANA_ABOVE          = 4,
    TARGET_HEALTH_BELOW = 5,
    TARGET_HEALTH_ABOVE = 6,
    HAS_AURA            = 7,
    MISSING_AURA        = 8,
    TARGET_HAS_AURA     = 9,
    TARGET_MISSING_AURA = 10,
    IN_MELEE_RANGE      = 11,
    NOT_IN_MELEE_RANGE  = 12,
};

// ─── Single Rotation Entry ─────────────────────────────────────────────────────
struct RotationEntry
{
    uint32            spellId;
    std::string       spellName;
    RotationCategory  category;
    uint16            priorityOrder;
    RotationTarget    targetType;
    RotationCondition conditionType;
    int32             conditionValue;
};

// ─── Spec Rotation Data (loaded from SQL) ──────────────────────────────────────
struct SpecRotation
{
    uint8       classId;
    uint8       specIndex;
    std::string specName;
    BotRole     role;
    float       preferredRange;
    std::string description;

    // Entries sorted by category, then priority_order
    std::vector<RotationEntry> maintenance;
    std::vector<RotationEntry> defensive;
    std::vector<RotationEntry> rotation;
    std::vector<RotationEntry> utility;

    // Get all entries for a given category
    const std::vector<RotationEntry>& GetEntries(RotationCategory cat) const
    {
        switch (cat)
        {
            case RotationCategory::MAINTENANCE: return maintenance;
            case RotationCategory::DEFENSIVE:   return defensive;
            case RotationCategory::ROTATION:     return rotation;
            case RotationCategory::UTILITY:      return utility;
        }
        return rotation; // fallback
    }
};

// Key: (classId << 8) | specIndex
using SpecKey = uint16;
inline SpecKey MakeSpecKey(uint8 classId, uint8 specIndex) { return (uint16(classId) << 8) | specIndex; }

// ─── Rotation Engine Singleton ─────────────────────────────────────────────────
class RotationEngine
{
public:
    static RotationEngine& Instance()
    {
        static RotationEngine instance;
        return instance;
    }

    // Load or reload all rotation data from rpgbots database
    uint32 LoadFromDB();

    // Get the rotation for a specific class/spec (returns nullptr if not configured)
    const SpecRotation* GetRotation(uint8 classId, uint8 specIndex) const
    {
        auto it = _rotations.find(MakeSpecKey(classId, specIndex));
        if (it != _rotations.end())
            return &it->second;
        return nullptr;
    }

    // Check if any rotation data is loaded
    bool HasRotations() const { return !_rotations.empty(); }

    // Get count of loaded specs
    uint32 GetSpecCount() const { return static_cast<uint32>(_rotations.size()); }

    // Get total entry count across all specs
    uint32 GetEntryCount() const { return _totalEntries; }

private:
    RotationEngine() = default;

    std::unordered_map<SpecKey, SpecRotation> _rotations;
    uint32 _totalEntries = 0;
};

#define sRotationEngine RotationEngine::Instance()

// Registration
void AddRotationEngine();
