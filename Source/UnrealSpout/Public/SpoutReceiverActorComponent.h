// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Components/ActorComponent.h"

#include "SpoutReceiverActorComponent.generated.h"

UCLASS( ClassGroup=(Custom), DisplayName = "Spout Receiver", meta=(BlueprintSpawnableComponent) )
class UNREALSPOUT_API USpoutReceiverActorComponent : public UActorComponent
{
	GENERATED_BODY()

	struct SpoutReceiverContext;
	TSharedPtr<SpoutReceiverContext> context;

	UPROPERTY()
	UTexture2D* IntermediateTexture2D = nullptr;

	void Tick_RenderThread(FRHICommandListImmediate& RHICmdList, void* hSharehandle, FTextureRenderTargetResource* OutputRenderTargetResource);

public:	
	
	USpoutReceiverActorComponent();

protected:
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout")
	FName SubscribeName = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout")
	UTextureRenderTarget2D* OutputRenderTarget = nullptr;
};
