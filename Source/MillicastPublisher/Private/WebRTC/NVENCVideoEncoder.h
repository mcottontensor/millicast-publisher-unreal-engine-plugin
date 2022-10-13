#pragma once

#include "VideoEncoder.h"
#include "CodecPacket.h"

class FNVENCVideoEncoder : public webrtc::VideoEncoder
{
public:
	FNVENCVideoEncoder();
	virtual ~FNVENCVideoEncoder() override;

	virtual int32 RegisterEncodeCompleteCallback(webrtc::EncodedImageCallback* callback) override;
	virtual int32 Release() override;

	// WebRTC Interface
	virtual int InitEncode(webrtc::VideoCodec const* codec_settings, webrtc::VideoEncoder::Settings const& settings) override;
	virtual int32 Encode(webrtc::VideoFrame const& frame, std::vector<webrtc::VideoFrameType> const* frame_types) override;
	virtual void SetRates(RateControlParameters const& parameters) override;
	virtual webrtc::VideoEncoder::EncoderInfo GetEncoderInfo() const override;

	AVEncoder::FVideoEncoder::FLayerConfig GetConfig() const { return EncoderConfig; }

private:
	void OnEncodedPacket(uint32 InLayerIndex, const AVEncoder::FVideoEncoderInputFrame* InFrame, const AVEncoder::FCodecPacket& InPacket);
	void UpdateConfig(AVEncoder::FVideoEncoder::FLayerConfig const& Config);
	void HandlePendingRateChange();
	void CreateAVEncoder(TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput);

	// "this" cannot be a shared_ptr because webrtc wants a unique_ptr so instead we use
	// an owned shared ptr to make sure this encoder hasnt gone away between encode
	// and the callback
	struct FDeleteCheck
	{
		FNVENCVideoEncoder* Self;
	};
	TSharedPtr<FDeleteCheck> DeleteCheck;

	TSharedPtr<AVEncoder::FVideoEncoder> NVENCEncoder;
	AVEncoder::FVideoEncoder::FLayerConfig EncoderConfig;
	int32 WebRtcProposedTargetBitrate = 5000000;
	webrtc::EncodedImageCallback* OnEncodedImageCallback = nullptr;
	TOptional<RateControlParameters> PendingRateChange;
};