// Copyright Millicast 2022. All Rights Reserved.

#include "Stats.h"
#include "WebRTC/PeerConnection.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMillicastStats, Log, All);
DEFINE_LOG_CATEGORY(LogMillicastStats);

FPublisherStats FPublisherStats::Instance;

void FPublisherStats::Tick(float DeltaTime)
{
	if (!GEngine)
	{
		return;
	}

	// Have a one second window where we sample certain timings
	Timings.SamplingWindow += DeltaTime;
	if (Timings.SamplingWindow >= 1.0f)
	{
		Timings.SamplingWindow = 0.0f;
		CalculatedStats.RenderFPS = Timings.FramesSubmitted;
		Timings.FramesSubmitted = 0;
	}

	if (Timings.TextureReadbackEnd > Timings.TextureReadbackStart)
	{
		CalculatedStats.TextureCaptureMs = FPlatformTime::ToMilliseconds64(Timings.TextureReadbackEnd - Timings.TextureReadbackStart);
	}

	for (FRTCStatsCollector* Collector : StatsCollectors)
	{
		Collector->Poll();
	}

	int MessageKey = 100;
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Capture Time = %.2f ms"), CalculatedStats.TextureCaptureMs), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Rendered FPS = %d"), CalculatedStats.RenderFPS), true);
}

void FPublisherStats::RegisterStatsCollector(FRTCStatsCollector* Connection)
{
	StatsCollectors.Add(Connection);
}

void FPublisherStats::UnregisterStatsCollector(FRTCStatsCollector* Connection)
{
	StatsCollectors.Remove(Connection);
}

/*
 * FRTCStatsCollector
 */

FRTCStatsCollector::FRTCStatsCollector(class FWebRTCPeerConnection* InPeerConnection)
:PeerConnection(InPeerConnection)
{
	FPublisherStats::Get().RegisterStatsCollector(this);
}

FRTCStatsCollector::~FRTCStatsCollector()
{
	FPublisherStats::Get().UnregisterStatsCollector(this);
}

void FRTCStatsCollector::Poll()
{
	PeerConnection->PollStats();

	int MessageKey = 200;
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Total Encode Time = %.2f s"), TotalEncodeTime), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Avg Encode Time = %.2f ms"), AvgEncodeTime), true);
	GEngine->AddOnScreenDebugMessage(MessageKey++, 0.0f, FColor::Green, FString::Printf(TEXT("Encode FPS = %.0f"), EncodeFPS), true);
}

void FRTCStatsCollector::OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& Report)
{
	double NewTotalEncodedFrames = 0;
	double NewTotalEncodeTime = 0;

	for (const webrtc::RTCStats& Stats : *Report)
	{
		const FString StatsType = FString(Stats.type());
		const FString StatsId = FString(Stats.id().c_str());

		//UE_LOG(LogMillicastStats, Log, TEXT("Type: %s Id: %s"), *StatsType, *StatsId);

		for (const webrtc::RTCStatsMemberInterface* StatMember : Stats.Members())
		{
			const FString MemberName = FString(StatMember->name());
			const FString MemberValue = FString(StatMember->ValueToString().c_str());

			//UE_LOG(LogMillicastStats, Log, TEXT("    %s = %s"), *MemberName, *MemberValue);

			if (StatsType == "outbound-rtp")
			{
				if (MemberName == "totalEncodeTime")
				{
					NewTotalEncodeTime = FCString::Atod(*MemberValue);
				}
				else if (MemberName == "framesEncoded")
				{
					NewTotalEncodedFrames = FCString::Atod(*MemberValue);
				}
				else if (MemberName == "framesPerSecond")
				{
					EncodeFPS = FCString::Atod(*MemberValue);
				}
			}
		}
	}

	const double EncodedFramesDelta = NewTotalEncodedFrames - TotalEncodedFrames;
	if (EncodedFramesDelta)
	{
		const double EncodeTimeDelta = (NewTotalEncodeTime - TotalEncodeTime) * 1000.0;
		AvgEncodeTime = EncodeTimeDelta / EncodedFramesDelta;
		TotalEncodedFrames = NewTotalEncodedFrames;
		TotalEncodeTime = NewTotalEncodeTime;
	}
}

void FRTCStatsCollector::AddRef() const
{
	FPlatformAtomics::InterlockedIncrement(&RefCount);
}

rtc::RefCountReleaseStatus FRTCStatsCollector::Release() const
{
	if (FPlatformAtomics::InterlockedDecrement(&RefCount) == 0)
	{
		return rtc::RefCountReleaseStatus::kDroppedLastRef;
	}

	return rtc::RefCountReleaseStatus::kOtherRefsRemained;
}