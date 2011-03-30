//  $Id$
//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2005 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2006 Joerg Henrichs, SuperTuxKart-Team, Steve Baker
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "states_screens/minimal_race_gui.hpp"

#include "irrlicht.h"
using namespace irr;

#include "audio/music_manager.hpp"
#include "config/user_config.hpp"
#include "graphics/camera.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/material_manager.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/modaldialog.hpp"
#include "guiengine/scalable_font.hpp"
#include "io/file_manager.hpp"
#include "input/input.hpp"
#include "input/input_manager.hpp"
#include "items/attachment.hpp"
#include "items/attachment_manager.hpp"
#include "items/powerup_manager.hpp"
#include "karts/kart_properties_manager.hpp"
#include "modes/follow_the_leader.hpp"
#include "modes/linear_world.hpp"
#include "modes/world.hpp"
#include "race/race_manager.hpp"
#include "tracks/track.hpp"
#include "utils/constants.hpp"
#include "utils/string_utils.hpp"
#include "utils/translation.hpp"

/** The constructor is called before anything is attached to the scene node.
 *  So rendering to a texture can be done here. But world is not yet fully
 *  created, so only the race manager can be accessed safely.
 */
MinimalRaceGUI::MinimalRaceGUI()
{    
    m_enabled = true;
    
    // Originally m_map_height was 100, and we take 480 as minimum res
    const float scaling = irr_driver->getFrameSize().Height / 480.0f;
    // Marker texture has to be power-of-two for (old) OpenGL compliance
    m_marker_rendered_size  =  2 << ((int) ceil(1.0 + log(32.0 * scaling)));
    m_marker_ai_size        = (int)( 24.0f * scaling);
    m_marker_player_size    = (int)( 34.0f * scaling);
    m_map_width             = (int)(200.0f * scaling);
    m_map_height            = (int)(200.0f * scaling);

    // The location of the minimap varies with number of 
    // splitscreen players:
    switch(race_manager->getNumLocalPlayers())
    {
    case 1 : // Lower left corner
             m_map_left   = 10;
             m_map_bottom = UserConfigParams::m_height-10;
             break;
    case 2:  // Middle of left side
             m_map_left   = 10;
             m_map_bottom = UserConfigParams::m_height/2 + m_map_height/2;
             break;
    case 3:  // Lower right quarter (which is not used by a player)
             m_map_left   = UserConfigParams::m_width/2 + 10;
             m_map_bottom = UserConfigParams::m_height-10;
             break;
    case 4:  // Middle of the screen.
             m_map_left   = UserConfigParams::m_width/2-m_map_width/2;
             m_map_bottom = UserConfigParams::m_height/2 + m_map_height/2;
             break;
    }
    
    // Minimap is also rendered bigger via OpenGL, so find power-of-two again
    const int map_texture   = 2 << ((int) ceil(1.0 + log(128.0 * scaling)));
    m_map_rendered_width    = map_texture;
    m_map_rendered_height   = map_texture;

    m_max_font_height       = GUIEngine::getFontHeight() + 10;
    m_small_font_max_height = GUIEngine::getSmallFontHeight() + 5;

    m_plunger_face     = material_manager->getMaterial("plungerface.png");
    m_music_icon       = material_manager->getMaterial("notes.png");
    createMarkerTexture();
    
    m_gauge_full      = irr_driver->getTexture( file_manager->getGUIDir() + "gauge_full.png" );
    m_gauge_empty     = irr_driver->getTexture( file_manager->getGUIDir() + "gauge_empty.png" );
    m_gauge_goal      = irr_driver->getTexture( file_manager->getGUIDir() + "gauge_goal.png" );

    // Translate strings only one in constructor to avoid calling
    // gettext in each frame.
    //I18N: Shown at the end of a race
    m_string_lap      = _("Lap");
    m_string_rank     = _("Rank");
    
    //I18N: as in "ready, set, go", shown at the beginning of the race
    m_string_ready    = _("Ready!");
    m_string_set      = _("Set!");
    m_string_go       = _("Go!");
     
    // Scaled fonts don't look good atm.
    m_font_scale      = 1.0f; //race_manager->getNumLocalPlayers()==1 ? 1.2f : 1.0f;

    //read icon frame picture
    m_icons_frame=material_manager->getMaterial("icons-frame.png");

    // Determine maximum length of the rank/lap text, in order to
    // align those texts properly on the right side of the viewport.
    gui::ScalableFont* font = GUIEngine::getFont(); 
    float old_scale = font->getScale();
    font->setScale(m_font_scale);
    m_lap_width             = font->getDimension(m_string_lap.c_str()).Width;
    m_timer_width           = font->getDimension(L"99:99:99").Width;
    m_rank_width            = font->getDimension(L"9/9").Width;

    int w;
    if (race_manager->getMinorMode()==RaceManager::MINOR_MODE_FOLLOW_LEADER ||
        race_manager->getMinorMode()==RaceManager::MINOR_MODE_3_STRIKES     ||
        race_manager->getNumLaps() > 9)
        w = font->getDimension(L" 99/99").Width;
    else
        w = font->getDimension(L" 9/9").Width;
    m_lap_width += w;
    font->setScale(old_scale);
        
}   // MinimalRaceGUI

//-----------------------------------------------------------------------------
MinimalRaceGUI::~MinimalRaceGUI()
{
    irr_driver->removeTexture(m_marker);
}   // ~MinimalRaceGUI

//-----------------------------------------------------------------------------
/** Creates a texture with the markers for all karts in the current race
 *  on it. This assumes that nothing is attached to the scene node at
 *  this stage.
 */
void MinimalRaceGUI::createMarkerTexture()
{
    unsigned int num_karts = race_manager->getNumberOfKarts();
    unsigned int npower2   = 1;
    // Textures must be power of 2, so 
    while(npower2<num_karts) npower2*=2;

    int radius     = (m_marker_rendered_size>>1)-1;
    IrrDriver::RTTProvider rttProvider(core::dimension2du(m_marker_rendered_size
                                                          *npower2,
                                                          m_marker_rendered_size),
                                     "MinimalRaceGUI::markers");
    scene::ICameraSceneNode *camera = irr_driver->addCameraSceneNode();
    core::matrix4 projection;
    projection.buildProjectionMatrixOrthoLH((float)(m_marker_rendered_size*npower2), 
                                            (float)(m_marker_rendered_size), 
                                            -1.0f, 1.0f);
    camera->setProjectionMatrix(projection, true);
    core::vector3df center( (float)(m_marker_rendered_size*npower2>>1),
                            (float)(m_marker_rendered_size>>1), 0.0f);
    camera->setPosition(center);
    camera->setUpVector(core::vector3df(0,1,0));
    camera->setTarget(center + core::vector3df(0,0,4));
    // The call to render sets the projection matrix etc. So we have to call 
    // this now before doing the direct OpenGL calls.
    // FIXME: perhaps we should use three calls to irr_driver: begin(),
    // render(), end() - so we could do the rendering by calling to
    // draw2DPolygon() between render() and end(), avoiding the
    // call to camera->render()
    camera->render();
    // We have to reset the material here, since otherwise the last
    // set material (i.e from the kart selection screen) will be used
    // when rednering to the texture.
    video::SMaterial m;
    m.setTexture(0, NULL);
    irr_driver->getVideoDriver()->setMaterial(m);
    for(unsigned int i=0; i<num_karts; i++)
    {
        const std::string& kart_ident = race_manager->getKartIdent(i);
        assert(kart_ident.size() > 0);
        
        const KartProperties *kp=kart_properties_manager->getKart(kart_ident);
        assert(kp != NULL);
        
        core::vector2df center((float)((m_marker_rendered_size>>1)
                                +i*m_marker_rendered_size), 
                               (float)(m_marker_rendered_size>>1)  );
        int count = kp->getShape();
        video::ITexture *t = kp->getMinimapIcon();
        if(t)
        {
            video::ITexture *t = kp->getIconMaterial()->getTexture();
            core::recti dest_rect(i*m_marker_rendered_size, 
                                  0,
                                  (i+1)*m_marker_rendered_size,
                                  m_marker_rendered_size);
            core::recti source_rect(core::vector2di(0,0), t->getSize());
            irr_driver->getVideoDriver()->draw2DImage(t, dest_rect, 
                                                      source_rect,
                                                      /*clipRect*/0,
                                                      /*color*/   0,
                                                      /*useAlpha*/true);
        }
        else   // no special minimap icon defined
        {
            video::S3DVertex *vertices = new video::S3DVertex[count+1];
            unsigned short int *index  = new unsigned short int[count+1];
            video::SColor color        = kp->getColor();
            createRegularPolygon(count, (float)radius, center, color, 
                vertices, index);
            irr_driver->getVideoDriver()->draw2DVertexPrimitiveList(vertices, 
                                            count, index, count-2,
                                            video::EVT_STANDARD, 
                                            scene::EPT_TRIANGLE_FAN);
            delete [] vertices;
            delete [] index;
        }   // if special minimap icon defined
    }

    m_marker = rttProvider.renderToTexture(-1, /*is_2d_render*/true);
    irr_driver->removeCameraSceneNode(camera);
}   // createMarkerTexture

//-----------------------------------------------------------------------------
/** Creates the 2D vertices for a regular polygon. Adopted from Irrlicht.
 *  \param n Number of vertices to use.
 *  \param radius Radius of the polygon.
 *  \param center The center point of the polygon.
 *  \param v Pointer to the array of vertices.
 */
void MinimalRaceGUI::createRegularPolygon(unsigned int n, float radius, 
                                   const core::vector2df &center,
                                   const video::SColor &color,
                                   video::S3DVertex *v, 
                                   unsigned short int *index)
{
    float f = 2*M_PI/(float)n;
    for (unsigned int i=0; i<n; i++)
    {
        float p = i*f;
        core::vector2df X = center + core::vector2df( sin(p)*radius, 
                                                     -cos(p)*radius);
        v[i].Pos.X = X.X;
        v[i].Pos.Y = X.Y;
        v[i].Color = color;
        index[i]   = i;
    }
}   // createRegularPolygon


//-----------------------------------------------------------------------------
/** Render all global parts of the race gui, i.e. things that are only 
 *  displayed once even in splitscreen.
 *  \param dt Timestep sized.
 */
void MinimalRaceGUI::renderGlobal(float dt)
{
    cleanupMessages(dt);
    
    // Special case : when 3 players play, use 4th window to display such 
    // stuff (but we must clear it)
    if (race_manager->getNumLocalPlayers() == 3 && 
        !GUIEngine::ModalDialog::isADialogActive())
    {
        static video::SColor black = video::SColor(255,0,0,0);
        irr_driver->getVideoDriver()
            ->draw2DRectangle(black,
                              core::rect<s32>(UserConfigParams::m_width/2, 
                                              UserConfigParams::m_height/2,
                                              UserConfigParams::m_width, 
                                              UserConfigParams::m_height));
    }
    
    World *world = World::getWorld();
    assert(world != NULL);
    if(world->getPhase() >= WorldStatus::READY_PHASE &&
       world->getPhase() <= WorldStatus::GO_PHASE      )
    {
        drawGlobalReadySetGo();
    }

    // Timer etc. are not displayed unless the game is actually started.
    if(!world->isRacePhase()) return;
    if (!m_enabled) return;

    drawGlobalTimer();
    if(world->getPhase() == WorldStatus::GO_PHASE ||
       world->getPhase() == WorldStatus::MUSIC_PHASE)
    {
        drawGlobalMusicDescription();
    }

    drawGlobalMiniMap();
}   // renderGlobal

//-----------------------------------------------------------------------------
/** Render the details for a single player, i.e. speed, energy, 
 *  collectibles, ...
 *  \param kart Pointer to the kart for which to render the view.
 */
void MinimalRaceGUI::renderPlayerView(const Kart *kart)
{
    if (!m_enabled) return;
    
    const core::recti &viewport    = kart->getCamera()->getViewport();
    core::vector2df scaling = kart->getCamera()->getScaling();
    //std::cout << "Applied ratio : " << viewport.getWidth()/800.0f << std::endl;
    
    scaling *= viewport.getWidth()/800.0f; // scale race GUI along screen size
    
    //std::cout << "Scale : " << scaling.X << ", " << scaling.Y << std::endl;

    if (kart->hasViewBlockedByPlunger())
    {
        int offset_y = viewport.UpperLeftCorner.Y;
        
        const int screen_width = viewport.LowerRightCorner.X 
                               - viewport.UpperLeftCorner.X;
        const int plunger_size = viewport.LowerRightCorner.Y 
                               - viewport.UpperLeftCorner.Y;
        int plunger_x = viewport.UpperLeftCorner.X + screen_width/2 
                      - plunger_size/2;
        
        video::ITexture *t=m_plunger_face->getTexture();
        core::rect<s32> dest(plunger_x,              offset_y, 
                             plunger_x+plunger_size, offset_y+plunger_size);
        const core::rect<s32> source(core::position2d<s32>(0,0), 
                                     t->getOriginalSize());
                
        irr_driver->getVideoDriver()->draw2DImage(t, dest, source, 
                                                  NULL /* clip */, 
                                                  NULL /* color */, 
                                                  true /* alpha */);
    }

    
    drawAllMessages     (kart, viewport, scaling);
    if(!World::getWorld()->isRacePhase()) return;

    MinimalRaceGUI::KartIconDisplayInfo* info = World::getWorld()->getKartsDisplayInfo();

    drawPowerupIcons    (kart, viewport, scaling);
    drawEnergyMeter     (kart, viewport, scaling);
    drawRankLap         (info, kart, viewport);

}   // renderPlayerView

//-----------------------------------------------------------------------------
/** Displays the racing time on the screen.s
 */
void MinimalRaceGUI::drawGlobalTimer()
{
    assert(World::getWorld() != NULL);
    
    if(!World::getWorld()->shouldDrawTimer()) return;
    std::string s = StringUtils::timeToString(World::getWorld()->getTime());
    core::stringw sw(s.c_str());

    static video::SColor time_color = video::SColor(255, 255, 255, 255);
    int x,y;
    switch(race_manager->getNumLocalPlayers())
    {
    case 1: x = 10; y=0; break;
    case 2: x = 10; y=0; break;
    case 3: x = UserConfigParams::m_width   - m_timer_width-10; 
            y = UserConfigParams::m_height/2; break;
    case 4: x = UserConfigParams::m_width/2 - m_timer_width/2; 
            y = 0;       break;
    }   // switch        

    core::rect<s32> pos(x,                         y, 
                        UserConfigParams::m_width, y+50);
    
    
    gui::ScalableFont* font = GUIEngine::getFont();
    float old_scale = font->getScale();
    font->setScale(m_font_scale);
    font->draw(sw.c_str(), pos, time_color, false, false, NULL, true /* ignore RTL */);
    font->setScale(old_scale);
}   // drawGlobalTimer

//-----------------------------------------------------------------------------
/** Draws the mini map and the position of all karts on it.
 */
void MinimalRaceGUI::drawGlobalMiniMap()
{
    World *world = World::getWorld();
    // arenas currently don't have a map.
    if(world->getTrack()->isArena()) return;

    const video::ITexture *mini_map=world->getTrack()->getMiniMap();
    
    int upper_y = m_map_bottom-m_map_height;
    int lower_y = m_map_bottom;
    
    core::rect<s32> dest(m_map_left,               upper_y, 
                         m_map_left + m_map_width, lower_y);
    core::rect<s32> source(core::position2di(0, 0), mini_map->getOriginalSize());
    irr_driver->getVideoDriver()->draw2DImage(mini_map, dest, source, 0, 0, true);

    for(unsigned int i=0; i<world->getNumKarts(); i++)
    {
        const Kart *kart = world->getKart(i);
        if(kart->isEliminated()) continue;   // don't draw eliminated kart
        const Vec3& xyz = kart->getXYZ();
        Vec3 draw_at;
        world->getTrack()->mapPoint2MiniMap(xyz, &draw_at);

        core::rect<s32> source(i    *m_marker_rendered_size,
                               0, 
                               (i+1)*m_marker_rendered_size, 
                               m_marker_rendered_size);
        int marker_half_size = (kart->getController()->isPlayerController() 
                                ? m_marker_player_size 
                                : m_marker_ai_size                        )>>1;
        core::rect<s32> position(m_map_left+(int)(draw_at.getX()-marker_half_size), 
                                 lower_y   -(int)(draw_at.getY()+marker_half_size),
                                 m_map_left+(int)(draw_at.getX()+marker_half_size), 
                                 lower_y   -(int)(draw_at.getY()-marker_half_size));

        // Highlight the player icons with some backgorund image.
        if (kart->getController()->isPlayerController())
        {
            video::SColor colors[4];
            for (unsigned int i=0;i<4;i++)
            {
                colors[i]=kart->getKartProperties()->getColor();
            }
            const core::rect<s32> rect(core::position2d<s32>(0,0),
                m_icons_frame->getTexture()->getOriginalSize());
            
            irr_driver->getVideoDriver()->draw2DImage(
                m_icons_frame->getTexture(), position, rect,
                NULL, colors, true);
        }   // if isPlayerController

        irr_driver->getVideoDriver()->draw2DImage(m_marker, position, source, 
                                                  NULL, NULL, true);
    }   // for i<getNumKarts
}   // drawGlobalMiniMap

//-----------------------------------------------------------------------------
void MinimalRaceGUI::drawPowerupIcons(const Kart* kart, 
                               const core::recti &viewport, 
                               const core::vector2df &scaling)
{
    // If player doesn't have anything, do nothing.
    const Powerup* powerup = kart->getPowerup();
    if(powerup->getType() == PowerupManager::POWERUP_NOTHING) return;
    int n  = kart->getNumPowerup() ;
    if(n<1) return;    // shouldn't happen, but just in case
    if(n>5) n=5;       // Display at most 5 items

    int nSize=(int)(64.0f*std::min(scaling.X, scaling.Y));
        
    int itemSpacing = (int)(std::min(scaling.X, scaling.Y)*30);
    
    int x1 = viewport.UpperLeftCorner.X  + viewport.getWidth()/2 
           - (n * itemSpacing)/2;
    int y1 = viewport.UpperLeftCorner.Y  + (int)(20 * scaling.Y);

    assert(powerup != NULL);
    assert(powerup->getIcon() != NULL);
    video::ITexture *t=powerup->getIcon()->getTexture();
    assert(t != NULL);
    core::rect<s32> rect(core::position2di(0, 0), t->getOriginalSize());
    
    for ( int i = 0 ; i < n ; i++ )
    {
        int x2=(int)(x1+i*itemSpacing);
        core::rect<s32> pos(x2, y1, x2+nSize, y1+nSize);
        irr_driver->getVideoDriver()->draw2DImage(t, pos, rect, NULL, 
                                                  NULL, true);
    }   // for i
}   // drawPowerupIcons

//-----------------------------------------------------------------------------
/** Energy meter that gets filled with nitro. This function is called from
 *  drawSpeedAndEnergy, which defines the correct position of the energy
 *  meter.
 *  \param x X position of the meter.
 *  \param y Y position of the meter.
 *  \param kart Kart to display the data for.
 *  \param scaling Scaling applied (in case of split screen)
 */
void MinimalRaceGUI::drawEnergyMeter(const Kart *kart,              
                                     const core::recti &viewport, 
                                     const core::vector2df &scaling)
{
    float state = (float)(kart->getEnergy()) / MAX_NITRO;
    if      (state < 0.0f) state = 0.0f;
    else if (state > 1.0f) state = 1.0f;
    
    int h = (int)(viewport.getHeight()/3);
    int w = h/4; // gauge image is so 1:4
    
    // In split screen mode of 3 or 4 players, the players on
    // the left side will have the energy meter on the left side
    int mirrored = race_manager->getNumLocalPlayers()>=3 &&
                   viewport.UpperLeftCorner.X==0;

    int x = mirrored ? 0 : viewport.LowerRightCorner.X - w;
    int y = viewport.UpperLeftCorner.Y + viewport.getHeight()/2- h/2;
    
    // Background
    // ----------
    core::rect<s32> dest(x+mirrored*w, y+mirrored*h, 
                         x+(1-mirrored)*w, y+(1-mirrored)*h);

    irr_driver->getVideoDriver()->draw2DImage(m_gauge_empty, dest,
                                              core::rect<s32>(0, 0, 64, 256) /* source rect */,
                                              NULL /* clip rect */, NULL /* colors */,
                                              true /* alpha */);
    // Target
    // ------
    if (race_manager->getCoinTarget() > 0)
    {
        float coin_target = (float)race_manager->getCoinTarget() / MAX_NITRO;
        
        const int EMPTY_TOP_PIXELS = 4;
        const int EMPTY_BOTTOM_PIXELS = 3;
        int y1 = y + (int)(EMPTY_TOP_PIXELS + 
                             (h - EMPTY_TOP_PIXELS - EMPTY_BOTTOM_PIXELS)
                            *(1.0f - coin_target)                        );
        if (state >= 1.0f) y1 = y;
        
        core::rect<s32> clip(x, y1, x + w, y + h);
        irr_driver->getVideoDriver()->draw2DImage(m_gauge_goal, core::rect<s32>(x, y, x+w, y+h) /* dest rect */,
                                                  core::rect<s32>(0, 0, 64, 256) /* source rect */,
                                                  &clip, NULL /* colors */, true /* alpha */);
    }
    
    // Filling (current state)
    // -----------------------
    if (state > 0.0f)
    {
        const int EMPTY_TOP_PIXELS = 4;
        const int EMPTY_BOTTOM_PIXELS = 3;
        int y1 = y + (int)(EMPTY_TOP_PIXELS 
                           + (h - EMPTY_TOP_PIXELS - EMPTY_BOTTOM_PIXELS)
                              *(1.0f - state)                             );
        if (state >= 1.0f) y1 = y;
        core::rect<s32> dest(x+mirrored*w,
                             mirrored ? y+h : y,
                             x+(1-mirrored)*w,
                             mirrored ? y : y + h);
        core::rect<s32> clip(x, y1, x + w, y + h);
        core::rect<s32> tex_c(0,
                              mirrored ? 256 :   0,
                              64,
                              mirrored ?   0 : 256);
        irr_driver->getVideoDriver()->draw2DImage(m_gauge_full, dest,
                                                  tex_c,
                                                  &clip, NULL /* colors */, true /* alpha */);
    }
    
    
}   // drawEnergyMeter

//-----------------------------------------------------------------------------
/** Displays the rank and the lap of the kart.
 *  \param info Info object c
*/
void MinimalRaceGUI::drawRankLap(const KartIconDisplayInfo* info, 
                                 const Kart* kart,
                                 const core::recti &viewport)
{
    // Don't display laps or ranks if the kart has already finished the race.
    if (kart->hasFinishedRace()) return;

    core::recti pos;

    gui::ScalableFont* font = (race_manager->getNumLocalPlayers() > 2 
                            ? GUIEngine::getSmallFont() 
                            : GUIEngine::getFont());
    float scale = font->getScale();
    font->setScale(m_font_scale);
    // Add a black shadow to make the text better readable on
    // 'white' tracks (e.g. with snow and ice).
    font->setShadow(video::SColor(255, 0, 0, 0));
    static video::SColor color = video::SColor(255, 255, 255, 255);
    WorldWithRank *world    = (WorldWithRank*)(World::getWorld());

    if (world->displayRank())
    {
        pos.UpperLeftCorner.Y   = viewport.UpperLeftCorner.Y;
        pos.LowerRightCorner.Y  = viewport.UpperLeftCorner.Y+50;
        // Split screen 3 or 4 players, left side:
        if(viewport.LowerRightCorner.X < UserConfigParams::m_width)
        {
            pos.UpperLeftCorner.X   = 10;
            pos.LowerRightCorner.X  = viewport.LowerRightCorner.X;
        }
        else
        {
            pos.UpperLeftCorner.X   = viewport.LowerRightCorner.X
                                    - m_rank_width-10;
            pos.LowerRightCorner.X  = viewport.LowerRightCorner.X;
        }

        char str[256];
        sprintf(str, "%d/%d", kart->getPosition(), 
                world->getCurrentNumKarts());
        font->draw(str, pos, color);
    }
    
    // Don't display laps in follow the leader mode
    if(world->raceHasLaps())
    {
        const int lap = info[kart->getWorldKartId()].lap;
    
        // don't display 'lap 0/...'
        if(lap>=0)
        {
            pos.LowerRightCorner.Y  = viewport.LowerRightCorner.Y;
            pos.UpperLeftCorner.Y   = viewport.LowerRightCorner.Y-60;
            pos.LowerRightCorner.X  = viewport.LowerRightCorner.X;
            // Split screen 3 or 4 players, left side:
            if(viewport.LowerRightCorner.X < UserConfigParams::m_width)
            {
                pos.UpperLeftCorner.X = 10;
            }
            else
            {
                pos.UpperLeftCorner.X = (int)(viewport.LowerRightCorner.X
                                              - m_lap_width -10          );
            }

            char str[256];
            sprintf(str, "%d/%d", lap+1, race_manager->getNumLaps());
            core::stringw s = m_string_lap+" "+str;
            font->draw(s.c_str(), pos, color);
        }
    }
    font->setScale(scale);
    font->disableShadow();
} // drawRankLap

//-----------------------------------------------------------------------------
/** Removes messages which have been displayed long enough. This function
 *  must be called after drawAllMessages, otherwise messages which are only
 *  displayed once will not be drawn!
 **/

void MinimalRaceGUI::cleanupMessages(const float dt)
{
    AllMessageType::iterator p =m_messages.begin(); 
    while(p!=m_messages.end())
    {
        if((*p).done(dt))
        {
            p = m_messages.erase(p);
        }
        else
        {
            ++p;
        }
    }
}   // cleanupMessages

//-----------------------------------------------------------------------------
/** Displays all messages in the message queue
 **/
void MinimalRaceGUI::drawAllMessages(const Kart* kart,
                              const core::recti &viewport, 
                              const core::vector2df &scaling)
{    
    int y = viewport.LowerRightCorner.Y - m_small_font_max_height - 10;
          
    const int x = (viewport.LowerRightCorner.X + viewport.UpperLeftCorner.X)/2;
    const int w = (viewport.LowerRightCorner.X - viewport.UpperLeftCorner.X);    
    
    // First line of text somewhat under the top of the viewport.
    y = (int)(viewport.UpperLeftCorner.Y + 164*scaling.Y);

    gui::ScalableFont* font = GUIEngine::getFont();
    int font_height = m_max_font_height;
    if (race_manager->getNumLocalPlayers() > 2)
    {
        font = GUIEngine::getSmallFont();
        font_height = m_small_font_max_height;
    }
    
    // The message are displayed in reverse order, so that a multi-line
    // message (addMessage("1", ...); addMessage("2",...) is displayed
    // in the right order: "1" on top of "2"
    for (AllMessageType::const_iterator i = m_messages.begin(); 
         i != m_messages.end(); ++i)
    {
        TimedMessage const &msg = *i;

         // less important messages are not displayed in minimal mode
        if (!msg.m_important) continue;
        
        // Display only messages for all karts, or messages for this kart
        if (msg.m_kart && msg.m_kart!=kart) continue;

        core::rect<s32> pos(x - w/2, y, x + w/2, y + font_height);
        
        font->draw(core::stringw(msg.m_message.c_str()).c_str(),
                   pos, msg.m_color, true /* hcenter */, 
                   true /* vcenter */);
        
        y += font_height;
    }   // for i in all messages
}   // drawAllMessages

//-----------------------------------------------------------------------------
/** Adds a message to the message queue. The message is displayed for a
 *  certain amount of time (unless time<0, then the message is displayed
 *  once).
 **/
void MinimalRaceGUI::addMessage(const core::stringw &msg, const Kart *kart, 
                         float time, int font_size, 
                         const video::SColor &color, bool important)
{
    m_messages.push_back(TimedMessage(msg, kart, time, font_size, color,
                                      important));
}   // addMessage

//-----------------------------------------------------------------------------
// Displays the description given for the music currently being played -
// usually the title and composer.
void MinimalRaceGUI::drawGlobalMusicDescription()
{
     // show no music description when it's off
    if (!UserConfigParams::m_music) return;
    
    gui::IGUIFont*       font = GUIEngine::getFont();

    float race_time = World::getWorld()->getTime();
    // In follow the leader the clock counts backwards, so convert the
    // countdown time to time since start:
    if(race_manager->getMinorMode()==RaceManager::MINOR_MODE_FOLLOW_LEADER)
        race_time = ((FollowTheLeaderRace*)World::getWorld())->getClockStartTime()
                  - race_time;
    // ---- Manage pulsing effect
    // 3.0 is the duration of ready/set (TODO: don't hardcode)
    float timeProgression = (float)(race_time) /
                            (float)(stk_config->m_music_credit_time - 2.0f);
    
    const int x_pulse = (int)(sin(race_time*9.0f)*10.0f);
    const int y_pulse = (int)(cos(race_time*9.0f)*10.0f);
    
    float resize = 1.0f;
    if (timeProgression < 0.1)
    {
        resize = timeProgression/0.1f;
    }
    else if (timeProgression > 0.9)
    {
        resize = 1.0f - (timeProgression - 0.9f)/0.1f;
    }
    
    const float resize3 = resize*resize*resize;
    
    // Get song name, and calculate its size, allowing us to position stuff
    const MusicInformation* mi = music_manager->getCurrentMusic();
    if (!mi) return;
    
    std::string s="\""+mi->getTitle()+"\"";
    core::stringw thetext(s.c_str());
    
    core::dimension2d< u32 > textSize = font->getDimension(thetext.c_str());
    int textWidth = textSize.Width;
    
    int textWidth2 = 0;
    core::stringw thetext_composer;
    if (mi->getComposer()!="")
    {
        // I18N: string used to show the author of the music. (e.g. "Sunny Song" by "John Doe")
        thetext_composer = _("by");
        thetext_composer += " ";
        thetext_composer += mi->getComposer().c_str();
        textWidth2 = font->getDimension(thetext_composer.c_str()).Width;
    }
    const int max_text_size = (int)(UserConfigParams::m_width*2.0f/3.0f);
    if (textWidth  > max_text_size) textWidth  = max_text_size;
    if (textWidth2 > max_text_size) textWidth2 = max_text_size;

    const int ICON_SIZE = 64;
    const int y         = UserConfigParams::m_height - 80;
    // the 20 is an arbitrary space left between the note icon and the text
    const int noteX     = (UserConfigParams::m_width / 2) 
                        - std::max(textWidth, textWidth2)/2 - ICON_SIZE/2 - 20;
    const int noteY     = y;
    // the 20 is an arbitrary space left between the note icon and the text
    const int textXFrom = (UserConfigParams::m_width / 2) 
                        - std::max(textWidth, textWidth2)/2 + 20;
    const int textXTo   = (UserConfigParams::m_width / 2) 
                        + std::max(textWidth, textWidth2)/2 + 20;

    // ---- Draw "by" text
    const int text_y = (int)(UserConfigParams::m_height - 80*(resize3) 
                     + 40*(1-resize));
    
    static const video::SColor white = video::SColor(255, 255, 255, 255);
    if(mi->getComposer()!="")
    {
        core::rect<s32> pos_by(textXFrom, text_y+40,
                               textXTo,   text_y+40);
        std::string s="by "+mi->getComposer();
        font->draw(core::stringw(s.c_str()).c_str(), pos_by, white, 
                   true, true);
    }
    
    // ---- Draw "song name" text
    core::rect<s32> pos(textXFrom, text_y,
                        textXTo,   text_y);
    
    font->draw(thetext.c_str(), pos, white, true /* hcenter */, 
               true /* vcenter */);
 
    // Draw music icon
    int iconSizeX = (int)(ICON_SIZE*resize + x_pulse*resize*resize);
    int iconSizeY = (int)(ICON_SIZE*resize + y_pulse*resize*resize);
    
    video::ITexture *t = m_music_icon->getTexture();
    core::rect<s32> dest(noteX-iconSizeX/2+20,    
                         noteY-iconSizeY/2+ICON_SIZE/2,
                         noteX+iconSizeX/2+20,
                         noteY+iconSizeY/2+ICON_SIZE/2);
    const core::rect<s32> source(core::position2d<s32>(0,0), 
                                 t->getOriginalSize());
    
    irr_driver->getVideoDriver()->draw2DImage(t, dest, source,
                                              NULL, NULL, true);
}   // drawGlobalMusicDescription

// ----------------------------------------------------------------------------
/** Draws the ready-set-go message on the screen.
 */
void MinimalRaceGUI::drawGlobalReadySetGo()
{
    switch (World::getWorld()->getPhase())
    {
    case WorldStatus::READY_PHASE:
        {
            static video::SColor color = video::SColor(255, 255, 255, 255);
            core::rect<s32> pos(UserConfigParams::m_width>>1, 
                                UserConfigParams::m_height>>1,
                                UserConfigParams::m_width>>1,
                                UserConfigParams::m_height>>1);
            gui::IGUIFont* font = GUIEngine::getTitleFont();
            font->draw(m_string_ready.c_str(), pos, color, true, true);
        }
        break;
    case WorldStatus::SET_PHASE:
        {
            static video::SColor color = video::SColor(255, 255, 255, 255);
            core::rect<s32> pos(UserConfigParams::m_width>>1, 
                                UserConfigParams::m_height>>1,
                                UserConfigParams::m_width>>1, 
                                UserConfigParams::m_height>>1);
            gui::IGUIFont* font = GUIEngine::getTitleFont();
            //I18N: as in "ready, set, go", shown at the beginning of the race
            font->draw(m_string_set.c_str(), pos, color, true, true);
        }
        break;
    case WorldStatus::GO_PHASE:
        {
            static video::SColor color = video::SColor(255, 255, 255, 255);
            core::rect<s32> pos(UserConfigParams::m_width>>1, 
                                UserConfigParams::m_height>>1,
                                UserConfigParams::m_width>>1, 
                                UserConfigParams::m_height>>1);
            //gui::IGUIFont* font = irr_driver->getRaceFont();
            gui::IGUIFont* font = GUIEngine::getTitleFont();
            //I18N: as in "ready, set, go", shown at the beginning of the race
            font->draw(m_string_go.c_str(), pos, color, true, true);
        }
        break;
    default: 
         break;
    }   // switch
}   // drawGlobalReadySetGo
