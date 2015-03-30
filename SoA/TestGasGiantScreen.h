#pragma once

#ifndef TestGasGiantScreen_h__
#define TestGasGiantScreen_h__

#include <Vorb/Events.hpp>
#include <Vorb/graphics/GLProgram.h>
#include <Vorb/ui/IGameScreen.h>
#include <Vorb/VorbPreDecl.inl>
#include <Vorb/graphics/TextureCache.h>
#include <Vorb/Events.hpp>

#include <vector>

class GasGiantRenderer;

DECL_VG(class GLProgramManager);
DECL_VIO(class IOManager);

class TestGasGiantScreen : public vui::IGameScreen {
public:
    /************************************************************************/
    /* IGameScreen functionality                                            */
    /************************************************************************/
    virtual i32 getNextScreen() const override;
    virtual i32 getPreviousScreen() const override;
    virtual void build() override;
    virtual void destroy(const vui::GameTime& gameTime) override;
    virtual void onEntry(const vui::GameTime& gameTime) override;
    virtual void onExit(const vui::GameTime& gameTime) override;
    virtual void update(const vui::GameTime& gameTime) override;
    virtual void draw(const vui::GameTime& gameTime) override;

private:
    GasGiantRenderer* m_gasGiantRenderer;
    f32v3 m_eyePos;
    vg::TextureCache m_textureCache;

    AutoDelegatePool m_hooks;
};

#endif