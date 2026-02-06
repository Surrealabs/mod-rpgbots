// module entry point for mod-rpgbots

#include "ScriptMgr.h"

void Addmod_rpgbots_PersonalitySystem();

void AddRPGbotsCommands();

void AddCustomPsychology();
void AddCustomTemperament();

void AddBotSessionSystem();

void AddArmyOfAlts();

void AddBotPaladin();
void AddBotWarlock();

void Addmod_rpgbotsScripts()
{
    // Class behavior profiles (register first so they're available to other systems)
    AddBotPaladin();
    AddBotWarlock();

    Addmod_rpgbots_PersonalitySystem();
    AddRPGbotsCommands();
    AddCustomPsychology();
    AddCustomTemperament();
    AddBotSessionSystem();
    AddArmyOfAlts();
}
