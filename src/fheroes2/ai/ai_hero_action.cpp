/***************************************************************************
 *   Copyright (C) 2010 by Andrey Afletdinov <fheroes2@gmail.com>          *
 *                                                                         *
 *   Part of the Free Heroes2 Engine:                                      *
 *   http://sourceforge.net/projects/fheroes2                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <algorithm>
#include <cassert>

#include "agg.h"
#include "ai.h"
#include "army.h"
#include "battle.h"
#include "castle.h"
#include "game.h"
#include "game_delays.h"
#include "game_interface.h"
#include "heroes.h"
#include "interface_gamearea.h"
#include "kingdom.h"
#include "logging.h"
#include "maps_objects.h"
#include "maps_tiles.h"
#include "payment.h"
#include "race.h"
#include "settings.h"
#include "world.h"

namespace
{
    uint32_t AIGetAllianceColors()
    {
        // accumulate colors
        uint32_t colors = 0;

        if ( Settings::Get().GameType() & Game::TYPE_HOTSEAT ) {
            const Colors vcolors( Players::HumanColors() );

            for ( const int color : vcolors ) {
                const Player * player = Players::Get( color );
                if ( player )
                    colors |= player->GetFriends();
            }
        }
        else {
            const Player * player = Players::Get( Players::HumanColors() );
            if ( player )
                colors = player->GetFriends();
        }

        return colors;
    }

    // Never cache the value of this function as it depends on hero's path and location.
    bool AIHeroesShowAnimation( const Heroes & hero, const uint32_t colors )
    {
        if ( Settings::Get().AIMoveSpeed() == 0 ) {
            return false;
        }

        if ( colors == 0 )
            return false;

        const int32_t indexFrom = hero.GetIndex();
        if ( !Maps::isValidAbsIndex( indexFrom ) )
            return false;

        if ( !world.GetTiles( indexFrom ).isFog( colors ) )
            return true;

        const Route::Path & path = hero.GetPath();
        if ( path.isValid() && world.GetTiles( path.front().GetIndex() ).GetFogDirections( colors ) != DIRECTION_ALL )
            return true;

        return false;
    }
}

namespace AI
{
    void AIToMonster( Heroes & hero, s32 dst_index );
    void AIToPickupResource( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToTreasureChest( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToArtifact( Heroes & hero, s32 dst_index );
    void AIToObjectResource( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToWagon( Heroes & hero, s32 dst_index );
    void AIToSkeleton( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToCaptureObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToFlotSam( const Heroes & hero, s32 dst_index );
    void AIToObservationTower( Heroes & hero, s32 dst_index );
    void AIToMagellanMaps( Heroes & hero, s32 dst_index );
    void AIToTeleports( Heroes & hero, const int32_t startIndex );
    void AIToWhirlpools( Heroes & hero, const int32_t startIndex );
    void AIToPrimarySkillObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToExperienceObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToWitchsHut( Heroes & hero, s32 dst_index );
    void AIToShrine( Heroes & hero, s32 dst_index );
    void AIToGoodMoraleObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToMagicWell( Heroes & hero, s32 dst_index );
    void AIToArtesianSpring( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToXanadu( Heroes & hero, s32 dst_index );
    void AIToEvent( Heroes & hero, s32 dst_index );
    void AIToUpgradeArmyObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToPoorMoraleObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToPyramid( Heroes & hero, s32 dst_index );
    void AIToGoodLuckObject( Heroes & hero, s32 dst_index );
    void AIToObelisk( Heroes & hero, const Maps::Tiles & tile );
    void AIToTreeKnowledge( Heroes & hero, s32 dst_index );
    void AIToDaemonCave( Heroes & hero, s32 dst_index );
    void AIToCastle( Heroes & hero, s32 dst_index );
    void AIToSign( Heroes & hero, s32 dst_index );
    void AIToDwellingJoinMonster( Heroes & hero, s32 dst_index );
    void AIToHeroes( Heroes & hero, s32 dst_index );
    void AIToDwellingRecruitMonster( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToDwellingBattleMonster( Heroes & hero, const MP2::MapObjectType objectType, const int32_t tileIndex );
    void AIToStables( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToAbandoneMine( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToBarrier( const Heroes & hero, s32 dst_index );
    void AIToTravellersTent( const Heroes & hero, s32 dst_index );
    void AIToShipwreckSurvivor( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index );
    void AIToBoat( Heroes & hero, s32 dst_index );
    void AIToCoast( Heroes & hero, s32 dst_index );
    void AIMeeting( Heroes & hero1, Heroes & hero2 );
    void AIWhirlpoolTroopLoseEffect( Heroes & hero );
    void AIToJail( const Heroes & hero, const int32_t tileIndex );
    void AIToHutMagi( Heroes & hero, const MP2::MapObjectType objectType, const int32_t tileIndex );
    void AIToAlchemistTower( Heroes & hero );

    int AISelectPrimarySkill( const Heroes & hero )
    {
        switch ( hero.GetRace() ) {
        case Race::KNGT: {
            if ( 5 > hero.GetDefense() )
                return Skill::Primary::DEFENSE;
            if ( 5 > hero.GetAttack() )
                return Skill::Primary::ATTACK;
            if ( 3 > hero.GetKnowledge() )
                return Skill::Primary::KNOWLEDGE;
            if ( 3 > hero.GetPower() )
                return Skill::Primary::POWER;
            break;
        }

        case Race::BARB: {
            if ( 5 > hero.GetAttack() )
                return Skill::Primary::ATTACK;
            if ( 5 > hero.GetDefense() )
                return Skill::Primary::DEFENSE;
            if ( 3 > hero.GetPower() )
                return Skill::Primary::POWER;
            if ( 3 > hero.GetKnowledge() )
                return Skill::Primary::KNOWLEDGE;
            break;
        }

        case Race::SORC:
        case Race::WZRD: {
            if ( 5 > hero.GetKnowledge() )
                return Skill::Primary::KNOWLEDGE;
            if ( 5 > hero.GetPower() )
                return Skill::Primary::POWER;
            if ( 3 > hero.GetDefense() )
                return Skill::Primary::DEFENSE;
            if ( 3 > hero.GetAttack() )
                return Skill::Primary::ATTACK;
            break;
        }

        case Race::WRLK:
        case Race::NECR: {
            if ( 5 > hero.GetPower() )
                return Skill::Primary::POWER;
            if ( 5 > hero.GetKnowledge() )
                return Skill::Primary::KNOWLEDGE;
            if ( 3 > hero.GetAttack() )
                return Skill::Primary::ATTACK;
            if ( 3 > hero.GetDefense() )
                return Skill::Primary::DEFENSE;
            break;
        }

        default:
            break;
        }

        switch ( Rand::Get( 1, 4 ) ) {
        case 1:
            return Skill::Primary::ATTACK;
        case 2:
            return Skill::Primary::DEFENSE;
        case 3:
            return Skill::Primary::POWER;
        case 4:
            return Skill::Primary::KNOWLEDGE;
        default:
            break;
        }

        return Skill::Primary::UNKNOWN;
    }

    void AIBattleLose( Heroes & hero, const Battle::Result & res, bool attacker, int color, const fheroes2::Point * centerOn = nullptr, const bool playSound = false )
    {
        const uint32_t reason = attacker ? res.AttackerResult() : res.DefenderResult();

        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            if ( centerOn != nullptr ) {
                Interface::Basic::Get().GetGameArea().SetCenter( *centerOn );
            }

            if ( playSound ) {
                AGG::PlaySound( M82::KILLFADE );
            }
            hero.FadeOut();
        }

        hero.SetKillerColor( color );
        hero.SetFreeman( reason );
    }

    void HeroesAction( Heroes & hero, s32 dst_index )
    {
        const Maps::Tiles & tile = world.GetTiles( dst_index );
        const MP2::MapObjectType objectType = tile.GetObject( dst_index != hero.GetIndex() );
        bool isAction = true;

        const bool isActionObject = MP2::isActionObject( objectType, hero.isShipMaster() );
        if ( isActionObject )
            hero.SetModes( Heroes::ACTION );

        switch ( objectType ) {
        case MP2::OBJ_BOAT:
            AIToBoat( hero, dst_index );
            break;
        case MP2::OBJ_COAST:
            AIToCoast( hero, dst_index );
            break;

        case MP2::OBJ_MONSTER:
            AIToMonster( hero, dst_index );
            break;
        case MP2::OBJ_HEROES:
            AIToHeroes( hero, dst_index );
            break;
        case MP2::OBJ_CASTLE:
            AIToCastle( hero, dst_index );
            break;

        // pickup object
        case MP2::OBJ_RESOURCE:
        case MP2::OBJ_BOTTLE:
        case MP2::OBJ_CAMPFIRE:
            AIToPickupResource( hero, objectType, dst_index );
            break;

        case MP2::OBJ_WATERCHEST:
        case MP2::OBJ_TREASURECHEST:
            AIToTreasureChest( hero, objectType, dst_index );
            break;
        case MP2::OBJ_ARTIFACT:
            AIToArtifact( hero, dst_index );
            break;

        case MP2::OBJ_MAGICGARDEN:
        case MP2::OBJ_LEANTO:
        case MP2::OBJ_WINDMILL:
        case MP2::OBJ_WATERWHEEL:
            AIToObjectResource( hero, objectType, dst_index );
            break;

        case MP2::OBJ_WAGON:
            AIToWagon( hero, dst_index );
            break;
        case MP2::OBJ_SKELETON:
            AIToSkeleton( hero, objectType, dst_index );
            break;
        case MP2::OBJ_FLOTSAM:
            AIToFlotSam( hero, dst_index );
            break;

        case MP2::OBJ_ALCHEMYLAB:
        case MP2::OBJ_MINES:
        case MP2::OBJ_SAWMILL:
        case MP2::OBJ_LIGHTHOUSE:
            AIToCaptureObject( hero, objectType, dst_index );
            break;
        case MP2::OBJ_ABANDONEDMINE:
            AIToAbandoneMine( hero, objectType, dst_index );
            break;

        case MP2::OBJ_SHIPWRECKSURVIVOR:
            AIToShipwreckSurvivor( hero, objectType, dst_index );
            break;

        // event
        case MP2::OBJ_EVENT:
            AIToEvent( hero, dst_index );
            break;

        case MP2::OBJ_SIGN:
            AIToSign( hero, dst_index );
            break;

        // increase view
        case MP2::OBJ_OBSERVATIONTOWER:
            AIToObservationTower( hero, dst_index );
            break;
        case MP2::OBJ_MAGELLANMAPS:
            AIToMagellanMaps( hero, dst_index );
            break;

        // teleports
        case MP2::OBJ_STONELITHS:
            AIToTeleports( hero, dst_index );
            break;
        case MP2::OBJ_WHIRLPOOL:
            AIToWhirlpools( hero, dst_index );
            break;

        // primary skill modification
        case MP2::OBJ_FORT:
        case MP2::OBJ_MERCENARYCAMP:
        case MP2::OBJ_DOCTORHUT:
        case MP2::OBJ_STANDINGSTONES:
            AIToPrimarySkillObject( hero, objectType, dst_index );
            break;

        // experience modification
        case MP2::OBJ_GAZEBO:
            AIToExperienceObject( hero, objectType, dst_index );
            break;

        // witchs hut
        case MP2::OBJ_WITCHSHUT:
            AIToWitchsHut( hero, dst_index );
            break;

        // shrine circle
        case MP2::OBJ_SHRINE1:
        case MP2::OBJ_SHRINE2:
        case MP2::OBJ_SHRINE3:
            AIToShrine( hero, dst_index );
            break;

        // luck modification
        case MP2::OBJ_FOUNTAIN:
        case MP2::OBJ_FAERIERING:
        case MP2::OBJ_IDOL:
        case MP2::OBJ_MERMAID:
            AIToGoodLuckObject( hero, dst_index );
            break;

        // morale modification
        case MP2::OBJ_OASIS:
        case MP2::OBJ_TEMPLE:
        case MP2::OBJ_WATERINGHOLE:
        case MP2::OBJ_BUOY:
            AIToGoodMoraleObject( hero, objectType, dst_index );
            break;

        case MP2::OBJ_OBELISK:
            AIToObelisk( hero, tile );
            break;

        // magic point
        case MP2::OBJ_ARTESIANSPRING:
            AIToArtesianSpring( hero, objectType, dst_index );
            break;
        case MP2::OBJ_MAGICWELL:
            AIToMagicWell( hero, dst_index );
            break;

        // increase skill
        case MP2::OBJ_XANADU:
            AIToXanadu( hero, dst_index );
            break;

        case MP2::OBJ_HILLFORT:
        case MP2::OBJ_FREEMANFOUNDRY:
            AIToUpgradeArmyObject( hero, objectType, dst_index );
            break;

        case MP2::OBJ_SHIPWRECK:
        case MP2::OBJ_GRAVEYARD:
        case MP2::OBJ_DERELICTSHIP:
            AIToPoorMoraleObject( hero, objectType, dst_index );
            break;

        case MP2::OBJ_PYRAMID:
            AIToPyramid( hero, dst_index );
            break;
        case MP2::OBJ_DAEMONCAVE:
            AIToDaemonCave( hero, dst_index );
            break;

        case MP2::OBJ_TREEKNOWLEDGE:
            AIToTreeKnowledge( hero, dst_index );
            break;

        // accept army
        case MP2::OBJ_WATCHTOWER:
        case MP2::OBJ_EXCAVATION:
        case MP2::OBJ_CAVE:
        case MP2::OBJ_TREEHOUSE:
        case MP2::OBJ_ARCHERHOUSE:
        case MP2::OBJ_GOBLINHUT:
        case MP2::OBJ_DWARFCOTT:
        case MP2::OBJ_HALFLINGHOLE:
        case MP2::OBJ_PEASANTHUT:
        case MP2::OBJ_THATCHEDHUT:
            AIToDwellingJoinMonster( hero, dst_index );
            break;

        // recruit army
        case MP2::OBJ_RUINS:
        case MP2::OBJ_TREECITY:
        case MP2::OBJ_WAGONCAMP:
        case MP2::OBJ_DESERTTENT:
        // loyalty ver
        case MP2::OBJ_WATERALTAR:
        case MP2::OBJ_AIRALTAR:
        case MP2::OBJ_FIREALTAR:
        case MP2::OBJ_EARTHALTAR:
        case MP2::OBJ_BARROWMOUNDS:
            AIToDwellingRecruitMonster( hero, objectType, dst_index );
            break;

        // recruit army (battle)
        case MP2::OBJ_DRAGONCITY:
        case MP2::OBJ_CITYDEAD:
        case MP2::OBJ_TROLLBRIDGE:
            AIToDwellingBattleMonster( hero, objectType, dst_index );
            break;

        // recruit genie
        case MP2::OBJ_ANCIENTLAMP:
            AIToDwellingRecruitMonster( hero, objectType, dst_index );
            break;

        case MP2::OBJ_STABLES:
            AIToStables( hero, objectType, dst_index );
            break;
        case MP2::OBJ_ARENA:
            AIToPrimarySkillObject( hero, objectType, dst_index );
            break;

        case MP2::OBJ_BARRIER:
            AIToBarrier( hero, dst_index );
            break;
        case MP2::OBJ_TRAVELLERTENT:
            AIToTravellersTent( hero, dst_index );
            break;

        case MP2::OBJ_JAIL:
            AIToJail( hero, dst_index );
            break;
        case MP2::OBJ_HUTMAGI:
            AIToHutMagi( hero, objectType, dst_index );
            break;
        case MP2::OBJ_ALCHEMYTOWER:
            AIToAlchemistTower( hero );
            break;

        // AI has no advantage or knowledge to use these objects
        case MP2::OBJ_ORACLE:
        case MP2::OBJ_TRADINGPOST:
        case MP2::OBJ_EYEMAGI:
        case MP2::OBJ_SPHINX:
        case MP2::OBJ_SIRENS:
            break;
        default:
            assert( !isActionObject ); // AI should know what to do with this type of action object! Please add logic for it.
            isAction = false;
            break;
        }

        if ( MP2::isNeedStayFront( objectType ) )
            hero.GetPath().Reset();

        // ignore empty tiles
        if ( isAction )
            AI::Get().HeroesActionComplete( hero );
    }

    void AIToHeroes( Heroes & hero, s32 dst_index )
    {
        Heroes * other_hero = world.GetTiles( dst_index ).GetHeroes();
        if ( !other_hero )
            return;

        if ( hero.GetColor() == other_hero->GetColor() ) {
            DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " meeting " << other_hero->GetName() );
            AIMeeting( hero, *other_hero );
        }
        else if ( hero.isFriends( other_hero->GetColor() ) ) {
            DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " disable meeting" );
        }
        else {
            const Castle * other_hero_castle = other_hero->inCastle();
            if ( other_hero_castle && other_hero == other_hero_castle->GetHeroes().GuardFirst() ) {
                AIToCastle( hero, dst_index );
                return;
            }

            const bool playVanishingHeroSound = other_hero->isControlHuman();

            DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " attack enemy hero " << other_hero->GetName() );

            // new battle
            Battle::Result res = Battle::Loader( hero.GetArmy(), other_hero->GetArmy(), dst_index );

            // loss defender
            if ( !res.DefenderWins() )
                AIBattleLose( *other_hero, res, false, hero.GetColor(), nullptr, playVanishingHeroSound );

            // loss attacker
            if ( !res.AttackerWins() )
                AIBattleLose( hero, res, true, other_hero->GetColor(), &( other_hero->GetCenter() ), playVanishingHeroSound );

            // wins attacker
            if ( res.AttackerWins() ) {
                hero.IncreaseExperience( res.GetExperienceAttacker() );
            }
            else
                // wins defender
                if ( res.DefenderWins() ) {
                other_hero->IncreaseExperience( res.GetExperienceDefender() );
            }
        }
    }

    void AIToCastle( Heroes & hero, s32 dst_index )
    {
        Castle * castle = world.getCastleEntrance( Maps::GetPoint( dst_index ) );
        if ( castle == nullptr ) {
            // Something is wrong while calling this function for incorrect tile.
            assert( 0 );
            return;
        }

        if ( hero.GetColor() == castle->GetColor() ) {
            DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " goto castle " << castle->GetName() );
            castle->MageGuildEducateHero( hero );
            hero.SetVisited( dst_index );
        }
        if ( hero.isFriends( castle->GetColor() ) ) {
            DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " disable visiting" );
        }
        else {
            CastleHeroes heroes = castle->GetHeroes();

            // first attack to guest hero
            if ( heroes.FullHouse() ) {
                AIToHeroes( hero, dst_index );
                return;
            }

            Army & army = castle->GetActualArmy();
            // bool allow_enter = false;

            if ( army.isValid() && army.GetColor() != hero.GetColor() ) {
                DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " attack enemy castle " << castle->GetName() );

                Heroes * defender = heroes.GuardFirst();
                castle->ActionPreBattle();

                const bool playVanishingHeroSound = defender != nullptr && defender->isControlHuman();

                // new battle
                Battle::Result res = Battle::Loader( hero.GetArmy(), army, dst_index );

                castle->ActionAfterBattle( res.AttackerWins() );

                // loss defender
                if ( !res.DefenderWins() && defender )
                    AIBattleLose( *defender, res, false, hero.GetColor(), nullptr, playVanishingHeroSound );

                // loss attacker
                if ( !res.AttackerWins() )
                    AIBattleLose( hero, res, true, castle->GetColor(), &( castle->GetCenter() ), playVanishingHeroSound );

                // wins attacker
                if ( res.AttackerWins() ) {
                    castle->GetKingdom().RemoveCastle( castle );
                    hero.GetKingdom().AddCastle( castle );
                    world.CaptureObject( dst_index, hero.GetColor() );
                    castle->Scoute();

                    hero.IncreaseExperience( res.GetExperienceAttacker() );
                    // allow_enter = true;
                }
                else
                    // wins defender
                    if ( res.DefenderWins() && defender ) {
                    defender->IncreaseExperience( res.GetExperienceDefender() );
                }
            }
            else {
                DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " capture enemy castle " << castle->GetName() );

                castle->GetKingdom().RemoveCastle( castle );
                hero.GetKingdom().AddCastle( castle );
                world.CaptureObject( dst_index, hero.GetColor() );
                castle->Scoute();
                // allow_enter = true;
            }
        }
    }

    void AIToMonster( Heroes & hero, s32 dst_index )
    {
        bool destroy = false;
        Maps::Tiles & tile = world.GetTiles( dst_index );
        Troop troop = tile.QuantityTroop();

        const NeutralMonsterJoiningCondition join = Army::GetJoinSolution( hero, tile, troop );

        if ( join.reason == NeutralMonsterJoiningCondition::Reason::Alliance ) {
            if ( hero.GetArmy().CanJoinTroop( troop ) ) {
                DEBUG_LOG( DBG_AI, DBG_INFO, troop.GetName() << " join " << hero.GetName() << " as a part of alliance." );
                hero.GetArmy().JoinTroop( troop );
            }
            else {
                DEBUG_LOG( DBG_AI, DBG_INFO, troop.GetName() << " unblock way for " << hero.GetName() << " as a part of alliance." );
            }

            destroy = true;
        }
        else if ( join.reason == NeutralMonsterJoiningCondition::Reason::Bane ) {
            DEBUG_LOG( DBG_AI, DBG_INFO, troop.GetName() << " run away from " << hero.GetName() << " as a part of bane." );
            destroy = true;
        }
        else if ( join.reason == NeutralMonsterJoiningCondition::Reason::Free ) {
            if ( hero.GetArmy().CanJoinTroop( troop ) ) {
                DEBUG_LOG( DBG_AI, DBG_INFO, troop.GetName() << " join " << hero.GetName() << "." );
                hero.GetArmy().JoinTroop( troop );
                destroy = true;
            }
        }
        else if ( join.reason == NeutralMonsterJoiningCondition::Reason::ForMoney ) {
            const int32_t joiningCost = troop.GetCost().gold;
            if ( hero.GetKingdom().AllowPayment( payment_t( Resource::GOLD, joiningCost ) ) && hero.GetArmy().CanJoinTroop( troop ) ) {
                DEBUG_LOG( DBG_AI, DBG_INFO, join.monsterCount << " " << troop.GetName() << " join " << hero.GetName() << " for " << joiningCost << " gold." );
                hero.GetArmy().JoinTroop( troop.GetMonster(), join.monsterCount );
                hero.GetKingdom().OddFundsResource( Funds( Resource::GOLD, joiningCost ) );
                destroy = true;
            }
        }
        else if ( join.reason == NeutralMonsterJoiningCondition::Reason::RunAway ) {
            // TODO: AI should still chase monsters which it can defeat without losses to get extra experience.
            DEBUG_LOG( DBG_AI, DBG_INFO, troop.GetName() << " run away from " << hero.GetName() << "." );
            destroy = true;
        }

        // fight
        if ( !destroy ) {
            DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " attacked monster " << troop.GetName() );
            Army army( tile );
            Battle::Result res = Battle::Loader( hero.GetArmy(), army, dst_index );

            if ( res.AttackerWins() ) {
                hero.IncreaseExperience( res.GetExperienceAttacker() );
                destroy = true;
            }
            else {
                AIBattleLose( hero, res, true, Color::NONE );
                tile.MonsterSetCount( army.GetCountMonsters( troop.GetMonster() ) );
                if ( tile.MonsterJoinConditionFree() )
                    tile.MonsterSetJoinCondition( Monster::JOIN_CONDITION_MONEY );
            }
        }

        if ( destroy ) {
            tile.RemoveObjectSprite();
            tile.MonsterSetCount( 0 );
            tile.setAsEmpty();
        }

        hero.unmarkHeroMeeting();
    }

    void AIToPickupResource( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        if ( objectType != MP2::OBJ_BOTTLE )
            hero.GetKingdom().AddFundsResource( tile.QuantityFunds() );

        tile.RemoveObjectSprite();
        tile.QuantityReset();
        hero.GetPath().Reset();

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " pickup small resource" );
    }

    void AIToTreasureChest( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );
        u32 gold = tile.QuantityGold();

        if ( tile.isWater() ) {
            if ( gold ) {
                const Artifact & art = tile.QuantityArtifact();

                if ( art.isValid() && !hero.PickupArtifact( art ) )
                    gold = GoldInsteadArtifact( objectType );
            }
        }
        else {
            const Artifact & art = tile.QuantityArtifact();

            if ( gold ) {
                const u32 expr = gold > 500 ? gold - 500 : 500;

                if ( hero.getAIRole() == Heroes::Role::HUNTER ) {
                    // Only 10% chance of choosing experience. Make AI rich!
                    if ( Rand::Get( 1, 10 ) == 1 ) {
                        gold = 0;
                        hero.IncreaseExperience( expr );
                    }
                }
                else if ( Rand::Get( 1, 2 ) == 1 ) {
                    // 50/50 chance.
                    gold = 0;
                    hero.IncreaseExperience( expr );
                }
            }
            else if ( art.isValid() && !hero.PickupArtifact( art ) )
                gold = GoldInsteadArtifact( objectType );
        }

        if ( gold )
            hero.GetKingdom().AddFundsResource( Funds( Resource::GOLD, gold ) );

        tile.RemoveObjectSprite();
        tile.QuantityReset();

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToObjectResource( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );
        const ResourceCount & rc = tile.QuantityResourceCount();

        if ( rc.isValid() )
            hero.GetKingdom().AddFundsResource( Funds( rc ) );

        if ( MP2::isCaptureObject( objectType ) )
            AIToCaptureObject( hero, objectType, dst_index );

        tile.QuantityReset();
        hero.setVisitedForAllies( dst_index );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToSkeleton( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        // artifact
        if ( tile.QuantityIsValid() ) {
            const Artifact & art = tile.QuantityArtifact();

            if ( !hero.PickupArtifact( art ) ) {
                u32 gold = GoldInsteadArtifact( objectType );
                hero.GetKingdom().AddFundsResource( Funds( Resource::GOLD, gold ) );
            }

            tile.QuantityReset();
        }

        hero.SetVisitedWideTile( dst_index, objectType, Visit::GLOBAL );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToWagon( Heroes & hero, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        if ( tile.QuantityIsValid() ) {
            const Artifact & art = tile.QuantityArtifact();

            if ( art.isValid() )
                hero.PickupArtifact( art );
            else
                hero.GetKingdom().AddFundsResource( tile.QuantityFunds() );

            tile.QuantityReset();
        }

        hero.SetVisited( dst_index, Visit::GLOBAL );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToCaptureObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        // capture object
        if ( !hero.isFriends( tile.QuantityColor() ) ) {
            bool capture = true;

            if ( tile.CaptureObjectIsProtection() ) {
                const Troop & troop = tile.QuantityTroop();
                Army army;
                army.JoinTroop( troop );

                Battle::Result result = Battle::Loader( hero.GetArmy(), army, dst_index );

                if ( result.AttackerWins() ) {
                    hero.IncreaseExperience( result.GetExperienceAttacker() );
                    tile.SetQuantity3( 0 );
                }
                else {
                    capture = false;
                    AIBattleLose( hero, result, true, Color::NONE );
                    tile.MonsterSetCount( army.GetCountMonsters( troop.GetMonster() ) );
                }
            }

            if ( capture ) {
                // update abandone mine
                if ( objectType == MP2::OBJ_ABANDONEDMINE ) {
                    Maps::Tiles::UpdateAbandoneMineSprite( tile );
                    hero.SetMapsObject( MP2::OBJ_MINES );
                }

                tile.QuantitySetColor( hero.GetColor() );
            }
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << " captured: " << MP2::StringObject( objectType ) );
    }

    void AIToFlotSam( const Heroes & hero, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        hero.GetKingdom().AddFundsResource( tile.QuantityFunds() );
        tile.RemoveObjectSprite();
        tile.QuantityReset();

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToSign( Heroes & hero, s32 dst_index )
    {
        hero.SetVisited( dst_index, Visit::GLOBAL );
        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToObservationTower( Heroes & hero, s32 dst_index )
    {
        Maps::ClearFog( dst_index, Game::GetViewDistance( Game::VIEW_OBSERVATION_TOWER ), hero.GetColor() );
        hero.SetVisited( dst_index, Visit::GLOBAL );
        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToMagellanMaps( Heroes & hero, s32 dst_index )
    {
        const Funds payment( Resource::GOLD, 1000 );
        Kingdom & kingdom = hero.GetKingdom();

        if ( !hero.isObjectTypeVisited( MP2::OBJ_MAGELLANMAPS, Visit::GLOBAL ) && kingdom.AllowPayment( payment ) ) {
            hero.SetVisited( dst_index, Visit::GLOBAL );
            hero.setVisitedForAllies( dst_index );
            world.ActionForMagellanMaps( hero.GetColor() );
            kingdom.OddFundsResource( payment );
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToTeleports( Heroes & hero, const int32_t startIndex )
    {
        assert( hero.GetPath().empty() );

        const int32_t indexTo = world.NextTeleport( startIndex );
        if ( startIndex == indexTo ) {
            DEBUG_LOG( DBG_AI, DBG_WARN, "AI hero " << hero.GetName() << " has nowhere to go through stone liths." );
            return;
        }

        assert( world.GetTiles( indexTo ).GetObject() != MP2::OBJ_HEROES );

        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            hero.FadeOut();
        }

        hero.Move2Dest( indexTo );
        hero.GetPath().Reset();

        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            Interface::Basic::Get().GetGameArea().SetCenter( hero.GetCenter() );
            hero.FadeIn();
        }

        hero.ActionNewPosition( false );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToWhirlpools( Heroes & hero, const int32_t startIndex )
    {
        assert( hero.GetPath().empty() );

        const int32_t indexTo = world.NextWhirlpool( startIndex );
        if ( startIndex == indexTo ) {
            DEBUG_LOG( DBG_AI, DBG_WARN, "AI hero " << hero.GetName() << " has nowhere to go through the whirlpool." );
            return;
        }

        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            hero.FadeOut();
        }

        hero.Move2Dest( indexTo );
        hero.GetPath().Reset();

        AIWhirlpoolTroopLoseEffect( hero );

        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            Interface::Basic::Get().GetGameArea().SetCenter( hero.GetCenter() );
            hero.FadeIn();
        }

        hero.ActionNewPosition( false );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToPrimarySkillObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        const Maps::Tiles & tile = world.GetTiles( dst_index );

        int skill = Skill::Primary::UNKNOWN;

        switch ( objectType ) {
        case MP2::OBJ_FORT:
            skill = Skill::Primary::DEFENSE;
            break;
        case MP2::OBJ_MERCENARYCAMP:
            skill = Skill::Primary::ATTACK;
            break;
        case MP2::OBJ_DOCTORHUT:
            skill = Skill::Primary::KNOWLEDGE;
            break;
        case MP2::OBJ_STANDINGSTONES:
            skill = Skill::Primary::POWER;
            break;
        case MP2::OBJ_ARENA:
            if ( Settings::Get().ExtHeroArenaCanChoiseAnySkills() )
                skill = AISelectPrimarySkill( hero );
            else {
                switch ( Rand::Get( 1, 3 ) ) {
                case 1:
                case 2:
                    skill = Rand::Get( 1 ) ? Skill::Primary::ATTACK : Skill::Primary::DEFENSE;
                    break;

                default:
                    skill = Skill::Primary::POWER;
                    break;
                }
            }
            break;

        default:
            break;
        }

        if ( ( MP2::OBJ_ARENA == objectType && !hero.isObjectTypeVisited( objectType ) ) || !hero.isVisited( tile ) ) {
            // increase skill
            hero.IncreasePrimarySkill( skill );
            hero.SetVisited( dst_index );

            // fix double action tile
            hero.SetVisitedWideTile( dst_index, objectType );
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToExperienceObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        const Maps::Tiles & tile = world.GetTiles( dst_index );

        u32 exp = 0;

        switch ( objectType ) {
        case MP2::OBJ_GAZEBO:
            exp = 1000;
            break;
        default:
            break;
        }

        // check already visited
        if ( !hero.isVisited( tile ) && exp ) {
            hero.SetVisited( dst_index );
            hero.IncreaseExperience( exp );
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToWitchsHut( Heroes & hero, s32 dst_index )
    {
        const Skill::Secondary & skill = world.GetTiles( dst_index ).QuantitySkill();

        // check full
        if ( skill.isValid() && !hero.HasMaxSecondarySkill() && !hero.HasSecondarySkill( skill.Skill() ) )
            hero.LearnSkill( skill );

        hero.SetVisited( dst_index );
        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToShrine( Heroes & hero, s32 dst_index )
    {
        const Spell & spell = world.GetTiles( dst_index ).QuantitySpell();
        const u32 spell_level = spell.Level();

        if ( spell.isValid() &&
             // check spell book
             hero.HaveSpellBook() &&
             // check present spell (skip bag artifacts)
             !hero.HaveSpell( spell, true ) &&
             // check valid level spell and wisdom skill
             !( 3 == spell_level && Skill::Level::NONE == hero.GetLevelSkill( Skill::Secondary::WISDOM ) ) ) {
            hero.AppendSpellToBook( spell );
        }

        // All heroes will know which spell is here.
        hero.SetVisited( dst_index, Visit::GLOBAL );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToGoodLuckObject( Heroes & hero, s32 dst_index )
    {
        hero.SetVisited( dst_index );
        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToGoodMoraleObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        u32 move = 0;

        switch ( objectType ) {
        case MP2::OBJ_OASIS:
            move = 800;
            break;
        case MP2::OBJ_WATERINGHOLE:
            move = 400;
            break;
        default:
            break;
        }

        // check already visited
        if ( !hero.isObjectTypeVisited( objectType ) ) {
            // modify morale
            hero.SetVisited( dst_index );
            if ( move )
                hero.IncreaseMovePoints( move );

            // fix double action tile
            hero.SetVisitedWideTile( dst_index, objectType );
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToMagicWell( Heroes & hero, s32 dst_index )
    {
        const u32 max = hero.GetMaxSpellPoints();

        if ( hero.GetSpellPoints() < max && !hero.isObjectTypeVisited( MP2::OBJ_MAGICWELL ) ) {
            hero.SetSpellPoints( max );
        }

        hero.SetVisited( dst_index );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToArtesianSpring( Heroes & hero, const MP2::MapObjectType objectType, int32_t dst_index )
    {
        const u32 max = hero.GetMaxSpellPoints();

        if ( !world.isAnyKingdomVisited( objectType, dst_index ) && hero.GetSpellPoints() < max * 2 ) {
            hero.SetSpellPoints( max * 2 );
        }

        hero.SetVisitedWideTile( dst_index, objectType, Visit::GLOBAL );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToXanadu( Heroes & hero, s32 dst_index )
    {
        const Maps::Tiles & tile = world.GetTiles( dst_index );
        const u32 level1 = hero.GetLevelSkill( Skill::Secondary::DIPLOMACY );
        const u32 level2 = hero.GetLevel();

        if ( !hero.isVisited( tile )
             && ( ( level1 == Skill::Level::BASIC && 7 < level2 ) || ( level1 == Skill::Level::ADVANCED && 5 < level2 )
                  || ( level1 == Skill::Level::EXPERT && 3 < level2 ) || ( 9 < level2 ) ) ) {
            hero.IncreasePrimarySkill( Skill::Primary::ATTACK );
            hero.IncreasePrimarySkill( Skill::Primary::DEFENSE );
            hero.IncreasePrimarySkill( Skill::Primary::KNOWLEDGE );
            hero.IncreasePrimarySkill( Skill::Primary::POWER );
            hero.SetVisited( dst_index );
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToEvent( Heroes & hero, s32 dst_index )
    {
        // check event maps
        MapEvent * event_maps = world.GetMapEvent( Maps::GetPoint( dst_index ) );

        if ( event_maps && event_maps->isAllow( hero.GetColor() ) && event_maps->computer ) {
            if ( event_maps->resources.GetValidItemsCount() )
                hero.GetKingdom().AddFundsResource( event_maps->resources );
            if ( event_maps->artifact.isValid() )
                hero.PickupArtifact( event_maps->artifact );
            event_maps->SetVisited( hero.GetColor() );

            if ( event_maps->cancel ) {
                hero.SetMapsObject( MP2::OBJ_ZERO );
                world.RemoveMapObject( event_maps );
            }
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToUpgradeArmyObject( Heroes & hero, const MP2::MapObjectType objectType, s32 /*dst_index*/ )
    {
        switch ( objectType ) {
        case MP2::OBJ_HILLFORT:
            if ( hero.GetArmy().HasMonster( Monster::DWARF ) )
                hero.GetArmy().UpgradeMonsters( Monster::DWARF );
            if ( hero.GetArmy().HasMonster( Monster::ORC ) )
                hero.GetArmy().UpgradeMonsters( Monster::ORC );
            if ( hero.GetArmy().HasMonster( Monster::OGRE ) )
                hero.GetArmy().UpgradeMonsters( Monster::OGRE );
            break;

        case MP2::OBJ_FREEMANFOUNDRY:
            if ( hero.GetArmy().HasMonster( Monster::PIKEMAN ) )
                hero.GetArmy().UpgradeMonsters( Monster::PIKEMAN );
            if ( hero.GetArmy().HasMonster( Monster::SWORDSMAN ) )
                hero.GetArmy().UpgradeMonsters( Monster::SWORDSMAN );
            if ( hero.GetArmy().HasMonster( Monster::IRON_GOLEM ) )
                hero.GetArmy().UpgradeMonsters( Monster::IRON_GOLEM );
            break;

        default:
            break;
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToPoorMoraleObject( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );
        u32 gold = tile.QuantityGold();
        bool complete = false;

        if ( gold ) {
            Army army( tile );

            Battle::Result res = Battle::Loader( hero.GetArmy(), army, dst_index );
            if ( res.AttackerWins() ) {
                hero.IncreaseExperience( res.GetExperienceAttacker() );
                complete = true;
                const Artifact & art = tile.QuantityArtifact();

                if ( art.isValid() && !hero.PickupArtifact( art ) )
                    gold = GoldInsteadArtifact( objectType );

                hero.GetKingdom().AddFundsResource( Funds( Resource::GOLD, gold ) );
            }
            else {
                AIBattleLose( hero, res, true, Color::NONE );
            }
        }

        if ( complete )
            tile.QuantityReset();
        else if ( 0 == gold && !hero.isObjectTypeVisited( objectType ) ) {
            // modify morale
            hero.SetVisited( dst_index );
            hero.SetVisited( dst_index, Visit::GLOBAL );
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToPyramid( Heroes & hero, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );
        const Spell & spell = tile.QuantitySpell();

        if ( spell.isValid() ) {
            // battle
            Army army( tile );

            Battle::Result res = Battle::Loader( hero.GetArmy(), army, dst_index );

            if ( res.AttackerWins() ) {
                hero.IncreaseExperience( res.GetExperienceAttacker() );

                // check magick book
                if ( hero.HaveSpellBook() &&
                     // check skill level for wisdom
                     Skill::Level::EXPERT == hero.GetLevelSkill( Skill::Secondary::WISDOM ) ) {
                    hero.AppendSpellToBook( spell );
                }

                tile.QuantityReset();
                hero.SetVisited( dst_index, Visit::GLOBAL );
            }
            else {
                AIBattleLose( hero, res, true, Color::NONE );
            }
        }
        else {
            hero.SetVisited( dst_index, Visit::LOCAL );
            hero.SetVisited( dst_index, Visit::GLOBAL );
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToObelisk( Heroes & hero, const Maps::Tiles & tile )
    {
        if ( !hero.isVisited( tile, Visit::GLOBAL ) ) {
            hero.SetVisited( tile.GetIndex(), Visit::GLOBAL );
            Kingdom & kingdom = hero.GetKingdom();
            kingdom.PuzzleMaps().Update( kingdom.CountVisitedObjects( MP2::OBJ_OBELISK ), world.CountObeliskOnMaps() );
            // TODO: once any piece of the puzzle is open and the area is discovered and a hero has nothing to do make him dig around this area.
            // TODO: once all pieces are open add a special condition to make AI to dig the Ultimate artifact.
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToTreeKnowledge( Heroes & hero, s32 dst_index )
    {
        const Maps::Tiles & tile = world.GetTiles( dst_index );

        if ( !hero.isVisited( tile ) ) {
            const Funds & funds = tile.QuantityFunds();

            if ( 0 == funds.GetValidItemsCount() || hero.GetKingdom().AllowPayment( funds ) ) {
                if ( funds.GetValidItemsCount() )
                    hero.GetKingdom().OddFundsResource( funds );
                hero.SetVisited( dst_index );
                hero.IncreaseExperience( Heroes::GetExperienceFromLevel( hero.GetLevel() ) - hero.GetExperience() );
            }
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToDaemonCave( Heroes & hero, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        if ( tile.QuantityIsValid() ) {
            Army army( tile );

            Battle::Result res = Battle::Loader( hero.GetArmy(), army, dst_index );
            if ( res.AttackerWins() ) {
                hero.IncreaseExperience( res.GetExperienceAttacker() );
                hero.GetKingdom().AddFundsResource( tile.QuantityFunds() );
            }
            else {
                AIBattleLose( hero, res, true, Color::NONE );
            }

            tile.QuantityReset();
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToDwellingJoinMonster( Heroes & hero, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );
        const Troop & troop = tile.QuantityTroop();

        if ( troop.isValid() && hero.GetArmy().JoinTroop( troop ) ) {
            tile.MonsterSetCount( 0 );
            hero.unmarkHeroMeeting();
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToDwellingRecruitMonster( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );
        const Troop & troop = tile.QuantityTroop();

        if ( troop.isValid() ) {
            Kingdom & kingdom = hero.GetKingdom();
            const payment_t paymentCosts = troop.GetCost();

            if ( kingdom.AllowPayment( paymentCosts ) && hero.GetArmy().JoinTroop( troop ) ) {
                tile.MonsterSetCount( 0 );
                kingdom.OddFundsResource( paymentCosts );

                // remove ancient lamp sprite
                if ( MP2::OBJ_ANCIENTLAMP == objectType ) {
                    tile.RemoveObjectSprite();
                    tile.setAsEmpty();
                }

                hero.unmarkHeroMeeting();
            }
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToDwellingBattleMonster( Heroes & hero, const MP2::MapObjectType objectType, const int32_t tileIndex )
    {
        Maps::Tiles & tile = world.GetTiles( tileIndex );
        const Troop & troop = tile.QuantityTroop();

        bool allowToRecruit = true;
        if ( Color::NONE == tile.QuantityColor() ) {
            // Not captured / defeated yet.
            Army army( tile );
            Battle::Result res = Battle::Loader( hero.GetArmy(), army, tileIndex );
            if ( res.AttackerWins() ) {
                hero.IncreaseExperience( res.GetExperienceAttacker() );
                tile.QuantitySetColor( hero.GetColor() );
                tile.SetObjectPassable( true );

                hero.unmarkHeroMeeting();
            }
            else {
                AIBattleLose( hero, res, true, Color::NONE );
                allowToRecruit = false;
            }
        }

        // recruit monster
        if ( allowToRecruit && troop.isValid() ) {
            AIToDwellingRecruitMonster( hero, objectType, tileIndex );
        }

        hero.SetVisited( tileIndex, Visit::GLOBAL );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() << ", object: " << MP2::StringObject( objectType ) );
    }

    void AIToStables( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        // check already visited
        if ( !hero.isObjectTypeVisited( objectType ) ) {
            hero.SetVisited( dst_index );
            hero.IncreaseMovePoints( 400 );
        }

        if ( hero.GetArmy().HasMonster( Monster::CAVALRY ) )
            hero.GetArmy().UpgradeMonsters( Monster::CAVALRY );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToAbandoneMine( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        AIToCaptureObject( hero, objectType, dst_index );
    }

    void AIToBarrier( const Heroes & hero, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );
        const Kingdom & kingdom = hero.GetKingdom();

        if ( kingdom.IsVisitTravelersTent( tile.QuantityColor() ) ) {
            tile.RemoveObjectSprite();
            tile.setAsEmpty();
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToTravellersTent( const Heroes & hero, s32 dst_index )
    {
        const Maps::Tiles & tile = world.GetTiles( dst_index );
        Kingdom & kingdom = hero.GetKingdom();

        kingdom.SetVisitTravelersTent( tile.QuantityColor() );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToShipwreckSurvivor( Heroes & hero, const MP2::MapObjectType objectType, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        if ( hero.IsFullBagArtifacts() )
            hero.GetKingdom().AddFundsResource( Funds( Resource::GOLD, GoldInsteadArtifact( objectType ) ) );
        else
            hero.PickupArtifact( tile.QuantityArtifact() );

        tile.RemoveObjectSprite();
        tile.QuantityReset();

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToArtifact( Heroes & hero, s32 dst_index )
    {
        Maps::Tiles & tile = world.GetTiles( dst_index );

        if ( !hero.IsFullBagArtifacts() ) {
            u32 cond = tile.QuantityVariant();
            Artifact art = tile.QuantityArtifact();

            bool result = false;

            // 1,2,3 - gold, gold + res
            if ( 0 < cond && cond < 4 ) {
                Funds payment = tile.QuantityFunds();

                if ( hero.GetKingdom().AllowPayment( payment ) ) {
                    result = true;
                    hero.GetKingdom().OddFundsResource( payment );
                }
            }
            else if ( 3 < cond && cond < 6 ) {
                // 4,5 - bypass wisdom and leadership requirement
                result = true;
            }
            else
                // 6 - 50 rogues, 7 - 1 gin, 8,9,10,11,12,13 - 1 monster level4
                if ( 5 < cond && cond < 14 ) {
                Army army( tile );

                // new battle
                Battle::Result res = Battle::Loader( hero.GetArmy(), army, dst_index );
                if ( res.AttackerWins() ) {
                    hero.IncreaseExperience( res.GetExperienceAttacker() );
                    result = true;
                }
                else {
                    AIBattleLose( hero, res, true, Color::NONE );
                }
            }
            else {
                result = true;
            }

            if ( result && hero.PickupArtifact( art ) ) {
                tile.RemoveObjectSprite();
                tile.QuantityReset();
            }
        }

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToBoat( Heroes & hero, s32 dst_index )
    {
        if ( hero.isShipMaster() )
            return;

        const s32 from_index = hero.GetIndex();

        // disabled nearest coasts (on week MP2::isWeekLife)
        MapsIndexes coasts = Maps::ScanAroundObjectWithDistance( from_index, 4, MP2::OBJ_COAST );
        coasts.push_back( from_index );

        for ( MapsIndexes::const_iterator it = coasts.begin(); it != coasts.end(); ++it )
            hero.SetVisited( *it );

        hero.setLastGroundRegion( world.GetTiles( from_index ).GetRegion() );

        const fheroes2::Point & destPos = Maps::GetPoint( dst_index );
        const fheroes2::Point offset( destPos - hero.GetCenter() );

        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            hero.FadeOut( offset );
        }

        hero.setDirection( world.GetTiles( dst_index ).getBoatDirection() );
        hero.ResetMovePoints();
        hero.Move2Dest( dst_index );
        hero.SetMapsObject( MP2::OBJ_ZERO );
        world.GetTiles( dst_index ).resetObjectSprite();
        hero.SetShipMaster( true );
        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            Interface::Basic::Get().GetGameArea().SetCenter( hero.GetCenter() );
        }
        hero.GetPath().Reset();

        AI::Get().HeroesClearTask( hero );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIToCoast( Heroes & hero, s32 dst_index )
    {
        if ( !hero.isShipMaster() )
            return;

        const int fromIndex = hero.GetIndex();
        Maps::Tiles & from = world.GetTiles( fromIndex );

        // Calculate the offset before making the action.
        const fheroes2::Point & prevPosition = Maps::GetPoint( dst_index );
        const fheroes2::Point offset( prevPosition - hero.GetCenter() );

        hero.ResetMovePoints();
        hero.Move2Dest( dst_index );
        from.setBoat( Maps::GetDirection( fromIndex, dst_index ) );
        hero.SetShipMaster( false );
        hero.GetPath().Reset();

        if ( AIHeroesShowAnimation( hero, AIGetAllianceColors() ) ) {
            Interface::Basic::Get().GetGameArea().SetCenter( prevPosition );
            hero.FadeIn( fheroes2::Point( offset.x * Game::AIHeroAnimSkip(), offset.y * Game::AIHeroAnimSkip() ) );
        }
        hero.ActionNewPosition( true );

        AI::Get().HeroesClearTask( hero );

        DEBUG_LOG( DBG_AI, DBG_INFO, hero.GetName() );
    }

    void AIMeeting( Heroes & left, Heroes & right )
    {
        left.markHeroMeeting( right.GetID() );
        right.markHeroMeeting( left.GetID() );

        if ( Settings::Get().ExtWorldEyeEagleAsScholar() )
            Heroes::ScholarAction( left, right );

        const bool rightToLeft = right.getStatsValue() < left.getStatsValue();

        if ( rightToLeft )
            left.GetArmy().JoinStrongestFromArmy( right.GetArmy() );
        else
            right.GetArmy().JoinStrongestFromArmy( left.GetArmy() );

        if ( rightToLeft )
            left.GetBagArtifacts().exchangeArtifacts( right.GetBagArtifacts() );
        else
            right.GetBagArtifacts().exchangeArtifacts( left.GetBagArtifacts() );
    }

    void HeroesMove( Heroes & hero )
    {
        const Route::Path & path = hero.GetPath();

        if ( path.isValid() ) {
            hero.SetMove( true );

            Interface::Basic & basicInterface = Interface::Basic::Get();
            Interface::GameArea & gameArea = basicInterface.GetGameArea();

            const Settings & conf = Settings::Get();

            const uint32_t colors = AIGetAllianceColors();
            bool recenterNeeded = true;

            int heroAnimationFrameCount = 0;
            fheroes2::Point heroAnimationOffset;
            int heroAnimationSpriteId = 0;

            const bool hideAIMovements = ( conf.AIMoveSpeed() == 0 );
            const bool noMovementAnimation = ( conf.AIMoveSpeed() == 10 );

            const std::vector<Game::DelayType> delayTypes = { Game::CURRENT_AI_DELAY };

            while ( LocalEvent::Get().HandleEvents( !hideAIMovements && Game::isDelayNeeded( delayTypes ) ) ) {
                if ( hero.isFreeman() || !hero.isMoveEnabled() ) {
                    break;
                }

                if ( hideAIMovements || !AIHeroesShowAnimation( hero, colors ) ) {
                    hero.Move( true );
                    recenterNeeded = true;
                }
                else if ( Game::validateAnimationDelay( Game::CURRENT_AI_DELAY ) ) {
                    // re-center in case hero appears from the fog
                    if ( recenterNeeded ) {
                        gameArea.SetCenter( hero.GetCenter() );
                        recenterNeeded = false;
                    }

                    bool resetHeroSprite = false;
                    if ( heroAnimationFrameCount > 0 ) {
                        gameArea.ShiftCenter( fheroes2::Point( heroAnimationOffset.x * Game::AIHeroAnimSkip(), heroAnimationOffset.y * Game::AIHeroAnimSkip() ) );
                        gameArea.SetRedraw();
                        heroAnimationFrameCount -= Game::AIHeroAnimSkip();
                        if ( ( heroAnimationFrameCount & 0x3 ) == 0 ) { // % 4
                            hero.SetSpriteIndex( heroAnimationSpriteId );

                            if ( heroAnimationFrameCount == 0 )
                                resetHeroSprite = true;
                            else
                                ++heroAnimationSpriteId;
                        }
                        const int offsetStep = ( ( 4 - ( heroAnimationFrameCount & 0x3 ) ) & 0x3 ); // % 4
                        hero.SetOffset( fheroes2::Point( heroAnimationOffset.x * offsetStep, heroAnimationOffset.y * offsetStep ) );
                    }

                    if ( heroAnimationFrameCount == 0 ) {
                        if ( resetHeroSprite ) {
                            hero.SetSpriteIndex( heroAnimationSpriteId - 1 );
                        }

                        if ( hero.Move( noMovementAnimation ) ) {
                            if ( AIHeroesShowAnimation( hero, colors ) ) {
                                gameArea.SetCenter( hero.GetCenter() );
                            }
                        }
                        else {
                            fheroes2::Point movement( hero.MovementDirection() );
                            if ( movement != fheroes2::Point() ) { // don't waste resources for no movement
                                heroAnimationOffset = movement;
                                gameArea.ShiftCenter( movement );
                                heroAnimationFrameCount = 32 - Game::AIHeroAnimSkip();
                                heroAnimationSpriteId = hero.GetSpriteIndex();
                                if ( Game::AIHeroAnimSkip() < 4 ) {
                                    hero.SetSpriteIndex( heroAnimationSpriteId - 1 );
                                    hero.SetOffset( fheroes2::Point( heroAnimationOffset.x * Game::AIHeroAnimSkip(), heroAnimationOffset.y * Game::AIHeroAnimSkip() ) );
                                }
                                else {
                                    ++heroAnimationSpriteId;
                                }
                            }
                        }
                    }

                    basicInterface.Redraw( Interface::REDRAW_GAMEAREA );
                    fheroes2::Display::instance().render();
                }

                if ( Game::validateAnimationDelay( Game::MAPS_DELAY ) ) {
                    // will be animated in hero loop
                    u32 & frame = Game::MapsAnimationFrame();
                    ++frame;
                }
            }

            hero.SetMove( false );
        }
        else if ( !path.empty() && path.GetFrontDirection() == Direction::UNKNOWN ) {
            if ( MP2::isActionObject( hero.GetMapsObject(), hero.isShipMaster() ) )
                hero.Action( hero.GetIndex(), true );
        }
    }

    void AIWhirlpoolTroopLoseEffect( Heroes & hero )
    {
        Army & heroArmy = hero.GetArmy();

        // Arrange the hero's army for the passage of the whirlpool first
        heroArmy.ArrangeForWhirlpool();

        Troop * weakestTroop = heroArmy.GetWeakestTroop();
        assert( weakestTroop != nullptr );
        if ( weakestTroop == nullptr ) {
            return;
        }

        // Whirlpool effect affects heroes only with more than one creature in more than one slot
        if ( heroArmy.GetCount() == 1 && weakestTroop->GetCount() == 1 ) {
            return;
        }

        if ( 1 == Rand::Get( 1, 3 ) ) {
            if ( weakestTroop->GetCount() == 1 ) {
                weakestTroop->Reset();
            }
            else {
                weakestTroop->SetCount( Monster::GetCountFromHitPoints( weakestTroop->GetID(), weakestTroop->GetHitPoints()
                                                                                                   - weakestTroop->GetHitPoints() * Game::GetWhirlpoolPercent() / 100 ) );
            }
        }
    }

    void AIToJail( const Heroes & hero, const int32_t tileIndex )
    {
        const Kingdom & kingdom = hero.GetKingdom();

        if ( kingdom.GetHeroes().size() < Kingdom::GetMaxHeroes() ) {
            Maps::Tiles & tile = world.GetTiles( tileIndex );

            tile.RemoveObjectSprite();
            tile.setAsEmpty();

            Heroes * prisoner = world.FromJailHeroes( tileIndex );

            if ( prisoner ) {
                prisoner->Recruit( hero.GetColor(), Maps::GetPoint( tileIndex ) );
            }
        }
    }

    void AIToHutMagi( Heroes & hero, const MP2::MapObjectType objectType, const int32_t tileIndex )
    {
        if ( !hero.isObjectTypeVisited( objectType, Visit::GLOBAL ) ) {
            hero.SetVisited( tileIndex, Visit::GLOBAL );
            const MapsIndexes eyeMagiIndexes = Maps::GetObjectPositions( MP2::OBJ_EYEMAGI, true );
            for ( const int32_t index : eyeMagiIndexes ) {
                Maps::ClearFog( index, Game::GetViewDistance( Game::VIEW_MAGI_EYES ), hero.GetColor() );
            }
        }
    }

    void AIToAlchemistTower( Heroes & hero )
    {
        BagArtifacts & bag = hero.GetBagArtifacts();
        const uint32_t cursed = static_cast<uint32_t>( std::count_if( bag.begin(), bag.end(), []( const Artifact & art ) { return art.isAlchemistRemove(); } ) );
        if ( cursed == 0 ) {
            return;
        }

        const payment_t payment = PaymentConditions::ForAlchemist();

        if ( hero.GetKingdom().AllowPayment( payment ) ) {
            hero.GetKingdom().OddFundsResource( payment );

            for ( Artifact & artifact : bag ) {
                if ( artifact.isAlchemistRemove() ) {
                    artifact = Artifact::UNKNOWN;
                }
            }
        }

        DEBUG_LOG( DBG_GAME, DBG_INFO, hero.GetName() << " visited Alchemist Tower to remove " << cursed << " artifacts." );
    }
}
