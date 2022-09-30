// Copyright Millicast 2022. All Rights Reserved.
#pragma once

#include "Tickable.h"

/*
* Timing information taken during the life cycle of a frame as it passes through the publisher.
*/
struct FStatTimings
{
	private:
		uint64 FrameRendered;
		uint64 TextureReadbackStart;
		uint64 TextureReadbackEnd;
		uint64 WebRTCFrameSubmit;
		uint64 FramesSubmitted = 0;
		float SamplingWindow = 0;

		friend class FPublisherStats;
	
	public:
		void MarkFrameRendered() { FrameRendered = FPlatformTime::Cycles64(); FramesSubmitted++; }
		void MarkTextureReadbackStart() { TextureReadbackStart = FPlatformTime::Cycles64(); }
		void MarkTextureReadbackEnd() { TextureReadbackEnd = FPlatformTime::Cycles64(); }
		void MarkWebRTCFrameSubmit() { WebRTCFrameSubmit = FPlatformTime::Cycles64(); }
};

/*
* Stats that are calculated from timings or extracted from WebRTC peer connection.
*/
struct FCalculatedStats
{
	uint64 RenderFPS;
	double TextureCaptureMs;
	double EncodeMs;
};

class FRTCStatsCallback : public webrtc::RTCStatsCollectorCallback
{
	public:
		// Begin RTCStatsCollectorCallback interface
		void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override;
		void AddRef() const override;
		rtc::RefCountReleaseStatus Release() const override;
		// End RTCStatsCollectorCallback interface
	private:
		mutable int32 RefCount;
};

/*
* Some basic performance stats about how the publisher is running, e.g. how long capture/encode takes.
* Stats are drawn to screen for now as it is useful to observe them in realtime.
*/
class FPublisherStats : FTickableGameObject
{
	public:
		FStatTimings Timings;
		FCalculatedStats CalculatedStats;
		rtc::scoped_refptr<webrtc::RTCStatsCollectorCallback> WebRTCStatsCallback;

	private:
		// Intent is to access through FPublisherStats::Get()
		FPublisherStats();
		static FPublisherStats Instance;

	public:
		
		static FPublisherStats& Get() { return Instance; }

		void Tick(float DeltaTime);
		FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(MillicastProducerStats, STATGROUP_Tickables); }

	// todo: poll webrtc stats
};

