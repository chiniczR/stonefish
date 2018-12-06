//
//  OpenGLPipeline.cpp
//  Stonefish
//
//  Created by Patryk Cieslak on 30/03/2014.
//  Copyright(c) 2014-2018 Patryk Cieslak. All rights reserved.
//

#include "graphics/OpenGLPipeline.h"

#include "core/SimulationManager.h"
#include "graphics/GLSLShader.h"
#include "graphics/OpenGLContent.h"
#include "graphics/OpenGLOcean.h"
#include "graphics/OpenGLCamera.h"
#include "graphics/OpenGLRealCamera.h"
#include "graphics/OpenGLDepthCamera.h"
#include "graphics/OpenGLAtmosphere.h"
#include "graphics/OpenGLLight.h"
#include "core/Console.h"
#include "entities/forcefields/Ocean.h"
#include "entities/forcefields/Atmosphere.h"
#include "controllers/PathGenerator.h"
#include "controllers/PathFollowingController.h"

namespace sf
{

OpenGLPipeline::OpenGLPipeline(RenderSettings s, HelperSettings h) : rSettings(s), hSettings(h)
{
    drawingQueueMutex = SDL_CreateMutex();
    
    //Set default OpenGL options
    cInfo("Setting up basic OpenGL parameters...");
    
    //OpenGL flags and params
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glPointSize(2.f);
    glLineWidth(2.f);
    glLineStipple(3, 0xE4E4);
    glPatchParameteri(GL_PATCH_VERTICES, 4);
    glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, new GLfloat[2]{1,1});
    glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, new GLfloat[4]{1,1,1,1});
    if(s.msaa) glEnable(GL_MULTISAMPLE); else glDisable(GL_MULTISAMPLE);
    
    GLint texUnits;
    GLint maxTexLayers;
    GLint maxUniforms;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texUnits);
    glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxTexLayers);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &maxUniforms);
    cInfo("%d texture units available.", texUnits);
    cInfo("%d texture layers allowed.", maxTexLayers);
    cInfo("%d uniforms in fragment shader allowed.", maxUniforms);
    
    if(GLEW_VERSION_3_3)
    {
        cInfo("OpenGL 3.3 supported.");
        
        if(GLEW_VERSION_4_3)
            cInfo("OpenGL 4.3 supported.");
        else
            cWarning("OpenGL 4.3 not supported!");
    }
    else
        cCritical("OpenGL 3.3 not supported - minimum requirements not met!");
    
    //Load shaders and create rendering buffers
    OpenGLAtmosphere::BuildAtmosphereAPI(rSettings.atmosphere);
    OpenGLCamera::Init();
    OpenGLDepthCamera::Init();
    content = new OpenGLContent();
    
    //Create display framebuffer
    glGenFramebuffers(1, &screenFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, screenFBO);
    
    glGenTextures(1, &screenTex);
    glBindTexture(GL_TEXTURE_2D, screenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rSettings.windowW, rSettings.windowH, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); //Cheaper/better gaussian blur for GUI background
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //Cheaper/better gaussian blur for GUI background
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, screenTex, 0);
    
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status != GL_FRAMEBUFFER_COMPLETE)
    cError("Display FBO initialization failed!");
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

OpenGLPipeline::~OpenGLPipeline()
{
    OpenGLCamera::Destroy();
    OpenGLLight::Destroy();
    delete content;
    
    glDeleteTextures(1, &screenTex);
    glDeleteFramebuffers(1, &screenFBO);
    SDL_DestroyMutex(drawingQueueMutex);
}

RenderSettings OpenGLPipeline::getRenderSettings() const
{
    return rSettings;
}
    
HelperSettings& OpenGLPipeline::getHelperSettings()
{
    return hSettings;
}
    
GLuint OpenGLPipeline::getScreenTexture()
{
    return screenTex;
}

SDL_mutex* OpenGLPipeline::getDrawingQueueMutex()
{
    return drawingQueueMutex;
}
    
OpenGLContent* OpenGLPipeline::getContent()
{
    return content;
}

void OpenGLPipeline::AddToDrawingQueue(Renderable r)
{
	drawingQueue.push_back(r);
}

bool OpenGLPipeline::isDrawingQueueEmpty()
{
	return drawingQueue.empty();
}
    
void OpenGLPipeline::PerformDrawingQueueCopy()
{
    if(!drawingQueue.empty())
    {
        drawingQueueCopy.clear();
        SDL_LockMutex(drawingQueueMutex);
        drawingQueueCopy.insert(drawingQueueCopy.end(), drawingQueue.begin(), drawingQueue.end());
        //Update camera transforms to ensure consistency
        for(unsigned int i=0; i < content->getViewsCount(); ++i)
            if(content->getView(i)->getType() == ViewType::CAMERA)
                ((OpenGLRealCamera*)content->getView(i))->UpdateTransform();
            else if(content->getView(i)->getType() == ViewType::DEPTH_CAMERA)
                ((OpenGLDepthCamera*)content->getView(i))->UpdateTransform();
        //Update light transforms to ensure consistency
        for(unsigned int i=0; i < content->getLightsCount(); ++i)
            content->getLight(i)->UpdateTransform();
        drawingQueue.clear(); //Enable update of drawing queue by clearing old queue
        SDL_UnlockMutex(drawingQueueMutex);
    }
}

void OpenGLPipeline::DrawDisplay()
{
	glBindFramebuffer(GL_READ_FRAMEBUFFER, screenFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, rSettings.windowW, rSettings.windowH, 0, 0, rSettings.windowW, rSettings.windowH, GL_COLOR_BUFFER_BIT, GL_NEAREST);
}

void OpenGLPipeline::DrawObjects()
{
    for(unsigned int i=0; i<drawingQueueCopy.size(); ++i)
    {
        if(drawingQueueCopy[i].type == RenderableType::SOLID)
            content->DrawObject(drawingQueueCopy[i].objectId, drawingQueueCopy[i].lookId, drawingQueueCopy[i].model);
    }
}
    
void OpenGLPipeline::DrawHelpers()
{
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    
    //Coordinate systems
    if(hSettings.showCoordSys)
    {
        content->DrawCoordSystem(glm::rotate((float)M_PI, glm::vec3(0,1.f,0)), 1.f);
        
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::SOLID_CS)
                content->DrawCoordSystem(drawingQueueCopy[h].model, 0.5f);
        }
    }
    
    //Discrete and multibody joints
    if(hSettings.showJoints)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::MULTIBODY_AXIS)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,0.5f,1.f,1.f));
            else if(drawingQueueCopy[h].type == RenderableType::JOINT_LINES)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,0.5f,1.f,1.f));
        }
    }
    
    //Sensors
    if(hSettings.showSensors)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::SENSOR_CS)
                content->DrawCoordSystem(drawingQueueCopy[h].model, 0.25f);
            else if(drawingQueueCopy[h].type == RenderableType::SENSOR_POINTS)
                content->DrawPrimitives(PrimitiveType::POINTS, drawingQueueCopy[h].points, glm::vec4(1.f,1.f,0,1.f), drawingQueueCopy[h].model);
            else if(drawingQueueCopy[h].type == RenderableType::SENSOR_LINES)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,1.f,0,1.f), drawingQueueCopy[h].model);
            else if(drawingQueueCopy[h].type == RenderableType::SENSOR_LINE_STRIP)
                content->DrawPrimitives(PrimitiveType::LINE_STRIP, drawingQueueCopy[h].points, glm::vec4(1.f,1.f,0,1.f), drawingQueueCopy[h].model);
        }
    }
    
    //Actuators
    if(hSettings.showActuators)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            if(drawingQueueCopy[h].type == RenderableType::ACTUATOR_LINES)
                content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(1.f,0.5f,0,1.f), drawingQueueCopy[h].model);
        }
    }
    
    //Fluid dynamics
    if(hSettings.showFluidDynamics)
    {
        for(size_t h=0; h<drawingQueueCopy.size(); ++h)
        {
            switch(drawingQueueCopy[h].type)
            {
                case RenderableType::HYDRO_CS:
                    content->DrawEllipsoid(drawingQueueCopy[h].model, glm::vec3(0.02f), glm::vec4(0.2f, 0.5f, 1.f, 1.f)); //drawingQueueCopy[h].scale*2.f);
                    break;
                    
                case RenderableType::HYDRO_CYLINDER:
                    content->DrawCylinder(drawingQueueCopy[h].model, drawingQueueCopy[h].points[0], glm::vec4(0.2f, 0.5f, 1.f, 1.f));
                    break;
                    
                case RenderableType::HYDRO_ELLIPSOID:
                    content->DrawEllipsoid(drawingQueueCopy[h].model, drawingQueueCopy[h].points[0], glm::vec4(0.2f, 0.5f, 1.f, 1.f));
                    break;
                    
                case RenderableType::HYDRO_POINTS:
                    content->DrawPrimitives(PrimitiveType::POINTS, drawingQueueCopy[h].points, glm::vec4(0.3f, 0.7f, 1.f, 1.f), drawingQueueCopy[h].model);
                    break;
                    
                case RenderableType::HYDRO_LINES:
                    content->DrawPrimitives(PrimitiveType::LINES, drawingQueueCopy[h].points, glm::vec4(0.2f, 0.5f, 1.f, 1.f), drawingQueueCopy[h].model);
                    break;
                    
                case RenderableType::HYDRO_LINE_STRIP:
                    content->DrawPrimitives(PrimitiveType::LINE_STRIP, drawingQueueCopy[h].points, glm::vec4(0.2f, 0.5f, 1.f, 1.f), drawingQueueCopy[h].model);
                    break;
                    
                default:
                    break;
            }
        }
    }
    
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void OpenGLPipeline::Render(SimulationManager* sim)
{
    //Double-buffering of drawing queue
    PerformDrawingQueueCopy();
    
    //Choose rendering mode
    unsigned int renderMode = 0; //Defaults to rendering without ocean
    Ocean* ocean = sim->getOcean();
    if(ocean != NULL)
    {
        ocean->getOpenGLOcean()->Simulate();
        renderMode = rSettings.ocean > RenderQuality::QUALITY_DISABLED ? (ocean->hasWaves() ? 2 : 1) : 0;
    }
    Atmosphere* atm = sim->getAtmosphere();
    
    //Bake shadow maps for lights (independent of view)
    if(rSettings.shadows > RenderQuality::QUALITY_DISABLED)
	{
		content->SetDrawingMode(DrawingMode::FLAT);
        for(unsigned int i=0; i<content->getLightsCount(); ++i)
            content->getLight(i)->BakeShadowmap(this);
	}
    
    //Clear display framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, screenFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    
    //Loop through all views -> trackballs, cameras, depth cameras...
    for(unsigned int i=0; i<content->getViewsCount(); ++i)
    {
        OpenGLView* view = content->getView(i);
        
        if(view->needsUpdate())
        {
            glDisable(GL_BLEND);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            
            if(view->getType() == DEPTH_CAMERA)
            {
                OpenGLDepthCamera* camera = (OpenGLDepthCamera*)view;
                GLint* viewport = camera->GetViewport();
                content->SetViewportSize(viewport[2],viewport[3]);
            
                glBindFramebuffer(GL_FRAMEBUFFER, camera->getRenderFBO());
                glClear(GL_DEPTH_BUFFER_BIT); //Only depth is rendered
                
                camera->SetViewport();
                content->SetCurrentView(camera);
                content->SetDrawingMode(DrawingMode::FLAT);
                DrawObjects();
                
                glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                camera->DrawLDR(screenFBO);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                
                delete viewport;
            }
            else if(view->getType() == CAMERA || view->getType() == TRACKBALL)
            {
                //Apply view properties
                OpenGLCamera* camera = (OpenGLCamera*)view;
                OpenGLLight::SetCamera(camera);
                GLint* viewport = camera->GetViewport();
                content->SetViewportSize(viewport[2],viewport[3]);
            
                //Bake parallel-split shadowmaps for sun
                if(rSettings.shadows > RenderQuality::QUALITY_DISABLED)
                {
                    content->SetDrawingMode(DrawingMode::FLAT);
                    atm->getOpenGLAtmosphere()->BakeShadowmaps(this, camera);
                }
            
                //Clear reflection framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, camera->getReflectionFBO());
                glDrawBuffer(GL_COLOR_ATTACHMENT0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                
                //Clear main framebuffer and setup camera
                glBindFramebuffer(GL_FRAMEBUFFER, camera->getRenderFBO());
                GLenum renderBuffs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
                glDrawBuffers(2, renderBuffs);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                camera->SetViewport();
                content->SetCurrentView(camera);
                
                //Draw scene
                if(renderMode == 0) //NO OCEAN
                {
                    //Render all objects
                    content->SetDrawingMode(DrawingMode::FULL);
                    DrawObjects();
            
                    //Ambient occlusion
                    if(rSettings.ao > RenderQuality::QUALITY_DISABLED)
                        camera->DrawAO(1.f);
            
                    //Render sky (at the end to take profit of early bailing)
                    atm->getOpenGLAtmosphere()->DrawSkyAndSun(camera);
            
                    //Go to postprocessing stage
                    camera->EnterPostprocessing();
                } 
                else if(renderMode == 1) //OCEAN WITHOUT WAVES (FLAT SURFACE)
                {
                    OpenGLOcean* glOcean = ocean->getOpenGLOcean();
            
                    if(camera->GetEyePosition().z >= 0.f) //Camera is above water surface
                    {
                        //Render all objects
                        content->SetDrawingMode(DrawingMode::UNDERWATER);
                        content->EnableClipPlane(glm::vec4(0,0,-1.f,0));
                        DrawObjects();
                        content->DisableClipPlane();
            
                        content->SetDrawingMode(DrawingMode::FULL);
                        content->EnableClipPlane(glm::vec4(0,0,1.f,0));
                        DrawObjects();
                        content->DisableClipPlane();
            
                        //Ambient occlusion
                        if(rSettings.ao > RenderQuality::QUALITY_DISABLED)
                            camera->DrawAO(1.f);
            
                        //Generate reflection texture
                        glBindFramebuffer(GL_FRAMEBUFFER, camera->getReflectionFBO());
                        glDrawBuffer(GL_COLOR_ATTACHMENT0);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        content->SetCurrentView(camera, true);
            
                        //Render all objects
                        content->SetDrawingMode(DrawingMode::FULL);
                        content->EnableClipPlane(glm::vec4(0,0,1.f,0));
                        glCullFace(GL_FRONT);
                        DrawObjects();
                        glCullFace(GL_BACK);
                        content->DisableClipPlane();
            
                        //Draw surface
                        glBindFramebuffer(GL_FRAMEBUFFER, camera->getRenderFBO());
                        glDrawBuffers(2, renderBuffs);
                        camera->SetViewport();
                        content->SetCurrentView(camera);
            
                        //Draw water surface
                        glOcean->DrawSurface(camera->GetEyePosition(), camera->GetViewMatrix(), camera->GetInfiniteProjectionMatrix(), camera->getReflectionTexture(), viewport);
                        
                        //Render sky
                        atm->getOpenGLAtmosphere()->DrawSkyAndSun(camera);
                        
                        //Go to postprocessing stage
                        camera->EnterPostprocessing();
                    }
                    else //Camera is underwater
                    {
                        //Render all objects
                        content->SetDrawingMode(DrawingMode::UNDERWATER);
                        content->EnableClipPlane(glm::vec4(0,0,-1.f,0));
                        DrawObjects();
                        content->DisableClipPlane();
            
                        content->SetDrawingMode(DrawingMode::FULL);
                        content->EnableClipPlane(glm::vec4(0,0,1.f,0));
                        DrawObjects();
                        content->DisableClipPlane();
            
                        //Ambient occlusion
                        if(rSettings.ao > RenderQuality::QUALITY_DISABLED) {
                            GLfloat factor = expf(-ocean->getTurbidity()/1000.f);
                            factor *= factor*factor;
                            camera->DrawAO(factor);
                        }
            
                        //Render planar reflection
                        glBindFramebuffer(GL_FRAMEBUFFER, camera->getReflectionFBO());
                        glDrawBuffer(GL_COLOR_ATTACHMENT0);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        content->SetCurrentView(camera, true);
            
                        //Render all objects
                        content->SetDrawingMode(DrawingMode::UNDERWATER);
                        content->EnableClipPlane(glm::vec4(0,0,-1.f,0));
                        glCullFace(GL_FRONT);
                        DrawObjects();
                        glCullFace(GL_BACK);
                        glm::vec3 eyePos = camera->GetEyePosition();
                        eyePos.z = -eyePos.z;
                        glOcean->DrawBackground(eyePos, content->GetViewMatrix(), camera->GetProjectionMatrix());
                        content->DisableClipPlane();
            
                        //Draw water surface
                        glBindFramebuffer(GL_FRAMEBUFFER, camera->getRenderFBO());
                        glDrawBuffers(2, renderBuffs);
                        camera->SetViewport();
                        content->SetCurrentView(camera);
                        glOcean->DrawBacksurface(camera->GetEyePosition(), camera->GetViewMatrix(), camera->GetInfiniteProjectionMatrix(), camera->getReflectionTexture(), viewport);
            
                        //Distant pool background
                        glOcean->DrawBackground(camera->GetEyePosition(), camera->GetViewMatrix(), camera->GetProjectionMatrix());
            
                        //Underwater blur
                        camera->GenerateLinearDepth(0);
                        camera->EnterPostprocessing();
                        camera->GenerateBlurArray();
            
                        //Apply blur
                        glBindFramebuffer(GL_FRAMEBUFFER, camera->getRenderFBO());
                        glOcean->DrawVolume(camera->getPostprocessTexture(2), camera->getLinearDepthTexture());
            
                        //Render sky if camera crossing water plane
                        //atm->getOpenGLAtmosphere()->DrawSkyAndSun(view);
            
                        //Go to postprocessing again
                        camera->EnterPostprocessing();
                    }
                }
                else if(renderMode == 2) //OCEAN WITH WAVES
                {
                    /////////////////
                    //// ADD AO !!! /////
                    /////////////////////
                    
                    //Update ocean for this camera
                    OpenGLOcean* glOcean = ocean->getOpenGLOcean();
                    glOcean->UpdateSurface(camera->GetEyePosition(), camera->GetViewMatrix(), camera->GetProjectionMatrix());
                    
                    //Generating stencil mask
                    glOcean->DrawUnderwaterMask(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                    
                    //Drawing underwater without stencil test
                    content->SetDrawingMode(DrawingMode::UNDERWATER);
                    DrawObjects();
                    
                    //Draw surface (disable depth testing but write to depth buffer)
                    glEnable(GL_BLEND);
                    glBlendFunc(GL_ONE, GL_SRC_ALPHA);
                    //glDepthFunc(GL_ALWAYS);
                    glOcean->DrawSurface(camera->GetEyePosition(), camera->GetViewMatrix(), camera->GetProjectionMatrix(), camera->getReflectionTexture(), viewport);
                    glDisable(GL_BLEND);
                    //glDepthFunc(GL_LEQUAL);
                    glOcean->DrawBacksurface(camera->GetEyePosition(), camera->GetViewMatrix(), camera->GetProjectionMatrix(), camera->getReflectionTexture(), viewport);
                    
                    //Stencil masking
                    glEnable(GL_STENCIL_TEST);
                    glStencilMask(0x00);
                    
                    //Draw only underwater stuff
                    glStencilFunc(GL_EQUAL, 1, 0xFF);
                    
                    //Distant pool background
                    //glOcean->DrawBackground(camera->GetEyePosition(), camera->GetViewMatrix(), camera->GetProjectionMatrix());
                    
                    //Underwater blur
                    //glBindFramebuffer(GL_FRAMEBUFFER, camera->getRenderFBO());
                    /*camera->EnterPostprocessing();
                    camera->GenerateBlurArray();
                    glBindFramebuffer(GL_FRAMEBUFFER, camera->getRenderFBO());
                    glOcean->DrawVolume(camera->getPostprocessTexture(2), camera->getLinearDepthTexture());*/
                    
                    //Draw only above water stuff
                    glStencilFunc(GL_EQUAL, 0, 0xFF);
                    
                    //Draw all objects as above surface (depth testing will secure drawing only what is above water)
                    content->SetDrawingMode(DrawingMode::FULL);
                    DrawObjects();
                    
                    //Render sky (left for the end to only fill empty spaces)
                    atm->getOpenGLAtmosphere()->DrawSkyAndSun(camera);
                    
                    glDisable(GL_STENCIL_TEST);
                    
                    //camera->GenerateLinearDepth(0);
                    
                    //Go to postprocessing stage
                    camera->EnterPostprocessing();
                    //glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                    //camera->DrawSSR();
                }
            
                //Post-processing
                glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                camera->DrawLDR(screenFBO);
            
                //Helper objects
                if(camera->getType() == ViewType::TRACKBALL)
                {
                    //Overlay debugging info
                    glBindFramebuffer(GL_FRAMEBUFFER, screenFBO); //No depth buffer, just one color buffer
                    content->SetProjectionMatrix(camera->GetProjectionMatrix());
                    content->SetViewMatrix(camera->GetViewMatrix());
                    glDisable(GL_CULL_FACE);
                    
                    //Simulation debugging
                    DrawHelpers();
                    if(hSettings.showBulletDebugInfo) sim->RenderBulletDebug();
                    
                    //Graphics debugging
                    /*if(ocean != NULL)
                    {
                        //ocean->getOpenGLOcean().ShowOceanSpectrum(glm::vec2((GLfloat)viewport[2], (GLfloat)viewport[3]), glm::vec4(0,200,300,300));
                        //ocean->getOpenGLOcean()->ShowTexture(4, glm::vec4(0,0,256,256));
                    }*/
            
                    /*atm->getOpenGLAtmosphere()->ShowAtmosphereTexture(AtmosphereTextures::TRANSMITTANCE,glm::vec4(0,0,200,200));
                    atm->getOpenGLAtmosphere()->ShowAtmosphereTexture(AtmosphereTextures::IRRADIANCE,glm::vec4(200,0,200,200));
                    atm->getOpenGLAtmosphere()->ShowAtmosphereTexture(AtmosphereTextures::SCATTERING,glm::vec4(400,0,200,200));
                    atm->getOpenGLAtmosphere()->ShowSunShadowmaps(0, 0, 0.05f);*/
                    
                    //camera->ShowSceneTexture(SceneComponent::NORMAL, glm::vec4(0,0,300,200));
                    //camera->ShowViewNormalTexture(glm::vec4(0,200,300,200));
                    //camera->ShowLinearDepthTexture(glm::vec4(0,400,300,200));
                    
                    //camera->ShowDeinterleavedDepthTexture(glm::vec4(0,400,300,200), 0);
                    //camera->ShowDeinterleavedDepthTexture(glm::vec4(0,400,300,200), 8);
                    //camera->ShowDeinterleavedDepthTexture(glm::vec4(0,600,300,200), 9);
                    //camera->ShowDeinterleavedAOTexture(glm::vec4(0,600,300,200), 0);
                    //camera->ShowAmbientOcclusion(glm::vec4(0,800,300,200));
                    
                    glEnable(GL_CULL_FACE);
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                }
            
                delete viewport;
            }
        }
        else
        {
            GLint* viewport = view->GetViewport();
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
			view->DrawLDR(screenFBO);
            delete viewport;
        }
    }
}

}