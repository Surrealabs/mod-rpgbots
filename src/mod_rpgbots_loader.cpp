// module entry point for mod-rpgbots

#include "ScriptMgr.h"

void Addmod_rpgbots_PersonalitySystem();

void AddRPGbotsCommands();

void AddCustomPsychology();
void AddCustomTemperament();

void AddBotSessionSystem();

void AddArmyOfAlts();

void Addmod_rpgbotsScripts()
{
    Addmod_rpgbots_PersonalitySystem();
    AddRPGbotsCommands();
    AddCustomPsychology();
    AddCustomTemperament();
    AddBotSessionSystem();
    AddArmyOfAlts();
}
