// Copyright Millicast 2022. All Rights Reserved.

#include "Texture2DVideoSourceAdapter.h"
#include "MillicastPublisherPrivate.h"
#include "FrameBufferRHI.h"
#include "RHI/CopyTexture.h"

void FTexture2DVideoSourceAdapter::OnFrameReady(const FTexture2DRHIRef& FrameBuffer, bool ReadColor)
{
	const int64 Timestamp = rtc::TimeMicros();

	if (!AdaptVideoFrame(Timestamp, FrameBuffer->GetSizeXY()))
		return;

	if (!CaptureContext)
	{
		FIntPoint FBSize = FrameBuffer->GetSizeXY();
		CaptureContext = MakeUnique<FAVEncoderContext>(FBSize.X, FBSize.Y, true);
	}

	FAVEncoderContext::FCapturerInput CapturerInput = CaptureContext->ObtainCapturerInput();
	AVEncoder::FVideoEncoderInputFrame* InputFrame = CapturerInput.InputFrame;
	FTexture2DRHIRef Texture = CapturerInput.Texture.GetValue();

	const int32 FrameId = InputFrame->GetFrameID();
	InputFrame->SetTimestampUs(Timestamp);

	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	CopyTexture(RHICmdList, FrameBuffer, Texture);

	rtc::scoped_refptr<FFrameBufferRHI> Buffer = new rtc::RefCountedObject<FFrameBufferRHI>(Texture, InputFrame, CaptureContext->GetVideoEncoderInput());
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
