// PersonalitySystem.h
// Handles character personalities, XP, and affixes for mod-rpgbots

#ifndef MODULE_RPG_BOTS_PERSONALITY_SYSTEM_H
#define MODULE_RPG_BOTS_PERSONALITY_SYSTEM_H

#include "ScriptMgr.h"
#include "Player.h"

class PersonalitySystem : public PlayerScript
{
public:
    PersonalitySystem();

    void OnLogin(Player* player) override;
    void OnGiveXP(Player* player, uint32& xp, uint8 /*restType*/) override;
    // Add more hooks as needed for affix/temperament/psychology management
};

#endif // MODULE_RPG_BOTS_PERSONALITY_SYSTEM_H
