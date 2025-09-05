#pragma once

#include "SceneView.h"
#include "SceneViewExtension.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"

class AViewportSpoutSender;
class FSceneTextureUniformParameters;

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
    virtual void PostRenderBasePassDeferred_RenderThread(
        FRDGBuilder& GraphBuilder,
        FSceneView& InView,
        const FRenderTargetBindingSlots& RenderTargets,
        TRDGUniformBufferRef<FSceneTextureUniformParameters> SceneTextures) override;

    virtual void PostRenderBasePassMobile_RenderThread(
        FRHICommandList& RHICmdList,
        FSceneView& InView) override;

private:
    AViewportSpoutSender* Owner = nullptr;
}; 