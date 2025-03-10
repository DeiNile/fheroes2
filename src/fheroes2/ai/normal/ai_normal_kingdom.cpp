/***************************************************************************
 *   Free Heroes of Might and Magic II: https://github.com/ihhub/fheroes2  *
 *   Copyright (C) 2020                                                    *
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

#include <cassert>

#include "agg.h"
#include "ai_normal.h"
#include "game_interface.h"
#include "ground.h"
#include "kingdom.h"
#include "logging.h"
#include "mus.h"
#include "world.h"

namespace
{
    const double fighterStrengthMultipler = 3;

    void setHeroRoles( KingdomHeroes & heroes )
    {
        if ( heroes.empty() ) {
            // No heroes exist.
            return;
        }

        if ( heroes.size() == 1 ) {
            // A single hero has no roles.
            heroes[0]->setAIRole( Heroes::Role::HUNTER );
            return;
        }

        // Set hero's roles. First calculate each hero strength and sort it in descending order.
        std::vector<std::pair<double, Heroes *>> heroStrength;
        for ( Heroes * hero : heroes ) {
            heroStrength.emplace_back( hero->GetArmy().GetStrength(), hero );
        }

        std::sort( heroStrength.begin(), heroStrength.end(),
                   []( const std::pair<double, Heroes *> & first, const std::pair<double, Heroes *> & second ) { return first.first > second.first; } );

        const double medianStrength = heroStrength[heroStrength.size() / 2].first;

        for ( std::pair<double, Heroes *> & hero : heroStrength ) {
            if ( hero.first > medianStrength * fighterStrengthMultipler ) {
                hero.second->setAIRole( Heroes::Role::FIGHTER );
            }
            else {
                hero.second->setAIRole( Heroes::Role::HUNTER );
            }
        }
    }
}

namespace AI
{
    void Normal::KingdomTurn( Kingdom & kingdom )
    {
        const int color = kingdom.GetColor();

        if ( kingdom.isLoss() || color == Color::NONE ) {
            kingdom.LossPostActions();
            return;
        }

        // reset indicator
        Interface::StatusWindow & status = Interface::Basic::Get().GetStatusWindow();
        status.RedrawTurnProgress( 0 );

        AGG::PlayMusic( MUS::COMPUTER_TURN, true, true );

        KingdomHeroes & heroes = kingdom.GetHeroes();
        KingdomCastles & castles = kingdom.GetCastles();

        DEBUG_LOG( DBG_AI, DBG_INFO, Color::String( color ) << " starts the turn: " << castles.size() << " castles, " << heroes.size() << " heroes" );
        DEBUG_LOG( DBG_AI, DBG_TRACE, "Funds: " << kingdom.GetFunds().String() );

        // Step 1. Scan visible map (based on game difficulty), add goals and threats
        std::vector<std::pair<int, const Army *> > enemyArmies;

        const int mapSize = world.w() * world.h();
        _mapObjects.clear();
        _regions.clear();
        _regions.resize( world.getRegionCount() );

        for ( int idx = 0; idx < mapSize; ++idx ) {
            const Maps::Tiles & tile = world.GetTiles( idx );
            const MP2::MapObjectType objectType = tile.GetObject();

            if ( !kingdom.isValidKingdomObject( tile, objectType ) )
                continue;

            const uint32_t regionID = tile.GetRegion();
            if ( regionID >= _regions.size() ) {
                // shouldn't be possible, assert
                assert( regionID < _regions.size() );
                continue;
            }

            RegionStats & stats = _regions[regionID];
            if ( objectType != MP2::OBJ_COAST )
                stats.validObjects.emplace_back( idx, objectType );

            if ( !tile.isFog( color ) ) {
                _mapObjects.emplace_back( idx, objectType );

                const int tileColor = tile.QuantityColor();
                if ( objectType == MP2::OBJ_HEROES ) {
                    const Heroes * hero = tile.GetHeroes();
                    if ( !hero )
                        continue;

                    if ( hero->GetColor() == color ) {
                        ++stats.friendlyHeroCount;
                    }
                    else if ( !Players::isFriends( color, hero->GetColor() ) ) {
                        const Army & heroArmy = hero->GetArmy();
                        enemyArmies.emplace_back( idx, &heroArmy );

                        const double heroThreat = heroArmy.GetStrength();
                        if ( stats.highestThreat < heroThreat ) {
                            stats.highestThreat = heroThreat;
                        }
                    }
                }
                else if ( objectType == MP2::OBJ_CASTLE && tileColor != Color::NONE && !Players::isFriends( color, tileColor ) ) {
                    const Castle * castle = world.getCastleEntrance( Maps::GetPoint( idx ) );
                    if ( !castle )
                        continue;

                    const Army & castleArmy = castle->GetArmy();
                    enemyArmies.emplace_back( idx, &castleArmy );

                    const double castleThreat = castleArmy.GetStrength();
                    if ( stats.highestThreat < castleThreat ) {
                        stats.highestThreat = castleThreat;
                    }
                }
                else if ( objectType == MP2::OBJ_MONSTER ) {
                    stats.averageMonster += Army( tile ).GetStrength();
                    ++stats.monsterCount;
                }
            }
            else {
                ++stats.fogCount;
            }
        }

        DEBUG_LOG( DBG_AI, DBG_TRACE, Color::String( color ) << " found " << _mapObjects.size() << " valid objects" );

        status.RedrawTurnProgress( 1 );

        // Step 2. Update AI variables and recalculate resource budget
        const bool slowEarlyGame = world.CountDay() < 5 && castles.size() == 1;

        for ( auto it = heroes.begin(); it != heroes.end(); ++it ) {
            if ( *it ) {
                _combinedHeroStrength += ( *it )->GetArmy().GetStrength();
            }
        }

        const uint32_t threatDistanceLimit = 2500; // 25 tiles, roughly how much maxed out hero can move in a turn
        std::set<int> castlesInDanger;

        for ( auto enemy = enemyArmies.begin(); enemy != enemyArmies.end(); ++enemy ) {
            if ( enemy->second == nullptr )
                continue;

            const double attackerStrength = enemy->second->GetStrength();

            for ( size_t idx = 0; idx < castles.size(); ++idx ) {
                const Castle * castle = castles[idx];
                if ( castle ) {
                    const int castleIndex = castle->GetIndex();
                    // skip precise distance check if army is too far away to be a threat
                    if ( Maps::GetApproximateDistance( enemy->first, castleIndex ) * Maps::Ground::roadPenalty > threatDistanceLimit )
                        continue;

                    const double defenders = castle->GetArmy().GetStrength();

                    const double attackerThreat = attackerStrength - defenders;
                    if ( attackerThreat > 0 ) {
                        const uint32_t dist = _pathfinder.getDistance( enemy->first, castleIndex, color, attackerStrength );
                        if ( dist && dist < threatDistanceLimit ) {
                            // castle is under threat
                            castlesInDanger.insert( castleIndex );
                        }
                    }
                }
            }
        }

        int32_t heroLimit = world.w() / Maps::SMALL + 1;
        if ( _personality == EXPLORER )
            ++heroLimit;
        if ( slowEarlyGame )
            heroLimit = 2;

        // Step 3. Do some hero stuff.

        // If a hero is standing in a castle most likely he has nothing to do so let's try to give him more army.
        for ( Heroes * hero : heroes ) {
            HeroesActionComplete( *hero );
        }

        setHeroRoles( heroes );

        const bool moreTasksForHeroes = HeroesTurn( heroes );

        status.RedrawTurnProgress( 6 );

        // Step 4. Buy new heroes, adjust roles, sort heroes based on priority or strength

        // sort castles by value: best first
        VecCastles sortedCastleList( castles );
        sortedCastleList.SortByBuildingValue();

        if ( ( moreTasksForHeroes && heroes.size() < static_cast<size_t>( heroLimit ) ) || heroes.empty() ) { // safe to cast as heroLimit is > 0
            const Recruits & rec = kingdom.GetRecruits();
            Castle * recruitmentCastle = nullptr;
            int lowestHeroCount = heroLimit;

            // search for best castle to recruit hero from
            for ( Castle * castle : sortedCastleList ) {
                if ( castle && castle->isCastle() ) {
                    const Heroes * hero = castle->GetHeroes().Guest();
                    const int mapIndex = castle->GetIndex();

                    // Make sure there is no hero in castle already and we're not under threat while having other heroes.
                    if ( hero != nullptr || ( !heroes.empty() && castlesInDanger.find( mapIndex ) != castlesInDanger.end() ) )
                        continue;

                    const uint32_t regionID = world.GetTiles( mapIndex ).GetRegion();
                    const int heroCount = _regions[regionID].friendlyHeroCount;

                    // don't buy a second hero if castle is on locked island
                    if ( world.getRegion( regionID ).getNeighboursCount() == 0 && heroCount > 0 )
                        continue;

                    if ( recruitmentCastle == nullptr || lowestHeroCount > heroCount ) {
                        recruitmentCastle = castle;
                        lowestHeroCount = heroCount;
                        if ( lowestHeroCount == 0 )
                            break;
                    }
                }
            }

            // target found, buy hero
            if ( recruitmentCastle ) {
                Heroes * recruit = nullptr;
                Heroes * firstRecruit = rec.GetHero1();
                Heroes * secondRecruit = rec.GetHero2();
                if ( firstRecruit && secondRecruit && secondRecruit->getRecruitValue() > firstRecruit->getRecruitValue() ) {
                    recruit = recruitmentCastle->RecruitHero( secondRecruit );
                }
                else {
                    recruit = recruitmentCastle->RecruitHero( firstRecruit );
                }

                if ( recruit && !slowEarlyGame ) {
                    CastleTurn( *recruitmentCastle, castlesInDanger.find( recruitmentCastle->GetIndex() ) != castlesInDanger.end() );
                    ReinforceHeroInCastle( *recruit, *recruitmentCastle, kingdom.GetFunds() );
                }
            }
        }

        status.RedrawTurnProgress( 7 );

        // Step 5. Move newly hired heroes if any.
        setHeroRoles( heroes );

        HeroesTurn( heroes );

        status.RedrawTurnProgress( 9 );

        // sync up castle list (if conquered new ones during the turn)
        if ( castles.size() != sortedCastleList.size() ) {
            sortedCastleList = castles;
            sortedCastleList.SortByBuildingValue();
        }

        // Step 6. Castle development according to kingdom budget
        for ( Castle * castle : sortedCastleList ) {
            if ( castle != nullptr ) {
                CastleTurn( *castle, castlesInDanger.find( castle->GetIndex() ) != castlesInDanger.end() );
            }
        }
    }
}
