#include "stdafx.h"
#include "TestGasGiantScreen.h"

#include "GasGiantRenderer.h"
#include <Vorb\graphics\GLProgram.h>
#include <Vorb\graphics\GpuMemory.h>
#include <Vorb\graphics\ImageIO.h>
#include <Vorb\io\IOManager.h>
#include <Vorb/ui/InputDispatcher.h>

#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

i32 TestGasGiantScreen::getNextScreen() const {
    return SCREEN_INDEX_NO_SCREEN;
}

i32 TestGasGiantScreen::getPreviousScreen() const {
    return SCREEN_INDEX_NO_SCREEN;
}

void TestGasGiantScreen::build() {

}
void TestGasGiantScreen::destroy(const vui::GameTime& gameTime) {

}

void TestGasGiantScreen::onEntry(const vui::GameTime& gameTime) {
    m_hooks.addAutoHook(vui::InputDispatcher::key.onKeyDown, [&](Sender s, const vui::KeyEvent& e) {
        if(e.keyCode == VKEY_F1) {
            m_gasGiantRenderer->reloadShader();
        }
    });
    glEnable(GL_DEPTH_TEST);
    
    m_gasGiantRenderer = new GasGiantRenderer();
    //vg::Texture tex = m_textureCache.addTexture("Textures/Test/GasGiantLookup.png", &SamplerState::POINT_CLAMP);
    //m_gasGiantRenderer->setColorBandLookupTexture(tex.id);

    VGTexture colorBandLookup;
    glGenTextures(1, &colorBandLookup);
    glBindTexture(GL_TEXTURE_2D, colorBandLookup);
    
    vg::BitmapResource rs = vg::ImageIO().load("Textures/Test/GasGiantLookup.png");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, rs.width, rs.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rs.data);

    m_gasGiantRenderer->setColorBandLookupTexture(colorBandLookup);

    glClearColor(0, 0, 0, 1);
    glClearDepth(1.0);
    m_eyePos = f32v3(0, 0, 2);
}

void TestGasGiantScreen::onExit(const vui::GameTime& gameTime) {
    delete m_gasGiantRenderer;
}

void TestGasGiantScreen::update(const vui::GameTime& gameTime) {
    glm::vec4 rotated = glm::vec4(m_eyePos, 1) * glm::rotate(glm::mat4(), (float)(15 * gameTime.elapsed), glm::vec3(0, 1, 0));
    m_eyePos.x = rotated.x;
    m_eyePos.y = rotated.y;
    m_eyePos.z = rotated.z;
}

void TestGasGiantScreen::draw(const vui::GameTime& gameTime) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    f32m4 mvp = glm::translate(f32m4(1.0f), f32v3(0, 0, 0)) * glm::perspectiveFov(90.0f, 1280.0f, 720.0f, 0.1f, 1000.0f) * glm::lookAt(m_eyePos, f32v3(0, 0, 0), f32v3(0, 1, 0));
    m_gasGiantRenderer->drawGasGiant(mvp);
}


//pColor = vec4(texture(unColorBandLookup, vec2(0.5, fPosition.y)).rgb, 1.0);