#include "VRStage.h"
#include <OpenSG/OSGSolidBackground.h>
#include <OpenSG/OSGTextureBuffer.h>
#include <OpenSG/OSGRenderBuffer.h>

#include "core/objects/VRCamera.h"
#include "core/objects/material/VRMaterial.h"

using namespace OSG;

VRObjectPtr VRStage::copy(vector<VRObjectPtr> children) {
    VRStagePtr g = VRStage::create(getBaseName());
    return g;
}

void VRStage::saveContent(xmlpp::Element* e) {
    VRObject::saveContent(e);
    //e->set_attribute("group", group);
    //e->set_attribute("active", toString(active));
}

void VRStage::loadContent(xmlpp::Element* e) {
    VRObject::loadContent(e);
    //setGroup( e->get_attribute("group")->get_value().c_str() );
    //setActive( e->get_attribute("active")->get_value().c_str() );
}

VRStage::VRStage(string name) : VRObject(name) {
    initStage();
}

VRStage::~VRStage() {}

VRStagePtr VRStage::create(string name) { return shared_ptr<VRStage>(new VRStage(name) ); }
VRStagePtr VRStage::ptr() { return static_pointer_cast<VRStage>( shared_from_this() ); }

void VRStage::setActive(bool b) {
    active = b;
    if (b) setCore(stage, "Stage", true);
    else setCore(Group::create(), "Object", true);
}

bool VRStage::isActive() { return active; }

void VRStage::initStage() {
    stage = SimpleStage::create();
    setCore(stage, "Stage");
    stage->setSize(0.0f, 0.0f, 1.0f, 1.0f);

    SolidBackgroundRefPtr gb = SolidBackground::create();
    gb->setColor( Color3f(1,1,1) );
    stage->setBackground(gb);
}

void VRStage::initFBO() {
    fboImg = Image::create();
    fboTex = TextureObjChunk::create();
    fboTex->setImage(fboImg);
    fbo = FrameBufferObject::create();
    TextureBufferRefPtr texBuf = TextureBuffer::create();
    RenderBufferRefPtr depthBuf = RenderBuffer::create();
    depthBuf->setInternalFormat(GL_DEPTH_COMPONENT24);
    texBuf->setTexture(fboTex);
    fbo->setColorAttachment(texBuf, 0);
    fbo->setDepthAttachment(depthBuf);
    fbo->editMFDrawBuffers()->push_back(GL_COLOR_ATTACHMENT0_EXT);
    fbo->setPostProcessOnDeactivate(true);
    stage->setRenderTarget(fbo);

    target->setTexture(fboTex);
}

void VRStage::update() {
    if (target) {
        if (!fboTex) initFBO();
        fboImg->set(Image::OSG_RGB_PF, size[0], size[1]);
    }

    if (fbo) {
        fbo->setWidth (size[0]);
        fbo->setHeight(size[1]);
    }
}

void VRStage::setSize( Vec2i s ) { size = s; update(); }
void VRStage::setTarget(VRMaterialPtr mat) { target = mat; update(); }
void VRStage::setCamera(VRCameraPtr cam) { stage->setCamera( cam->getCam() ); }