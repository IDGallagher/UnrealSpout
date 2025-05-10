#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SceneViewExtension.h"
#include "SpoutCopyViewExtension.h"
#include "ViewportSpoutSender.generated.h"

class USceneCaptureComponent2D;
class USpoutSenderActorComponent;
class UTextureRenderTarget2D;

/**
 * Actor that mirrors the player's viewport to a render-target and publishes it
 * via Spout.  Drop one in a level or spawn at runtime.
 */
UCLASS(Blueprintable, HideCategories = (Input, Collision, Replication), ClassGroup=(Spout))
class UNREALSPOUT_API AViewportSpoutSender : public AActor
{
   GENERATED_BODY()

public:
   AViewportSpoutSender();

   UTextureRenderTarget2D* GetCaptureRenderTarget() const { return ViewRT; }
   USpoutSenderActorComponent* GetSpoutSender() const { return SpoutSender; }

protected:
   virtual void BeginPlay() override;
   virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
   void ValidateOrCreateRT();
   void SyncToPlayerCamera() const;

   UPROPERTY(VisibleAnywhere)
   USceneComponent* Root;

   UPROPERTY(VisibleAnywhere)
   USceneCaptureComponent2D* SceneCapture;

   UPROPERTY(VisibleAnywhere)
   USpoutSenderActorComponent* SpoutSender;

   UPROPERTY(Transient)
   UTextureRenderTarget2D* ViewRT = nullptr;

   UPROPERTY(EditAnywhere, Category="Spout")
   FName PublishName = TEXT("Viewport");

   /** If true the SceneCapture component captures every frame internally.
       If false we call CaptureScene() manually in Tick.  */
   UPROPERTY(EditAnywhere, Category="Capture")
   bool bAutoCapture = true;

   int32 LastW = 0;
   int32 LastH = 0;

   TSharedPtr<FSpoutCopyViewExtension, ESPMode::ThreadSafe> ViewExt;
}; 