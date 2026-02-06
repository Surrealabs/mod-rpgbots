// BotWarlock.cpp
// Class behavior profile for Warlock (CLASS_WARLOCK = 9)
// Defines specs: Affliction (Ranged DPS), Demonology (Ranged DPS), Destruction (Ranged DPS)
//
// WotLK 3.3.5a Warlock spell IDs — real spell IDs from the game.

#include "BotBehavior.h"
#include "ScriptMgr.h"
#include "Log.h"

namespace
{

// ─── Warlock Spell IDs (Max rank at 80) ────────────────────────────────────────

// DoTs
constexpr uint32 CORRUPTION             = 47813;  // Rank 10
constexpr uint32 CURSE_OF_AGONY         = 47864;  // Rank 9
constexpr uint32 CURSE_OF_THE_ELEMENTS  = 47865;  // Rank 5
constexpr uint32 CURSE_OF_DOOM          = 47867;  // Rank 3
constexpr uint32 IMMOLATE               = 47811;  // Rank 11
constexpr uint32 UNSTABLE_AFFLICTION    = 47843;  // Rank 5 (Affliction talent)
constexpr uint32 SEED_OF_CORRUPTION     = 47836;  // Rank 3 (AoE)

// Direct Damage
constexpr uint32 SHADOW_BOLT            = 47809;  // Rank 13
constexpr uint32 INCINERATE             = 47838;  // Rank 4
constexpr uint32 CHAOS_BOLT             = 59172;  // Rank 4 (Destruction talent)
constexpr uint32 SOUL_FIRE              = 47825;  // Rank 6
constexpr uint32 SHADOWBURN             = 47827;  // Rank 10 (execute-ish)
constexpr uint32 DRAIN_SOUL             = 47855;  // Rank 6 (execute / shard gen)
constexpr uint32 DRAIN_LIFE             = 47857;  // Rank 9

// Channeled / Procs
constexpr uint32 HAUNT                  = 59164;  // Rank 4 (Affliction talent)
constexpr uint32 CONFLAGRATE            = 17962;  // Rank 7 (Destruction talent)

// AoE
constexpr uint32 RAIN_OF_FIRE           = 47820;  // Rank 7
constexpr uint32 HELLFIRE               = 47823;  // Rank 5
constexpr uint32 HOWL_OF_TERROR         = 17928;  // Rank 2 (AoE fear)

// Buffs / Utility
constexpr uint32 LIFE_TAP               = 57946;  // Rank 8 (mana from HP)
constexpr uint32 DARK_PACT              = 59092;  // Rank 5 (mana from pet)
constexpr uint32 FEL_ARMOR              = 47893;  // Rank 4
constexpr uint32 DEMON_ARMOR            = 47889;  // Rank 9
constexpr uint32 DEMONIC_EMPOWERMENT    = 47193;  // (Demonology talent)
constexpr uint32 METAMORPHOSIS          = 47241;  // (Demonology talent — big CD)
constexpr uint32 IMMOLATION_AURA        = 50589;  // Metamorphosis ability

// Pet Summons
constexpr uint32 SUMMON_IMP             = 688;
constexpr uint32 SUMMON_VOIDWALKER      = 697;
constexpr uint32 SUMMON_SUCCUBUS        = 712;
constexpr uint32 SUMMON_FELHUNTER       = 691;
constexpr uint32 SUMMON_FELGUARD        = 30146;  // (Demonology talent)

// Crowd Control / Defensives
constexpr uint32 FEAR                   = 6215;   // Rank 3
constexpr uint32 DEATH_COIL_WARLOCK     = 47860;  // Rank 4 (horror + self-heal)
constexpr uint32 SHADOWFURY             = 47847;  // Rank 5 (AoE stun, Destruction)
constexpr uint32 SOUL_SHATTER           = 29858;  // Threat drop
constexpr uint32 SOULSTONE              = 47884;  // Rank 7 (battle rez)

// ─── Spec Definitions ──────────────────────────────────────────────────────────

BotClassProfile BuildWarlockProfile()
{
    BotClassProfile profile;
    profile.classId  = CLASS_WARLOCK;
    profile.className = "Warlock";

    // ── Affliction (Ranged DPS) ────────────────────────────────────────────
    {
        BotSpecProfile spec;
        spec.specName = "Affliction";
        spec.role     = BotRole::ROLE_RANGED_DPS;
        spec.behaviorDescription =
            "Maintain all DoTs on the primary target. Apply Haunt on cooldown "
            "for the damage amplification window, then refresh DoTs during it. "
            "Priority: Haunt > Corruption > Unstable Affliction > Curse of "
            "Agony > Shadow Bolt filler. Use Drain Soul below 25% HP for "
            "execute damage and soul shard generation. Life Tap to maintain "
            "mana. Use Seed of Corruption for AoE trash packs.";

        spec.spellPriority = {
            HAUNT,                  // Apply on CD — amplifies all DoT damage
            CORRUPTION,             // Instant DoT, keep rolling
            UNSTABLE_AFFLICTION,    // Cast DoT, high DPET
            CURSE_OF_AGONY,         // Long-duration curse
            DRAIN_SOUL,             // Execute: target below 25% HP
            SHADOW_BOLT,            // Filler when all DoTs are up
            SEED_OF_CORRUPTION,     // AoE: use on trash packs
            LIFE_TAP,               // Mana management
            DEATH_COIL_WARLOCK,     // Emergency self-heal / CC
            SOUL_SHATTER,           // Threat dump if needed
        };

        spec.selfBuffs = {
            FEL_ARMOR,              // Spellpower + healing received buff
        };

        spec.partyBuffs = {};       // Warlock doesn't have party buffs per se

        spec.preferredRange = 30.0f;
        profile.specs.push_back(spec);
    }

    // ── Demonology (Ranged DPS) ────────────────────────────────────────────
    {
        BotSpecProfile spec;
        spec.specName = "Demonology";
        spec.role     = BotRole::ROLE_RANGED_DPS;
        spec.behaviorDescription =
            "Focus on empowering the demon pet. Maintain Immolate and "
            "Corruption. Use Shadow Bolt as primary filler. Pop Demonic "
            "Empowerment on cooldown to buff the pet. Save Metamorphosis for "
            "burst windows — during Meta use Immolation Aura and Shadow Bolt "
            "spam. Summon Felguard as primary pet for its Cleave damage. "
            "Use Life Tap or Dark Pact for mana. Soul Fire during Decimation "
            "procs (target below 35%).";

        spec.spellPriority = {
            METAMORPHOSIS,          // Major cooldown — use for burst
            IMMOLATION_AURA,        // Only during Metamorphosis
            DEMONIC_EMPOWERMENT,    // Buff pet on cooldown
            CORRUPTION,             // Instant DoT, keep rolling
            IMMOLATE,               // Fire DoT, keep rolling
            CURSE_OF_DOOM,          // Long-duration, high damage curse
            SOUL_FIRE,              // Decimation proc (target < 35%)
            SHADOW_BOLT,            // Primary filler
            RAIN_OF_FIRE,           // AoE when 3+ targets
            LIFE_TAP,               // Mana management
            DARK_PACT,              // Alt mana source from pet
            SOUL_SHATTER,           // Threat dump
        };

        spec.selfBuffs = {
            FEL_ARMOR,              // Spellpower buff
        };

        spec.partyBuffs = {};

        spec.preferredRange = 30.0f;
        profile.specs.push_back(spec);
    }

    // ── Destruction (Ranged DPS) ───────────────────────────────────────────
    {
        BotSpecProfile spec;
        spec.specName = "Destruction";
        spec.role     = BotRole::ROLE_RANGED_DPS;
        spec.behaviorDescription =
            "Hard-hitting direct damage caster. Keep Immolate up for "
            "Conflagrate procs. Priority: Chaos Bolt on cooldown > Conflagrate "
            "(consumes Immolate, instant burst) > Immolate refresh > Incinerate "
            "filler. Use Shadowburn as execute (target below 20%). Pop "
            "Shadowfury for AoE stun. Curse of the Elements for raid debuff. "
            "Life Tap for mana. Summon Imp as primary pet for the Fire Bolt "
            "damage and Blood Pact stamina buff.";

        spec.spellPriority = {
            CHAOS_BOLT,             // Big nuke, use on cooldown
            CONFLAGRATE,            // Instant burst, consumes Immolate
            IMMOLATE,               // Maintain for Conflagrate to consume
            CURSE_OF_THE_ELEMENTS,  // Raid damage debuff
            SHADOWBURN,             // Execute: target below 20%
            INCINERATE,             // Primary filler (fire)
            SHADOWFURY,             // AoE stun utility
            RAIN_OF_FIRE,           // AoE when 3+ targets
            LIFE_TAP,               // Mana management
            SOUL_SHATTER,           // Threat dump
        };

        spec.selfBuffs = {
            FEL_ARMOR,              // Spellpower buff
        };

        spec.partyBuffs = {};

        spec.preferredRange = 30.0f;
        profile.specs.push_back(spec);
    }

    return profile;
}

} // anonymous namespace

// ─── Registration ──────────────────────────────────────────────────────────────
void AddBotWarlock()
{
    BotClassProfile profile = BuildWarlockProfile();
    sBotProfiles.Register(profile);

    LOG_INFO("module", "RPGBots: Registered Warlock profile ({} specs: {})",
        profile.specs.size(),
        [&]() {
            std::string s;
            for (size_t i = 0; i < profile.specs.size(); ++i)
            {
                if (i > 0) s += ", ";
                s += profile.specs[i].specName;
                s += " (";
                s += BotRoleName(profile.specs[i].role);
                s += ")";
            }
            return s;
        }());
}
