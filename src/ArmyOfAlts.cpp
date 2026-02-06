// ArmyOfAlts.cpp
// Spawns alt characters into the world as bots, adds to party, follows master

#include "Chat.h"
#include "CommandScript.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "WorldSession.h"
#include "Group.h"
#include "World.h"
#include "DatabaseEnv.h"
#include "QueryHolder.h"
#include "CharacterCache.h"
#include "Map.h"
#include "MapMgr.h"
#include "Log.h"
#include "GameTime.h"
#include "Random.h"
#include "MotionMaster.h"
#include "SocialMgr.h"
#include "BotAI.h"
#include "BotBehavior.h"
#include <cmath>

using namespace Acore::ChatCommands;

// ─── BotLoginQueryHolder ───────────────────────────────────────────────────────
// Replicates the LoginQueryHolder from CharacterHandler.cpp (which is a local class)
// so we can load a character's full data from outside the normal login flow.
class BotLoginQueryHolder : public CharacterDatabaseQueryHolder
{
    uint32 m_accountId;
    ObjectGuid m_guid;
public:
    BotLoginQueryHolder(uint32 accountId, ObjectGuid guid)
        : m_accountId(accountId), m_guid(guid) {}

    ObjectGuid GetGuid() const { return m_guid; }
    uint32 GetAccountId() const { return m_accountId; }

    bool Initialize()
    {
        SetSize(MAX_PLAYER_LOGIN_QUERY);
        bool res = true;
        ObjectGuid::LowType lowGuid = m_guid.GetCounter();

        CharacterDatabasePreparedStatement* stmt = nullptr;

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_FROM, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_AURAS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_AURAS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELL);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELLS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_DAILYQUESTSTATUS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DAILY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_WEEKLYQUESTSTATUS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_WEEKLY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_MONTHLYQUESTSTATUS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MONTHLY_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SEASONALQUESTSTATUS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SEASONAL_QUEST_STATUS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_REPUTATION);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_REPUTATION, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_INVENTORY);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INVENTORY, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACTIONS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACTIONS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAIL);
        stmt->SetData(0, lowGuid);
        stmt->SetData(1, uint32(GameTime::GetGameTime().count()));
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAILS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_MAILITEMS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_MAIL_ITEMS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SOCIALLIST);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SOCIAL_LIST, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_HOMEBIND);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_HOME_BIND, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SPELLCOOLDOWNS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SPELL_COOLDOWNS, stmt);

        if (sWorld->getBoolConfig(CONFIG_DECLINED_NAMES_USED))
        {
            stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_DECLINEDNAMES);
            stmt->SetData(0, lowGuid);
            res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_DECLINED_NAMES, stmt);
        }

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ACHIEVEMENTS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACHIEVEMENTS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_CRITERIAPROGRESS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CRITERIA_PROGRESS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_EQUIPMENTSETS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_EQUIPMENT_SETS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_ENTRY_POINT);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ENTRY_POINT, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_GLYPHS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_GLYPHS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_TALENTS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_TALENTS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_PLAYER_ACCOUNT_DATA);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_ACCOUNT_DATA, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_SKILLS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_SKILLS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_RANDOMBG);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_RANDOM_BG, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_BANNED);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BANNED, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHARACTER_QUESTSTATUSREW);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_QUEST_STATUS_REW, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_BREW_OF_THE_MONTH);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_BREW_OF_THE_MONTH, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_ACCOUNT_INSTANCELOCKTIMES);
        stmt->SetData(0, m_accountId);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_INSTANCE_LOCK_TIMES, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CORPSE_LOCATION);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CORPSE_LOCATION, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_SETTINGS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_CHARACTER_SETTINGS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_PETS);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_PET_SLOTS, stmt);

        stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_CHAR_ACHIEVEMENT_OFFLINE_UPDATES);
        stmt->SetData(0, lowGuid);
        res &= SetPreparedQuery(PLAYER_LOGIN_QUERY_LOAD_OFFLINE_ACHIEVEMENTS_UPDATES, stmt);

        return res;
    }
};

// ─── Dismiss a single bot ──────────────────────────────────────────────────────
static void DismissOneBot(BotInfo& entry)
{
    Player* bot = entry.player;
    WorldSession* botSession = entry.session;

    if (!bot)
        return;

    LOG_INFO("module", "RPGBots: Dismissing bot {}", bot->GetName());

    // Stop movement
    bot->GetMotionMaster()->Clear();
    bot->AttackStop();

    // Remove from group
    if (Group* group = bot->GetGroup())
        group->RemoveMember(bot->GetGUID());

    // Remove from map
    if (bot->FindMap() && bot->IsInWorld())
    {
        bot->GetMap()->RemovePlayerFromMap(bot, false);
    }

    // Remove from global lookups
    ObjectAccessor::RemoveObject(bot);

    // Save character state
    bot->SaveToDB(false, true);

    // Mark offline in DB
    CharacterDatabase.Execute("UPDATE characters SET online = 0 WHERE guid = {}", bot->GetGUID().GetCounter());

    // Cleanup and delete
    bot->CleanupsBeforeDelete();
    delete bot;
    delete botSession;

    entry.player = nullptr;
    entry.session = nullptr;
}

// ─── Dismiss all bots for a master ────────────────────────────────────────────
static void DismissAllBots(ObjectGuid::LowType masterGuidLow)
{
    auto bots = sBotMgr.RemoveAllBots(masterGuidLow);
    for (auto& entry : bots)
        DismissOneBot(entry);
}

// ─── Bot spawn callback (runs after DB queries complete) ───────────────────────
static void FinishBotSpawn(ObjectGuid masterGuid, WorldSession* botSession, ObjectGuid botGuid,
                           CharacterDatabaseQueryHolder const& holder)
{
    Player* master = ObjectAccessor::FindPlayer(masterGuid);
    if (!master)
    {
        LOG_ERROR("module", "RPGBots: Master player gone before bot spawn completed");
        delete botSession;
        return;
    }

    // Create the bot Player object (this sets botSession->_player = bot)
    Player* bot = new Player(botSession);

    if (!bot->LoadFromDB(botGuid, holder))
    {
        LOG_ERROR("module", "RPGBots: Failed to load bot character {}", botGuid.ToString());
        botSession->SetPlayer(nullptr);
        delete bot;
        delete botSession;
        return;
    }

    bot->GetMotionMaster()->Initialize();

    // Relocate bot near master with a random offset
    float angle = frand(0.0f, 2.0f * float(M_PI));
    float dist  = frand(2.0f, 5.0f);
    float x = master->GetPositionX() + dist * std::cos(angle);
    float y = master->GetPositionY() + dist * std::sin(angle);
    float z = master->GetPositionZ();
    float o = master->GetOrientation();

    Map* masterMap = master->GetMap();

    // Override bot's position and map to master's location
    bot->Relocate(x, y, z, o);
    bot->m_mapId = master->GetMapId();
    bot->ResetMap();
    bot->SetMap(masterMap);
    bot->UpdatePositionData();

    // Send initial packets (no-op for null socket, but may set internal state)
    bot->SendInitialPacketsBeforeAddToMap();

    // Register in global hash maps so FindPlayer() works
    ObjectAccessor::AddObject(bot);

    // Add to map grid — this calls AddToWorld() and makes the bot visible
    if (!masterMap->AddPlayerToMap(bot))
    {
        LOG_ERROR("module", "RPGBots: Failed to add bot {} to map", bot->GetName());
        ObjectAccessor::RemoveObject(bot);
        botSession->SetPlayer(nullptr);
        delete bot;
        delete botSession;
        return;
    }

    bot->SendInitialPacketsAfterAddToMap();

    // Mark character as online in DB
    CharacterDatabasePreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_UPD_CHAR_ONLINE);
    stmt->SetData(0, bot->GetGUID().GetCounter());
    CharacterDatabase.Execute(stmt);

    bot->SetInGameTime(GameTime::GetGameTimeMS().count());

    // ── Party: create or join ──
    Group* group = master->GetGroup();
    if (!group)
    {
        group = new Group();
        group->Create(master);
    }
    group->AddMember(bot);

    // ── Detect role and spec ──
    BotRole role = DetectBotRole(bot);
    uint8 specIdx = DetectSpecIndex(bot);

    // Register with BotManager
    ObjectGuid::LowType masterLow = master->GetGUID().GetCounter();
    sBotMgr.AddBot(masterLow, { bot, botSession, role, specIdx, false, false });

    // ── Start following master ──
    bot->GetMotionMaster()->MoveFollow(master, 4.0f, float(M_PI));

    // Role name for display
    const char* roleName = "DPS";
    if (role == BotRole::ROLE_TANK) roleName = "Tank";
    else if (role == BotRole::ROLE_HEALER) roleName = "Healer";
    else if (role == BotRole::ROLE_MELEE_DPS) roleName = "Melee DPS";
    else if (role == BotRole::ROLE_RANGED_DPS) roleName = "Ranged DPS";

    // Notify master
    ChatHandler(master->GetSession()).PSendSysMessage("|cff00ff00{} has joined your party as {}!|r", bot->GetName(), roleName);
    LOG_INFO("module", "RPGBots: Bot {} spawned as {} for {}", bot->GetName(), roleName, master->GetName());
}

// ─── Command Script ────────────────────────────────────────────────────────────
class ArmyOfAlts : public CommandScript
{
public:
    ArmyOfAlts() : CommandScript("ArmyOfAlts") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable armyTable =
        {
            { "spawn",   HandleArmySpawnCommand,   SEC_GAMEMASTER, Console::No },
            { "list",    HandleArmyListCommand,    SEC_GAMEMASTER, Console::No },
            { "dismiss", HandleArmyDismissCommand, SEC_GAMEMASTER, Console::No },
            { "role",    HandleArmyRoleCommand,    SEC_GAMEMASTER, Console::No },
        };
        static ChatCommandTable commandTable =
        {
            { "army", armyTable },
        };
        return commandTable;
    }

    // .army list — show available alts on this account
    static bool HandleArmyListCommand(ChatHandler* handler)
    {
        Player* master = handler->GetSession()->GetPlayer();
        if (!master)
            return false;

        uint32 accountId = master->GetSession()->GetAccountId();
        ObjectGuid::LowType masterGuid = master->GetGUID().GetCounter();

        QueryResult result = CharacterDatabase.Query(
            "SELECT guid, name, level, class FROM characters WHERE account = {} AND guid != {}",
            accountId, masterGuid);

        if (!result)
        {
            handler->PSendSysMessage("|cffff0000No alts found on this account.|r");
            return true;
        }

        handler->PSendSysMessage("|cff00ff00=== Your Alts ===|r");
        do {
            Field* fields = result->Fetch();
            uint32 guid  = fields[0].Get<uint32>();
            std::string name = fields[1].Get<std::string>();
            uint8 level  = fields[2].Get<uint8>();
            uint8 cls    = fields[3].Get<uint8>();
            handler->PSendSysMessage("  GUID: {} | {} | Level {} | Class {}", guid, name, level, cls);
        } while (result->NextRow());

        handler->PSendSysMessage("|cff00ff00Use .army spawn <name> to summon an alt.|r");
        return true;
    }

    // .army spawn <name> — spawn a specific alt by character name
    static bool HandleArmySpawnCommand(ChatHandler* handler, Optional<std::string> nameArg)
    {
        Player* master = handler->GetSession()->GetPlayer();
        if (!master)
            return false;

        uint32 accountId = master->GetSession()->GetAccountId();
        ObjectGuid::LowType masterGuidLow = master->GetGUID().GetCounter();

        // Find the alt in the character database
        QueryResult result;
        if (nameArg)
        {
            result = CharacterDatabase.Query(
                "SELECT guid, name FROM characters WHERE account = {} AND guid != {} AND name = '{}'",
                accountId, masterGuidLow, *nameArg);
        }
        else
        {
            result = CharacterDatabase.Query(
                "SELECT guid, name FROM characters WHERE account = {} AND guid != {} LIMIT 1",
                accountId, masterGuidLow);
        }

        if (!result)
        {
            handler->PSendSysMessage("|cffff0000No alt found. Use .army list to see available alts.|r");
            return true;
        }

        uint32 altGuidLow = (*result)[0].Get<uint32>();
        std::string altName = (*result)[1].Get<std::string>();
        ObjectGuid altGuid = ObjectGuid::Create<HighGuid::Player>(altGuidLow);

        // Check if alt is already online (either real player or existing bot)
        if (ObjectAccessor::FindPlayer(altGuid))
        {
            handler->PSendSysMessage("|cffff0000{} is already in the world!|r", altName);
            return true;
        }

        // Create a socketless WorldSession for the bot
        // Uses the real account ID so LoadFromDB's account check passes
        WorldSession* botSession = new WorldSession(
            accountId,                          // account id (must match character's account)
            std::string(altName),               // session name
            0,                                  // account flags
            nullptr,                            // no socket — this is a bot
            SEC_PLAYER,                         // security
            EXPANSION_WRATH_OF_THE_LICH_KING,   // expansion
            0,                                  // mute time
            LOCALE_enUS,                        // locale
            0,                                  // recruiter
            false,                              // isARecruiter
            true,                               // skipQueue
            0                                   // totalTime
        );
        // NOTE: We intentionally do NOT register this session with sWorldSessionMgr
        // to avoid colliding with the master's real session (same account ID).

        // Build the login query holder (same queries the normal login uses)
        auto queryHolder = std::make_shared<BotLoginQueryHolder>(accountId, altGuid);
        if (!queryHolder->Initialize())
        {
            handler->PSendSysMessage("|cffff0000Failed to initialize bot login queries.|r");
            delete botSession;
            return true;
        }

        // Execute the queries asynchronously through the MASTER's session update loop.
        // When the queries finish, FinishBotSpawn() is called to complete the spawn.
        ObjectGuid masterGuid = master->GetGUID();
        master->GetSession()->AddQueryHolderCallback(
            CharacterDatabase.DelayQueryHolder(queryHolder)
        ).AfterComplete([masterGuid, botSession, altGuid](SQLQueryHolderBase const& holder)
        {
            FinishBotSpawn(masterGuid, botSession, altGuid,
                           static_cast<CharacterDatabaseQueryHolder const&>(holder));
        });

        handler->PSendSysMessage("|cff00ff00Spawning {}... They will join your party shortly.|r", altName);
        LOG_INFO("module", "RPGBots: {} spawning alt {} (GUID: {})",
            master->GetName(), altName, altGuidLow);

        return true;
    }

    // .army role <name> <tank|heal|dps|rdps> — manually override a bot's role
    static bool HandleArmyRoleCommand(ChatHandler* handler, std::string nameArg, std::string roleArg)
    {
        Player* master = handler->GetSession()->GetPlayer();
        if (!master)
            return false;

        ObjectGuid::LowType masterLow = master->GetGUID().GetCounter();

        BotInfo* info = sBotMgr.FindBot(masterLow, nameArg);
        if (!info)
        {
            handler->PSendSysMessage("|cffff0000No bot named '{}' found in your army.|r", nameArg);
            return true;
        }

        BotRole newRole;
        if (roleArg == "tank")
            newRole = BotRole::ROLE_TANK;
        else if (roleArg == "heal" || roleArg == "healer")
            newRole = BotRole::ROLE_HEALER;
        else if (roleArg == "dps" || roleArg == "melee")
            newRole = BotRole::ROLE_MELEE_DPS;
        else if (roleArg == "rdps" || roleArg == "ranged")
            newRole = BotRole::ROLE_RANGED_DPS;
        else
        {
            handler->PSendSysMessage("|cffff0000Unknown role '{}'. Use: tank, heal, dps, rdps|r", roleArg);
            return true;
        }

        info->role = newRole;

        const char* roleName = "DPS";
        if (newRole == BotRole::ROLE_TANK) roleName = "Tank";
        else if (newRole == BotRole::ROLE_HEALER) roleName = "Healer";
        else if (newRole == BotRole::ROLE_MELEE_DPS) roleName = "Melee DPS";
        else if (newRole == BotRole::ROLE_RANGED_DPS) roleName = "Ranged DPS";

        handler->PSendSysMessage("|cff00ff00{} is now set to {}.|r", nameArg, roleName);
        return true;
    }

    // .army dismiss — dismiss all bot alts
    static bool HandleArmyDismissCommand(ChatHandler* handler)
    {
        Player* master = handler->GetSession()->GetPlayer();
        if (!master)
            return false;

        ObjectGuid::LowType masterLow = master->GetGUID().GetCounter();
        if (!sBotMgr.HasBots(masterLow))
        {
            handler->PSendSysMessage("|cffff0000You have no bot alts to dismiss.|r");
            return true;
        }

        uint32 count = sBotMgr.GetBots(masterLow)->size();
        DismissAllBots(masterLow);
        handler->PSendSysMessage("|cff00ff00Dismissed {} bot alt(s). Army removed.|r", count);
        return true;
    }
};

// ─── Player Script: auto-dismiss bots when master logs out ─────────────────────
class ArmyBotCleanup : public PlayerScript
{
public:
    ArmyBotCleanup() : PlayerScript("ArmyBotCleanup", {PLAYERHOOK_ON_LOGOUT}) {}

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        ObjectGuid::LowType masterLow = player->GetGUID().GetCounter();
        if (sBotMgr.HasBots(masterLow))
        {
            LOG_INFO("module", "RPGBots: Master {} logging out, dismissing all bots", player->GetName());
            DismissAllBots(masterLow);
        }
    }
};

void AddArmyOfAlts()
{
    new ArmyOfAlts();
    new ArmyBotCleanup();
}
