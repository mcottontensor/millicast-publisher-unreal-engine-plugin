// Copyright Millicast 2022. All Rights Reserved.

#include "Stats.h"

FPublisherStats FPublisherStats::Instance;

FPublisherStats::FPublisherStats()
{
	WebRTCStatsCallback = new rtc::RefCountedObject<FRTCStatsCallback>();
}

void FPublisherStats::Tick(float DeltaTime)
{
	if(!GEngine)
	{
		return;
	}

	// Have a one second window where we sample certain timings
	Timings.SamplingWindow += DeltaTime;
	if(Timings.SamplingWindow >= 1.0f)
	{
		Timings.SamplingWindow = 0.0f;
		CalculatedStats.RenderFPS = Timings.FramesSubmitted;
		Timings.FramesSubmitted = 0;
	}

	if(Timings.TextureReadbackEnd > Timings.TextureReadbackStart)
	{
		CalculatedStats.TextureCaptureMs = FPlatformTime::ToMilliseconds64(Timings.TextureReadbackEnd - Timings.TextureReadbackStart);
	}

	int MessageKey = 100;

	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Rendered frames - %d fps"), CalculatedStats.RenderFPS), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Capture - %.2f ms"), CalculatedStats.TextureCaptureMs), true);
}

/*
* FRTCStatsCallback
*/

void FRTCStatsCallback::AddRef() const
{
	FPlatformAtomics::InterlockedIncrement(&RefCount);
}

rtc::RefCountReleaseStatus FRTCStatsCallback::Release() const
{
	if (FPlatformAtomics::InterlockedDecrement(&RefCount) == 0)
	{
		return rtc::RefCountReleaseStatus::kDroppedLastRef;
	}

	return rtc::RefCountReleaseStatus::kOtherRefsRemained;
}

void FRTCStatsCallback::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& Report)
{
	// todo: handle rtc stats report
}