// Copyright Millicast 2022. All Rights Reserved.

#include "VideoEncoderVPX.h"
#include "FrameBufferRHI.h"

FVideoEncoderVPX::FVideoEncoderVPX(int VPXVersion)
{
	std::unique_ptr<webrtc::VideoEncoder> WebRTCEncoder;
	if (VPXVersion == 8)
	{
		WebRTCEncoder = webrtc::VP8Encoder::Create();
	}
	else if (VPXVersion == 9)
	{
		WebRTCEncoder = webrtc::VP9Encoder::Create();
	}
	else
	{
		checkf(false, TEXT("Bad VPX version number supplied to VideoEncoderVPX"));
	}

	SharedContext = MakeShared<FVideoEncoderVPX::FSharedContext>(std::move(WebRTCEncoder));
}

FVideoEncoderVPX::~FVideoEncoderVPX()
{
}

void FVideoEncoderVPX::SetFecControllerOverride(webrtc::FecControllerOverride* fec_controller_override)
{
	SharedContext->WebRTCEncoder->SetFecControllerOverride(fec_controller_override);
}

int FVideoEncoderVPX::InitEncode(webrtc::VideoCodec const* codec_settings, webrtc::VideoEncoder::Settings const& settings)
{
	return SharedContext->WebRTCEncoder->InitEncode(codec_settings, settings);
}

int32 FVideoEncoderVPX::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback)
{
	return SharedContext->WebRTCEncoder->RegisterEncodeCompleteCallback(callback);
}

int32 FVideoEncoderVPX::Release()
{
	return SharedContext->WebRTCEncoder->Release();
}

int32 FVideoEncoderVPX::Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types)
{
	rtc::scoped_refptr<FFrameBufferRHI> VideoFrameBuffer(static_cast<FFrameBufferRHI*>(frame.video_frame_buffer().get()));
	TWeakPtr<FSharedContext> WeakContext = SharedContext;
	VideoFrameBuffer->ConvertI420([WeakContext, VideoFrameBuffer, frame, frame_types](){
		if (TSharedPtr<FSharedContext> Context = WeakContext.Pin())
		{
			Context->WebRTCEncoder->Encode(frame, frame_types);
		}
	});
	return WEBRTC_VIDEO_CODEC_OK;
}

void FVideoEncoderVPX::SetRates(RateControlParameters const& parameters)
{
	SharedContext->WebRTCEncoder->SetRates(parameters);
}

void FVideoEncoderVPX::OnPacketLossRateUpdate(float packet_loss_rate)
{
	SharedContext->WebRTCEncoder->OnPacketLossRateUpdate(packet_loss_rate);
}

void FVideoEncoderVPX::OnRttUpdate(int64_t rtt_ms)
{
	SharedContext->WebRTCEncoder->OnRttUpdate(rtt_ms);
}

void FVideoEncoderVPX::OnLossNotification(const LossNotification& loss_notification)
{
	SharedContext->WebRTCEncoder->OnLossNotification(loss_notification);
}

webrtc::VideoEncoder::EncoderInfo FVideoEncoderVPX::GetEncoderInfo() const
{
	VideoEncoder::EncoderInfo info = SharedContext->WebRTCEncoder->GetEncoderInfo();
	info.supports_native_handle = true;
	return info;
}
