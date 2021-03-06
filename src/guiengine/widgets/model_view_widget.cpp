//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2009-2015 Marianne Gagnon
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

#include "config/user_config.hpp"
#include "graphics/central_settings.hpp"
#include "graphics/render_info.hpp"
#include "guiengine/engine.hpp"
#include "guiengine/widgets/model_view_widget.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/render_target.hpp"

#include <IAnimatedMesh.h>
#include <IAnimatedMeshSceneNode.h>
#include <ICameraSceneNode.h>
#include <ILightSceneNode.h>
#include <ISceneManager.h>
#include <IMeshSceneNode.h>

#include <algorithm>

using namespace GUIEngine;
using namespace irr::core;
using namespace irr::gui;

ModelViewWidget::ModelViewWidget() :
IconButtonWidget(IconButtonWidget::SCALE_MODE_KEEP_TEXTURE_ASPECT_RATIO, false, false)
{
    m_rtt_main_node = NULL;
    m_camera = NULL;
    m_light = NULL;
    m_type = WTYPE_MODEL_VIEW;
    m_render_target = NULL;
    m_rotation_mode = ROTATE_OFF;
    m_render_info = new RenderInfo();
    m_angle = 0;

    // so that the base class doesn't complain there is no icon defined
    m_properties[PROP_ICON]="gui/main_help.png";

    m_rtt_unsupported = false;
}   // ModelViewWidget

// -----------------------------------------------------------------------------
ModelViewWidget::~ModelViewWidget()
{
    GUIEngine::needsUpdate.remove(this);
#ifndef SERVER_ONLY    
    delete m_render_info;
#endif
}   // ~ModelViewWidget

// -----------------------------------------------------------------------------
void ModelViewWidget::add()
{
    // so that the base class doesn't complain there is no icon defined
    m_properties[PROP_ICON]="gui/main_help.png";

    IconButtonWidget::add();

    /*
     FIXME: remove this unclean thing, I think irrlicht provides this feature:
     virtual void IGUIElement::OnPostRender (u32 timeMs)
     \brief animate the element and its children.
     */
    GUIEngine::needsUpdate.push_back(this);

}   // add

// -----------------------------------------------------------------------------
void ModelViewWidget::clearModels()
{
    m_models.clearWithoutDeleting();
    m_model_location.clear();
    m_model_scale.clear();
    m_model_frames.clear();
    m_model_render_info_affected.clear();

    if (m_rtt_main_node != NULL) m_rtt_main_node->remove();
    if (m_light != NULL) m_light->remove();
    if (m_camera != NULL) m_camera->remove();

    m_rtt_main_node = NULL;
    m_camera = NULL;
    m_light = NULL;
}   // clearModels

// -----------------------------------------------------------------------------

void ModelViewWidget::addModel(irr::scene::IMesh* mesh, const Vec3& location,
                               const Vec3& scale, const int frame,
                               bool all_parts_colorized)
{
    if(!mesh) return;

    m_models.push_back(mesh);
    m_model_location.push_back(location);
    m_model_scale.push_back(scale);
    m_model_frames.push_back(frame);
    m_model_render_info_affected.push_back(all_parts_colorized);
#ifndef SERVER_ONLY
    if (!CVS->isGLSL())
        m_render_target = NULL;
#endif
}   // addModel

// -----------------------------------------------------------------------------
void ModelViewWidget::update(float delta)
{
    if (m_rtt_unsupported) return;

    if (m_rotation_mode == ROTATE_CONTINUOUSLY)
    {
        m_angle += delta*m_rotation_speed;
        if (m_angle > 360) m_angle -= 360;
    }
    else if (m_rotation_mode == ROTATE_TO)
    {
        // check if we should rotate clockwise or counter-clockwise to reach the target faster
        // (taking wrap-arounds into account)
        const int angle_distance_from_end  = (int)(360 - m_angle);
        const int target_distance_from_end = (int)(360 - m_angle);

        int distance_with_positive_rotation;
        int distance_with_negative_rotation;

        if (m_angle < m_rotation_target)
        {
            distance_with_positive_rotation = (int)(m_rotation_target - m_angle);
            distance_with_negative_rotation = (int)(m_angle + target_distance_from_end);
        }
        else
        {
            distance_with_positive_rotation = (int)(angle_distance_from_end + m_rotation_target);
            distance_with_negative_rotation = (int)(m_angle - m_rotation_target);
        }

        //Log::info("ModelViewWidget", "distance_with_positive_rotation = %d; "
        //    "distance_with_negative_rotation = %d; angle = %f", distance_with_positive_rotation,
        //    distance_with_negative_rotation, angle);

        if (distance_with_positive_rotation < distance_with_negative_rotation)
        {
            m_angle += m_rotation_speed * delta*(3.0f + std::min(distance_with_positive_rotation, distance_with_negative_rotation)*2.0f);
        }
        else
        {
            m_angle -= m_rotation_speed * delta*(3.0f + std::min(distance_with_positive_rotation, distance_with_negative_rotation)*2.0f);
        }
        if (m_angle > 360) m_angle -= 360;
        if (m_angle < 0) m_angle += 360;

        // stop rotating when target reached
        if (fabsf(m_angle - m_rotation_target) < 2.0f) m_rotation_mode = ROTATE_OFF;
    }
   
#ifdef SERVER_ONLY 
    return;
#else
    if (m_render_target == NULL)
    {
        std::string name = "model view ";
        name += m_properties[PROP_ID].c_str();
        m_render_target = irr_driver->createRenderTarget(irr::core::dimension2du(512,512), name);
    }

    if (m_rtt_main_node == NULL)
    {
        setupRTTScene();
    }    

    m_rtt_main_node->setRotation(core::vector3df(0.0f, m_angle, 0.0f));

    m_rtt_main_node->setVisible(true);

    m_render_target->renderToTexture(m_camera, GUIEngine::getLatestDt());

    m_rtt_main_node->setVisible(false);
#endif
}   // update

// ----------------------------------------------------------------------------
void ModelViewWidget::setupRTTScene()
{
#ifndef SERVER_ONLY
    irr_driver->suppressSkyBox();

    if (m_rtt_main_node != NULL) m_rtt_main_node->remove();
    if (m_light != NULL) m_light->remove();
    if (m_camera != NULL) m_camera->remove();

    m_rtt_main_node = NULL;
    m_camera = NULL;
    m_light = NULL;

    irr_driver->clearLights();

    if (m_model_frames[0] == -1)
    {
        scene::ISceneNode* node = irr_driver->addMesh(m_models.get(0), "rtt_mesh",
            NULL, m_render_info, m_model_render_info_affected[0]);
        node->setPosition(m_model_location[0].toIrrVector());
        node->setScale(m_model_scale[0].toIrrVector());
        node->setMaterialFlag(video::EMF_FOG_ENABLE, false);
        m_rtt_main_node = node;
    }
    else
    {
        scene::IAnimatedMeshSceneNode* node =
        irr_driver->addAnimatedMesh((scene::IAnimatedMesh*)m_models.get(0), "rtt_mesh",
            NULL, m_render_info, m_model_render_info_affected[0]);
        node->setPosition(m_model_location[0].toIrrVector());
        node->setFrameLoop(m_model_frames[0], m_model_frames[0]);
        node->setAnimationSpeed(0);
        node->setScale(m_model_scale[0].toIrrVector());
        node->setMaterialFlag(video::EMF_FOG_ENABLE, false);
        m_rtt_main_node = node;
    }

    assert(m_rtt_main_node != NULL);
    assert(m_models.size() == m_model_location.size());
    assert(m_models.size() == m_model_frames.size());
    assert(m_models.size() == m_model_scale.size());
    assert(m_models.size() == m_model_render_info_affected.size());

    const int mesh_amount = m_models.size();
    for (int n = 1; n<mesh_amount; n++)
    {
        if (m_model_frames[n] == -1)
        {
            scene::ISceneNode* node =
            irr_driver->addMesh(m_models.get(n), "rtt_node", m_rtt_main_node,
                m_render_info, m_model_render_info_affected[n]);
            node->setPosition(m_model_location[n].toIrrVector());
            node->updateAbsolutePosition();
            node->setScale(m_model_scale[n].toIrrVector());
        }
        else
        {
            scene::IAnimatedMeshSceneNode* node =
            irr_driver->addAnimatedMesh((scene::IAnimatedMesh*)m_models.get(n),
                "modelviewrtt", m_rtt_main_node, m_render_info,
                m_model_render_info_affected[n]);
            node->setPosition(m_model_location[n].toIrrVector());
            node->setFrameLoop(m_model_frames[n], m_model_frames[n]);
            node->setAnimationSpeed(0);
            node->updateAbsolutePosition();
            node->setScale(m_model_scale[n].toIrrVector());
            //Log::info("ModelViewWidget", "Set frame %d", m_model_frames[n]);
        }
    }

    irr_driver->setAmbientLight(video::SColor(255, 35, 35, 35));

    const core::vector3df &spot_pos = core::vector3df(0, 30, 40);

    if (!CVS->isGLSL())
    {
        scene::ILightSceneNode* light = irr_driver->getSceneManager()
            ->addLightSceneNode(NULL, spot_pos, video::SColorf(1.0f,1.0f,1.0f),
                                1600 /* radius */);
        light->setLightType(video::ELT_SPOT);
        light->setRotation((core::vector3df(0, 10, 0) - spot_pos).getHorizontalAngle());
        light->updateAbsolutePosition();
        m_light = light;
    }
    else
    {
        m_light = irr_driver->addLight(spot_pos, 0.3f /* energy */, 10 /* distance */, 1.0f /* r */, 1.0f /* g */, 1.0f /* g*/, true, NULL);
    }

    m_rtt_main_node->setMaterialFlag(video::EMF_GOURAUD_SHADING, true);
    m_rtt_main_node->setMaterialFlag(video::EMF_LIGHTING, true);

    const int materials = m_rtt_main_node->getMaterialCount();
    for (int n = 0; n<materials; n++)
    {
        m_rtt_main_node->getMaterial(n).setFlag(video::EMF_LIGHTING, true);

        // set size of specular highlights
        m_rtt_main_node->getMaterial(n).Shininess = 100.0f;
        m_rtt_main_node->getMaterial(n).SpecularColor.set(255, 50, 50, 50);
        m_rtt_main_node->getMaterial(n).DiffuseColor.set(255, 150, 150, 150);

        m_rtt_main_node->getMaterial(n).setFlag(video::EMF_GOURAUD_SHADING,
                                                true);
    }

    m_camera = irr_driver->getSceneManager()->addCameraSceneNode();
    m_camera->setAspectRatio(1.0f);

    m_camera->setPosition(core::vector3df(0.0, 20.0f, 70.0f));
    m_camera->setUpVector(core::vector3df(0.0, 1.0, 0.0));
    m_camera->setTarget(core::vector3df(0, 10, 0.0f));
    m_camera->setFOV(DEGREE_TO_RAD*50.0f);
    m_camera->updateAbsolutePosition();

#endif
}   // setupRTTScene

// ----------------------------------------------------------------------------
void ModelViewWidget::setRotateOff()
{
    m_rotation_mode = ROTATE_OFF;
}   // setRotateOff

// ----------------------------------------------------------------------------
void ModelViewWidget::setRotateContinuously(float speed)
{
    m_rotation_mode = ROTATE_CONTINUOUSLY;
    m_rotation_speed = speed;
}   // setRotateContinuously

// ----------------------------------------------------------------------------
void ModelViewWidget::setRotateTo(float targetAngle, float speed)
{
    m_rotation_mode = ROTATE_TO;
    m_rotation_speed = speed;
    m_rotation_target = targetAngle;
}   // setRotateTo

// ----------------------------------------------------------------------------
bool ModelViewWidget::isRotating()
{
    return m_rotation_mode != ROTATE_OFF ? true : false;
}   // isRotating

// ----------------------------------------------------------------------------
void ModelViewWidget::elementRemoved()
{
#ifndef SERVER_ONLY
    m_render_target = NULL;
    IconButtonWidget::elementRemoved();
#endif
}   // elementRemoved

// ----------------------------------------------------------------------------
void ModelViewWidget::clearRttProvider()
{
    m_render_target = NULL;
}   // clearRttProvider

// ----------------------------------------------------------------------------
void ModelViewWidget::drawRTTScene(const irr::core::rect<s32>& dest_rect) const
{
#ifndef SERVER_ONLY
    if(m_render_target != NULL)
        m_render_target->draw2DImage(dest_rect, NULL, video::SColor(255, 255, 255, 255), true);
#endif
}   // drawRTTScene

