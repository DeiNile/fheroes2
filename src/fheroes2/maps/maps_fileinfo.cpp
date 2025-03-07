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

#if defined( ANDROID ) || defined( _MSC_VER )
#include <locale>
#endif
#include <algorithm>
#include <cassert>
#include <cstring>
#include <map>

#include "artifact.h"
#include "color.h"
#include "difficulty.h"
#include "dir.h"
#include "game_io.h"
#include "game_over.h"
#include "logging.h"
#include "maps_fileinfo.h"
#include "maps_tiles.h"
#include "mp2.h"
#include "mp2_helper.h"
#include "race.h"
#include "serialize.h"
#include "settings.h"
#include "system.h"
#include "tools.h"

namespace
{
    const size_t mapNameLength = 16;
    const size_t mapDescriptionLength = 200;

    template <typename CharType>
    bool CaseInsensitiveCompare( const std::basic_string<CharType> & lhs, const std::basic_string<CharType> & rhs )
    {
        typename std::basic_string<CharType>::const_iterator li = lhs.begin();
        typename std::basic_string<CharType>::const_iterator ri = rhs.begin();

        while ( li != lhs.end() && ri != rhs.end() ) {
            const CharType lc = std::tolower( *li, std::locale() );
            const CharType rc = std::tolower( *ri, std::locale() );

            ++li;
            ++ri;

            if ( lc < rc ) {
                return true;
            }
            if ( lc > rc ) {
                return false;
            }
            // the chars are "equal", so proceed to check the next pair
        }

        // we came to the end of either (or both) strings, left is "smaller" if it was shorter:
        return li == lhs.end() && ri != rhs.end();
    }

    uint8_t ByteToColor( const int byte )
    {
        switch ( byte ) {
        case 0:
            return Color::BLUE;
        case 1:
            return Color::GREEN;
        case 2:
            return Color::RED;
        case 3:
            return Color::YELLOW;
        case 4:
            return Color::ORANGE;
        case 5:
            return Color::PURPLE;

        default:
            break;
        }

        return Color::NONE;
    }

    int ByteToRace( const int byte )
    {
        switch ( byte ) {
        case 0x00:
            return Race::KNGT;
        case 0x01:
            return Race::BARB;
        case 0x02:
            return Race::SORC;
        case 0x03:
            return Race::WRLK;
        case 0x04:
            return Race::WZRD;
        case 0x05:
            return Race::NECR;
        case 0x06:
            return Race::MULT;
        case 0x07:
            return Race::RAND;

        default:
            break;
        }

        return Race::NONE;
    }
}

namespace Editor
{
    enum
    {
        Wins = 0x1000,
        CaptureTown = 0x1001,
        DefeatHero = 0x1002,
        FindArtifact = 0x1003,
        SideWins = 0x1004,
        AccumulateGold = 0x1005,
        CompAlsoWins = 0x0100,
        AllowNormalVictory = 0x0200,
        Loss = 0x2000,
        LoseTown = 0x2001,
        LoseHero = 0x2002,
        OutTime = 0x2003
    };
}

Maps::FileInfo::FileInfo()
    : _version( GameVersion::SUCCESSION_WARS )
{
    Reset();
}

Maps::FileInfo::FileInfo( const FileInfo & f )
{
    *this = f;
}

Maps::FileInfo & Maps::FileInfo::operator=( const FileInfo & f )
{
    file = f.file;
    name = f.name;
    description = f.description;
    size_w = f.size_w;
    size_h = f.size_h;
    difficulty = f.difficulty;

    for ( int32_t i = 0; i < KINGDOMMAX; ++i ) {
        races[i] = f.races[i];
        unions[i] = f.unions[i];
    }

    kingdom_colors = f.kingdom_colors;
    allow_human_colors = f.allow_human_colors;
    allow_comp_colors = f.allow_comp_colors;
    rnd_races = f.rnd_races;
    conditions_wins = f.conditions_wins;
    comp_also_wins = f.comp_also_wins;
    allow_normal_victory = f.allow_normal_victory;
    wins1 = f.wins1;
    wins2 = f.wins2;
    conditions_loss = f.conditions_loss;
    loss1 = f.loss1;
    loss2 = f.loss2;
    localtime = f.localtime;
    startWithHeroInEachCastle = f.startWithHeroInEachCastle;
    _version = f._version;

    return *this;
}

void Maps::FileInfo::Reset( void )
{
    file.clear();
    name.clear();
    description.clear();
    size_w = 0;
    size_h = 0;
    difficulty = 0;
    kingdom_colors = 0;
    allow_human_colors = 0;
    allow_comp_colors = 0;
    rnd_races = 0;
    conditions_wins = 0;
    comp_also_wins = false;
    allow_normal_victory = false;
    wins1 = 0;
    wins2 = 0;
    conditions_loss = 0;
    loss1 = 0;
    loss2 = 0;
    localtime = 0;
    startWithHeroInEachCastle = false;

    _version = GameVersion::SUCCESSION_WARS;

    for ( int32_t i = 0; i < KINGDOMMAX; ++i ) {
        races[i] = Race::NONE;
        unions[i] = ByteToColor( i );
    }
}

bool Maps::FileInfo::ReadSAV( const std::string & filename )
{
    Reset();
    return Game::LoadSAV2FileInfo( filename, *this );
}

bool Maps::FileInfo::ReadMP2( const std::string & filename )
{
    Reset();
    StreamFile fs;

    if ( !fs.open( filename, "rb" ) ) {
        DEBUG_LOG( DBG_GAME, DBG_WARN, "file not found " << filename );
        return false;
    }

    file = filename;
    kingdom_colors = 0;
    allow_human_colors = 0;
    allow_comp_colors = 0;
    rnd_races = 0;
    localtime = 0;

    // magic byte
    if ( fs.getBE32() != 0x5C000000 ) {
        DEBUG_LOG( DBG_GAME, DBG_WARN, "incorrect maps file " << filename );
        return false;
    }

    // level
    switch ( fs.getLE16() ) {
    case 0x00:
        difficulty = Difficulty::EASY;
        break;
    case 0x01:
        difficulty = Difficulty::NORMAL;
        break;
    case 0x02:
        difficulty = Difficulty::HARD;
        break;
    case 0x03:
        difficulty = Difficulty::EXPERT;
        break;
    default:
        difficulty = Difficulty::NORMAL;
        break;
    }

    // width
    size_w = fs.get();

    // height
    size_h = fs.get();

    const Colors colors( Color::ALL );

    // kingdom color - blue, green, red, yellow, orange, purple
    for ( const int color : colors ) {
        if ( fs.get() != 0 ) {
            kingdom_colors |= color;
        }
    }

    // allow human color - blue, green, red, yellow, orange, purple
    for ( const int color : colors ) {
        if ( fs.get() != 0 ) {
            allow_human_colors |= color;
        }
    }

    // allow comp color - blue, green, red, yellow, orange, purple
    for ( const int color : colors ) {
        if ( fs.get() != 0 ) {
            allow_comp_colors |= color;
        }
    }

    // kingdom count
    // fs.seekg(0x1A, std::ios_base::beg);
    // fs.get();

    // wins
    fs.seek( 0x1D );
    conditions_wins = fs.get();
    // data wins
    comp_also_wins = ( fs.get() != 0 );
    // data wins
    allow_normal_victory = ( fs.get() != 0 );
    // data wins
    wins1 = fs.getLE16();
    // data wins
    fs.seek( 0x2c );
    wins2 = fs.getLE16();

    // loss
    fs.seek( 0x22 );
    conditions_loss = fs.get();
    // data loss
    loss1 = fs.getLE16();
    // data loss
    fs.seek( 0x2e );
    loss2 = fs.getLE16();

    // start with hero
    fs.seek( 0x25 );
    startWithHeroInEachCastle = ( 0 == fs.get() );

    // race color
    for ( const int color : colors ) {
        const int race = ByteToRace( fs.get() );
        races[Color::GetIndex( color )] = race;
        if ( Race::RAND == race )
            rnd_races |= color;
    }

    bool skipUnionSetup = false;
    // If loss conditions are LOSS_HERO and victory conditions are VICTORY_DEFEAT_EVERYONE then we have to verify the color to which this object belongs to.
    // If the color is under computer control only we have to make it as an ally for human player.
    if ( conditions_loss == LOSS_HERO && conditions_wins == VICTORY_DEFEAT_EVERYONE && Colors( allow_human_colors ).size() == 1 ) {
        // Each tile needs 16 + 8 + 8 + 8 + 8 + 8 + 8 + 8 + 8 + 16 + 32 + 32 = 160 bits or 20 bytes.
        fs.seek( MP2::MP2OFFSETDATA + ( loss1 + loss2 * size_w ) * 20 );

        MP2::mp2tile_t mp2tile;
        MP2::loadTile( fs, mp2tile );

        Maps::Tiles tile;
        tile.Init( 0, mp2tile );

        std::pair<int, int> colorRace = Maps::Tiles::ColorRaceFromHeroSprite( tile.GetObjectSpriteIndex() );
        if ( ( colorRace.first & allow_human_colors ) == 0 ) {
            const int side1 = colorRace.first | allow_human_colors;
            const int side2 = allow_comp_colors ^ colorRace.first;
            FillUnions( side1, side2 );
            conditions_wins = VICTORY_DEFEAT_OTHER_SIDE;
            skipUnionSetup = true;
        }
    }

    // name
    fs.seek( 0x3A );
    name = fs.toString( mapNameLength );

    // description
    fs.seek( 0x76 );
    description = fs.toString( mapDescriptionLength );

    // fill unions
    if ( conditions_wins == VICTORY_DEFEAT_OTHER_SIDE && !skipUnionSetup ) {
        int side1 = 0;
        int side2 = 0;

        const Colors availableColors( kingdom_colors );

        assert( !availableColors.empty() );
        wins1 += Color::GetIndex( availableColors.front() );

        for ( const int color : availableColors ) {
            if ( Color::GetIndex( color ) < wins1 )
                side1 |= color;
            else
                side2 |= color;
        }
        FillUnions( side1, side2 );
    }

    // Determine the type of the map.
    const size_t pos = filename.rfind( '.' );
    if ( pos != std::string::npos ) {
        const std::string fileExtension = StringLower( filename.substr( pos + 1 ) );
        _version = ( fileExtension == "mx2" || fileExtension == "hxc" ) ? GameVersion::PRICE_OF_LOYALTY : GameVersion::SUCCESSION_WARS;
    }

    return true;
}

void Maps::FileInfo::FillUnions( const int side1Colors, const int side2Colors )
{
    for ( uint32_t i = 0; i < KINGDOMMAX; ++i ) {
        const int color = ByteToColor( i );

        if ( side1Colors & color )
            unions[i] = side1Colors;
        else if ( side2Colors & color )
            unions[i] = side2Colors;
        else
            unions[i] = color;
    }
}

bool Maps::FileInfo::FileSorting( const FileInfo & fi1, const FileInfo & fi2 )
{
    return CaseInsensitiveCompare( fi1.file, fi2.file );
}

bool Maps::FileInfo::NameSorting( const FileInfo & fi1, const FileInfo & fi2 )
{
    return CaseInsensitiveCompare( fi1.name, fi2.name );
}

int Maps::FileInfo::KingdomRace( int color ) const
{
    switch ( color ) {
    case Color::BLUE:
        return races[0];
    case Color::GREEN:
        return races[1];
    case Color::RED:
        return races[2];
    case Color::YELLOW:
        return races[3];
    case Color::ORANGE:
        return races[4];
    case Color::PURPLE:
        return races[5];
    default:
        break;
    }
    return 0;
}

uint32_t Maps::FileInfo::ConditionWins() const
{
    switch ( conditions_wins ) {
    case 0:
        return GameOver::WINS_ALL;
    case 1:
        return allow_normal_victory ? GameOver::WINS_TOWN | GameOver::WINS_ALL : GameOver::WINS_TOWN;
    case 2:
        return allow_normal_victory ? GameOver::WINS_HERO | GameOver::WINS_ALL : GameOver::WINS_HERO;
    case 3:
        return allow_normal_victory ? GameOver::WINS_ARTIFACT | GameOver::WINS_ALL : GameOver::WINS_ARTIFACT;
    case 4:
        return GameOver::WINS_SIDE;
    case 5:
        return allow_normal_victory ? GameOver::WINS_GOLD | GameOver::WINS_ALL : GameOver::WINS_GOLD;
    default:
        break;
    }

    return GameOver::COND_NONE;
}

uint32_t Maps::FileInfo::ConditionLoss() const
{
    switch ( conditions_loss ) {
    case 0:
        return GameOver::LOSS_ALL;
    case 1:
        return GameOver::LOSS_TOWN;
    case 2:
        return GameOver::LOSS_HERO;
    case 3:
        return GameOver::LOSS_TIME;
    default:
        break;
    }

    return GameOver::COND_NONE;
}

bool Maps::FileInfo::WinsCompAlsoWins( void ) const
{
    return comp_also_wins && ( ( GameOver::WINS_TOWN | GameOver::WINS_GOLD ) & ConditionWins() );
}

int Maps::FileInfo::WinsFindArtifactID( void ) const
{
    return wins1 ? wins1 - 1 : Artifact::UNKNOWN;
}

bool Maps::FileInfo::WinsFindUltimateArtifact( void ) const
{
    return 0 == wins1;
}

u32 Maps::FileInfo::WinsAccumulateGold( void ) const
{
    return wins1 * 1000;
}

fheroes2::Point Maps::FileInfo::WinsMapsPositionObject( void ) const
{
    return fheroes2::Point( wins1, wins2 );
}

fheroes2::Point Maps::FileInfo::LossMapsPositionObject( void ) const
{
    return fheroes2::Point( loss1, loss2 );
}

u32 Maps::FileInfo::LossCountDays( void ) const
{
    return loss1;
}

int Maps::FileInfo::AllowCompHumanColors( void ) const
{
    return allow_human_colors & allow_comp_colors;
}

int Maps::FileInfo::AllowHumanColors( void ) const
{
    return allow_human_colors;
}

int Maps::FileInfo::HumanOnlyColors( void ) const
{
    return allow_human_colors & ~allow_comp_colors;
}

int Maps::FileInfo::ComputerOnlyColors( void ) const
{
    return allow_comp_colors & ~allow_human_colors;
}

bool Maps::FileInfo::isAllowCountPlayers( int playerCount ) const
{
    const int humanOnly = Color::Count( HumanOnlyColors() );
    const int compHuman = Color::Count( AllowCompHumanColors() );

    return humanOnly <= playerCount && playerCount <= humanOnly + compHuman;
}

bool Maps::FileInfo::isMultiPlayerMap( void ) const
{
    return 1 < Color::Count( HumanOnlyColors() );
}

std::string Maps::FileInfo::String( void ) const
{
    std::ostringstream os;

    os << "file: " << file << ", "
       << "name: " << name << ", "
       << "kingdom colors: " << static_cast<int>( kingdom_colors ) << ", "
       << "allow human colors: " << static_cast<int>( allow_human_colors ) << ", "
       << "allow comp colors: " << static_cast<int>( allow_comp_colors ) << ", "
       << "rnd races: " << static_cast<int>( rnd_races ) << ", "
       << "conditions wins: " << static_cast<int>( conditions_wins ) << ", "
       << "comp also wins: " << ( comp_also_wins ? "true" : "false" ) << ", "
       << "allow normal victory: " << ( allow_normal_victory ? "true" : "false" ) << ", "
       << "wins1: " << wins1 << ", "
       << "wins2: " << wins2 << ", "
       << "conditions loss: " << static_cast<int>( conditions_loss ) << ", "
       << "loss1: " << loss1 << ", "
       << "loss2: " << loss2;

    return os.str();
}

StreamBase & Maps::operator<<( StreamBase & msg, const FileInfo & fi )
{
    // Only the basename of map filename (fi.file) is saved
    msg << System::GetBasename( fi.file ) << fi.name << fi.description << fi.size_w << fi.size_h << fi.difficulty << static_cast<u8>( KINGDOMMAX );

    for ( u32 ii = 0; ii < KINGDOMMAX; ++ii )
        msg << fi.races[ii] << fi.unions[ii];

    msg << fi.kingdom_colors << fi.allow_human_colors << fi.allow_comp_colors << fi.rnd_races << fi.conditions_wins << fi.comp_also_wins << fi.allow_normal_victory
        << fi.wins1 << fi.wins2 << fi.conditions_loss << fi.loss1 << fi.loss2 << fi.localtime << fi.startWithHeroInEachCastle;

    msg << static_cast<int>( fi._version );

    return msg;
}

StreamBase & Maps::operator>>( StreamBase & msg, FileInfo & fi )
{
    u8 kingdommax;

    // Only the basename of map filename (fi.file) is loaded
    msg >> fi.file >> fi.name >> fi.description >> fi.size_w >> fi.size_h >> fi.difficulty >> kingdommax;

    for ( u32 ii = 0; ii < kingdommax; ++ii )
        msg >> fi.races[ii] >> fi.unions[ii];

    msg >> fi.kingdom_colors >> fi.allow_human_colors >> fi.allow_comp_colors >> fi.rnd_races >> fi.conditions_wins >> fi.comp_also_wins >> fi.allow_normal_victory
        >> fi.wins1 >> fi.wins2 >> fi.conditions_loss >> fi.loss1 >> fi.loss2 >> fi.localtime >> fi.startWithHeroInEachCastle;

    int temp = 0;
    msg >> temp;
    fi._version = static_cast<GameVersion>( temp );
    return msg;
}

MapsFileInfoList Maps::PrepareMapsFileInfoList( const bool multi )
{
    const Settings & conf = Settings::Get();

    ListFiles maps = Settings::FindFiles( "maps", ".mp2", false );
    if ( conf.isPriceOfLoyaltySupported() ) {
        maps.Append( Settings::FindFiles( "maps", ".mx2", false ) );
    }

    // create a list of unique maps (based on the map file name) and filter it by the preferred number of players
    std::map<std::string, Maps::FileInfo> uniqueMaps;

    const int prefNumOfPlayers = conf.PreferablyCountPlayers();

    for ( const std::string & mapFile : maps ) {
        Maps::FileInfo fi;

        if ( fi.ReadMP2( mapFile ) ) {
            if ( ( !multi && !fi.isMultiPlayerMap() ) || ( multi && prefNumOfPlayers > 1 && fi.isAllowCountPlayers( prefNumOfPlayers ) ) ) {
                uniqueMaps[System::GetBasename( mapFile )] = fi;
            }
        }
    }

    MapsFileInfoList result;

    result.reserve( uniqueMaps.size() );

    for ( const auto & item : uniqueMaps ) {
        result.push_back( item.second );
    }

    std::sort( result.begin(), result.end(), Maps::FileInfo::NameSorting );

    return result;
}
