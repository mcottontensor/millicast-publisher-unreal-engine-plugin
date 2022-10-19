// Copyright Millicast 2022. All Rights Reserved.

#include "AsyncTextureReadback.h"
#include "CopyTexture.h"

FAsyncTextureReadback::~FAsyncTextureReadback()
{
	GDynamicRHI->RHIUnmapStagingSurface(ReadbackTexture);
	ReadbackBuffer = nullptr;
}

void FAsyncTextureReadback::ReadbackAsync_RenderThread(FTexture2DRHIRef SourceTexture, TFunction<void(uint8*,int,int,int)> OnReadbackComplete)
{
	checkf(IsInRenderingThread(), TEXT("Texture readback can only occur on the rendering thread."));

	Initialize(SourceTexture);

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	// Copy the passed texture into a staging texture (we do this to ensure PixelFormat and texture type is correct for readback)
	RHICmdList.Transition(FRHITransitionInfo(SourceTexture, ERHIAccess::Unknown, ERHIAccess::CopySrc));
	RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::CopySrc, ERHIAccess::CopyDest));
	CopyTexture(RHICmdList, SourceTexture, StagingTexture);

	// Copy the staging texture from GPU to CPU
	RHICmdList.Transition(FRHITransitionInfo(StagingTexture, ERHIAccess::CopyDest, ERHIAccess::CopySrc));
	RHICmdList.Transition(FRHITransitionInfo(ReadbackTexture, ERHIAccess::CPURead, ERHIAccess::CopyDest));
	RHICmdList.CopyTexture(StagingTexture, ReadbackTexture, {});
	RHICmdList.Transition(FRHITransitionInfo(ReadbackTexture, ERHIAccess::CopyDest, ERHIAccess::CPURead));

	TSharedRef<FAsyncTextureReadback> ThisRef = AsShared();
	RHICmdList.EnqueueLambda([ThisRef, OnReadbackComplete](FRHICommandListImmediate&) {
		uint8* Pixels = static_cast<uint8*>(ThisRef->ReadbackBuffer);

		OnReadbackComplete(Pixels, ThisRef->Width, ThisRef->Height, ThisRef->MappedStride);
	});
	
}

void FAsyncTextureReadback::Initialize(FTexture2DRHIRef Texture)
{
	int InWidth = Texture->GetSizeX();
	int InHeight = Texture->GetSizeY();
	if (InWidth == Width && InHeight == Height)
	{
		// No need to initialize, we already have textures at the correct resolutions.
		return;
	}

	Width = InWidth;
	Height = InHeight;

	// Create a staging texture with a resolution matching the texture passed in.
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("TextureReadbackStagingTexture"));
		StagingTexture = GDynamicRHI->RHICreateTexture2D(Width, Height, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_RenderTargetable, ERHIAccess::CopySrc, CreateInfo);
	}

	// Create a texture mapped to CPU memory so we can do an easy readback
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("MappedCPUReadbackTexture"));
		ReadbackTexture = GDynamicRHI->RHICreateTexture2D(Width, Height, EPixelFormat::PF_B8G8R8A8, 1, 1, TexCreate_CPUReadback, ERHIAccess::CPURead, CreateInfo);

		int32 BufferWidth = 0, BufferHeight = 0;
		GDynamicRHI->RHIMapStagingSurface(ReadbackTexture, nullptr, ReadbackBuffer, BufferWidth, BufferHeight);
		MappedStride = BufferWidth;
	}
	
}