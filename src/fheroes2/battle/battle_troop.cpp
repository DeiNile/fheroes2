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
#include <cstring>
#include <iomanip>

#include "agg_image.h"
#include "battle.h"
#include "battle_arena.h"
#include "battle_army.h"
#include "battle_cell.h"
#include "battle_interface.h"
#include "battle_tower.h"
#include "battle_troop.h"
#include "game_static.h"
#include "logging.h"
#include "monster_anim.h"
#include "morale.h"
#include "speed.h"
#include "spell_info.h"
#include "tools.h"
#include "translations.h"
#include "world.h"

Battle::ModeDuration::ModeDuration( u32 mode, u32 duration )
    : std::pair<u32, u32>( mode, duration )
{}

bool Battle::ModeDuration::isMode( u32 mode ) const
{
    return ( first & mode ) != 0;
}

bool Battle::ModeDuration::isZeroDuration( void ) const
{
    return 0 == second;
}

void Battle::ModeDuration::DecreaseDuration( void )
{
    if ( second )
        --second;
}

Battle::ModesAffected::ModesAffected()
{
    reserve( 3 );
}

u32 Battle::ModesAffected::GetMode( u32 mode ) const
{
    const_iterator it = std::find_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } );
    return it == end() ? 0 : ( *it ).second;
}

void Battle::ModesAffected::AddMode( u32 mode, u32 duration )
{
    iterator it = std::find_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } );
    if ( it == end() )
        emplace_back( mode, duration );
    else
        ( *it ).second = duration;
}

void Battle::ModesAffected::RemoveMode( u32 mode )
{
    iterator it = std::find_if( begin(), end(), [mode]( const Battle::ModeDuration & v ) { return v.isMode( mode ); } );
    if ( it != end() ) {
        if ( it + 1 != end() )
            std::swap( *it, back() );
        pop_back();
    }
}

void Battle::ModesAffected::DecreaseDuration( void )
{
    std::for_each( begin(), end(), []( Battle::ModeDuration & v ) { v.DecreaseDuration(); } );
}

u32 Battle::ModesAffected::FindZeroDuration( void ) const
{
    const_iterator it = std::find_if( begin(), end(), []( const Battle::ModeDuration & v ) { return v.isZeroDuration(); } );
    return it == end() ? 0 : ( *it ).first;
}

Battle::Unit::Unit( const Troop & t, int32_t pos, bool ref, const Rand::DeterministicRandomGenerator & randomGenerator, const uint32_t uid )
    : ArmyTroop( nullptr, t )
    , animation( id )
    , _uid( uid )
    , hp( t.GetHitPoints() )
    , count0( t.GetCount() )
    , dead( 0 )
    , shots( t.GetShots() )
    , disruptingray( 0 )
    , reflect( ref )
    , mirror( nullptr )
    , idleTimer( animation.getIdleDelay() )
    , blindanswer( false )
    , customAlphaMask( 255 )
    , _randomGenerator( randomGenerator )
{
    // set position
    if ( Board::isValidIndex( pos ) ) {
        if ( t.isWide() )
            pos += ( reflect ? -1 : 1 );
        SetPosition( pos );
    }
    else {
        DEBUG_LOG( DBG_BATTLE, DBG_TRACE, "Invalid position " << pos << " for board" );
    }
}

Battle::Unit::~Unit()
{
    // reset summon elemental and mirror image
    if ( Modes( CAP_SUMMONELEM ) || Modes( CAP_MIRRORIMAGE ) ) {
        SetCount( 0 );
    }
}

void Battle::Unit::SetPosition( s32 pos )
{
    if ( position.GetHead() )
        position.GetHead()->SetUnit( nullptr );
    if ( position.GetTail() )
        position.GetTail()->SetUnit( nullptr );

    position.Set( pos, isWide(), reflect );

    if ( position.GetHead() )
        position.GetHead()->SetUnit( this );
    if ( position.GetTail() )
        position.GetTail()->SetUnit( this );
}

void Battle::Unit::SetPosition( const Position & pos )
{
    if ( position.GetHead() )
        position.GetHead()->SetUnit( nullptr );
    if ( position.GetTail() )
        position.GetTail()->SetUnit( nullptr );

    position = pos;

    if ( position.GetHead() )
        position.GetHead()->SetUnit( this );
    if ( position.GetTail() )
        position.GetTail()->SetUnit( this );

    if ( isWide() ) {
        reflect = GetHeadIndex() < GetTailIndex();
    }
}

void Battle::Unit::SetReflection( bool r )
{
    if ( reflect != r )
        position.Swap();

    reflect = r;
}

void Battle::Unit::UpdateDirection( void )
{
    // set auto reflect
    SetReflection( GetArena()->GetArmyColor1() != GetArmyColor() );
}

bool Battle::Unit::UpdateDirection( const fheroes2::Rect & pos )
{
    bool need = position.GetRect().x == pos.x ? reflect : position.GetRect().x > pos.x;

    if ( need != reflect ) {
        SetReflection( need );
        return true;
    }
    return false;
}

bool Battle::Unit::isBattle( void ) const
{
    return true;
}

bool Battle::Unit::isModes( u32 v ) const
{
    return Modes( v );
}

std::string Battle::Unit::GetShotString( void ) const
{
    if ( Troop::GetShots() == GetShots() )
        return std::to_string( Troop::GetShots() );

    std::string output( std::to_string( Troop::GetShots() ) );
    output += " (";
    output += std::to_string( GetShots() );
    output += ')';

    return output;
}

std::string Battle::Unit::GetSpeedString() const
{
    const uint32_t speedValue = GetSpeed( true, false );

    std::string output( Speed::String( speedValue ) );
    output += " (";
    output += std::to_string( speedValue );
    output += ')';

    return output;
}

uint32_t Battle::Unit::GetInitialCount() const
{
    return count0;
}

u32 Battle::Unit::GetDead( void ) const
{
    return dead;
}

u32 Battle::Unit::GetHitPointsLeft( void ) const
{
    return GetHitPoints() - ( GetCount() - 1 ) * Monster::GetHitPoints();
}

uint32_t Battle::Unit::GetMissingHitPoints() const
{
    const uint32_t totalHitPoints = count0 * Monster::GetHitPoints();
    assert( totalHitPoints > hp );
    return totalHitPoints - hp;
}

u32 Battle::Unit::GetAffectedDuration( u32 mod ) const
{
    return affected.GetMode( mod );
}

u32 Battle::Unit::GetSpeed( void ) const
{
    return GetSpeed( false, false );
}

int Battle::Unit::GetMorale() const
{
    int armyTroopMorale = ArmyTroop::GetMorale();

    // enemy Bone dragons affect morale
    if ( isAffectedByMorale() && GetArena()->getEnemyForce( GetArmyColor() ).HasMonster( Monster::BONE_DRAGON ) && armyTroopMorale > Morale::TREASON )
        --armyTroopMorale;

    return armyTroopMorale;
}

bool Battle::Unit::isUID( u32 v ) const
{
    return _uid == v;
}

u32 Battle::Unit::GetUID( void ) const
{
    return _uid;
}

Battle::Unit * Battle::Unit::GetMirror()
{
    return mirror;
}

void Battle::Unit::SetMirror( Unit * ptr )
{
    mirror = ptr;
}

u32 Battle::Unit::GetShots( void ) const
{
    return shots;
}

const Battle::Position & Battle::Unit::GetPosition( void ) const
{
    return position;
}

s32 Battle::Unit::GetHeadIndex( void ) const
{
    return position.GetHead() ? position.GetHead()->GetIndex() : -1;
}

s32 Battle::Unit::GetTailIndex( void ) const
{
    return position.GetTail() ? position.GetTail()->GetIndex() : -1;
}

void Battle::Unit::SetRandomMorale( void )
{
    const int morale = GetMorale();

    if ( morale > 0 && static_cast<int32_t>( _randomGenerator.Get( 1, 24 ) ) <= morale ) {
        SetModes( MORALE_GOOD );
    }
    else if ( morale < 0 && static_cast<int32_t>( _randomGenerator.Get( 1, 12 ) ) <= -morale ) {
        if ( isControlHuman() ) {
            SetModes( MORALE_BAD );
        }
        // AI is given a cheeky 25% chance to avoid it - because they build armies from random troops
        else if ( _randomGenerator.Get( 1, 4 ) != 1 ) {
            SetModes( MORALE_BAD );
        }
    }
}

void Battle::Unit::SetRandomLuck( void )
{
    const int32_t luck = GetLuck();
    const int32_t chance = static_cast<int32_t>( _randomGenerator.Get( 1, 24 ) );

    if ( luck > 0 && chance <= luck ) {
        SetModes( LUCK_GOOD );
    }
    else if ( luck < 0 && chance <= -luck ) {
        SetModes( LUCK_BAD );
    }

    // Bless, Curse and Luck do stack
}

bool Battle::Unit::isFlying( void ) const
{
    return ArmyTroop::isFlying() && !Modes( SP_SLOW );
}

bool Battle::Unit::isValid( void ) const
{
    return GetCount() != 0;
}

bool Battle::Unit::isReflect( void ) const
{
    return reflect;
}

bool Battle::Unit::OutOfWalls( void ) const
{
    return Board::isOutOfWallsIndex( GetHeadIndex() ) || ( isWide() && Board::isOutOfWallsIndex( GetTailIndex() ) );
}

bool Battle::Unit::canReach( int index ) const
{
    if ( !Board::isValidIndex( index ) )
        return false;

    if ( isFlying() || ( isArchers() && !isHandFighting() ) )
        return true;

    const bool isIndirectAttack = isReflect() == Board::isNegativeDistance( GetHeadIndex(), index );
    const int from = ( isWide() && isIndirectAttack ) ? GetTailIndex() : GetHeadIndex();
    return Board::GetDistance( from, index ) <= GetSpeed( true, false );
}

bool Battle::Unit::canReach( const Unit & unit ) const
{
    if ( unit.Modes( CAP_TOWER ) )
        return false;

    const bool isIndirectAttack = isReflect() == Board::isNegativeDistance( GetHeadIndex(), unit.GetHeadIndex() );
    const int target = ( unit.isWide() && isIndirectAttack ) ? unit.GetTailIndex() : unit.GetHeadIndex();
    return canReach( target );
}

bool Battle::Unit::isHandFighting( void ) const
{
    if ( GetCount() && !Modes( CAP_TOWER ) ) {
        const Indexes around = Board::GetAroundIndexes( *this );

        for ( Indexes::const_iterator it = around.begin(); it != around.end(); ++it ) {
            const Unit * enemy = Board::GetCell( *it )->GetUnit();
            if ( enemy && enemy->GetColor() != GetColor() )
                return true;
        }
    }

    return false;
}

bool Battle::Unit::isHandFighting( const Unit & a, const Unit & b )
{
    return a.isValid() && !a.Modes( CAP_TOWER ) && b.isValid() && b.GetColor() != a.GetCurrentColor()
           && ( Board::isNearIndexes( a.GetHeadIndex(), b.GetHeadIndex() ) || ( b.isWide() && Board::isNearIndexes( a.GetHeadIndex(), b.GetTailIndex() ) )
                || ( a.isWide()
                     && ( Board::isNearIndexes( a.GetTailIndex(), b.GetHeadIndex() )
                          || ( b.isWide() && Board::isNearIndexes( a.GetTailIndex(), b.GetTailIndex() ) ) ) ) );
}

int Battle::Unit::GetAnimationState() const
{
    return animation.getCurrentState();
}

bool Battle::Unit::isIdling() const
{
    return GetAnimationState() == Monster_Info::IDLE;
}

bool Battle::Unit::checkIdleDelay()
{
    return idleTimer.checkDelay();
}

void Battle::Unit::NewTurn( void )
{
    if ( isRegenerating() )
        hp = ArmyTroop::GetHitPoints();

    ResetModes( TR_RESPONSED );
    ResetModes( TR_MOVED );
    ResetModes( TR_HARDSKIP );
    ResetModes( TR_SKIPMOVE );
    ResetModes( LUCK_GOOD );
    ResetModes( LUCK_BAD );
    ResetModes( MORALE_GOOD );
    ResetModes( MORALE_BAD );

    // decrease spell duration
    affected.DecreaseDuration();

    // remove spell duration
    u32 mode = 0;
    while ( 0 != ( mode = affected.FindZeroDuration() ) ) {
        affected.RemoveMode( mode );
        ResetModes( mode );

        // cancel mirror image
        if ( mode == CAP_MIRROROWNER && mirror ) {
            if ( Arena::GetInterface() ) {
                std::vector<Unit *> images;
                images.push_back( mirror );
                Arena::GetInterface()->RedrawActionRemoveMirrorImage( images );
            }

            mirror->SetCount( 0 );
            mirror = nullptr;
        }
    }
}

u32 Battle::Unit::GetSpeed( bool skipStandingCheck, bool skipMovedCheck ) const
{
    uint32_t modesToCheck = SP_BLIND | IS_PARALYZE_MAGIC;
    if ( !skipMovedCheck ) {
        modesToCheck |= TR_MOVED;
    }

    if ( !skipStandingCheck && ( !GetCount() || Modes( modesToCheck ) ) )
        return Speed::STANDING;

    uint32_t speed = Monster::GetSpeed();
    Spell spell;

    if ( Modes( SP_HASTE ) ) {
        return Speed::GetHasteSpeedFromSpell( speed );
    }
    else if ( Modes( SP_SLOW ) ) {
        return Speed::GetSlowSpeedFromSpell( speed );
    }

    return speed;
}

uint32_t Battle::Unit::GetMoveRange() const
{
    return isFlying() ? ARENASIZE : GetSpeed( false, false );
}

uint32_t Battle::Unit::CalculateRetaliationDamage( uint32_t damageTaken ) const
{
    // Check if there will be retaliation in the first place
    if ( damageTaken > hp || Modes( CAP_MIRRORIMAGE ) || !AllowResponse() )
        return 0;

    const uint32_t unitsLeft = ( hp - damageTaken ) / Monster::GetHitPoints();

    uint32_t damagePerUnit = 0;
    if ( Modes( SP_CURSE ) )
        damagePerUnit = Monster::GetDamageMin();
    else if ( Modes( SP_BLESS ) )
        damagePerUnit = Monster::GetDamageMax();
    else
        damagePerUnit = ( Monster::GetDamageMin() + Monster::GetDamageMax() ) / 2;

    return unitsLeft * damagePerUnit;
}

u32 Battle::Unit::CalculateMinDamage( const Unit & enemy ) const
{
    return CalculateDamageUnit( enemy, ArmyTroop::GetDamageMin() );
}

u32 Battle::Unit::CalculateMaxDamage( const Unit & enemy ) const
{
    return CalculateDamageUnit( enemy, ArmyTroop::GetDamageMax() );
}

u32 Battle::Unit::CalculateDamageUnit( const Unit & enemy, double dmg ) const
{
    if ( isArchers() ) {
        if ( !isHandFighting() ) {
            // check skill archery +%10, +%25, +%50
            if ( GetCommander() ) {
                dmg += ( dmg * GetCommander()->GetSecondaryValues( Skill::Secondary::ARCHERY ) / 100 );
            }

            // check castle defense
            if ( GetArena()->IsShootingPenalty( *this, enemy ) )
                dmg /= 2;

            // check spell shield
            if ( enemy.Modes( SP_SHIELD ) )
                dmg /= Spell( Spell::SHIELD ).ExtraValue();
        }
        else if ( !isAbilityPresent( fheroes2::MonsterAbilityType::NO_MELEE_PENALTY ) ) {
            dmg /= 2;
        }
    }

    // after blind
    if ( blindanswer )
        dmg /= 2;

    // stone cap.
    if ( enemy.Modes( SP_STONE ) )
        dmg /= 2;

    // check monster capability
    switch ( GetID() ) {
    case Monster::CRUSADER:
        // double damage for undead
        if ( enemy.isUndead() )
            dmg *= 2;
        break;
    case Monster::FIRE_ELEMENT:
        if ( enemy.GetID() == Monster::WATER_ELEMENT )
            dmg *= 2;
        break;
    case Monster::WATER_ELEMENT:
        if ( enemy.GetID() == Monster::FIRE_ELEMENT )
            dmg *= 2;
        break;
    case Monster::AIR_ELEMENT:
        if ( enemy.GetID() == Monster::EARTH_ELEMENT )
            dmg *= 2;
        break;
    case Monster::EARTH_ELEMENT:
        if ( enemy.GetID() == Monster::AIR_ELEMENT )
            dmg *= 2;
        break;
    default:
        break;
    }

    int r = GetAttack() - enemy.GetDefense();
    if ( enemy.isDragons() && Modes( SP_DRAGONSLAYER ) )
        r += Spell( Spell::DRAGONSLAYER ).ExtraValue();

    // Attack bonus is 20% to 300%
    dmg *= 1 + ( 0 < r ? 0.1 * std::min( r, 20 ) : 0.05 * std::max( r, -16 ) );

    return static_cast<u32>( dmg ) < 1 ? 1 : static_cast<u32>( dmg );
}

u32 Battle::Unit::GetDamage( const Unit & enemy ) const
{
    u32 res = 0;

    if ( Modes( SP_BLESS ) )
        res = CalculateMaxDamage( enemy );
    else if ( Modes( SP_CURSE ) )
        res = CalculateMinDamage( enemy );
    else
        res = _randomGenerator.Get( CalculateMinDamage( enemy ), CalculateMaxDamage( enemy ) );

    if ( Modes( LUCK_GOOD ) )
        res <<= 1; // mul 2
    else if ( Modes( LUCK_BAD ) )
        res >>= 1; // div 2

    return res;
}

u32 Battle::Unit::HowManyWillKilled( u32 dmg ) const
{
    return dmg >= hp ? GetCount() : GetCount() - Monster::GetCountFromHitPoints( *this, hp - dmg );
}

u32 Battle::Unit::ApplyDamage( u32 dmg )
{
    if ( dmg && GetCount() ) {
        u32 killed = HowManyWillKilled( dmg );

        // mirror image dies if recieves any damage
        if ( Modes( CAP_MIRRORIMAGE ) ) {
            dmg = hp;
            killed = GetCount();
        }

        DEBUG_LOG( DBG_BATTLE, DBG_TRACE, dmg << " to " << String() << " and killed: " << killed );

        // clean paralyze or stone magic
        if ( Modes( IS_PARALYZE_MAGIC ) ) {
            SetModes( TR_RESPONSED );
            SetModes( TR_MOVED );
            ResetModes( IS_PARALYZE_MAGIC );
            affected.RemoveMode( IS_PARALYZE_MAGIC );
        }

        // blind
        if ( Modes( SP_BLIND ) ) {
            ResetBlind();
        }

        if ( killed >= GetCount() ) {
            dead += GetCount();
            SetCount( 0 );
        }
        else {
            dead += killed;
            SetCount( GetCount() - killed );
        }
        hp -= ( dmg >= hp ? hp : dmg );

        return killed;
    }

    return 0;
}

void Battle::Unit::PostKilledAction( void )
{
    // kill mirror image (master)
    if ( Modes( CAP_MIRROROWNER ) ) {
        modes = 0;
        mirror->hp = 0;
        mirror->SetCount( 0 );
        mirror->mirror = nullptr;
        mirror = nullptr;
        ResetModes( CAP_MIRROROWNER );
    }
    // kill mirror image (slave)
    if ( Modes( CAP_MIRRORIMAGE ) && mirror != nullptr ) {
        mirror->ResetModes( CAP_MIRROROWNER );
        mirror = nullptr;
    }

    ResetModes( TR_RESPONSED );
    ResetModes( TR_HARDSKIP );
    ResetModes( TR_SKIPMOVE );
    ResetModes( LUCK_GOOD );
    ResetModes( LUCK_BAD );
    ResetModes( MORALE_GOOD );
    ResetModes( MORALE_BAD );
    ResetModes( IS_MAGIC );

    SetModes( TR_MOVED );

    // save troop to graveyard
    // skip mirror and summon
    if ( !Modes( CAP_MIRRORIMAGE ) && !Modes( CAP_SUMMONELEM ) )
        Arena::GetGraveyard()->AddTroop( *this );

    Cell * head = Board::GetCell( GetHeadIndex() );
    Cell * tail = Board::GetCell( GetTailIndex() );
    if ( head )
        head->SetUnit( nullptr );
    if ( tail )
        tail->SetUnit( nullptr );

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, String() << ", is dead..." );
    // possible also..
}

u32 Battle::Unit::Resurrect( u32 points, bool allow_overflow, bool skip_dead )
{
    u32 resurrect = Monster::GetCountFromHitPoints( *this, hp + points ) - GetCount();

    if ( hp == 0 ) // Skip turn if already dead
        SetModes( TR_MOVED );

    SetCount( GetCount() + resurrect );
    hp += points;

    if ( allow_overflow ) {
        if ( count0 < GetCount() )
            count0 = GetCount();
    }
    else if ( GetCount() > count0 ) {
        resurrect -= GetCount() - count0;
        SetCount( count0 );
        hp = ArmyTroop::GetHitPoints();
    }

    if ( !skip_dead )
        dead -= ( resurrect < dead ? resurrect : dead );

    return resurrect;
}

u32 Battle::Unit::ApplyDamage( Unit & enemy, u32 dmg )
{
    u32 killed = ApplyDamage( dmg );
    u32 resurrect;

    if ( killed )
        switch ( enemy.GetID() ) {
        case Monster::GHOST:
            resurrect = killed * static_cast<Monster &>( enemy ).GetHitPoints();
            DEBUG_LOG( DBG_BATTLE, DBG_TRACE, String() << ", enemy: " << enemy.String() << " resurrect: " << resurrect );
            // grow troop
            enemy.Resurrect( resurrect, true, false );
            break;

        case Monster::VAMPIRE_LORD:
            resurrect = killed * Monster::GetHitPoints();
            DEBUG_LOG( DBG_BATTLE, DBG_TRACE, String() << ", enemy: " << enemy.String() << " resurrect: " << resurrect );
            // restore hit points
            enemy.Resurrect( resurrect, false, false );
            break;

        default:
            break;
        }

    return killed;
}

bool Battle::Unit::AllowApplySpell( const Spell & spell, const HeroBase * hero, std::string * msg, bool forceApplyToAlly ) const
{
    if ( Modes( CAP_MIRRORIMAGE ) && ( spell == Spell::ANTIMAGIC || spell == Spell::MIRRORIMAGE ) ) {
        return false;
    }

    if ( Modes( CAP_MIRROROWNER ) && spell == Spell::MIRRORIMAGE ) {
        return false;
    }

    // check global
    // if(GetArena()->DisableCastSpell(spell, msg)) return false; // disable - recursion!

    if ( hero && spell.isApplyToFriends() && GetColor() != hero->GetColor() )
        return false;
    if ( hero && spell.isApplyToEnemies() && GetColor() == hero->GetColor() && !forceApplyToAlly )
        return false;
    if ( isMagicResist( spell, ( hero ? hero->GetPower() : 0 ) ) )
        return false;

    const HeroBase * myhero = GetCommander();
    if ( !myhero )
        return true;

    // check artifact
    Artifact guard_art( Artifact::UNKNOWN );
    switch ( spell.GetID() ) {
    case Spell::CURSE:
    case Spell::MASSCURSE:
        guard_art = Artifact::HOLY_PENDANT;
        break;
    case Spell::HYPNOTIZE:
        guard_art = Artifact::PENDANT_FREE_WILL;
        break;
    case Spell::DEATHRIPPLE:
    case Spell::DEATHWAVE:
        guard_art = Artifact::PENDANT_LIFE;
        break;
    case Spell::BERSERKER:
        guard_art = Artifact::SERENITY_PENDANT;
        break;
    case Spell::BLIND:
        guard_art = Artifact::SEEING_EYE_PENDANT;
        break;
    case Spell::PARALYZE:
        guard_art = Artifact::KINETIC_PENDANT;
        break;
    case Spell::HOLYWORD:
    case Spell::HOLYSHOUT:
        guard_art = Artifact::PENDANT_DEATH;
        break;
    case Spell::DISPEL:
        guard_art = Artifact::WAND_NEGATION;
        break;

    default:
        break;
    }

    if ( guard_art.isValid() && myhero->hasArtifact( guard_art ) ) {
        if ( msg ) {
            *msg = _( "The %{artifact} artifact is in effect for this battle, disabling %{spell} spell." );
            StringReplace( *msg, "%{artifact}", guard_art.GetName() );
            StringReplace( *msg, "%{spell}", spell.GetName() );
        }
        return false;
    }

    return true;
}

bool Battle::Unit::isUnderSpellEffect( const Spell & spell ) const
{
    switch ( spell.GetID() ) {
    case Spell::BLESS:
    case Spell::MASSBLESS:
        return Modes( SP_BLESS );

    case Spell::BLOODLUST:
        return Modes( SP_BLOODLUST );

    case Spell::CURSE:
    case Spell::MASSCURSE:
        return Modes( SP_CURSE );

    case Spell::HASTE:
    case Spell::MASSHASTE:
        return Modes( SP_HASTE );

    case Spell::SHIELD:
    case Spell::MASSSHIELD:
        return Modes( SP_SHIELD );

    case Spell::SLOW:
    case Spell::MASSSLOW:
        return Modes( SP_SLOW );

    case Spell::STONESKIN:
    case Spell::STEELSKIN:
        return Modes( SP_STONESKIN | SP_STEELSKIN );

    case Spell::BLIND:
    case Spell::PARALYZE:
    case Spell::STONE:
        return Modes( SP_BLIND | SP_PARALYZE | SP_STONE );

    case Spell::DRAGONSLAYER:
        return Modes( SP_DRAGONSLAYER );

    case Spell::ANTIMAGIC:
        return Modes( SP_ANTIMAGIC );

    case Spell::BERSERKER:
        return Modes( SP_BERSERKER );

    case Spell::HYPNOTIZE:
        return Modes( SP_HYPNOTIZE );

    case Spell::MIRRORIMAGE:
        return Modes( CAP_MIRROROWNER );

    case Spell::DISRUPTINGRAY:
        return GetDefense() < spell.ExtraValue();

    default:
        break;
    }
    return false;
}

bool Battle::Unit::ApplySpell( const Spell & spell, const HeroBase * hero, TargetInfo & target )
{
    // HACK!!! Chain lightining is the only spell which can't be casted on allies but could be applied on them
    const bool isForceApply = ( spell.GetID() == Spell::CHAINLIGHTNING );

    if ( !AllowApplySpell( spell, hero, nullptr, isForceApply ) )
        return false;

    DEBUG_LOG( DBG_BATTLE, DBG_TRACE, spell.GetName() << " to " << String() );

    const u32 spoint = hero ? hero->GetPower() : DEFAULT_SPELL_DURATION;

    if ( spell.isDamage() )
        SpellApplyDamage( spell, spoint, hero, target );
    else if ( spell.isRestore() )
        SpellRestoreAction( spell, spoint, hero );
    else {
        SpellModesAction( spell, spoint, hero );
    }

    return true;
}

std::vector<Spell> Battle::Unit::getCurrentSpellEffects() const
{
    std::vector<Spell> spellList;

    if ( Modes( SP_BLESS ) ) {
        spellList.emplace_back( Spell::BLESS );
    }
    if ( Modes( SP_CURSE ) ) {
        spellList.emplace_back( Spell::CURSE );
    }
    if ( Modes( SP_HASTE ) ) {
        spellList.emplace_back( Spell::HASTE );
    }
    if ( Modes( SP_SLOW ) ) {
        spellList.emplace_back( Spell::SLOW );
    }
    if ( Modes( SP_SHIELD ) ) {
        spellList.emplace_back( Spell::SHIELD );
    }
    if ( Modes( SP_BLOODLUST ) ) {
        spellList.emplace_back( Spell::BLOODLUST );
    }
    if ( Modes( SP_STONESKIN ) ) {
        spellList.emplace_back( Spell::STONESKIN );
    }
    if ( Modes( SP_STEELSKIN ) ) {
        spellList.emplace_back( Spell::STEELSKIN );
    }
    if ( Modes( SP_BLIND ) ) {
        spellList.emplace_back( Spell::BLIND );
    }
    if ( Modes( SP_PARALYZE ) ) {
        spellList.emplace_back( Spell::PARALYZE );
    }
    if ( Modes( SP_STONE ) ) {
        spellList.emplace_back( Spell::STONE );
    }
    if ( Modes( SP_DRAGONSLAYER ) ) {
        spellList.emplace_back( Spell::DRAGONSLAYER );
    }
    if ( Modes( SP_BERSERKER ) ) {
        spellList.emplace_back( Spell::BERSERKER );
    }
    if ( Modes( SP_HYPNOTIZE ) ) {
        spellList.emplace_back( Spell::HYPNOTIZE );
    }
    if ( Modes( CAP_MIRROROWNER ) ) {
        spellList.emplace_back( Spell::MIRRORIMAGE );
    }

    return spellList;
}

std::string Battle::Unit::String( bool more ) const
{
    std::stringstream ss;

    ss << "Unit: "
       << "[ " <<
        // info
        GetCount() << " " << GetName() << ", " << Color::String( GetColor() ) << ", pos: " << GetHeadIndex() << ", " << GetTailIndex() << ( reflect ? ", reflect" : "" );

    if ( more )
        ss << ", mode("
           << "0x" << std::hex << modes << std::dec << ")"
           << ", uid("
           << "0x" << std::setw( 8 ) << std::setfill( '0' ) << std::hex << _uid << std::dec << ")"
           << ", speed(" << Speed::String( GetSpeed() ) << ", " << static_cast<int>( GetSpeed() ) << ")"
           << ", hp(" << hp << ")"
           << ", die(" << dead << ")"
           << ")";

    ss << " ]";

    return ss.str();
}

bool Battle::Unit::AllowResponse( void ) const
{
    return ( !Modes( SP_BLIND ) || blindanswer ) && !Modes( IS_PARALYZE_MAGIC ) && !Modes( SP_HYPNOTIZE )
           && ( isAbilityPresent( fheroes2::MonsterAbilityType::ALWAYS_RETALIATE ) || !Modes( TR_RESPONSED ) );
}

void Battle::Unit::SetResponse( void )
{
    SetModes( TR_RESPONSED );
}

void Battle::Unit::PostAttackAction()
{
    // decrease shots
    if ( isArchers() && !isHandFighting() ) {
        // check ammo cart artifact
        const HeroBase * hero = GetCommander();
        if ( !hero || !hero->hasArtifact( Artifact::AMMO_CART ) )
            --shots;
    }

    // clean berserker spell
    if ( Modes( SP_BERSERKER ) ) {
        ResetModes( SP_BERSERKER );
        affected.RemoveMode( SP_BERSERKER );
    }

    // clean hypnotize spell
    if ( Modes( SP_HYPNOTIZE ) ) {
        ResetModes( SP_HYPNOTIZE );
        affected.RemoveMode( SP_HYPNOTIZE );
    }

    // clean luck capability
    ResetModes( LUCK_GOOD );
    ResetModes( LUCK_BAD );
}

void Battle::Unit::ResetBlind( void )
{
    // remove blind action
    if ( Modes( SP_BLIND ) ) {
        SetModes( TR_MOVED );
        ResetModes( SP_BLIND );
        affected.RemoveMode( SP_BLIND );
    }
}

void Battle::Unit::SetBlindAnswer( bool value )
{
    blindanswer = value;
}

u32 Battle::Unit::GetAttack( void ) const
{
    u32 res = ArmyTroop::GetAttack();

    if ( Modes( SP_BLOODLUST ) )
        res += Spell( Spell::BLOODLUST ).ExtraValue();

    return res;
}

u32 Battle::Unit::GetDefense( void ) const
{
    u32 res = ArmyTroop::GetDefense();

    if ( Modes( SP_STONESKIN ) )
        res += Spell( Spell::STONESKIN ).ExtraValue();
    else if ( Modes( SP_STEELSKIN ) )
        res += Spell( Spell::STEELSKIN ).ExtraValue();

    // disrupting ray accumulate effect
    if ( disruptingray ) {
        const u32 step = disruptingray * Spell( Spell::DISRUPTINGRAY ).ExtraValue();

        if ( step >= res )
            res = 1;
        else
            res -= step;
    }

    // check moat
    const Castle * castle = Arena::GetCastle();

    if ( castle && castle->isBuild( BUILD_MOAT ) && ( Board::isMoatIndex( GetHeadIndex(), *this ) || Board::isMoatIndex( GetTailIndex(), *this ) ) ) {
        const uint32_t step = GameStatic::GetBattleMoatReduceDefense();

        if ( step >= res )
            res = 1;
        else
            res -= step;
    }

    return res;
}

s32 Battle::Unit::GetScoreQuality( const Unit & defender ) const
{
    const Unit & attacker = *this;

    const double defendersDamage = CalculateDamageUnit( attacker, ( static_cast<double>( defender.GetDamageMin() ) + defender.GetDamageMax() ) / 2.0 );
    const double attackerPowerLost = ( attacker.Modes( CAP_MIRRORIMAGE ) || defender.Modes( CAP_TOWER ) || defendersDamage >= hp ) ? 1.0 : defendersDamage / hp;
    const bool attackerIsArchers = isArchers();

    double attackerThreat = CalculateDamageUnit( defender, ( static_cast<double>( GetDamageMin() ) + GetDamageMax() ) / 2.0 );

    if ( !canReach( defender ) && !defender.Modes( CAP_TOWER ) && !attackerIsArchers ) {
        // Can't reach, so unit is not dangerous to defender at the moment
        attackerThreat /= 2;
    }

    // Monster special abilities
    if ( isTwiceAttack() ) {
        if ( attackerIsArchers || ignoreRetaliation() || defender.Modes( TR_RESPONSED ) ) {
            attackerThreat *= 2;
        }
        else {
            // check how much we will lose to retaliation
            attackerThreat += attackerThreat * ( 1.0 - attackerPowerLost );
        }
    }

    switch ( id ) {
    case Monster::UNICORN:
        attackerThreat += defendersDamage * 0.2 * ( 100 - defender.GetMagicResist( Spell::BLIND, DEFAULT_SPELL_DURATION ) ) / 100.0;
        break;
    case Monster::CYCLOPS:
        attackerThreat += defendersDamage * 0.2 * ( 100 - defender.GetMagicResist( Spell::PARALYZE, DEFAULT_SPELL_DURATION ) ) / 100.0;
        break;
    case Monster::MEDUSA:
        attackerThreat += defendersDamage * 0.2 * ( 100 - defender.GetMagicResist( Spell::STONE, DEFAULT_SPELL_DURATION ) ) / 100.0;
        break;
    case Monster::VAMPIRE_LORD:
        // Lifesteal
        attackerThreat *= 1.3;
        break;
    case Monster::GENIE:
        // Genie's ability to half enemy troops
        attackerThreat *= 2;
        break;
    case Monster::GHOST:
        // Ghost's ability to increase the numbers
        attackerThreat *= 3;
        break;
    }

    // force big priority on mirror images as they get destroyed in 1 hit
    if ( attacker.Modes( CAP_MIRRORIMAGE ) )
        attackerThreat *= 10;

    // Negative value of units that changed the side
    if ( attacker.Modes( SP_BERSERKER ) || attacker.Modes( SP_HYPNOTIZE ) ) {
        attackerThreat *= -1;
    }
    // Otherwise heavy penalty for hiting our own units
    else if ( attacker.GetArmyColor() == defender.GetArmyColor() ) {
        const bool isTower = ( dynamic_cast<const Battle::Tower *>( this ) != nullptr );
        if ( !isTower ) {
            // Calculation score quality of tower should not effect units.
            attackerThreat *= -2;
        }
    }
    // Finally ignore disabled units (if belong to the enemy)
    else if ( attacker.Modes( SP_BLIND ) || attacker.Modes( IS_PARALYZE_MAGIC ) ) {
        attackerThreat = 0;
    }

    // Avoid effectiveness scaling if we're dealing with archers
    if ( !attackerIsArchers || defender.isArchers() )
        attackerThreat *= attackerPowerLost;

    return static_cast<int>( attackerThreat * 100 );
}

u32 Battle::Unit::GetHitPoints( void ) const
{
    return hp;
}

int Battle::Unit::GetControl( void ) const
{
    return !GetArmy() ? CONTROL_AI : GetArmy()->GetControl();
}

bool Battle::Unit::isArchers( void ) const
{
    return ArmyTroop::isArchers() && shots;
}

void Battle::Unit::SpellModesAction( const Spell & spell, u32 duration, const HeroBase * hero )
{
    if ( hero ) {
        for ( const Artifact::type_t art : { Artifact::WIZARD_HAT, Artifact::ENCHANTED_HOURGLASS } )
            duration += hero->artifactCount( art ) * Artifact( art ).ExtraValue();
    }

    switch ( spell.GetID() ) {
    case Spell::BLESS:
    case Spell::MASSBLESS:
        if ( Modes( SP_CURSE ) ) {
            ResetModes( SP_CURSE );
            affected.RemoveMode( SP_CURSE );
        }
        SetModes( SP_BLESS );
        affected.AddMode( SP_BLESS, duration );
        break;

    case Spell::BLOODLUST:
        SetModes( SP_BLOODLUST );
        affected.AddMode( SP_BLOODLUST, 3 );
        break;

    case Spell::CURSE:
    case Spell::MASSCURSE:
        if ( Modes( SP_BLESS ) ) {
            ResetModes( SP_BLESS );
            affected.RemoveMode( SP_BLESS );
        }
        SetModes( SP_CURSE );
        affected.AddMode( SP_CURSE, duration );
        break;

    case Spell::HASTE:
    case Spell::MASSHASTE:
        if ( Modes( SP_SLOW ) ) {
            ResetModes( SP_SLOW );
            affected.RemoveMode( SP_SLOW );
        }
        SetModes( SP_HASTE );
        affected.AddMode( SP_HASTE, duration );
        break;

    case Spell::DISPEL:
    case Spell::MASSDISPEL:
        if ( Modes( IS_MAGIC ) ) {
            ResetModes( IS_MAGIC );
            affected.RemoveMode( IS_MAGIC );
        }
        break;

    case Spell::SHIELD:
    case Spell::MASSSHIELD:
        SetModes( SP_SHIELD );
        affected.AddMode( SP_SHIELD, duration );
        break;

    case Spell::SLOW:
    case Spell::MASSSLOW:
        if ( Modes( SP_HASTE ) ) {
            ResetModes( SP_HASTE );
            affected.RemoveMode( SP_HASTE );
        }
        SetModes( SP_SLOW );
        affected.AddMode( SP_SLOW, duration );
        break;

    case Spell::STONESKIN:
        if ( Modes( SP_STEELSKIN ) ) {
            ResetModes( SP_STEELSKIN );
            affected.RemoveMode( SP_STEELSKIN );
        }
        SetModes( SP_STONESKIN );
        affected.AddMode( SP_STONESKIN, duration );
        break;

    case Spell::BLIND:
        SetModes( SP_BLIND );
        blindanswer = false;
        affected.AddMode( SP_BLIND, duration );
        break;

    case Spell::DRAGONSLAYER:
        SetModes( SP_DRAGONSLAYER );
        affected.AddMode( SP_DRAGONSLAYER, duration );
        break;

    case Spell::STEELSKIN:
        if ( Modes( SP_STONESKIN ) ) {
            ResetModes( SP_STONESKIN );
            affected.RemoveMode( SP_STONESKIN );
        }
        SetModes( SP_STEELSKIN );
        affected.AddMode( SP_STEELSKIN, duration );
        break;

    case Spell::ANTIMAGIC:
        ResetModes( IS_MAGIC );
        SetModes( SP_ANTIMAGIC );
        affected.AddMode( SP_ANTIMAGIC, duration );
        break;

    case Spell::PARALYZE:
        SetModes( SP_PARALYZE );
        affected.AddMode( SP_PARALYZE, duration );
        break;

    case Spell::BERSERKER:
        SetModes( SP_BERSERKER );
        affected.AddMode( SP_BERSERKER, duration );
        break;

    case Spell::HYPNOTIZE: {
        SetModes( SP_HYPNOTIZE );
        uint32_t acount = hero ? hero->artifactCount( Artifact::GOLD_WATCH ) : 0;
        affected.AddMode( SP_HYPNOTIZE, ( acount ? duration * acount * 2 : duration ) );
        break;
    }

    case Spell::STONE:
        SetModes( SP_STONE );
        affected.AddMode( SP_STONE, duration );
        break;

    case Spell::MIRRORIMAGE:
        affected.AddMode( CAP_MIRRORIMAGE, duration );
        break;

    case Spell::DISRUPTINGRAY:
        ++disruptingray;
        break;

    default:
        break;
    }
}

void Battle::Unit::SpellApplyDamage( const Spell & spell, u32 spoint, const HeroBase * hero, TargetInfo & target )
{
    // TODO: use fheroes2::getSpellDamage function to remove code duplication.
    u32 dmg = spell.Damage() * spoint;

    switch ( GetID() ) {
    case Monster::IRON_GOLEM:
    case Monster::STEEL_GOLEM:
        switch ( spell.GetID() ) {
            // 50% damage
        case Spell::COLDRAY:
        case Spell::COLDRING:
        case Spell::FIREBALL:
        case Spell::FIREBLAST:
        case Spell::LIGHTNINGBOLT:
        case Spell::CHAINLIGHTNING:
        case Spell::ELEMENTALSTORM:
        case Spell::ARMAGEDDON:
            dmg /= 2;
            break;
        default:
            break;
        }
        break;

    case Monster::WATER_ELEMENT:
        switch ( spell.GetID() ) {
            // 200% damage
        case Spell::FIREBALL:
        case Spell::FIREBLAST:
            dmg *= 2;
            break;
        default:
            break;
        }
        break;

    case Monster::AIR_ELEMENT:
        switch ( spell.GetID() ) {
            // 200% damage
        case Spell::ELEMENTALSTORM:
        case Spell::LIGHTNINGBOLT:
        case Spell::CHAINLIGHTNING:
            dmg *= 2;
            break;
        default:
            break;
        }
        break;

    case Monster::FIRE_ELEMENT:
        switch ( spell.GetID() ) {
            // 200% damage
        case Spell::COLDRAY:
        case Spell::COLDRING:
            dmg *= 2;
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }

    // check artifact
    if ( hero ) {
        const HeroBase * defendingHero = GetCommander();

        switch ( spell.GetID() ) {
        case Spell::COLDRAY:
        case Spell::COLDRING:
            // +50%
            if ( hero->hasArtifact( Artifact::EVERCOLD_ICICLE ) )
                dmg += dmg * Artifact( Artifact::EVERCOLD_ICICLE ).ExtraValue() / 100;

            if ( defendingHero ) {
                // -50%
                if ( defendingHero->hasArtifact( Artifact::ICE_CLOAK ) )
                    dmg -= dmg * Artifact( Artifact::ICE_CLOAK ).ExtraValue() / 100;

                if ( defendingHero->hasArtifact( Artifact::HEART_ICE ) )
                    dmg -= dmg * Artifact( Artifact::HEART_ICE ).ExtraValue() / 100;

                // 100%
                if ( defendingHero->hasArtifact( Artifact::HEART_FIRE ) )
                    dmg *= 2;
            }
            break;

        case Spell::FIREBALL:
        case Spell::FIREBLAST:
            // +50%
            if ( hero->hasArtifact( Artifact::EVERHOT_LAVA_ROCK ) )
                dmg += dmg * Artifact( Artifact::EVERHOT_LAVA_ROCK ).ExtraValue() / 100;

            if ( defendingHero ) {
                // -50%
                if ( defendingHero->hasArtifact( Artifact::FIRE_CLOAK ) )
                    dmg -= dmg * Artifact( Artifact::FIRE_CLOAK ).ExtraValue() / 100;

                if ( defendingHero->hasArtifact( Artifact::HEART_FIRE ) )
                    dmg -= dmg * Artifact( Artifact::HEART_FIRE ).ExtraValue() / 100;

                // 100%
                if ( defendingHero->hasArtifact( Artifact::HEART_ICE ) )
                    dmg *= 2;
            }
            break;

        case Spell::LIGHTNINGBOLT:
            // +50%
            if ( hero->hasArtifact( Artifact::LIGHTNING_ROD ) )
                dmg += dmg * Artifact( Artifact::LIGHTNING_ROD ).ExtraValue() / 100;
            // -50%
            if ( defendingHero && defendingHero->hasArtifact( Artifact::LIGHTNING_HELM ) )
                dmg -= dmg * Artifact( Artifact::LIGHTNING_HELM ).ExtraValue() / 100;
            break;

        case Spell::CHAINLIGHTNING:
            // +50%
            if ( hero->hasArtifact( Artifact::LIGHTNING_ROD ) )
                dmg += dmg * Artifact( Artifact::LIGHTNING_ROD ).ExtraValue() / 100;
            // -50%
            if ( defendingHero && defendingHero->hasArtifact( Artifact::LIGHTNING_HELM ) )
                dmg -= dmg * Artifact( Artifact::LIGHTNING_HELM ).ExtraValue() / 100;
            // update orders damage
            switch ( target.damage ) {
            case 0:
                break;
            case 1:
                dmg /= 2;
                break;
            case 2:
                dmg /= 4;
                break;
            case 3:
                dmg /= 8;
                break;
            default:
                break;
            }
            break;

        case Spell::ELEMENTALSTORM:
        case Spell::ARMAGEDDON:
            // -50%
            if ( defendingHero && defendingHero->hasArtifact( Artifact::BROACH_SHIELDING ) )
                dmg /= 2;
            break;

        default:
            break;
        }
    }

    // apply damage
    if ( dmg ) {
        target.damage = dmg;
        target.killed = ApplyDamage( dmg );
    }
}

void Battle::Unit::SpellRestoreAction( const Spell & spell, u32 spoint, const HeroBase * hero )
{
    switch ( spell.GetID() ) {
    case Spell::CURE:
    case Spell::MASSCURE:
        // clear bad magic
        if ( Modes( IS_BAD_MAGIC ) ) {
            ResetModes( IS_BAD_MAGIC );
            affected.RemoveMode( IS_BAD_MAGIC );
        }
        // restore
        hp += ( spell.Restore() * spoint );
        if ( hp > ArmyTroop::GetHitPoints() )
            hp = ArmyTroop::GetHitPoints();
        break;

    case Spell::RESURRECT:
    case Spell::ANIMATEDEAD:
    case Spell::RESURRECTTRUE: {
        // remove from graveyard
        if ( !isValid() ) {
            // TODO: buggy behaviour
            Arena::GetGraveyard()->RemoveTroop( *this );
        }

        const uint32_t restore = fheroes2::getResurrectPoints( spell, spoint, hero );
        const u32 resurrect = Resurrect( restore, false, ( spell == Spell::RESURRECT ) );

        // Puts back the unit in the board
        SetPosition( GetPosition() );

        if ( Arena::GetInterface() ) {
            std::string str( _( "%{count} %{name} rise(s) from the dead!" ) );
            StringReplace( str, "%{count}", resurrect );
            StringReplace( str, "%{name}", GetName() );
            Arena::GetInterface()->SetStatus( str, true );
        }
        break;
    }

    default:
        break;
    }
}

bool Battle::Unit::isTwiceAttack( void ) const
{
    switch ( GetID() ) {
    case Monster::ELF:
    case Monster::GRAND_ELF:
    case Monster::RANGER:
        return !isHandFighting();

    default:
        break;
    }

    return ArmyTroop::isTwiceAttack();
}

bool Battle::Unit::isMagicResist( const Spell & spell, u32 spower ) const
{
    return 100 <= GetMagicResist( spell, spower );
}

u32 Battle::Unit::GetMagicResist( const Spell & spell, u32 spower ) const
{
    if ( Modes( SP_ANTIMAGIC ) )
        return 100;

    switch ( spell.GetID() ) {
    case Spell::CURE:
    case Spell::MASSCURE:
        if ( !isHaveDamage() && !( modes & IS_MAGIC ) )
            return 100;
        break;

    case Spell::RESURRECT:
    case Spell::RESURRECTTRUE:
    case Spell::ANIMATEDEAD:
        if ( GetCount() == count0 )
            return 100;
        break;

    case Spell::DISPEL:
    case Spell::MASSDISPEL:
        if ( !( modes & IS_MAGIC ) )
            return 100;
        break;

    case Spell::HYPNOTIZE:
        if ( fheroes2::getHypnorizeMonsterHPPoints( spell, spower, nullptr ) < hp )
            return 100;
        break;

    default:
        break;
    }

    return fheroes2::getSpellResistance( id, spell.GetID() );
}

int Battle::Unit::GetSpellMagic() const
{
    const std::vector<fheroes2::MonsterAbility> & abilities = fheroes2::getMonsterData( GetID() ).battleStats.abilities;
    const auto foundAbility = std::find( abilities.begin(), abilities.end(), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::SPELL_CASTER ) );
    if ( foundAbility == abilities.end() ) {
        // Not a spell caster.
        return Spell::NONE;
    }

    if ( _randomGenerator.Get( 1, 100 ) > foundAbility->percentage ) {
        // No luck to cast the spell.
        return Spell::NONE;
    }

    return foundAbility->value;
}

bool Battle::Unit::isHaveDamage( void ) const
{
    return hp < count0 * Monster::GetHitPoints();
}

int Battle::Unit::GetFrame( void ) const
{
    return animation.getFrame();
}

void Battle::Unit::SetCustomAlpha( uint32_t alpha )
{
    customAlphaMask = alpha;
}

uint32_t Battle::Unit::GetCustomAlpha() const
{
    return customAlphaMask;
}

void Battle::Unit::IncreaseAnimFrame( bool loop )
{
    animation.playAnimation( loop );
}

bool Battle::Unit::isFinishAnimFrame( void ) const
{
    return animation.isLastFrame();
}

bool Battle::Unit::SwitchAnimation( int rule, bool reverse )
{
    animation.switchAnimation( rule, reverse );
    return animation.isValid();
}

bool Battle::Unit::SwitchAnimation( const std::vector<int> & animationList, bool reverse )
{
    animation.switchAnimation( animationList, reverse );
    return animation.isValid();
}

int Battle::Unit::M82Attk( void ) const
{
    const fheroes2::MonsterSound & sounds = fheroes2::getMonsterData( id ).sounds;

    if ( isArchers() && !isHandFighting() ) {
        // Added a new shooter without sound? Grant him a voice!
        assert( sounds.rangeAttack != M82::UNKNOWN );
        return sounds.rangeAttack;
    }

    assert( sounds.meleeAttack != M82::UNKNOWN );
    return sounds.meleeAttack;
}

int Battle::Unit::M82Kill() const
{
    return fheroes2::getMonsterData( id ).sounds.death;
}

int Battle::Unit::M82Move() const
{
    return fheroes2::getMonsterData( id ).sounds.movement;
}

int Battle::Unit::M82Wnce() const
{
    return fheroes2::getMonsterData( id ).sounds.wince;
}

int Battle::Unit::M82Expl( void ) const
{
    switch ( GetID() ) {
    case Monster::VAMPIRE:
    case Monster::VAMPIRE_LORD:
        return M82::VAMPEXT1;
    case Monster::LICH:
    case Monster::POWER_LICH:
        return M82::LICHEXPL;

    default:
        break;
    }

    return M82::UNKNOWN;
}

fheroes2::Rect Battle::Unit::GetRectPosition() const
{
    return position.GetRect();
}

fheroes2::Point Battle::Unit::GetBackPoint() const
{
    const fheroes2::Rect & rt = position.GetRect();
    return reflect ? fheroes2::Point( rt.x + rt.width, rt.y + rt.height / 2 ) : fheroes2::Point( rt.x, rt.y + rt.height / 2 );
}

fheroes2::Point Battle::Unit::GetCenterPoint() const
{
    const fheroes2::Sprite & sprite = fheroes2::AGG::GetICN( GetMonsterSprite(), GetFrame() );

    const fheroes2::Rect & pos = position.GetRect();
    const s32 centerY = pos.y + pos.height + sprite.y() / 2 - 10;

    return fheroes2::Point( pos.x + pos.width / 2, centerY );
}

fheroes2::Point Battle::Unit::GetStartMissileOffset( size_t direction ) const
{
    return animation.getProjectileOffset( direction );
}

int Battle::Unit::GetArmyColor( void ) const
{
    return ArmyTroop::GetColor();
}

int Battle::Unit::GetColor( void ) const
{
    return GetArmyColor();
}

int Battle::Unit::GetCurrentColor() const
{
    if ( Modes( SP_BERSERKER ) )
        return -1; // be aware of unknown color
    else if ( Modes( SP_HYPNOTIZE ) )
        return GetArena()->GetOppositeColor( GetArmyColor() );

    // default
    return GetColor();
}

int Battle::Unit::GetCurrentOrArmyColor() const
{
    const int color = GetCurrentColor();

    if ( color < 0 ) { // unknown color in case of SP_BERSERKER mode
        return GetArmyColor();
    }

    return color;
}

int Battle::Unit::GetCurrentControl() const
{
    if ( Modes( SP_BERSERKER ) )
        return CONTROL_AI; // let's say that it belongs to AI which is not present in the battle

    if ( Modes( SP_HYPNOTIZE ) ) {
        const int color = GetCurrentColor();
        if ( color == GetArena()->GetForce1().GetColor() )
            return GetArena()->GetForce1().GetControl();
        else
            return GetArena()->GetForce2().GetControl();
    }

    return GetControl();
}

const HeroBase * Battle::Unit::GetCommander( void ) const
{
    return GetArmy() ? GetArmy()->GetCommander() : nullptr;
}

const HeroBase * Battle::Unit::GetCurrentOrArmyCommander() const
{
    return GetArena()->getCommander( GetCurrentOrArmyColor() );
}
