#include "NVENCVideoEncoder.h"
#include "NVENCFrameBuffer.h"

#include "VideoEncoderFactory.h"

FNVENCVideoEncoder::FNVENCVideoEncoder()
{
	DeleteCheck = MakeShared<FNVENCVideoEncoder::FDeleteCheck>();
	DeleteCheck->Self = this;
}

FNVENCVideoEncoder::~FNVENCVideoEncoder()
{
}

int32 FNVENCVideoEncoder::RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback)
{
	OnEncodedImageCallback = callback;
	return WEBRTC_VIDEO_CODEC_OK;
}

int32 FNVENCVideoEncoder::Release()
{
	OnEncodedImageCallback = nullptr;
	return WEBRTC_VIDEO_CODEC_OK;
}

void FNVENCVideoEncoder::HandlePendingRateChange()
{
	if(PendingRateChange.IsSet())
	{
		const RateControlParameters& RateChangeParams = PendingRateChange.GetValue();

		WebRtcProposedTargetBitrate = RateChangeParams.bitrate.get_sum_kbps() * 1000;

		EncoderConfig.MaxFramerate = RateChangeParams.framerate_fps;
		EncoderConfig.TargetBitrate = WebRtcProposedTargetBitrate;

		// Only the quality controlling peer should update the underlying encoder configuration with new bitrate/framerate.
		if (NVENCEncoder)
		{
			NVENCEncoder->UpdateLayerConfig(0, EncoderConfig);
		}

		// Clear the rate change request
		PendingRateChange.Reset();
	}
}

// Pass rate control parameters from WebRTC to our encoder
// This is how WebRTC can control the bitrate/framerate of the encoder.
void FNVENCVideoEncoder::SetRates(RateControlParameters const& parameters)
{
	PendingRateChange.Emplace(parameters);
}

webrtc::VideoEncoder::EncoderInfo FNVENCVideoEncoder::GetEncoderInfo() const
{
	VideoEncoder::EncoderInfo info;
	info.supports_native_handle = true;
	info.is_hardware_accelerated = true;
	info.has_internal_source = false;
	info.supports_simulcast = false;
	info.implementation_name = TCHAR_TO_UTF8(*FString::Printf(TEXT("MILLICAST_HW_ENCODER_%s"), GDynamicRHI->GetName()));

	//info.scaling_settings = VideoEncoder::ScalingSettings(LowQP, HighQP);

	// basically means HW encoder must be perfect and drop frames itself etc
	info.has_trusted_rate_controller = false;

	return info;
}

void FNVENCVideoEncoder::UpdateConfig(AVEncoder::FVideoEncoder::FLayerConfig const& InConfig)
{
	EncoderConfig = InConfig;

	// Only the quality controlling peer should update the underlying encoder configuration.
	if (NVENCEncoder)
	{
		NVENCEncoder->UpdateLayerConfig(0, EncoderConfig);
	}
}

int FNVENCVideoEncoder::InitEncode(webrtc::VideoCodec const* codec_settings, VideoEncoder::Settings const& settings)
{
	checkf(AVEncoder::FVideoEncoderFactory::Get().IsSetup(), TEXT("FVideoEncoderFactory not setup"));

	EncoderConfig.Width = codec_settings->width;
	EncoderConfig.Height = codec_settings->height;
	EncoderConfig.MaxBitrate = codec_settings->maxBitrate;
	EncoderConfig.TargetBitrate = codec_settings->startBitrate;
	EncoderConfig.MaxFramerate = codec_settings->maxFramerate;
	WebRtcProposedTargetBitrate = codec_settings->startBitrate;

	return WEBRTC_VIDEO_CODEC_OK;
}

int32 FNVENCVideoEncoder::Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types)
{
	// Get the frame buffer out of the frame
	FNVENCFrameBuffer* VideoFrameBuffer = static_cast<FNVENCFrameBuffer*>(frame.video_frame_buffer().get());

	if (!NVENCEncoder)
	{
		CreateAVEncoder(VideoFrameBuffer->GetVideoEncoderInput());
	}

	// Change rates, if required.
	HandlePendingRateChange();

	// Detect video frame not matching encoder encoding resolution
	// Note: This can happen when UE application changes its resolution and the encoder is not programattically updated.
	const int FrameWidth = frame.width();
	const int FrameHeight = frame.height();
	if (EncoderConfig.Width != FrameWidth || EncoderConfig.Height != FrameHeight)
	{
		EncoderConfig.Width = FrameWidth;
		EncoderConfig.Height = FrameHeight;
		NVENCEncoder->UpdateLayerConfig(0, EncoderConfig);
	}
	
	AVEncoder::FVideoEncoder::FEncodeOptions EncodeOptions;
	if (frame_types && (*frame_types)[0] == webrtc::VideoFrameType::kVideoFrameKey)
	{
		EncodeOptions.bForceKeyFrame = true;
	}

	AVEncoder::FVideoEncoderInputFrame* EncoderInputFrame = VideoFrameBuffer->GetFrame();
	EncoderInputFrame->SetTimestampRTP(frame.timestamp());

	// Encode the frame!
	NVENCEncoder->Encode(EncoderInputFrame, EncodeOptions);

	return WEBRTC_VIDEO_CODEC_OK;
}

void CreateH264FragmentHeader(const uint8* CodedData, size_t CodedDataSize, webrtc::RTPFragmentationHeader& Fragments)
{
	// count the number of nal units
	for (int pass = 0; pass < 2; ++pass)
	{
		size_t num_nal = 0;
		size_t offset = 0;
		while (offset < CodedDataSize)
		{
			// either a 0,0,1 or 0,0,0,1 sequence indicates a new 'nal'
			size_t nal_maker_length = 3;
			if (offset < (CodedDataSize - 3) && CodedData[offset] == 0 &&
				CodedData[offset + 1] == 0 && CodedData[offset + 2] == 1)
			{
			}
			else if (offset < (CodedDataSize - 4) && CodedData[offset] == 0 &&
				CodedData[offset + 1] == 0 && CodedData[offset + 2] == 0 &&
				CodedData[offset + 3] == 1)
			{
				nal_maker_length = 4;
			}
			else
			{
				++offset;
				continue;
			}
			if (pass == 1)
			{
				Fragments.fragmentationOffset[num_nal] = offset + nal_maker_length;
				Fragments.fragmentationLength[num_nal] = 0;
				if (num_nal > 0)
				{
					Fragments.fragmentationLength[num_nal - 1] = offset - Fragments.fragmentationOffset[num_nal - 1];
				}
			}
			offset += nal_maker_length;
			++num_nal;
		}
		if (pass == 0)
		{
			Fragments.VerifyAndAllocateFragmentationHeader(num_nal);
		}
		else if (pass == 1 && num_nal > 0)
		{
			Fragments.fragmentationLength[num_nal - 1] = offset - Fragments.fragmentationOffset[num_nal - 1];
		}
	}
}

void FNVENCVideoEncoder::OnEncodedPacket(uint32 InLayerIndex, const AVEncoder::FVideoEncoderInputFrame* InFrame, const AVEncoder::FCodecPacket& InPacket)
{
	webrtc::EncodedImage Image;

	webrtc::RTPFragmentationHeader FragHeader;
	CreateH264FragmentHeader(InPacket.Data, InPacket.DataSize, FragHeader);

	Image.timing_.packetization_finish_ms = FTimespan::FromSeconds(FPlatformTime::Seconds()).GetTotalMilliseconds();
	Image.timing_.encode_start_ms = InPacket.Timings.StartTs.GetTotalMilliseconds();
	Image.timing_.encode_finish_ms = InPacket.Timings.FinishTs.GetTotalMilliseconds();
	Image.timing_.flags = webrtc::VideoSendTiming::kTriggeredByTimer;

	Image.SetEncodedData(webrtc::EncodedImageBuffer::Create(const_cast<uint8_t*>(InPacket.Data), InPacket.DataSize));
	Image._encodedWidth = InFrame->GetWidth();
	Image._encodedHeight = InFrame->GetHeight();
	Image._frameType = InPacket.IsKeyFrame ? webrtc::VideoFrameType::kVideoFrameKey : webrtc::VideoFrameType::kVideoFrameDelta;
	Image.content_type_ = webrtc::VideoContentType::UNSPECIFIED;
	Image.qp_ = InPacket.VideoQP;
	Image.SetSpatialIndex(InLayerIndex);
	Image._completeFrame = true;
	Image.rotation_ = webrtc::VideoRotation::kVideoRotation_0;
	Image.SetTimestamp(InFrame->GetTimestampRTP());
	Image.capture_time_ms_ = InFrame->GetTimestampUs() / 1000.0;

	webrtc::CodecSpecificInfo CodecInfo;
	CodecInfo.codecType = webrtc::VideoCodecType::kVideoCodecH264;
	CodecInfo.codecSpecific.H264.packetization_mode = webrtc::H264PacketizationMode::NonInterleaved;
	CodecInfo.codecSpecific.H264.temporal_idx = webrtc::kNoTemporalIdx;
	CodecInfo.codecSpecific.H264.idr_frame = InPacket.IsKeyFrame;
	CodecInfo.codecSpecific.H264.base_layer_sync = false;

	if (OnEncodedImageCallback)
	{
		OnEncodedImageCallback->OnEncodedImage(Image, &CodecInfo, &FragHeader);
	}
}

void FNVENCVideoEncoder::CreateAVEncoder(TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput)
{
	// TODO: When we have multiple HW encoders do some factory checking and find the best encoder.
	const TArray<AVEncoder::FVideoEncoderInfo>& Available = AVEncoder::FVideoEncoderFactory::Get().GetAvailable();

	TUniquePtr<AVEncoder::FVideoEncoder> A = AVEncoder::FVideoEncoderFactory::Get().Create(Available[0].ID, EncoderInput, EncoderConfig);
	NVENCEncoder = TSharedPtr<AVEncoder::FVideoEncoder>(A.Release());
	checkf(NVENCEncoder, TEXT("Video encoder creation failed, check encoder config."));

	TWeakPtr<FNVENCVideoEncoder::FDeleteCheck> WeakCheck = DeleteCheck;
	NVENCEncoder->SetOnEncodedPacket([WeakCheck](uint32 InLayerIndex, const AVEncoder::FVideoEncoderInputFrame* InFrame, const AVEncoder::FCodecPacket& InPacket) 
	{
		if (TSharedPtr<FNVENCVideoEncoder::FDeleteCheck> Check = WeakCheck.Pin())
		{
			Check->Self->OnEncodedPacket(InLayerIndex, InFrame, InPacket);
		}
	});
}
