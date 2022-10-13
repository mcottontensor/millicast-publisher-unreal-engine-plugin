// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebRTCInc.h"
#include "VideoEncoderInput.h"
#include "RHI.h"
#include "RHI/AsyncTextureReadback.h"

namespace libyuv
{
	extern "C"
	{
		/** libyuv header can't be included here, so just declare this function to convert the frames. */
		int ARGBToI420(const uint8_t* src_bgra,
			int src_stride_bgra,
			uint8_t* dst_y,
			int dst_stride_y,
			uint8_t* dst_u,
			int dst_stride_u,
			uint8_t* dst_v,
			int dst_stride_v,
			int width,
			int height);
	}
} // namespace libyuv

class FNVENCFrameBuffer : public webrtc::VideoFrameBuffer
{
public:
	FNVENCFrameBuffer(FTexture2DRHIRef SourceTexture,
		AVEncoder::FVideoEncoderInputFrame* InputFrame,
		TSharedPtr<AVEncoder::FVideoEncoderInput> InputVideoEncoderInput)
		: TextureRef(SourceTexture)
		, Frame(InputFrame)
		, VideoEncoderInput(InputVideoEncoderInput)
	{
		Frame->Obtain();
	}

	~FNVENCFrameBuffer()
	{
		Frame->Release();
	}

	Type type() const override
	{
		return Type::kNative;
	}

	virtual int width() const override
	{
		return Frame->GetWidth();
	}

	virtual int height() const override
	{
		return Frame->GetHeight();
	}

	virtual rtc::scoped_refptr<webrtc::I420BufferInterface> ToI420() override
	{
		return Buffer;
	}

	virtual const webrtc::I420BufferInterface* GetI420() const override
	{
		return Buffer;
	}

	FTexture2DRHIRef GetTextureRHI() const
	{
		return TextureRef;
	}

	AVEncoder::FVideoEncoderInputFrame* GetFrame() const
	{
		return Frame;
	}

	TSharedPtr<AVEncoder::FVideoEncoderInput> GetVideoEncoderInput() const
	{
		return VideoEncoderInput;
	}

	void SetI420Data(uint8* B8G8R8A8Pixels, int Width, int Height, int Stride)
	{
		Buffer = webrtc::I420Buffer::Create(Width, Height);

		uint8* DataY = Buffer->MutableDataY();
		uint8* DataU = Buffer->MutableDataU();
		uint8* DataV = Buffer->MutableDataV();

		const auto STRIDES = Stride * 4;

		libyuv::ARGBToI420(
			B8G8R8A8Pixels,
			STRIDES,
			Buffer->MutableDataY(),
			Buffer->StrideY(),
			Buffer->MutableDataU(),
			Buffer->StrideU(),
			Buffer->MutableDataV(),
			Buffer->StrideV(),
			Buffer->width(),
			Buffer->height());
	}

private:
	FTexture2DRHIRef TextureRef;
	AVEncoder::FVideoEncoderInputFrame* Frame;
	TSharedPtr<AVEncoder::FVideoEncoderInput> VideoEncoderInput;
	rtc::scoped_refptr<webrtc::I420Buffer> Buffer;
};