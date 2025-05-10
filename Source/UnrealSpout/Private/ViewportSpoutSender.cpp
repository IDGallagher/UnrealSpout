#include "ViewportSpoutSender.h"
#include "SpoutSenderActorComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"

AViewportSpoutSender::AViewportSpoutSender()
{
   PrimaryActorTick.bCanEverTick = true;

   Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
   RootComponent = Root;

   SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
   SceneCapture->SetupAttachment(Root);
   SceneCapture->CaptureSource     = ESceneCaptureSource::SCS_FinalColorHDR;

   SpoutSender = CreateDefaultSubobject<USpoutSenderActorComponent>(TEXT("SpoutSender"));
}

void AViewportSpoutSender::BeginPlay()
{
   Super::BeginPlay();
   ValidateOrCreateRT();
   SceneCapture->bCaptureEveryFrame = true;
   SceneCapture->bCaptureOnMovement = true;
   SpoutSender->PublishName  = PublishName;
   SpoutSender->OutputTexture = ViewRT;
   if (!ViewExt.IsValid())
   {
       ViewExt = FSceneViewExtensions::NewExtension<FSpoutCopyViewExtension>(this);
   }
}

void AViewportSpoutSender::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
   ViewExt.Reset();
   if (ViewRT)
   {
      ViewRT->ConditionalBeginDestroy();
      ViewRT = nullptr;
   }
   Super::EndPlay(EndPlayReason);
}

void AViewportSpoutSender::SyncToPlayerCamera() const
{
   APlayerCameraManager* PCM = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
   if (!PCM || !SceneCapture) return;

   SceneCapture->SetWorldLocationAndRotation(PCM->GetCameraLocation(), PCM->GetCameraRotation());
   SceneCapture->FOVAngle = PCM->GetFOVAngle();
}

void AViewportSpoutSender::ValidateOrCreateRT()
{
   if (!GEngine || !GEngine->GameViewport) return;

   FVector2D Size;
   GEngine->GameViewport->GetViewportSize(Size);

   const int32 W = FMath::TruncToInt(Size.X);
   const int32 H = FMath::TruncToInt(Size.Y);

   if (ViewRT && W == LastW && H == LastH) return;

   LastW = W;  LastH = H;

   ViewRT = NewObject<UTextureRenderTarget2D>(this);
   ViewRT->ClearColor = FLinearColor::Black;
   ViewRT->InitCustomFormat(LastW, LastH, PF_FloatRGBA, true);
   ViewRT->UpdateResourceImmediate(true);

   SceneCapture->TextureTarget = ViewRT;
   SpoutSender ->OutputTexture = ViewRT;
} 