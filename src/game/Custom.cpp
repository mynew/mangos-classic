#include "CPlayer.h"
#include "Player.h"
#include "Custom.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "World.h"
#include "GridNotifiers.h"
#include "Spell.h"

const std::string Custom::m_ClassColor[] =
{
    "",
    MSG_COLOR_WARRIOR,
    MSG_COLOR_PALADIN,
    MSG_COLOR_HUNTER,
    MSG_COLOR_ROGUE,
    MSG_COLOR_PRIEST,
    "",
    MSG_COLOR_SHAMAN,
    MSG_COLOR_MAGE,
    MSG_COLOR_WARLOCK,
    "",
    MSG_COLOR_DRUID
};

CPlayer* Custom::GetCPlayer(const char* name)
{
    Player* plr = ObjectAccessor::FindPlayerByName(name);
    if (!plr)
        return NULL;

    return static_cast<CPlayer*>(plr);
}
CPlayer* Custom::GetCPlayer(ObjectGuid guid, bool inWorld /*=true*/)
{
    Player* plr = ObjectAccessor::FindPlayer(guid, inWorld);
    if (!plr)
        return NULL;

    return static_cast<CPlayer*>(plr);
}

Custom::SpellContainer Custom::GetSpellContainerByCreatureEntry(uint32 entry)
{
    SpellContainer spellContainer;

    if (TrainerSpellData const* spelldata = sObjectMgr.GetNpcTrainerSpells(entry))
        for (auto& itr : spelldata->spellList)
            spellContainer.push_back(itr.second);

    const CreatureInfo* creature = sObjectMgr.GetCreatureTemplate(entry);

    if (!creature)
        return spellContainer;

    uint32 trainertemplate = creature->TrainerTemplateId;

    if (trainertemplate)
        if (TrainerSpellData const* spelldata2 = sObjectMgr.GetNpcTrainerTemplateSpells(trainertemplate))
            for (auto& itr : spelldata2->spellList)
                spellContainer.push_back(itr.second);

    return spellContainer;
}

Custom::SpellContainer* Custom::GetCachedSpellContainer(uint32 crval)
{
    if (m_CachedSpellContainer.find(crval) != m_CachedSpellContainer.cend())
        return m_CachedSpellContainer[crval];

    return NULL;
}

uint8 Custom::PickFakeRace(uint8 fallbackrace, uint8 pclass, Team team)
{
    std::vector<uint8> playableRaces;

    for (uint8 i = RACE_HUMAN; i <= RACE_TROLL; ++i)
    {
        if (i == RACE_GOBLIN)
            continue;

        PlayerInfo const* info = sObjectMgr.GetPlayerInfo(i, pclass);
        if (!info)
            continue;

        if (Player::TeamForRace(i) == team)
            continue;

        playableRaces.push_back(i);
    }

    if (playableRaces.empty())
        return fallbackrace;

    return playableRaces[urand(0, playableRaces.size() - 1)];
}

void Custom::LoadFakePlayerBytes()
{
    uint32 count = 0;

    QueryResult* result = WorldDatabase.PQuery("SELECT race, maleBytes, maleBytes2, femaleBytes, femaleBytes2 FROM fakeplayerbytes");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            FakePlayerBytes bytes;

            uint8 race = fields[0].GetUInt8();
            bytes.PlayerBytes[GENDER_MALE] = fields[1].GetUInt32();
            bytes.PlayerBytes2[GENDER_MALE] = fields[2].GetUInt32();
            bytes.PlayerBytes[GENDER_FEMALE] = fields[3].GetUInt32();
            bytes.PlayerBytes2[GENDER_FEMALE] = fields[4].GetUInt32();

            m_FakePlayerBytesContainer.insert(std::make_pair(race, bytes));

            if (race && bytes.PlayerBytes[GENDER_MALE] && bytes.PlayerBytes2[GENDER_MALE] &&
                bytes.PlayerBytes[GENDER_FEMALE] && bytes.PlayerBytes[GENDER_FEMALE])
                ++count;
        } while (result->NextRow());
    }

    if (sWorld.getConfig(CONFIG_BOOL_CFBG_ENABLED) && (count < 10 || !result))
    {
        const char* message = "There was something wrong with loading fakeplayerbytes for crossfaction battlegrounds!";
        sLog.outError(message);
        sLog.outErrorDb(message);
        std::exit(EXIT_FAILURE);
    }
}

void Custom::LoadRefreshItems()
{
    auto result = WorldDatabase.PQuery("SELECT class, itemid FROM refreshitems");
    if (!result)
        return;

    do 
    {
        auto field = result->Fetch();
        uint8 classid = field[0].GetUInt8();
        uint32 itemid = field[1].GetUInt32();

        m_RefreshItems.insert(std::make_pair(classid, itemid));

        WorldDatabase.PExecute("UPDATE item_template SET BuyPrice = 0, SellPrice = 0 WHERE entry = %u", itemid);
    } while (result->NextRow());

    delete result;
}

void Unit::InterruptCasters()
{
    Unit *target = this;
    // there is also a spell wich has TARGET_RANDOM_ENEMY_CHAIN_IN_AREA but it's unused. So not really necessary.
    std::list<Unit*> targets;
    MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck u_check(target, GetMap()->GetVisibilityDistance());
    MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(target, searcher, GetMap()->GetVisibilityDistance());
    for (std::list<Unit*>::iterator iter = targets.begin(); iter != targets.end(); ++iter){

        if (!(*iter)->IsNonMeleeSpellCasted(false))
            continue;

        for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; i++){
            if ((*iter)->GetCurrentSpell(CurrentSpellTypes(i)) &&
                (*iter)->GetCurrentSpell(CurrentSpellTypes(i))->m_targets.getUnitTargetGuid() == target->GetObjectGuid())
                (*iter)->InterruptSpell(CurrentSpellTypes(CurrentSpellTypes(i)), false);
        }
    }
}
