// BotPaladin.cpp
// Class behavior profile for Paladin (CLASS_PALADIN = 2)
// Defines specs: Holy (Healer), Protection (Tank), Retribution (Melee DPS)
//
// WotLK 3.3.5a Paladin spell IDs — these are the real spell IDs from the game.

#include "BotBehavior.h"
#include "ScriptMgr.h"
#include "Log.h"

namespace
{

// ─── Paladin Spell IDs (Rank max at 80) ────────────────────────────────────────

// Seals & Auras
constexpr uint32 SEAL_OF_WISDOM         = 20166;
constexpr uint32 SEAL_OF_LIGHT          = 20165;
constexpr uint32 SEAL_OF_RIGHTEOUSNESS  = 21084;
constexpr uint32 SEAL_OF_COMMAND        = 20375;
constexpr uint32 SEAL_OF_VENGEANCE      = 31801;  // Alliance
constexpr uint32 DEVOTION_AURA          = 48942;  // Rank 10
constexpr uint32 RETRIBUTION_AURA       = 54043;  // Rank 7
constexpr uint32 CONCENTRATION_AURA     = 19746;
constexpr uint32 CRUSADER_AURA          = 32223;

// Blessings
constexpr uint32 BLESSING_OF_MIGHT      = 48934;  // Rank 10
constexpr uint32 BLESSING_OF_KINGS      = 20217;
constexpr uint32 BLESSING_OF_WISDOM     = 48938;  // Rank 9
constexpr uint32 BLESSING_OF_SANCTUARY  = 20911;

// Holy — Healing
constexpr uint32 HOLY_LIGHT             = 48782;  // Rank 13
constexpr uint32 FLASH_OF_LIGHT         = 48785;  // Rank 9
constexpr uint32 HOLY_SHOCK             = 48825;  // Rank 7
constexpr uint32 LAY_ON_HANDS           = 48788;  // Rank 4
constexpr uint32 BEACON_OF_LIGHT        = 53563;
constexpr uint32 SACRED_SHIELD          = 53601;
constexpr uint32 DIVINE_FAVOR           = 20216;
constexpr uint32 DIVINE_ILLUMINATION    = 31842;

// Protection — Tanking
constexpr uint32 RIGHTEOUS_FURY         = 25780;
constexpr uint32 SHIELD_OF_RIGHTEOUSNESS= 61411;  // Rank 2
constexpr uint32 HAMMER_OF_THE_RIGHTEOUS= 53595;  // Rank 4
constexpr uint32 AVENGERS_SHIELD        = 48827;  // Rank 4
constexpr uint32 HOLY_SHIELD            = 48952;  // Rank 6
constexpr uint32 CONSECRATION           = 48819;  // Rank 8
constexpr uint32 JUDGEMENT_OF_WISDOM     = 53408;
constexpr uint32 JUDGEMENT_OF_LIGHT      = 20271;
constexpr uint32 HAND_OF_RECKONING      = 62124;  // Taunt
constexpr uint32 DIVINE_PROTECTION      = 498;
constexpr uint32 DIVINE_SHIELD          = 642;
constexpr uint32 ARDENT_DEFENDER         = 31850;

// Retribution — Melee DPS
constexpr uint32 CRUSADER_STRIKE        = 35395;
constexpr uint32 DIVINE_STORM           = 53385;
constexpr uint32 JUDGEMENT_OF_WISDOM_RET = 53408;
constexpr uint32 HAMMER_OF_WRATH        = 48806;  // Rank 6 (execute)
constexpr uint32 EXORCISM               = 48801;  // Rank 9
constexpr uint32 AVENGING_WRATH         = 31884;
constexpr uint32 ART_OF_WAR             = 53488;  // Talent proc

// Utility
constexpr uint32 CLEANSE                = 4987;
constexpr uint32 HAND_OF_FREEDOM        = 1044;
constexpr uint32 HAND_OF_PROTECTION     = 10278;
constexpr uint32 HAMMER_OF_JUSTICE      = 10308;  // Rank 4 stun
constexpr uint32 DIVINE_PLEA            = 54428;

// ─── Spec Definitions ──────────────────────────────────────────────────────────

BotClassProfile BuildPaladinProfile()
{
    BotClassProfile profile;
    profile.classId  = CLASS_PALADIN;
    profile.className = "Paladin";

    // ── Holy (Healer) ──────────────────────────────────────────────────────
    {
        BotSpecProfile spec;
        spec.specName = "Holy";
        spec.role     = BotRole::ROLE_HEALER;
        spec.behaviorDescription =
            "Stay at medium range behind the group. Maintain Beacon of Light on "
            "the tank. Triage heals: Flash of Light for spot healing, Holy Light "
            "for heavy damage, Holy Shock as instant filler. Use Lay on Hands as "
            "an emergency cooldown. Keep Sacred Shield rolling on the tank. "
            "Manage mana with Divine Plea and Divine Illumination.";

        spec.spellPriority = {
            LAY_ON_HANDS,           // Emergency: target below 15% HP
            HOLY_SHOCK,             // Instant heal, use on cooldown for procs
            FLASH_OF_LIGHT,         // Fast, mana-efficient spot heal
            HOLY_LIGHT,             // Big heal for heavy damage
            BEACON_OF_LIGHT,        // Keep on tank (maintained buff)
            SACRED_SHIELD,          // Keep on tank
            DIVINE_FAVOR,           // Pop before a big Holy Light
            DIVINE_ILLUMINATION,    // Mana conservation cooldown
            DIVINE_PLEA,            // Mana regen when safe
            JUDGEMENT_OF_WISDOM,    // Mana return + keep judgement debuff up
            CONSECRATION,           // Only if in melee range and nothing to heal
            CLEANSE,                // Dispel harmful effects
        };

        spec.selfBuffs = {
            CONCENTRATION_AURA,     // Pushback resistance while casting
            SEAL_OF_WISDOM,         // Mana return on melee
        };

        spec.partyBuffs = {
            BLESSING_OF_KINGS,      // Default party buff
        };

        spec.preferredRange = 30.0f;
        profile.specs.push_back(spec);
    }

    // ── Protection (Tank) ──────────────────────────────────────────────────
    {
        BotSpecProfile spec;
        spec.specName = "Protection";
        spec.role     = BotRole::ROLE_TANK;
        spec.behaviorDescription =
            "Lead the group, position mobs facing away from party. Maintain "
            "Righteous Fury and Holy Shield. Build threat with Shield of "
            "Righteousness, Hammer of the Righteous, and Consecration. Use "
            "Avenger's Shield on pull and for snap threat. Hand of Reckoning "
            "to taunt loose mobs. Use Divine Protection and Ardent Defender "
            "when taking heavy damage. Judge Wisdom for mana sustain.";

        spec.spellPriority = {
            HAND_OF_RECKONING,      // Taunt: use when mob is on a party member
            SHIELD_OF_RIGHTEOUSNESS,// Primary single-target threat
            HAMMER_OF_THE_RIGHTEOUS,// Cleave threat (if talented)
            HOLY_SHIELD,            // Maintain block buff
            AVENGERS_SHIELD,        // Pull / snap threat
            CONSECRATION,           // AoE threat
            JUDGEMENT_OF_WISDOM,    // Keep judgement debuff up, mana
            HAMMER_OF_JUSTICE,      // Stun a dangerous add
            DIVINE_PROTECTION,      // Defensive cooldown
            DIVINE_PLEA,            // Mana sustain
        };

        spec.selfBuffs = {
            RIGHTEOUS_FURY,         // Must be active to tank
            DEVOTION_AURA,          // Armor aura for party
            SEAL_OF_VENGEANCE,      // Threat seal
        };

        spec.partyBuffs = {
            BLESSING_OF_SANCTUARY,  // Damage reduction + mana/rage returns
        };

        spec.preferredRange = 0.0f;  // Melee
        profile.specs.push_back(spec);
    }

    // ── Retribution (Melee DPS) ────────────────────────────────────────────
    {
        BotSpecProfile spec;
        spec.specName = "Retribution";
        spec.role     = BotRole::ROLE_MELEE_DPS;
        spec.behaviorDescription =
            "Stay behind the target. Use Crusader Strike and Divine Storm as "
            "core rotation. Keep judgement debuff active. Use Hammer of Wrath "
            "on targets below 20% HP (execute phase). Pop Avenging Wrath for "
            "burst damage windows. Use Exorcism on Art of War procs (instant). "
            "Consecration for AoE situations.";

        spec.spellPriority = {
            HAMMER_OF_WRATH,        // Execute: target below 20% HP
            CRUSADER_STRIKE,        // Core rotational ability
            DIVINE_STORM,           // Melee AoE + self-heal
            JUDGEMENT_OF_WISDOM_RET, // Keep debuff up
            CONSECRATION,           // AoE damage
            EXORCISM,               // Use on Art of War proc (instant cast)
            AVENGING_WRATH,         // Burst cooldown
            HAMMER_OF_JUSTICE,      // Stun for utility
        };

        spec.selfBuffs = {
            RETRIBUTION_AURA,       // Damage reflection aura
            SEAL_OF_COMMAND,        // AoE cleave seal (or Vengeance for ST)
        };

        spec.partyBuffs = {
            BLESSING_OF_MIGHT,      // AP buff for party
        };

        spec.preferredRange = 0.0f;  // Melee
        profile.specs.push_back(spec);
    }

    return profile;
}

} // anonymous namespace

// ─── Registration ──────────────────────────────────────────────────────────────
void AddBotPaladin()
{
    BotClassProfile profile = BuildPaladinProfile();
    sBotProfiles.Register(profile);

    LOG_INFO("module", "RPGBots: Registered Paladin profile ({} specs: {})",
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
