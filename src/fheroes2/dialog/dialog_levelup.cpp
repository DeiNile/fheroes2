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

#include "agg_image.h"
#include "cursor.h"
#include "dialog.h"
#include "game.h"
#include "heroes.h"
#include "icn.h"
#include "settings.h"
#include "text.h"
#include "tools.h"
#include "translations.h"
#include "ui_button.h"

void DialogPrimaryOnly( const std::string & name, const std::string & primary )
{
    std::string message = _( "%{name} has gained a level." );
    message.append( "\n \n" );
    message.append( _( "%{skill} +1" ) );
    StringReplace( message, "%{name}", name );
    StringReplace( message, "%{skill}", primary );
    Dialog::Message( "", message, Font::BIG, Dialog::OK );
}

int DialogOneSecondary( const std::string & name, const std::string & primary, const Skill::Secondary & sec )
{
    std::string message = _( "%{name} has gained a level." );
    message.append( "\n \n" );
    message.append( _( "%{skill} +1" ) );
    StringReplace( message, "%{name}", name );
    StringReplace( message, "%{skill}", primary );

    message.append( "\n \n" );
    message.append( _( "You have learned %{skill}." ) );
    StringReplace( message, "%{skill}", sec.GetName() );

    const fheroes2::Sprite & sprite_frame = fheroes2::AGG::GetICN( ICN::SECSKILL, 15 );

    // Create a surface with no alpha mode to fix SDL 1
    fheroes2::Sprite drawSurface = sprite_frame;

    // sprite
    const fheroes2::Sprite & sprite_skill = fheroes2::AGG::GetICN( ICN::SECSKILL, sec.GetIndexSprite1() );
    fheroes2::Blit( sprite_skill, drawSurface, 3, 3 );
    // text
    Text text_skill( Skill::Secondary::String( sec.Skill() ), Font::SMALL );
    text_skill.Blit( 3 + ( sprite_skill.width() - text_skill.w() ) / 2, 6, drawSurface );
    Text text_level( Skill::Level::String( sec.Level() ), Font::SMALL );
    text_level.Blit( 3 + ( sprite_skill.width() - text_level.w() ) / 2, sprite_skill.height() - 12, drawSurface );

    Dialog::SpriteInfo( "", message, drawSurface );

    return sec.Skill();
}

int DialogSelectSecondary( const std::string & name, const std::string & primary, const Skill::Secondary & sec1, const Skill::Secondary & sec2, Heroes & hero )
{
    std::string header = _( "%{name} has gained a level." );
    header.append( "\n \n" );
    header.append( _( "%{skill} +1" ) );
    StringReplace( header, "%{name}", name );
    StringReplace( header, "%{skill}", primary );

    fheroes2::Display & display = fheroes2::Display::instance();

    const bool isEvilInterface = Settings::Get().ExtGameEvilInterface();
    const int system = isEvilInterface ? ICN::SYSTEME : ICN::SYSTEM;

    // setup cursor
    const CursorRestorer cursorRestorer( true, Cursor::POINTER );

    const fheroes2::Sprite & sprite_frame = fheroes2::AGG::GetICN( ICN::SECSKILL, 15 );
    const fheroes2::Sprite & sprite_skill1 = fheroes2::AGG::GetICN( ICN::SECSKILL, sec1.GetIndexSprite1() );
    const fheroes2::Sprite & sprite_skill2 = fheroes2::AGG::GetICN( ICN::SECSKILL, sec2.GetIndexSprite1() );

    std::string message = _( "You may learn either:" );
    message.append( "\n%{skill1}\n" );
    message.append( " " );
    message.append( _( "or" ) );
    message.append( " " );
    message.append( "\n%{skill2}" );
    StringReplace( message, "%{skill1}", sec1.GetName() );
    StringReplace( message, "%{skill2}", sec2.GetName() );

    TextBox box1( header, Font::BIG, BOXAREA_WIDTH );
    TextBox box2( message, Font::BIG, BOXAREA_WIDTH );
    const int spacer = 10;

    Dialog::FrameBox box( box1.h() + spacer + box2.h() + 10 + sprite_frame.height(), true );

    fheroes2::Point pt;
    pt.x = box.GetArea().x + box.GetArea().width / 2 - fheroes2::AGG::GetICN( system, 9 ).width() - 20;
    pt.y = box.GetArea().y + box.GetArea().height - fheroes2::AGG::GetICN( system, 9 ).height();
    fheroes2::Button button_learn1( pt.x, pt.y, system, 9, 10 );

    pt.x = box.GetArea().x + box.GetArea().width / 2 + 20;
    pt.y = box.GetArea().y + box.GetArea().height - fheroes2::AGG::GetICN( system, 9 ).height();
    fheroes2::Button button_learn2( pt.x, pt.y, system, 9, 10 );

    const fheroes2::Rect & boxArea = box.GetArea();
    fheroes2::Point pos( boxArea.x, boxArea.y );

    if ( !header.empty() )
        box1.Blit( pos.x, pos.y );
    pos.y += box1.h() + spacer;

    if ( !message.empty() )
        box2.Blit( pos.x, pos.y );
    pos.y += box2.h() + spacer;

    // sprite1
    pos.x = box.GetArea().x + box.GetArea().width / 2 - sprite_frame.width() - 20;
    fheroes2::Blit( sprite_frame, display, pos.x, pos.y );
    pos.x += 3;
    fheroes2::Rect rect_image1( pos.x, pos.y, sprite_skill1.width(), sprite_skill1.height() );
    fheroes2::Blit( sprite_skill1, display, pos.x, pos.y + 3 );

    Text text;
    // text
    text.Set( Skill::Secondary::String( sec1.Skill() ), Font::SMALL );
    text.Blit( pos.x + ( sprite_skill1.width() - text.w() ) / 2, pos.y + 5 );
    text.Set( Skill::Level::String( sec1.Level() ), Font::SMALL );
    text.Blit( pos.x + ( sprite_skill1.width() - text.w() ) / 2, pos.y + sprite_skill1.height() - 12 );

    // sprite2
    pos.x = box.GetArea().x + box.GetArea().width / 2 + 20;
    fheroes2::Blit( sprite_frame, display, pos.x, pos.y );
    pos.x += 3;

    fheroes2::Rect rect_image2( pos.x, pos.y, sprite_skill2.width(), sprite_skill2.height() );
    fheroes2::Blit( sprite_skill2, display, pos.x, pos.y + 3 );
    // text
    Text name_skill2( Skill::Secondary::String( sec2.Skill() ), Font::SMALL );
    name_skill2.Blit( pos.x + ( sprite_skill2.width() - name_skill2.w() ) / 2, pos.y + 5 );
    Text name_level2( Skill::Level::String( sec2.Level() ), Font::SMALL );
    name_level2.Blit( pos.x + ( sprite_skill2.width() - name_level2.w() ) / 2, pos.y + sprite_skill2.height() - 12 );

    // hero button
    pt.x = box.GetArea().x + box.GetArea().width / 2 - 18;
    pt.y = box.GetArea().y + box.GetArea().height - 36;

    const int icnHeroes = isEvilInterface ? ICN::EVIL_ARMY_BUTTON : ICN::GOOD_ARMY_BUTTON;
    fheroes2::ButtonSprite button_hero
        = fheroes2::makeButtonWithBackground( pt.x, pt.y, fheroes2::AGG::GetICN( icnHeroes, 0 ), fheroes2::AGG::GetICN( icnHeroes, 1 ), display );

    text.Set( std::to_string( hero.GetSecondarySkills().Count() ) + "/" + std::to_string( HEROESMAXSKILL ), Font::BIG );
    text.Blit( box.GetArea().x + ( box.GetArea().width - text.w() ) / 2, pt.y - 15 );

    button_learn1.draw();
    button_learn2.draw();
    button_hero.draw();

    display.render();
    LocalEvent & le = LocalEvent::Get();

    // message loop
    while ( le.HandleEvents() ) {
        le.MousePressLeft( button_learn1.area() ) ? button_learn1.drawOnPress() : button_learn1.drawOnRelease();
        le.MousePressLeft( button_learn2.area() ) ? button_learn2.drawOnPress() : button_learn2.drawOnRelease();
        le.MousePressLeft( button_hero.area() ) ? button_hero.drawOnPress() : button_hero.drawOnRelease();

        if ( le.MouseClickLeft( button_learn1.area() ) || Game::HotKeyPressEvent( Game::EVENT_DEFAULT_LEFT ) )
            return sec1.Skill();
        else if ( le.MouseClickLeft( button_learn2.area() ) || Game::HotKeyPressEvent( Game::EVENT_DEFAULT_RIGHT ) )
            return sec2.Skill();
        else if ( le.MouseClickLeft( button_hero.area() ) || Game::HotKeyPressEvent( Game::EVENT_DEFAULT_READY ) ) {
            LocalEvent::GetClean();
            hero.OpenDialog( false, true, true, true );
            display.render();
        }

        if ( le.MouseClickLeft( rect_image1 ) ) {
            Dialog::SecondarySkillInfo( sec1, hero );
        }
        else if ( le.MouseClickLeft( rect_image2 ) ) {
            Dialog::SecondarySkillInfo( sec2, hero );
        }

        if ( le.MousePressRight( rect_image1 ) ) {
            Dialog::SecondarySkillInfo( sec1, hero, false );
            display.render();
        }
        else if ( le.MousePressRight( rect_image2 ) ) {
            Dialog::SecondarySkillInfo( sec2, hero, false );
            display.render();
        }
        else if ( le.MousePressRight( button_hero.area() ) ) {
            Dialog::Message( "", _( "View Hero" ), Font::BIG );
        }
    }

    return Skill::Secondary::UNKNOWN;
}

int Dialog::LevelUpSelectSkill( const std::string & name, const std::string & primary, const Skill::Secondary & sec1, const Skill::Secondary & sec2, Heroes & hero )
{
    int result = Skill::Secondary::UNKNOWN;

    if ( Skill::Secondary::UNKNOWN == sec1.Skill() && Skill::Secondary::UNKNOWN == sec2.Skill() )
        DialogPrimaryOnly( name, primary );
    else if ( Skill::Secondary::UNKNOWN == sec1.Skill() || Skill::Secondary::UNKNOWN == sec2.Skill() )
        result = DialogOneSecondary( name, primary, ( Skill::Secondary::UNKNOWN == sec2.Skill() ? sec1 : sec2 ) );
    else
        result = DialogSelectSecondary( name, primary, sec1, sec2, hero );

    return result;
}
