// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine.h"
#include "Components/ActorComponent.h"
#include "RHIResources.h"
#include "RHI.h"
#include "SpoutSenderActorComponent.generated.h"

// Forward-declare to avoid pulling heavy headers in most translation units
struct ID3D11Texture2D;

UCLASS( ClassGroup=(Custom), DisplayName="Spout Sender", meta=(BlueprintSpawnableComponent) )
class UNREALSPOUT_API USpoutSenderActorComponent : public UActorComponent
{
	GENERATED_BODY()

	struct SpoutSenderContext;
	TSharedPtr<SpoutSenderContext> context;

public:	
	// Sets default values for this component's properties
	USpoutSenderActorComponent();

	/** Access the native texture that Spout broadcasts to. */
	ID3D11Texture2D* GetSharedDX11Texture() const;

	/** Same texture wrapped as an RHI resource for RDG. Created on-demand. */
	FTextureRHIRef GetSharedTextureRHI();

private:
	mutable ID3D11Texture2D* CachedDX11 = nullptr;
	mutable FTextureRHIRef CachedRHI;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout")
	FName PublishName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spout")
	UTexture* OutputTexture;
};
