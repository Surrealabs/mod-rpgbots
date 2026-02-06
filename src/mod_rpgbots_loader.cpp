// module entry point for mod-rpgbots

#include "ScriptMgr.h"

void Addmod_rpgbots_PersonalitySystem();

void AddRPGbotsCommands();

void AddCustomPsychology();
void AddCustomTemperament();

void AddBotSessionSystem();

void AddArmyOfAlts();

void AddRotationEngine();
void AddBotAI();

void Addmod_rpgbotsScripts()
{
    // Rotation Engine — loads SQL rotation data at startup (must be first)
    AddRotationEngine();

    Addmod_rpgbots_PersonalitySystem();
    AddRPGbotsCommands();
    AddCustomPsychology();
    AddCustomTemperament();
    AddBotSessionSystem();
    AddArmyOfAlts();

    // Bot AI — follow, assist, combat (must be after ArmyOfAlts)
    AddBotAI();
}
