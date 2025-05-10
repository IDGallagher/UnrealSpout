#include "SpoutCopyViewExtension.h"
#include "ViewportSpoutSender.h"
#include "SpoutSenderActorComponent.h"
#include "Engine/TextureRenderTarget2D.h"

void FSpoutCopyViewExtension::PostRenderBasePass_RenderThread(FRHICommandListImmediate& RHICmdList,
                                                              FSceneView&)
{
   if (!Owner || !Owner->IsValidLowLevel())
      return;

   UTextureRenderTarget2D* ViewRT            = Owner->GetCaptureRenderTarget();
   USpoutSenderActorComponent* SpoutSender   = Owner->GetSpoutSender();
   if (!ViewRT || !SpoutSender)
      return;

   FTextureRHIRef   SrcRHI = ViewRT->GetRenderTargetResource()->GetRenderTargetTexture();
   FTextureRHIRef DstRHI = SpoutSender->GetSharedTextureRHI();
   if (!SrcRHI.IsValid() || !DstRHI.IsValid())
      return;

   FRDGBuilder GraphBuilder(RHICmdList);

   FRDGTextureRef Src = RegisterExternalTexture(GraphBuilder, SrcRHI, TEXT("SpoutSrc"));
   FRDGTextureRef Dst = RegisterExternalTexture(
       GraphBuilder,
       DstRHI,
       TEXT("SpoutDst"),
       ERDGTextureFlags::SkipTracking | ERDGTextureFlags::MultiFrame);

   AddCopyTexturePass(GraphBuilder, Src, Dst);
   GraphBuilder.Execute();
} 