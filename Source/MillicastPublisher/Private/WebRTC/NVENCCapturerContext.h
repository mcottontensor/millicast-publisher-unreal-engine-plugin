// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Optional.h"
#include "VideoEncoderInput.h"
#include "RHIStaticStates.h"
#include "RHIDefinitions.h"
#include "RHI.h"
#include "RenderResource.h"
#include "ScreenRendering.h"

class FNVENCCapturerContext
{
public:
	struct FCapturerInput
	{
		FCapturerInput()
			: InputFrame(nullptr)
			, Texture()
		{
		}

		FCapturerInput(AVEncoder::FVideoEncoderInputFrame* InFrame, FTexture2DRHIRef InTexture)
			: InputFrame(InFrame)
			, Texture(InTexture)
		{
		}

		AVEncoder::FVideoEncoderInputFrame* InputFrame;
		TOptional<FTexture2DRHIRef> Texture;
	};

public:
	FNVENCCapturerContext(int InCaptureWidth, int InCaptureHeight, bool bInFixedResolution);
	int GetCaptureWidth() const;
	int GetCaptureHeight() const;
	bool IsFixedResolution() const;
	void SetCaptureResolution(int NewCaptureWidth, int NewCaptureHeight);

	FCapturerInput ObtainCapturerInput();
	TSharedPtr<AVEncoder::FVideoEncoderInput> GetVideoEncoderInput() const;

private:
	TSharedPtr<AVEncoder::FVideoEncoderInput> CreateVideoEncoderInput(int InWidth, int InHeight, bool bInFixedResolution);
	void DeleteBackBuffers();

#if PLATFORM_WINDOWS
	FTexture2DRHIRef SetBackbufferTextureDX11(AVEncoder::FVideoEncoderInputFrame* InputFrame);
	FTexture2DRHIRef SetBackbufferTextureDX12(AVEncoder::FVideoEncoderInputFrame* InputFrame);
#endif // PLATFORM_WINDOWS

	FTexture2DRHIRef SetBackbufferTexturePureVulkan(AVEncoder::FVideoEncoderInputFrame* InputFrame);
	FTexture2DRHIRef SetBackbufferTextureCUDAVulkan(AVEncoder::FVideoEncoderInputFrame* InputFrame);

private:
	int CaptureWidth;
	int CaptureHeight;
	bool bFixedResolution;
	TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput = nullptr;
	TMap<AVEncoder::FVideoEncoderInputFrame*, FTexture2DRHIRef> BackBuffers;
};
