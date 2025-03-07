/***************************************************************************
 *   Copyright (C) 2009 by Andrey Afletdinov <fheroes2@gmail.com>          *
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
#include <cmath>
#include <numeric>

#include "agg_image.h"
#include "army.h"
#include "campaign_data.h"
#include "campaign_savedata.h"
#include "castle.h"
#include "color.h"
#include "game.h"
#include "heroes.h"
#include "heroes_base.h"
#include "icn.h"
#include "kingdom.h"
#include "logging.h"
#include "luck.h"
#include "maps_tiles.h"
#include "morale.h"
#include "payment.h"
#include "race.h"
#include "rand.h"
#include "screen.h"
#include "serialize.h"
#include "settings.h"
#include "text.h"
#include "tools.h"
#include "translations.h"
#include "world.h"

enum armysize_t
{
    ARMY_FEW = 1,
    ARMY_SEVERAL = 5,
    ARMY_PACK = 10,
    ARMY_LOTS = 20,
    ARMY_HORDE = 50,
    ARMY_THRONG = 100,
    ARMY_SWARM = 250,
    ARMY_ZOUNDS = 500,
    ARMY_LEGION = 1000
};

armysize_t ArmyGetSize( u32 count )
{
    if ( ARMY_LEGION <= count )
        return ARMY_LEGION;
    else if ( ARMY_ZOUNDS <= count )
        return ARMY_ZOUNDS;
    else if ( ARMY_SWARM <= count )
        return ARMY_SWARM;
    else if ( ARMY_THRONG <= count )
        return ARMY_THRONG;
    else if ( ARMY_HORDE <= count )
        return ARMY_HORDE;
    else if ( ARMY_LOTS <= count )
        return ARMY_LOTS;
    else if ( ARMY_PACK <= count )
        return ARMY_PACK;
    else if ( ARMY_SEVERAL <= count )
        return ARMY_SEVERAL;

    return ARMY_FEW;
}

std::string Army::TroopSizeString( const Troop & troop )
{
    std::string str;

    switch ( ArmyGetSize( troop.GetCount() ) ) {
    default:
        str = _( "A few\n%{monster}" );
        break;
    case ARMY_SEVERAL:
        str = _( "Several\n%{monster}" );
        break;
    case ARMY_PACK:
        str = _( "A pack of\n%{monster}" );
        break;
    case ARMY_LOTS:
        str = _( "Lots of\n%{monster}" );
        break;
    case ARMY_HORDE:
        str = _( "A horde of\n%{monster}" );
        break;
    case ARMY_THRONG:
        str = _( "A throng of\n%{monster}" );
        break;
    case ARMY_SWARM:
        str = _( "A swarm of\n%{monster}" );
        break;
    case ARMY_ZOUNDS:
        str = _( "Zounds of\n%{monster}" );
        break;
    case ARMY_LEGION:
        str = _( "A legion of\n%{monster}" );
        break;
    }

    StringReplace( str, "%{monster}", StringLower( troop.GetMultiName() ) );
    return str;
}

std::string Army::SizeString( u32 size )
{
    switch ( ArmyGetSize( size ) ) {
    default:
        break;
    case ARMY_SEVERAL:
        return _( "army|Several" );
    case ARMY_PACK:
        return _( "army|Pack" );
    case ARMY_LOTS:
        return _( "army|Lots" );
    case ARMY_HORDE:
        return _( "army|Horde" );
    case ARMY_THRONG:
        return _( "army|Throng" );
    case ARMY_SWARM:
        return _( "army|Swarm" );
    case ARMY_ZOUNDS:
        return _( "army|Zounds" );
    case ARMY_LEGION:
        return _( "army|Legion" );
    }

    return _( "army|Few" );
}

Troops::Troops( const Troops & troops )
    : std::vector<Troop *>()
{
    *this = troops;
}

Troops & Troops::operator=( const Troops & rhs )
{
    reserve( rhs.size() );
    for ( const_iterator it = rhs.begin(); it != rhs.end(); ++it )
        push_back( new Troop( **it ) );
    return *this;
}

Troops::~Troops()
{
    for ( iterator it = begin(); it != end(); ++it )
        delete *it;
}

size_t Troops::Size( void ) const
{
    return size();
}

void Troops::Assign( const Troop * it1, const Troop * it2 )
{
    Clean();

    iterator it = begin();

    while ( it != end() && it1 != it2 ) {
        if ( it1->isValid() )
            ( *it )->Set( *it1 );
        ++it;
        ++it1;
    }
}

void Troops::Assign( const Troops & troops )
{
    Clean();

    iterator it1 = begin();
    const_iterator it2 = troops.begin();

    while ( it1 != end() && it2 != troops.end() ) {
        if ( ( *it2 )->isValid() )
            ( *it1 )->Set( **it2 );
        ++it2;
        ++it1;
    }
}

void Troops::Insert( const Troops & troops )
{
    for ( const_iterator it = troops.begin(); it != troops.end(); ++it )
        push_back( new Troop( **it ) );
}

void Troops::PushBack( const Monster & mons, u32 count )
{
    push_back( new Troop( mons, count ) );
}

void Troops::PopBack( void )
{
    if ( !empty() ) {
        delete back();
        pop_back();
    }
}

Troop * Troops::GetTroop( size_t pos )
{
    return pos < size() ? at( pos ) : nullptr;
}

const Troop * Troops::GetTroop( size_t pos ) const
{
    return pos < size() ? at( pos ) : nullptr;
}

void Troops::UpgradeMonsters( const Monster & m )
{
    for ( iterator it = begin(); it != end(); ++it ) {
        if ( **it == m && ( *it )->isValid() ) {
            ( *it )->Upgrade();
        }
    }
}

u32 Troops::GetCountMonsters( const Monster & m ) const
{
    u32 c = 0;

    for ( const_iterator it = begin(); it != end(); ++it )
        if ( ( *it )->isValid() && **it == m )
            c += ( *it )->GetCount();

    return c;
}

double Troops::getReinforcementValue( const Troops & reinforcement ) const
{
    // NB items that are added in this vector are all of Troop* type, and not ArmyTroop* type
    // So the GetStrength() computation will be done based on troop strength only (not based on hero bonuses)
    Troops combined( *this );
    const double initialValue = combined.GetStrength();

    combined.Insert( reinforcement.GetOptimized() );
    combined.MergeTroops();
    combined.SortStrongest();

    while ( combined.Size() > ARMYMAXTROOPS ) {
        combined.PopBack();
    }

    return combined.GetStrength() - initialValue;
}

bool Troops::isValid( void ) const
{
    for ( const_iterator it = begin(); it != end(); ++it ) {
        if ( ( *it )->isValid() )
            return true;
    }
    return false;
}

u32 Troops::GetCount( void ) const
{
    uint32_t total = 0;
    for ( const_iterator it = begin(); it != end(); ++it ) {
        if ( ( *it )->isValid() )
            ++total;
    }
    return total;
}

bool Troops::HasMonster( const Monster & mons ) const
{
    const int monsterID = mons.GetID();
    for ( const_iterator it = begin(); it != end(); ++it ) {
        if ( ( *it )->isValid() && ( *it )->isMonster( monsterID ) ) {
            return true;
        }
    }
    return false;
}

bool Troops::AllTroopsAreUndead() const
{
    for ( const_iterator it = begin(); it != end(); ++it ) {
        if ( ( *it )->isValid() && !( *it )->isUndead() ) {
            return false;
        }
    }

    return true;
}

bool Troops::CanJoinTroop( const Monster & mons ) const
{
    return std::any_of( begin(), end(), [&mons]( const Troop * troop ) { return troop->isMonster( mons.GetID() ); } )
           || std::any_of( begin(), end(), []( const Troop * troop ) { return !troop->isValid(); } );
}

bool Troops::JoinTroop( const Monster & mons, uint32_t count, bool emptySlotFirst )
{
    if ( mons.isValid() && count ) {
        auto findEmptySlot = []( const Troop * troop ) { return !troop->isValid(); };
        auto findMonster = [&mons]( const Troop * troop ) { return troop->isValid() && troop->isMonster( mons.GetID() ); };

        iterator it = emptySlotFirst ? std::find_if( begin(), end(), findEmptySlot ) : std::find_if( begin(), end(), findMonster );
        if ( it == end() ) {
            it = emptySlotFirst ? std::find_if( begin(), end(), findMonster ) : std::find_if( begin(), end(), findEmptySlot );
        }

        if ( it != end() ) {
            if ( ( *it )->isValid() )
                ( *it )->SetCount( ( *it )->GetCount() + count );
            else
                ( *it )->Set( mons, count );

            DEBUG_LOG( DBG_GAME, DBG_INFO, std::dec << count << " " << ( *it )->GetName() );
            return true;
        }
    }

    return false;
}

bool Troops::JoinTroop( const Troop & troop )
{
    return troop.isValid() ? JoinTroop( troop.GetMonster(), troop.GetCount() ) : false;
}

bool Troops::CanJoinTroops( const Troops & troops2 ) const
{
    if ( this == &troops2 )
        return false;

    Troops troops1;
    troops1.Insert( *this );

    for ( const_iterator it = troops2.begin(); it != troops2.end(); ++it ) {
        if ( ( *it )->isValid() && !troops1.JoinTroop( **it ) ) {
            return false;
        }
    }

    return true;
}

void Troops::JoinTroops( Troops & troops2 )
{
    if ( this == &troops2 )
        return;

    for ( iterator it = troops2.begin(); it != troops2.end(); ++it )
        if ( ( *it )->isValid() ) {
            JoinTroop( **it );
            ( *it )->Reset();
        }
}

void Troops::MoveTroops( const Troops & from )
{
    if ( this == &from )
        return;

    size_t validTroops = 0;
    for ( const Troop * troop : from ) {
        if ( troop && troop->isValid() ) {
            ++validTroops;
        }
    }

    for ( Troop * troop : from ) {
        if ( troop && troop->isValid() ) {
            if ( validTroops == 1 ) {
                if ( JoinTroop( troop->GetMonster(), troop->GetCount() - 1 ) ) {
                    troop->SetCount( 1 );
                    break;
                }
            }
            else if ( JoinTroop( *troop ) ) {
                --validTroops;
                troop->Reset();
            }
        }
    }
}

// Return true when all valid troops have the same ID, or when there are no troops
bool Troops::AllTroopsAreTheSame( void ) const
{
    int firstMonsterId = Monster::UNKNOWN;
    for ( const Troop * troop : *this ) {
        if ( troop->isValid() ) {
            if ( firstMonsterId == Monster::UNKNOWN ) {
                firstMonsterId = troop->GetID();
            }
            else if ( troop->GetID() != firstMonsterId ) {
                return false;
            }
        }
    }
    return true;
}

double Troops::GetStrength() const
{
    double strength = 0;
    for ( const Troop * troop : *this ) {
        if ( troop && troop->isValid() )
            strength += troop->GetStrength();
    }
    return strength;
}

void Troops::Clean( void )
{
    std::for_each( begin(), end(), []( Troop * troop ) { troop->Reset(); } );
}

void Troops::UpgradeTroops( const Castle & castle )
{
    for ( iterator it = begin(); it != end(); ++it )
        if ( ( *it )->isValid() ) {
            payment_t payment = ( *it )->GetUpgradeCost();
            Kingdom & kingdom = castle.GetKingdom();

            if ( castle.GetRace() == ( *it )->GetRace() && castle.isBuild( ( *it )->GetUpgrade().GetDwelling() ) && kingdom.AllowPayment( payment ) ) {
                kingdom.OddFundsResource( payment );
                ( *it )->Upgrade();
            }
        }
}

Troop * Troops::GetFirstValid( void )
{
    iterator it = std::find_if( begin(), end(), []( const Troop * troop ) { return troop->isValid(); } );
    return it == end() ? nullptr : *it;
}

Troop * Troops::GetWeakestTroop() const
{
    const_iterator first = begin();
    const_iterator last = end();

    while ( first != last )
        if ( ( *first )->isValid() )
            break;
        else
            ++first;

    if ( first == end() )
        return nullptr;

    const_iterator lowest = first;

    if ( first != last )
        while ( ++first != last )
            if ( ( *first )->isValid() && Army::WeakestTroop( *first, *lowest ) )
                lowest = first;

    return *lowest;
}

const Troop * Troops::GetSlowestTroop() const
{
    const_iterator first = begin();
    const_iterator last = end();

    while ( first != last )
        if ( ( *first )->isValid() )
            break;
        else
            ++first;

    if ( first == end() )
        return nullptr;
    const_iterator lowest = first;

    if ( first != last )
        while ( ++first != last )
            if ( ( *first )->isValid() && Army::SlowestTroop( *first, *lowest ) )
                lowest = first;

    return *lowest;
}

void Troops::MergeTroops()
{
    for ( size_t slot = 0; slot < size(); ++slot ) {
        Troop * troop = at( slot );
        if ( !troop || !troop->isValid() )
            continue;

        const int id = troop->GetID();
        for ( size_t secondary = slot + 1; secondary < size(); ++secondary ) {
            Troop * secondaryTroop = at( secondary );
            if ( secondaryTroop && secondaryTroop->isValid() && id == secondaryTroop->GetID() ) {
                troop->SetCount( troop->GetCount() + secondaryTroop->GetCount() );
                secondaryTroop->Reset();
            }
        }
    }
}

Troops Troops::GetOptimized( void ) const
{
    Troops result;
    result.reserve( size() );

    for ( const_iterator it1 = begin(); it1 != end(); ++it1 )
        if ( ( *it1 )->isValid() ) {
            const int monsterId = ( *it1 )->GetID();
            iterator it2 = std::find_if( result.begin(), result.end(), [monsterId]( const Troop * troop ) { return troop->isMonster( monsterId ); } );

            if ( it2 == result.end() )
                result.push_back( new Troop( **it1 ) );
            else
                ( *it2 )->SetCount( ( *it2 )->GetCount() + ( *it1 )->GetCount() );
        }

    return result;
}

void Troops::SortStrongest()
{
    std::sort( begin(), end(), Army::StrongestTroop );
}

// Pre-battle arrangement for Monster or Neutral troops
void Troops::ArrangeForBattle( bool upgrade )
{
    const Troops & priority = GetOptimized();

    if ( priority.size() == 1 ) {
        const Monster & m = *priority.back();
        const u32 count = priority.back()->GetCount();

        Clean();

        if ( 49 < count ) {
            const u32 c = count / 5;
            at( 0 )->Set( m, c );
            at( 1 )->Set( m, c );
            at( 2 )->Set( m, c + count - ( c * 5 ) );
            at( 3 )->Set( m, c );
            at( 4 )->Set( m, c );

            if ( upgrade && at( 2 )->isAllowUpgrade() )
                at( 2 )->Upgrade();
        }
        else if ( 20 < count ) {
            const u32 c = count / 3;
            at( 1 )->Set( m, c );
            at( 2 )->Set( m, c + count - ( c * 3 ) );
            at( 3 )->Set( m, c );

            if ( upgrade && at( 2 )->isAllowUpgrade() )
                at( 2 )->Upgrade();
        }
        else
            at( 2 )->Set( m, count );
    }
    else {
        Assign( priority );
    }
}

void Troops::ArrangeForWhirlpool()
{
    // Make an "optimized" version first (each unit type occupies just one slot)
    const Troops optimizedTroops = GetOptimized();
    assert( optimizedTroops.size() > 0 && optimizedTroops.size() <= ARMYMAXTROOPS );

    // Already a full house, there is no room for further optimization
    if ( optimizedTroops.size() == ARMYMAXTROOPS ) {
        return;
    }

    Assign( optimizedTroops );

    // Look for a troop consisting of the weakest units
    Troop * troopOfWeakestUnits = nullptr;

    for ( Troop * troop : *this ) {
        assert( troop != nullptr );

        if ( !troop->isValid() ) {
            continue;
        }

        if ( troopOfWeakestUnits == nullptr || troopOfWeakestUnits->Monster::GetHitPoints() > troop->Monster::GetHitPoints() ) {
            troopOfWeakestUnits = troop;
        }
    }

    assert( troopOfWeakestUnits != nullptr );
    assert( troopOfWeakestUnits->GetCount() > 0 );

    // There is already just one unit in this troop, let's leave it as it is
    if ( troopOfWeakestUnits->GetCount() == 1 ) {
        return;
    }

    // Move one unit from this troop's slot...
    troopOfWeakestUnits->SetCount( troopOfWeakestUnits->GetCount() - 1 );

    // To the separate slot
    auto emptySlot = std::find_if( begin(), end(), []( const Troop * troop ) { return !troop->isValid(); } );
    assert( emptySlot != end() );

    ( *emptySlot )->Set( Monster( troopOfWeakestUnits->GetID() ), 1 );
}

void Troops::JoinStrongest( Troops & troops2, bool saveLast )
{
    if ( this == &troops2 )
        return;

    // validate the size (can be different from ARMYMAXTROOPS)
    if ( troops2.size() < size() )
        troops2.resize( size() );

    // first try to keep units in the same slots
    for ( size_t slot = 0; slot < size(); ++slot ) {
        Troop * leftTroop = at( slot );
        Troop * rightTroop = troops2[slot];
        if ( rightTroop && rightTroop->isValid() ) {
            if ( !leftTroop->isValid() ) {
                // if slot is empty, simply move the unit
                leftTroop->Set( *rightTroop );
                rightTroop->Reset();
            }
            else if ( leftTroop->GetID() == rightTroop->GetID() ) {
                // check if we can merge them
                leftTroop->SetCount( leftTroop->GetCount() + rightTroop->GetCount() );
                rightTroop->Reset();
            }
        }
    }

    // there's still unmerged units left and there's empty room for them
    for ( size_t slot = 0; slot < troops2.size(); ++slot ) {
        Troop * rightTroop = troops2[slot];
        if ( rightTroop && JoinTroop( rightTroop->GetMonster(), rightTroop->GetCount(), true ) ) {
            rightTroop->Reset();
        }
    }

    // if there's more units than slots, start optimizing
    if ( troops2.GetCount() ) {
        Troops rightPriority = troops2.GetOptimized();
        troops2.Clean();
        // strongest at the end
        std::sort( rightPriority.begin(), rightPriority.end(), Army::WeakestTroop );

        // 1. Merge any remaining stacks to free some space
        MergeTroops();

        // 2. Fill empty slots with best troops (if there are any)
        uint32_t count = GetCount();
        while ( count < ARMYMAXTROOPS && !rightPriority.empty() ) {
            JoinTroop( *rightPriority.back() );
            rightPriority.PopBack();
            ++count;
        }

        // 3. Swap weakest and strongest unit until there's no left
        while ( !rightPriority.empty() ) {
            Troop * weakest = GetWeakestTroop();

            if ( !weakest || Army::StrongestTroop( weakest, rightPriority.back() ) ) {
                // we're done processing if second army units are weaker
                break;
            }

            Army::SwapTroops( *weakest, *rightPriority.back() );
            std::sort( rightPriority.begin(), rightPriority.end(), Army::WeakestTroop );
        }

        // 4. The rest goes back to second army
        while ( !rightPriority.empty() ) {
            troops2.JoinTroop( *rightPriority.back() );
            rightPriority.PopBack();
        }
    }

    // save weakest unit to army2 (for heroes)
    if ( saveLast && !troops2.isValid() ) {
        Troop * weakest = GetWeakestTroop();

        if ( weakest && weakest->isValid() ) {
            troops2.JoinTroop( *weakest, 1 );
            weakest->SetCount( weakest->GetCount() - 1 );
        }
    }
}

void Troops::DrawMons32Line( int32_t cx, int32_t cy, uint32_t width, uint32_t first, uint32_t count, uint32_t drawPower, bool compact, bool isScouteView ) const
{
    if ( isValid() ) {
        if ( 0 == count )
            count = GetCount();

        const int chunk = width / count;
        if ( !compact )
            cx += chunk / 2;

        Text text;
        text.Set( Font::SMALL );

        for ( const_iterator it = begin(); it != end(); ++it ) {
            if ( ( *it )->isValid() ) {
                if ( 0 == first && count ) {
                    const fheroes2::Sprite & monster = fheroes2::AGG::GetICN( ICN::MONS32, ( *it )->GetSpriteIndex() );
                    text.Set( isScouteView ? Game::CountScoute( ( *it )->GetCount(), drawPower, compact ) : Game::CountThievesGuild( ( *it )->GetCount(), drawPower ) );

                    if ( compact ) {
                        const int offsetY = ( monster.height() < 37 ) ? 37 - monster.height() : 0;
                        int offset = ( chunk - monster.width() - text.w() ) / 2;
                        if ( offset < 0 )
                            offset = 0;
                        fheroes2::Blit( monster, fheroes2::Display::instance(), cx + offset, cy + offsetY + monster.y() );
                        text.Blit( cx + chunk - text.w() - offset, cy + 23 );
                    }
                    else {
                        const int offsetY = 30 - monster.height();
                        fheroes2::Blit( monster, fheroes2::Display::instance(), cx - monster.width() / 2 + monster.x(), cy + offsetY + monster.y() );
                        text.Blit( cx - text.w() / 2, cy + 29 );
                    }
                    cx += chunk;
                    --count;
                }
                else
                    --first;
            }
        }
    }
}

void Troops::SplitTroopIntoFreeSlots( const Troop & troop, const Troop & selectedSlot, const uint32_t slots )
{
    if ( slots < 1 || slots > ( Size() - GetCount() ) )
        return;

    const uint32_t chunk = troop.GetCount() / slots;
    uint32_t remainingCount = troop.GetCount() % slots;
    uint32_t remainingSlots = slots;

    auto TryCreateTroopChunk = [&remainingSlots, &remainingCount, chunk, troop]( Troop & newTroop ) {
        if ( remainingSlots <= 0 )
            return;

        if ( !newTroop.isValid() ) {
            newTroop.Set( troop.GetMonster(), remainingCount > 0 ? chunk + 1 : chunk );
            --remainingSlots;

            if ( remainingCount > 0 )
                --remainingCount;
        }
    };

    const iterator selectedSlotIterator = std::find( begin(), end(), &selectedSlot );

    // this means the selected slot is actually not part of the army, which is not the intended logic
    if ( selectedSlotIterator == end() )
        return;

    const size_t iteratorIndex = selectedSlotIterator - begin();

    // try to create chunks to the right of the selected slot
    for ( size_t i = iteratorIndex + 1; i < Size(); ++i ) {
        TryCreateTroopChunk( *GetTroop( i ) );
    }

    // this time, try to create chunks to the left of the selected slot
    for ( int i = static_cast<int>( iteratorIndex ) - 1; i >= 0; --i ) {
        TryCreateTroopChunk( *GetTroop( i ) );
    }
}

void Troops::AssignToFirstFreeSlot( const Troop & troop, const uint32_t splitCount )
{
    for ( iterator it = begin(); it != end(); ++it ) {
        if ( ( *it )->isValid() )
            continue;

        ( *it )->Set( troop.GetMonster(), splitCount );
        break;
    }
}

void Troops::JoinAllTroopsOfType( const Troop & targetTroop )
{
    const int troopID = targetTroop.GetID();
    const int totalMonsterCount = GetCountMonsters( troopID );

    for ( iterator it = begin(); it != end(); ++it ) {
        Troop * troop = *it;
        if ( !troop->isValid() || troop->GetID() != troopID )
            continue;

        if ( troop == &targetTroop ) {
            troop->SetCount( totalMonsterCount );
        }
        else {
            troop->Reset();
        }
    }
}

Army::Army( HeroBase * s )
    : commander( s )
    , combat_format( true )
    , color( Color::NONE )
{
    reserve( ARMYMAXTROOPS );
    for ( u32 ii = 0; ii < ARMYMAXTROOPS; ++ii )
        push_back( new ArmyTroop( this ) );
}

Army::Army( const Maps::Tiles & t )
    : commander( nullptr )
    , combat_format( true )
    , color( Color::NONE )
{
    reserve( ARMYMAXTROOPS );
    for ( u32 ii = 0; ii < ARMYMAXTROOPS; ++ii )
        push_back( new ArmyTroop( this ) );

    setFromTile( t );
}

Army::~Army()
{
    for ( iterator it = begin(); it != end(); ++it )
        delete *it;
    clear();
}

void Army::setFromTile( const Maps::Tiles & tile )
{
    Reset();

    const bool isCaptureObject = MP2::isCaptureObject( tile.GetObject() );
    if ( isCaptureObject )
        color = tile.QuantityColor();

    switch ( tile.GetObject( false ) ) {
    case MP2::OBJ_PYRAMID:
        at( 0 )->Set( Monster::VAMPIRE_LORD, 10 );
        at( 1 )->Set( Monster::ROYAL_MUMMY, 10 );
        at( 2 )->Set( Monster::ROYAL_MUMMY, 10 );
        at( 3 )->Set( Monster::ROYAL_MUMMY, 10 );
        at( 4 )->Set( Monster::VAMPIRE_LORD, 10 );
        break;

    case MP2::OBJ_GRAVEYARD:
        at( 0 )->Set( Monster::MUTANT_ZOMBIE, 100 );
        ArrangeForBattle( false );
        break;

    case MP2::OBJ_SHIPWRECK:
        at( 0 )->Set( Monster::GHOST, tile.GetQuantity2() );
        ArrangeForBattle( false );
        break;

    case MP2::OBJ_DERELICTSHIP:
        at( 0 )->Set( Monster::SKELETON, 200 );
        ArrangeForBattle( false );
        break;

    case MP2::OBJ_ARTIFACT:
        switch ( tile.QuantityVariant() ) {
        case 6:
            at( 0 )->Set( Monster::ROGUE, 50 );
            break;
        case 7:
            at( 0 )->Set( Monster::GENIE, 1 );
            break;
        case 8:
            at( 0 )->Set( Monster::PALADIN, 1 );
            break;
        case 9:
            at( 0 )->Set( Monster::CYCLOPS, 1 );
            break;
        case 10:
            at( 0 )->Set( Monster::PHOENIX, 1 );
            break;
        case 11:
            at( 0 )->Set( Monster::GREEN_DRAGON, 1 );
            break;
        case 12:
            at( 0 )->Set( Monster::TITAN, 1 );
            break;
        case 13:
            at( 0 )->Set( Monster::BONE_DRAGON, 1 );
            break;
        default:
            break;
        }
        ArrangeForBattle( false );
        break;

        // case MP2::OBJ_ABANDONEDMINE:
        //    at(0) = Troop(t);
        //    ArrangeForBattle(false);
        //    break;

    case MP2::OBJ_CITYDEAD:
        at( 0 )->Set( Monster::ZOMBIE, 20 );
        at( 1 )->Set( Monster::VAMPIRE_LORD, 5 );
        at( 2 )->Set( Monster::POWER_LICH, 5 );
        at( 3 )->Set( Monster::VAMPIRE_LORD, 5 );
        at( 4 )->Set( Monster::ZOMBIE, 20 );
        break;

    case MP2::OBJ_TROLLBRIDGE:
        at( 0 )->Set( Monster::TROLL, 4 );
        at( 1 )->Set( Monster::WAR_TROLL, 4 );
        at( 2 )->Set( Monster::TROLL, 4 );
        at( 3 )->Set( Monster::WAR_TROLL, 4 );
        at( 4 )->Set( Monster::TROLL, 4 );
        break;

    case MP2::OBJ_DRAGONCITY: {
        uint32_t monsterCount = 1;
        if ( Settings::Get().isCampaignGameType() ) {
            const Campaign::ScenarioVictoryCondition victoryCondition = Campaign::getCurrentScenarioVictoryCondition();
            if ( victoryCondition == Campaign::ScenarioVictoryCondition::CAPTURE_DRAGON_CITY ) {
                monsterCount = 2;
            }
        }

        at( 0 )->Set( Monster::GREEN_DRAGON, monsterCount );
        at( 1 )->Set( Monster::GREEN_DRAGON, monsterCount );
        at( 2 )->Set( Monster::GREEN_DRAGON, monsterCount );
        at( 3 )->Set( Monster::RED_DRAGON, monsterCount );
        at( 4 )->Set( Monster::BLACK_DRAGON, monsterCount );
        break;
    }

    case MP2::OBJ_DAEMONCAVE:
        at( 0 )->Set( Monster::EARTH_ELEMENT, 2 );
        at( 1 )->Set( Monster::EARTH_ELEMENT, 2 );
        at( 2 )->Set( Monster::EARTH_ELEMENT, 2 );
        at( 3 )->Set( Monster::EARTH_ELEMENT, 2 );
        break;

    default:
        if ( isCaptureObject ) {
            CapturedObject & co = world.GetCapturedObject( tile.GetIndex() );
            const Troop & troop = co.GetTroop();

            switch ( co.GetSplit() ) {
            case 3:
                if ( 3 > troop.GetCount() )
                    at( 0 )->Set( co.GetTroop() );
                else {
                    at( 0 )->Set( troop.GetMonster(), troop.GetCount() / 3 );
                    at( 4 )->Set( troop.GetMonster(), troop.GetCount() / 3 );
                    at( 2 )->Set( troop.GetMonster(), troop.GetCount() - at( 4 )->GetCount() - at( 0 )->GetCount() );
                }
                break;

            case 5:
                if ( 5 > troop.GetCount() )
                    at( 0 )->Set( co.GetTroop() );
                else {
                    at( 0 )->Set( troop.GetMonster(), troop.GetCount() / 5 );
                    at( 1 )->Set( troop.GetMonster(), troop.GetCount() / 5 );
                    at( 3 )->Set( troop.GetMonster(), troop.GetCount() / 5 );
                    at( 4 )->Set( troop.GetMonster(), troop.GetCount() / 5 );
                    at( 2 )->Set( troop.GetMonster(), troop.GetCount() - at( 0 )->GetCount() - at( 1 )->GetCount() - at( 3 )->GetCount() - at( 4 )->GetCount() );
                }
                break;

            default:
                at( 0 )->Set( co.GetTroop() );
                break;
            }
        }
        else {
            Troop troop = tile.QuantityTroop();

            at( 0 )->Set( troop );
            if ( troop.isValid() )
                ArrangeForBattle( true );
        }
        break;
    }
}

bool Army::isFullHouse( void ) const
{
    return GetCount() == size();
}

void Army::SetSpreadFormat( bool f )
{
    combat_format = f;
}

bool Army::isSpreadFormat( void ) const
{
    return combat_format;
}

int Army::GetColor( void ) const
{
    const HeroBase * currentCommander = GetCommander();
    return currentCommander != nullptr ? currentCommander->GetColor() : color;
}

void Army::SetColor( int cl )
{
    color = cl;
}

int Army::GetLuck( void ) const
{
    const HeroBase * currentCommander = GetCommander();
    return currentCommander != nullptr ? currentCommander->GetLuck() : GetLuckModificator( nullptr );
}

int Army::GetLuckModificator( const std::string * ) const
{
    return Luck::NORMAL;
}

int Army::GetMorale( void ) const
{
    const HeroBase * currentCommander = GetCommander();
    return currentCommander != nullptr ? currentCommander->GetMorale() : GetMoraleModificator( nullptr );
}

int Army::GetMoraleModificator( std::string * strs ) const
{
    // different race penalty
    std::set<int> races;
    bool hasUndead = false;
    bool allUndead = true;

    for ( const Troop * troop : *this )
        if ( troop->isValid() ) {
            races.insert( troop->GetRace() );
            hasUndead = hasUndead || troop->isUndead();
            allUndead = allUndead && troop->isUndead();
        }

    if ( allUndead )
        return Morale::NORMAL;

    int result = Morale::NORMAL;

    // artifact "Arm of the Martyr" adds the undead morale penalty
    hasUndead = hasUndead || ( GetCommander() && GetCommander()->hasArtifact( Artifact::ARM_MARTYR ) );

    const int count = static_cast<int>( races.size() );
    switch ( count ) {
    case 0:
    case 2:
        break;
    case 1:
        if ( !hasUndead && !AllTroopsAreTheSame() ) { // presence of undead discards "All %{race} troops +1" bonus
            ++result;
            if ( strs ) {
                std::string str = _( "All %{race} troops +1" );
                StringReplace( str, "%{race}", *races.begin() == Race::NONE ? _( "Multiple" ) : Race::String( *races.begin() ) );
                strs->append( str );
                *strs += '\n';
            }
        }
        break;
    default:
        const int penalty = count - 2;
        result -= penalty;
        if ( strs ) {
            std::string str = _( "Troops of %{count} alignments -%{penalty}" );
            StringReplace( str, "%{count}", count );
            StringReplace( str, "%{penalty}", penalty );
            strs->append( str );
            *strs += '\n';
        }
        break;
    }

    // undead in life group
    if ( hasUndead ) {
        result -= 1;
        if ( strs ) {
            strs->append( _( "Some undead in group -1" ) );
            *strs += '\n';
        }
    }

    return result;
}

double Army::GetStrength() const
{
    double result = 0;
    const uint32_t archery = ( commander ) ? commander->GetSecondaryValues( Skill::Secondary::ARCHERY ) : 0;
    // Hero bonus calculation is slow, cache it
    const int bonusAttack = ( commander ? commander->GetAttack() : 0 );
    const int bonusDefense = ( commander ? commander->GetDefense() : 0 );
    const int armyMorale = GetMorale();
    const int armyLuck = GetLuck();

    for ( const_iterator it = begin(); it != end(); ++it ) {
        const Troop * troop = *it;
        if ( troop != nullptr && troop->isValid() ) {
            double strength = troop->GetStrengthWithBonus( bonusAttack, bonusDefense );

            if ( archery > 0 && troop->isArchers() ) {
                strength *= sqrt( 1 + static_cast<double>( archery ) / 100 );
            }

            // GetMorale checks if unit is affected by it
            if ( troop->isAffectedByMorale() )
                strength *= 1 + ( ( armyMorale < 0 ) ? armyMorale / 12.0 : armyMorale / 24.0 );

            strength *= 1 + armyLuck / 24.0;

            result += strength;
        }
    }

    if ( commander ) {
        result += commander->GetSpellcastStrength( result );
    }

    return result;
}

void Army::Reset( bool soft )
{
    Troops::Clean();

    if ( commander && commander->isHeroes() ) {
        const Monster mons1( commander->GetRace(), DWELLING_MONSTER1 );

        if ( soft ) {
            const Monster mons2( commander->GetRace(), DWELLING_MONSTER2 );

            switch ( mons1.GetID() ) {
            case Monster::PEASANT:
                JoinTroop( mons1, Rand::Get( 30, 50 ) );
                break;
            case Monster::GOBLIN:
                JoinTroop( mons1, Rand::Get( 15, 25 ) );
                break;
            case Monster::SPRITE:
                JoinTroop( mons1, Rand::Get( 10, 20 ) );
                break;
            default:
                JoinTroop( mons1, Rand::Get( 6, 10 ) );
                break;
            }

            if ( Rand::Get( 1, 10 ) != 1 ) {
                switch ( mons2.GetID() ) {
                case Monster::ARCHER:
                case Monster::ORC:
                    JoinTroop( mons2, Rand::Get( 3, 5 ) );
                    break;
                default:
                    JoinTroop( mons2, Rand::Get( 2, 4 ) );
                    break;
                }
            }
        }
        else {
            JoinTroop( mons1, 1 );
        }
    }
}

void Army::SetCommander( HeroBase * c )
{
    commander = c;
}

HeroBase * Army::GetCommander( void )
{
    return ( !commander || ( commander->isCaptain() && !commander->isValid() ) ) ? nullptr : commander;
}

const Castle * Army::inCastle( void ) const
{
    return commander ? commander->inCastle() : nullptr;
}

const HeroBase * Army::GetCommander( void ) const
{
    return ( !commander || ( commander->isCaptain() && !commander->isValid() ) ) ? nullptr : commander;
}

int Army::GetControl( void ) const
{
    return commander ? commander->GetControl() : ( color == Color::NONE ? CONTROL_AI : Players::GetPlayerControl( color ) );
}

uint32_t Army::getTotalCount() const
{
    return std::accumulate( begin(), end(), 0u, []( const uint32_t count, const Troop * troop ) { return troop->isValid() ? count + troop->GetCount() : count; } );
}

std::string Army::String( void ) const
{
    std::ostringstream os;

    os << "color(" << Color::String( commander ? commander->GetColor() : color ) << "), ";

    if ( GetCommander() )
        os << "commander(" << GetCommander()->GetName() << "), ";

    os << ": ";

    for ( const_iterator it = begin(); it != end(); ++it )
        if ( ( *it )->isValid() )
            os << std::dec << ( *it )->GetCount() << " " << ( *it )->GetName() << ", ";

    return os.str();
}

void Army::JoinStrongestFromArmy( Army & army2 )
{
    bool save_last = army2.commander && army2.commander->isHeroes();
    JoinStrongest( army2, save_last );
}

u32 Army::ActionToSirens( void )
{
    u32 res = 0;

    for ( iterator it = begin(); it != end(); ++it )
        if ( ( *it )->isValid() ) {
            const u32 kill = ( *it )->GetCount() * 30 / 100;

            if ( kill ) {
                ( *it )->SetCount( ( *it )->GetCount() - kill );
                res += kill * static_cast<Monster *>( *it )->GetHitPoints();
            }
        }

    return res;
}

bool Army::isStrongerThan( const Army & target, double safetyRatio ) const
{
    if ( !target.isValid() )
        return true;

    const double str1 = GetStrength();
    const double str2 = target.GetStrength() * safetyRatio;

    DEBUG_LOG( DBG_GAME, DBG_TRACE, "Comparing troops: " << str1 << " versus " << str2 );

    return str1 > str2;
}

bool Army::isMeleeDominantArmy() const
{
    double meleeInfantry = 0;
    double other = 0;

    for ( const Troop * troop : *this ) {
        if ( troop != nullptr && troop->isValid() ) {
            if ( !troop->isArchers() && !troop->isFlying() ) {
                meleeInfantry += troop->GetStrength();
            }
            else {
                other += troop->GetStrength();
            }
        }
    }
    return meleeInfantry > other;
}

/* draw MONS32 sprite in line, first valid = 0, count = 0 */
void Army::DrawMons32Line( const Troops & troops, s32 cx, s32 cy, u32 width, u32 first, u32 count )
{
    troops.DrawMons32Line( cx, cy, width, first, count, Skill::Level::EXPERT, false, true );
}

void Army::DrawMonsterLines( const Troops & troops, int32_t posX, int32_t posY, uint32_t lineWidth, uint32_t drawType, bool compact, bool isScouteView )
{
    const uint32_t count = troops.GetCount();
    const int offsetX = lineWidth / 6;
    const int offsetY = compact ? 31 : 50;

    if ( count < 3 ) {
        troops.DrawMons32Line( posX + offsetX, posY + offsetY / 2 + 1, lineWidth * 2 / 3, 0, 0, drawType, compact, isScouteView );
    }
    else {
        const int firstLineTroopCount = 2;
        const int secondLineTroopCount = count - firstLineTroopCount;
        const int secondLineWidth = secondLineTroopCount == 2 ? lineWidth * 2 / 3 : lineWidth;

        troops.DrawMons32Line( posX + offsetX, posY, lineWidth * 2 / 3, 0, firstLineTroopCount, drawType, compact, isScouteView );
        troops.DrawMons32Line( posX, posY + offsetY, secondLineWidth, firstLineTroopCount, secondLineTroopCount, drawType, compact, isScouteView );
    }
}

NeutralMonsterJoiningCondition Army::GetJoinSolution( const Heroes & hero, const Maps::Tiles & tile, const Troop & troop )
{
    // Check for creature alliance/bane campaign awards, campaign only and of course, for human players
    // creature alliance -> if we have an alliance with the appropriate creature (inc. players) they will join for free
    // creature curse/bane -> same as above but all of them will flee even if you have just 1 peasant
    if ( Settings::Get().isCampaignGameType() && hero.isControlHuman() ) {
        const std::vector<Campaign::CampaignAwardData> campaignAwards = Campaign::CampaignSaveData::Get().getObtainedCampaignAwards();

        for ( size_t i = 0; i < campaignAwards.size(); ++i ) {
            const bool isAlliance = campaignAwards[i]._type == Campaign::CampaignAwardData::TYPE_CREATURE_ALLIANCE;
            const bool isCurse = campaignAwards[i]._type == Campaign::CampaignAwardData::TYPE_CREATURE_CURSE;

            if ( !isAlliance && !isCurse )
                continue;

            Monster monster( campaignAwards[i]._subType );
            while ( true ) {
                if ( troop.GetID() == monster.GetID() ) {
                    if ( isAlliance ) {
                        return { NeutralMonsterJoiningCondition::Reason::Alliance, troop.GetCount(),
                                 Campaign::CampaignAwardData::getAllianceJoiningMessage( monster.GetID() ),
                                 Campaign::CampaignAwardData::getAllianceFleeingMessage( monster.GetID() ) };
                    }
                    else {
                        return { NeutralMonsterJoiningCondition::Reason::Bane, troop.GetCount(), nullptr,
                                 Campaign::CampaignAwardData::getBaneFleeingMessage( monster.GetID() ) };
                    }
                }

                // try to cycle through the creature's upgrades
                if ( !monster.isAllowUpgrade() )
                    break;

                monster = monster.GetUpgrade();
            }
        }
    }

    if ( hero.hasArtifact( Artifact::HIDEOUS_MASK ) ) {
        return { NeutralMonsterJoiningCondition::Reason::None, 0, nullptr, nullptr };
    }

    if ( tile.MonsterJoinConditionSkip() || !troop.isValid() ) {
        return { NeutralMonsterJoiningCondition::Reason::None, 0, nullptr, nullptr };
    }

    // Neutral monsters don't care about hero's stats. Ignoring hero's stats makes hero's army strength be smaller in eyes of neutrals and they won't join so often.
    const double armyStrengthRatio = static_cast<const Troops &>( hero.GetArmy() ).GetStrength() / troop.GetStrength();

    if ( armyStrengthRatio > 2 ) {
        if ( tile.MonsterJoinConditionFree() ) {
            return { NeutralMonsterJoiningCondition::Reason::Free, troop.GetCount(), nullptr, nullptr };
        }

        if ( hero.HasSecondarySkill( Skill::Secondary::DIPLOMACY ) ) {
            const uint32_t amountToJoin = Monster::GetCountFromHitPoints( troop, troop.GetHitPoints() * hero.GetSecondaryValues( Skill::Secondary::DIPLOMACY ) / 100 );
            if ( amountToJoin > 0 ) {
                return { NeutralMonsterJoiningCondition::Reason::ForMoney, amountToJoin, nullptr, nullptr };
            }
        }
    }

    if ( armyStrengthRatio > 5 && !hero.isControlAI() ) {
        // ... surely flee before us
        return { NeutralMonsterJoiningCondition::Reason::RunAway, 0, nullptr, nullptr };
    }

    return { NeutralMonsterJoiningCondition::Reason::None, 0, nullptr, nullptr };
}

bool Army::WeakestTroop( const Troop * t1, const Troop * t2 )
{
    return t1->GetStrength() < t2->GetStrength();
}

bool Army::StrongestTroop( const Troop * t1, const Troop * t2 )
{
    return t1->GetStrength() > t2->GetStrength();
}

bool Army::SlowestTroop( const Troop * t1, const Troop * t2 )
{
    return t1->GetSpeed() < t2->GetSpeed();
}

bool Army::FastestTroop( const Troop * t1, const Troop * t2 )
{
    return t1->GetSpeed() > t2->GetSpeed();
}

void Army::SwapTroops( Troop & t1, Troop & t2 )
{
    std::swap( t1, t2 );
}

bool Army::SaveLastTroop( void ) const
{
    return commander && commander->isHeroes() && 1 == GetCount();
}

Monster Army::GetStrongestMonster() const
{
    Monster monster( Monster::UNKNOWN );
    for ( const Troop * troop : *this ) {
        if ( troop->isValid() && troop->GetMonster().GetMonsterStrength() > monster.GetMonsterStrength() ) {
            monster = troop->GetID();
        }
    }
    return monster;
}

void Army::resetInvalidMonsters() const
{
    for ( Troop * troop : *this ) {
        if ( troop->GetID() != Monster::UNKNOWN && !troop->isValid() ) {
            troop->Set( Monster::UNKNOWN, 0 );
        }
    }
}

StreamBase & operator<<( StreamBase & msg, const Army & army )
{
    msg << static_cast<u32>( army.size() );

    // Army: fixed size
    for ( Army::const_iterator it = army.begin(); it != army.end(); ++it )
        msg << **it;

    return msg << army.combat_format << army.color;
}

StreamBase & operator>>( StreamBase & msg, Army & army )
{
    u32 armysz;
    msg >> armysz;

    for ( Army::iterator it = army.begin(); it != army.end(); ++it )
        msg >> **it;

    msg >> army.combat_format >> army.color;

    // set army
    for ( Army::iterator it = army.begin(); it != army.end(); ++it ) {
        ArmyTroop * troop = static_cast<ArmyTroop *>( *it );
        if ( troop )
            troop->SetArmy( army );
    }

    // set later from owner (castle, heroes)
    army.commander = nullptr;

    return msg;
}
