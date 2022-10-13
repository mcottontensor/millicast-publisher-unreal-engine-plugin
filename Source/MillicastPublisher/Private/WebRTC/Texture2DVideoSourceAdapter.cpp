
// Copyright Millicast 2022. All Rights Reserved.

#include "Texture2DVideoSourceAdapter.h"
#include "Texture2DFrameBuffer.h"
#include "MillicastPublisherPrivate.h"

inline void CopyTexture(const FTexture2DRHIRef& SourceTexture, FTexture2DRHIRef& DestinationTexture)
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	IRendererModule* RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>("Renderer");

	// #todo-renderpasses there's no explicit resolve here? Do we need one?
	FRHIRenderPassInfo RPInfo(DestinationTexture, ERenderTargetActions::Load_Store);

	RHICmdList.BeginRenderPass(RPInfo, TEXT("CopyBackbuffer"));

	{
		RHICmdList.SetViewport(0, 0, 0.0f, DestinationTexture->GetSizeX(), DestinationTexture->GetSizeY(), 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		// New engine version...
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit);

		if(DestinationTexture->GetSizeX() != SourceTexture->GetSizeX() || DestinationTexture->GetSizeY() != SourceTexture->GetSizeY())
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SourceTexture);
		}
		else
		{
			PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Point>::GetRHI(), SourceTexture);
		}

		RendererModule->DrawRectangle(RHICmdList, 0, 0,                // Dest X, Y
		                              DestinationTexture->GetSizeX(),  // Dest Width
		                              DestinationTexture->GetSizeY(),  // Dest Height
		                              0, 0,                            // Source U, V
		                              1, 1,                            // Source USize, VSize
		                              DestinationTexture->GetSizeXY(), // Target buffer size
		                              FIntPoint(1, 1),                 // Source texture size
		                              VertexShader, EDRF_Default);
	}

	RHICmdList.EndRenderPass();
}

FTexture2DVideoSourceAdapter::FTexture2DVideoSourceAdapter() noexcept
{
}

void FTexture2DVideoSourceAdapter::OnFrameReady(const FTexture2DRHIRef& FrameBuffer)
{
	const int64 Timestamp = rtc::TimeMicros();

	if (!AdaptVideoFrame(Timestamp, FrameBuffer->GetSizeXY()))
		return;

	 if (!CaptureContext)
	 {
	 	FIntPoint FBSize = FrameBuffer->GetSizeXY();
	 	CaptureContext = MakeUnique<FNVENCCapturerContext>(FBSize.X, FBSize.Y, true);
	 }

	 FNVENCCapturerContext::FCapturerInput CapturerInput = CaptureContext->ObtainCapturerInput();
	 AVEncoder::FVideoEncoderInputFrame* InputFrame = CapturerInput.InputFrame;
	 FTexture2DRHIRef Texture = CapturerInput.Texture.GetValue();

	 const int32 FrameId = InputFrame->GetFrameID();
	 InputFrame->SetTimestampUs(Timestamp);

	 CopyTexture(FrameBuffer, Texture);

	 rtc::scoped_refptr<FNVENCFrameBuffer> Buffer = new rtc::RefCountedObject<FNVENCFrameBuffer>(Texture, InputFrame, CaptureContext->GetVideoEncoderInput());
	 webrtc::VideoFrame Frame = webrtc::VideoFrame::Builder()
	 							   .set_video_frame_buffer(Buffer)
	 							   .set_timestamp_us(Timestamp)
	 							   .set_rotation(webrtc::VideoRotation::kVideoRotation_0)
	 							   .build();

	 rtc::AdaptedVideoTrackSource::OnFrame(Frame);
	 InputFrame->Release();
}

webrtc::MediaSourceInterface::SourceState FTexture2DVideoSourceAdapter::state() const
{
	return webrtc::MediaSourceInterface::SourceState::kLive;
}

bool FTexture2DVideoSourceAdapter::AdaptVideoFrame(int64 TimestampUs, FIntPoint Resolution)
{
	int out_width, out_height, crop_width, crop_height, crop_x, crop_y;
	return rtc::AdaptedVideoTrackSource::AdaptFrame(Resolution.X, Resolution.Y, TimestampUs,
		&out_width, &out_height, &crop_width, &crop_height, &crop_x, &crop_y);
}
