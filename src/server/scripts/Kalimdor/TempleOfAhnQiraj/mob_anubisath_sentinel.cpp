/*
 * Copyright (C) 2016+     AzerothCore <www.azerothcore.org>, released under GNU GPL v2 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-GPL2
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

/* ScriptData
SDName: npc_anubisath_sentinel
SD%Complete: 95
SDComment: Shadow storm is not properly implemented in core it should only target ppl outside of melee range.
SDCategory: Temple of Ahn'Qiraj
EndScriptData */

#include "Cell.h"
#include "CellImpl.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Spell.h"
#include "WorldPacket.h"

enum Spells
{
    SPELL_MENDING_BUFF     = 2147,

    SPELL_KNOCK_BUFF       = 21737,
    SPELL_KNOCK            = 25778,
    SPELL_MANAB_BUFF       = 812,
    SPELL_MANAB            = 25779,

    SPELL_REFLECTAF_BUFF   = 13022,
    SPELL_REFLECTSFr_BUFF  = 19595,
    SPELL_THORNS_BUFF      = 25777,

    SPELL_THUNDER_BUFF     = 2834,
    SPELL_THUNDER          = 8732,

    SPELL_MSTRIKE_BUFF     = 9347,
    SPELL_MSTRIKE          = 24573,

    SPELL_STORM_BUFF       = 2148,
    SPELL_STORM            = 26546
};

class npc_anubisath_sentinel : public CreatureScript
{
public:
    npc_anubisath_sentinel() : CreatureScript("npc_anubisath_sentinel") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new aqsentinelAI(creature);
    }

    struct aqsentinelAI : public ScriptedAI
    {
        uint32 ability;
        int abselected;

        void selectAbility(int asel)
        {
            switch (asel)
            {
                case 0:
                    ability = SPELL_MENDING_BUFF;
                    break;
                case 1:
                    ability = SPELL_KNOCK_BUFF;
                    break;
                case 2:
                    ability = SPELL_MANAB_BUFF;
                    break;
                case 3:
                    ability = SPELL_REFLECTAF_BUFF;
                    break;
                case 4:
                    ability = SPELL_REFLECTSFr_BUFF;
                    break;
                case 5:
                    ability = SPELL_THORNS_BUFF;
                    break;
                case 6:
                    ability = SPELL_THUNDER_BUFF;
                    break;
                case 7:
                    ability = SPELL_MSTRIKE_BUFF;
                    break;
                case 8:
                    ability = SPELL_STORM_BUFF;
                    break;
            }
        }

        aqsentinelAI(Creature* creature) : ScriptedAI(creature)
        {
            ClearBuddyList();
            abselected = 0;                                     // just initialization of variable
        }

        uint64 NearbyGUID[3];

        void ClearBuddyList()
        {
            NearbyGUID[0] = NearbyGUID[1] = NearbyGUID[2] = 0;
        }

        void AddBuddyToList(uint64 CreatureGUID)
        {
            if (CreatureGUID == me->GetGUID())
                return;

            for (int i = 0; i < 3; ++i)
            {
                if (NearbyGUID[i] == CreatureGUID)
                    return;
                if (!NearbyGUID[i])
                {
                    NearbyGUID[i] = CreatureGUID;
                    return;
                }
            }
        }

        void GiveBuddyMyList(Creature* c)
        {
            aqsentinelAI* cai = CAST_AI(aqsentinelAI, (c)->AI());
            for (int i = 0; i < 3; ++i)
                if (NearbyGUID[i] && NearbyGUID[i] != c->GetGUID())
                    cai->AddBuddyToList(NearbyGUID[i]);
            cai->AddBuddyToList(me->GetGUID());
        }

        void SendMyListToBuddies()
        {
            for (int i = 0; i < 3; ++i)
                if (Creature* pNearby = ObjectAccessor::GetCreature(*me, NearbyGUID[i]))
                    GiveBuddyMyList(pNearby);
        }

        void CallBuddiesToAttack(Unit* who)
        {
            for (int i = 0; i < 3; ++i)
            {
                Creature* c = ObjectAccessor::GetCreature(*me, NearbyGUID[i]);
                if (c)
                {
                    if (!c->IsInCombat())
                    {
                        c->SetNoCallAssistance(true);
                        if (c->AI())
                            c->AI()->AttackStart(who);
                    }
                }
            }
        }

        void AddSentinelsNear(Unit* /*nears*/)
        {
            std::list<Creature*> assistList;
            me->GetCreatureListWithEntryInGrid(assistList, 15264, 70.0f);

            if (assistList.empty())
                return;

            for (std::list<Creature*>::const_iterator iter = assistList.begin(); iter != assistList.end(); ++iter)
                AddBuddyToList((*iter)->GetGUID());
        }

        int pickAbilityRandom(bool* chosenAbilities)
        {
            for (int t = 0; t < 2; ++t)
            {
                for (int i = !t ? (rand() % 9) : 0; i < 9; ++i)
                {
                    if (!chosenAbilities[i])
                    {
                        chosenAbilities[i] = true;
                        return i;
                    }
                }
            }
            return 0;                                           // should never happen
        }

        void GetOtherSentinels(Unit* who)
        {
            bool* chosenAbilities = new bool[9];
            memset(chosenAbilities, 0, 9 * sizeof(bool));
            selectAbility(pickAbilityRandom(chosenAbilities));

            ClearBuddyList();
            AddSentinelsNear(me);
            int bli;
            for (bli = 0; bli < 3; ++bli)
            {
                if (!NearbyGUID[bli])
                    break;

                Creature* pNearby = ObjectAccessor::GetCreature(*me, NearbyGUID[bli]);
                if (!pNearby)
                    break;

                AddSentinelsNear(pNearby);
                CAST_AI(aqsentinelAI, pNearby->AI())->gatherOthersWhenAggro = false;
                CAST_AI(aqsentinelAI, pNearby->AI())->selectAbility(pickAbilityRandom(chosenAbilities));
            }
            /*if (bli < 3)
                DoYell("I dont have enough buddies.", LANG_NEUTRAL, 0);*/
            SendMyListToBuddies();
            CallBuddiesToAttack(who);

            delete[] chosenAbilities;
        }

        bool gatherOthersWhenAggro;

        void Reset() override
        {
            if (!me->isDead())
            {
                for (int i = 0; i < 3; ++i)
                {
                    if (!NearbyGUID[i])
                        continue;
                    if (Creature* pNearby = ObjectAccessor::GetCreature(*me, NearbyGUID[i]))
                    {
                        if (pNearby->isDead())
                            pNearby->Respawn();
                    }
                }
            }
            ClearBuddyList();
            gatherOthersWhenAggro = true;
        }

        void GainSentinelAbility(uint32 id)
        {
            me->AddAura(id, me);
        }

        void EnterCombat(Unit* who) override
        {
            if (gatherOthersWhenAggro)
                GetOtherSentinels(who);

            GainSentinelAbility(ability);
            DoZoneInCombat();
        }

        void JustDied(Unit* /*killer*/) override
        {
            for (int ni = 0; ni < 3; ++ni)
            {
                Creature* sent = ObjectAccessor::GetCreature(*me, NearbyGUID[ni]);
                if (!sent)
                    continue;
                if (sent->isDead())
                    continue;
                sent->ModifyHealth(int32(sent->CountPctFromMaxHealth(50)));
                CAST_AI(aqsentinelAI, sent->AI())->GainSentinelAbility(ability);
            }
        }
    };
};

void AddSC_npc_anubisath_sentinel()
{
    new npc_anubisath_sentinel();
}
