#pragma once

#include "SceneView.h"
#include "SceneViewExtension.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class AViewportSpoutSender;

/**
 * Copies the ViewportSpoutSender's render-target into Spout's shared texture
 * each frame on the render thread via RDG.  Lives as long as its owning actor.
 */
class FSpoutCopyViewExtension final : public FSceneViewExtensionBase
{
public:
    explicit FSpoutCopyViewExtension(const FAutoRegister& AutoRegister,
                                     AViewportSpoutSender* InOwner)
        : FSceneViewExtensionBase(AutoRegister)
        , Owner(InOwner)
    {}

    /* -------- ISceneViewExtension required stubs -------- */
    virtual void SetupViewFamily          (FSceneViewFamily&)                       override {}
    virtual void SetupView               (FSceneViewFamily&, FSceneView&)          override {}
    virtual void BeginRenderViewFamily    (FSceneViewFamily&)                       override {}
    virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext&) const override { return true; }

    /* -------- Copy pass -------- */
    virtual void PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                 FSceneView& View) override;

private:
    AViewportSpoutSender* Owner = nullptr;
}; 