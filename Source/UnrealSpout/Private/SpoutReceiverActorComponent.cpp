// Fill out your copyright notice in the Description page of Project Settings.

#include "SpoutReceiverActorComponent.h"

#include <string>

#include "Windows/AllowWindowsPlatformTypes.h" 
#include <d3d11on12.h>
#include "Spout.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "RHICommandList.h"
#include "RHIUtilities.h"
#include "MediaShaders.h"

#include "RHI.h"
#include "RHIResources.h"          // RHICreateShaderResourceView
#include "RHIStaticStates.h"
#include "ShaderParameterUtils.h"  // SetShaderResourceViewParameter
#include "RenderCore.h"         // FRHIBatchedShaderParameters helpers


static spoutSenderNames senders;
/** Helper to open shared handles without duplicating code */
static spoutDirectX spoutdx;

class FTextureCopyVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTextureCopyVertexShader, Global);
public:

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FTextureCopyVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{}
	FTextureCopyVertexShader() {}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

class FTextureCopyPixelShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FTextureCopyPixelShader, Global);
public:

#if (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 25) || (ENGINE_MAJOR_VERSION == 5)
	LAYOUT_FIELD(FShaderResourceParameter, SrcTexture);
#else ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION <= 24
	FShaderResourceParameter SrcTexture;

	virtual bool Serialize(FArchive& Ar) override
	{
		bool bShaderHasOutdatedParams = FGlobalShader::Serialize(Ar);
		Ar << SrcTexture;
		return bShaderHasOutdatedParams;
	}
#endif

	static bool ShouldCache(EShaderPlatform Platform) { return true; }

	FTextureCopyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		SrcTexture.Bind(Initializer.ParameterMap, TEXT("SrcTexture"));
	}
	FTextureCopyPixelShader() {}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

struct FTextureVertex
{
	FVector4 Position;
	FVector2D UV;
};

IMPLEMENT_SHADER_TYPE(, FTextureCopyPixelShader, TEXT("/Plugin/UnrealSpout/SpoutReceiverCopyShader.usf"), TEXT("MainPixelShader"), SF_Pixel)

//////////////////////////////////////////////////////////////////////////

struct USpoutReceiverActorComponent::SpoutReceiverContext
{
	unsigned int width = 0, height = 0;
	DXGI_FORMAT dwFormat = DXGI_FORMAT_UNKNOWN;
	EPixelFormat format = PF_Unknown;
	FRHITexture* Texture;

	ID3D11Device* D3D11Device = nullptr;
	ID3D11DeviceContext* Context = nullptr;

	ID3D12Device* D3D12Device = nullptr;
	ID3D11On12Device* D3D11on12Device = nullptr;
	ID3D11Resource* WrappedDX11Resource = nullptr;

	SpoutReceiverContext(unsigned int width, unsigned int height, DXGI_FORMAT dwFormat, FRHITexture* Texture)
		: width(width)
		, height(height)
		, dwFormat(dwFormat)
		, Texture(Texture)
	{
		if (dwFormat == DXGI_FORMAT_B8G8R8A8_UNORM)
			format = PF_B8G8R8A8;
		else if (dwFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
			format = PF_FloatRGBA;
		else if (dwFormat == DXGI_FORMAT_R32G32B32A32_FLOAT)
			format = PF_A32B32G32R32F;

		FString RHIName = GDynamicRHI->GetName();

		if (RHIName == TEXT("D3D11"))
		{
			D3D11Device = (ID3D11Device*)GDynamicRHI->RHIGetNativeDevice();
			D3D11Device->GetImmediateContext(&Context);
		}
		else if (RHIName == TEXT("D3D12"))
		{
			D3D12Device = static_cast<ID3D12Device*>(GDynamicRHI->RHIGetNativeDevice());
			UINT DeviceFlags11 = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

			verify(D3D11On12CreateDevice(
				D3D12Device,
				DeviceFlags11,
				nullptr,
				0,
				nullptr,
				0,
				0,
				&D3D11Device,
				&Context,
				nullptr
			) == S_OK);

			verify(D3D11Device->QueryInterface(__uuidof(ID3D11On12Device), (void**)&D3D11on12Device) == S_OK);
			
			ID3D12Resource* NativeTex = (ID3D12Resource*)Texture->GetNativeResource();

			D3D11_RESOURCE_FLAGS rf11 = {};

			verify(D3D11on12Device->CreateWrappedResource(
				NativeTex, &rf11,
				D3D12_RESOURCE_STATE_COPY_DEST,
				D3D12_RESOURCE_STATE_PRESENT, __uuidof(ID3D11Resource),
				(void**)&WrappedDX11Resource) == S_OK);
		}
		else throw;
	}

	~SpoutReceiverContext()
	{
		if (WrappedDX11Resource)
		{
			D3D11on12Device->ReleaseWrappedResources(&WrappedDX11Resource, 1);
			WrappedDX11Resource = nullptr;
		}

		if (D3D11on12Device)
		{
			D3D11on12Device->Release();
			D3D11on12Device = nullptr;
		}

		if (D3D11Device)
		{
			D3D11Device->Release();
			D3D11Device = nullptr;
		}

		if (D3D12Device)
		{
			D3D12Device->Release();
			D3D12Device = nullptr;
		}

		if (Context)
		{
			Context->Release();
			Context = nullptr;
		}

	}

	void CopyResource(ID3D11Resource* SrcTexture)
	{
		check(IsInRenderingThread());
		if (!GWorld || !SrcTexture) return;

		FString RHIName = GDynamicRHI->GetName();

		if (RHIName == TEXT("D3D11"))
		{
			ID3D11Texture2D* NativeTex = (ID3D11Texture2D*)Texture->GetNativeResource();

			Context->CopyResource(NativeTex, SrcTexture);
			Context->Flush();
		}
		else if (RHIName == TEXT("D3D12"))
		{
			D3D11on12Device->AcquireWrappedResources(&WrappedDX11Resource, 1);
			Context->CopyResource(WrappedDX11Resource, SrcTexture);
			D3D11on12Device->ReleaseWrappedResources(&WrappedDX11Resource, 1);
			Context->Flush();
		}
	}
};

//////////////////////////////////////////////////////////////////////////

USpoutReceiverActorComponent::USpoutReceiverActorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
}

// Called when the game starts
void USpoutReceiverActorComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USpoutReceiverActorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void USpoutReceiverActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	check(IsInGameThread());
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OutputRenderTarget)
		return;

	unsigned int width = 0, height = 0;
	HANDLE hSharehandle = nullptr;
	DXGI_FORMAT dwFormat = DXGI_FORMAT_UNKNOWN;

	bool find_sender = senders.FindSender(TCHAR_TO_ANSI(*SubscribeName.ToString()), width, height, hSharehandle, (DWORD&)dwFormat);

	EPixelFormat format = PF_Unknown;

	if (dwFormat == DXGI_FORMAT_B8G8R8A8_UNORM)
		format = PF_B8G8R8A8;
	else if (dwFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
		format = PF_FloatRGBA;
	else if (dwFormat == DXGI_FORMAT_R32G32B32A32_FLOAT)
		format = PF_A32B32G32R32F;

	if (!find_sender
		|| !hSharehandle
		|| format == PF_Unknown
		|| width == 0
		|| height == 0)
		return;

	if (!this->IntermediateTextureResource)
	{
		this->IntermediateTextureResource = NewObject<UTextureRenderTarget2D>(this);
		this->IntermediateTextureResource->InitCustomFormat(width, height, format, false);
		this->IntermediateTextureResource->UpdateResourceImmediate(false);
	}

	FRHITexture* IntermediateRHI = IntermediateTextureResource->GetResource()->TextureRHI.GetReference();
	if (!IntermediateRHI) return;

	if (!context.IsValid())
	{
		context = TSharedPtr<SpoutReceiverContext>(new SpoutReceiverContext(width, height, dwFormat, IntermediateRHI));
	}

	ID3D11Texture2D* SharedTex = nullptr;
	verify(spoutdx.OpenDX11shareHandle(context->D3D11Device, &SharedTex, hSharehandle));
	context->CopyResource(SharedTex);
	SharedTex->Release();

	ENQUEUE_RENDER_COMMAND(SpoutReceiverRenderThreadOp)(
		[IntermediateRHI, this](FRHICommandListImmediate& RHICmdList) {
		if (!GWorld || !IntermediateRHI) return;

		FRHIRenderPassInfo RPInfo(IntermediateRHI, ERenderTargetActions::DontLoad_Store);
		RHICmdList.BeginRenderPass(RPInfo, TEXT("SpoutReceiver"));

		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FMediaShadersVS> VertexShader(GlobalShaderMap);
		TShaderMapRef<FTextureCopyPixelShader> PixelShader(GlobalShaderMap);

		FRHITextureSRVCreateInfo SRVDesc{};
		IntermediateTextureParameterSRV = RHICmdList.CreateShaderResourceView(IntermediateRHI, SRVDesc);

		if (PixelShader->SrcTexture.IsBound())
		{
			FRHIBatchedShaderParameters& BatchedParameters = RHICmdList.GetScratchShaderParameters();
			SetSRVParameter(BatchedParameters, PixelShader->SrcTexture, IntermediateTextureParameterSRV);
			RHICmdList.SetBatchedShaderParameters(PixelShader.GetPixelShader(), BatchedParameters);
		}

		RHICmdList.EndRenderPass();

		FRHICopyTextureInfo CopyInfo{};
		RHICmdList.CopyTexture(IntermediateRHI, IntermediateRHI, CopyInfo);
	});
}
