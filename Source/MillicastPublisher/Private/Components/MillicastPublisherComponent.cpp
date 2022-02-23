// Copyright Millicast 2022. All Rights Reserved.

#include "MillicastPublisherComponent.h"
#include "MillicastPublisherPrivate.h"

#include <string>

#include "Http.h"

#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"

#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "WebRTC/PeerConnection.h"

constexpr auto HTTP_OK = 200;

inline std::string to_string(const FString& Str)
{
	auto Ansi = StringCast<ANSICHAR>(*Str, Str.Len());
	std::string Res{ Ansi.Get(), static_cast<SIZE_T>(Ansi.Length()) };
	return Res;
}

inline FString ToString(const std::string& Str)
{
	auto Conv = StringCast<TCHAR>(Str.c_str(), Str.size());
	FString Res{ Conv.Length(), Conv.Get() };
	return Res;
}


UMillicastPublisherComponent::UMillicastPublisherComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PeerConnection = nullptr;
}

/**
	Initialize this component with the media source required for receiving Millicast audio, video.
	Returns false, if the MediaSource is already been set. This is usually the case when this component is
	initialized in Blueprints.
*/
bool UMillicastPublisherComponent::Initialize(UMillicastPublisherSource* InMediaSource)
{
	if (MillicastMediaSource == nullptr && InMediaSource != nullptr)
	{
		MillicastMediaSource = InMediaSource;
	}

	return InMediaSource != nullptr && InMediaSource == MillicastMediaSource;
}

/**
	Authenticating through director api
*/
bool UMillicastPublisherComponent::Publish()
{
	if (!IsValid(MillicastMediaSource)) return false;

	auto PostHttpRequest = FHttpModule::Get().CreateRequest();
	PostHttpRequest->SetURL(MillicastMediaSource->GetUrl());
	PostHttpRequest->SetVerb("POST");
	PostHttpRequest->SetHeader("Content-Type", "application/json");
	PostHttpRequest->SetHeader("Authorization", "Bearer " + MillicastMediaSource->PublishingToken);

	auto RequestData = MakeShared<FJsonObject>();
	RequestData->SetStringField("streamName", MillicastMediaSource->StreamName);

	FString SerializedRequestData;
	auto JsonWriter = TJsonWriterFactory<>::Create(&SerializedRequestData);
	FJsonSerializer::Serialize(RequestData, JsonWriter);

	PostHttpRequest->SetContentAsString(SerializedRequestData);

	PostHttpRequest->OnProcessRequestComplete()
		.BindLambda([this](FHttpRequestPtr Request,
			FHttpResponsePtr Response,
			bool bConnectedSuccessfully) {
		if (bConnectedSuccessfully && Response->GetResponseCode() == HTTP_OK) {
			FString ResponseDataString = Response->GetContentAsString();
			TSharedPtr<FJsonObject> ResponseDataJson;
			auto JsonReader = TJsonReaderFactory<>::Create(ResponseDataString);

			if (FJsonSerializer::Deserialize(JsonReader, ResponseDataJson)) {
				TSharedPtr<FJsonObject> DataField = ResponseDataJson->GetObjectField("data");
				auto Jwt = DataField->GetStringField("jwt");
				auto WebSocketUrlField = DataField->GetArrayField("urls")[0];
				FString WsUrl;

				WebSocketUrlField->TryGetString(WsUrl);

				UE_LOG(LogMillicastPublisher, Log, TEXT("WsUrl : %s \njwt : %s"),
					*WsUrl, *Jwt);


				StartWebSocketConnection(WsUrl, Jwt);
			}
		}
		else {
			UE_LOG(LogMillicastPublisher, Error, TEXT("Director HTTP request failed %d %s"), Response->GetResponseCode(), *Response->GetContentType());
			FString ErrorMsg = Response->GetContentAsString();
			OnAuthenticationFailure.Broadcast(Response->GetResponseCode(), ErrorMsg);
		}
	});

	return PostHttpRequest->ProcessRequest();	    
}

bool UMillicastPublisherComponent::PublishWithWsAndJwt(const FString& WsUrl, const FString& Jwt)
{
	return IsValid(MillicastMediaSource) && StartWebSocketConnection(WsUrl, Jwt);
}

/**
	Attempts to stop receiving audio, video.
*/
void UMillicastPublisherComponent::UnPublish()
{
	if(PeerConnection)
	{
		delete PeerConnection;
		PeerConnection = nullptr;

		WS->Close();
		WS = nullptr;

		MillicastMediaSource->StopCapture();
	}
}

bool UMillicastPublisherComponent::IsPublishing() const
{
	return PeerConnection != nullptr;
}

bool UMillicastPublisherComponent::StartWebSocketConnection(const FString& Url,
                                                     const FString& Jwt)
{
	if (!FModuleManager::Get().IsModuleLoaded("WebSockets"))
	{
		FModuleManager::Get().LoadModule("WebSockets");
	}

	WS = FWebSocketsModule::Get().CreateWebSocket(Url + "?token=" + Jwt);

	OnConnectedHandle = WS->OnConnected().AddLambda([this]() { OnConnected(); });
	OnConnectionErrorHandle = WS->OnConnectionError().AddLambda([this](const FString& Error) { OnConnectionError(Error); });
	OnClosedHandle = WS->OnClosed().AddLambda([this](int32 StatusCode, const FString& Reason, bool bWasClean) { OnClosed(StatusCode, Reason, bWasClean); });
	OnMessageHandle = WS->OnMessage().AddLambda([this](const FString& Msg) { OnMessage(Msg); });

	WS->Connect();

	return true;
}

bool UMillicastPublisherComponent::PublishToMillicast()
{
	PeerConnection =
		FWebRTCPeerConnection::Create(FWebRTCPeerConnection::GetDefaultConfig());

	CaptureAndAddTracks();

	auto * CreateSessionDescriptionObserver = PeerConnection->GetCreateDescriptionObserver();
	auto * LocalDescriptionObserver  = PeerConnection->GetLocalDescriptionObserver();
	auto * RemoteDescriptionObserver = PeerConnection->GetRemoteDescriptionObserver();

	CreateSessionDescriptionObserver->SetOnSuccessCallback([this](const std::string& type, const std::string& sdp) {
		UE_LOG(LogMillicastPublisher, Log, TEXT("pc.createOffer() | sucess\nsdp : %s"), *ToString(sdp));
		PeerConnection->SetLocalDescription(sdp, type);
	});

	CreateSessionDescriptionObserver->SetOnFailureCallback([](const std::string& err) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("pc.createOffer() | Error: %s"), *ToString(err));
	});

	LocalDescriptionObserver->SetOnSuccessCallback([this]() {
		UE_LOG(LogMillicastPublisher, Log, TEXT("pc.setLocalDescription() | sucess"));
		std::string sdp;
		(*PeerConnection)->local_description()->ToString(&sdp);

		auto DataJson = MakeShared<FJsonObject>();
		DataJson->SetStringField("name", MillicastMediaSource->StreamName);
		DataJson->SetStringField("sdp", ToString(sdp));

		auto Payload = MakeShared<FJsonObject>();
		Payload->SetStringField("type", "cmd");
		Payload->SetNumberField("transId", std::rand());
		Payload->SetStringField("name", "publish");
		Payload->SetObjectField("data", DataJson);

		FString StringStream;
		auto Writer = TJsonWriterFactory<>::Create(&StringStream);
		FJsonSerializer::Serialize(Payload, Writer);

		WS->Send(StringStream);
	});

	LocalDescriptionObserver->SetOnFailureCallback([](const std::string& err) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("Set local description failed : %s"), *ToString(err));
	});

	RemoteDescriptionObserver->SetOnSuccessCallback([this]() {
		UE_LOG(LogMillicastPublisher, Log, TEXT("Set remote description suceeded"));
	});
	RemoteDescriptionObserver->SetOnFailureCallback([](const std::string& err) {
		UE_LOG(LogMillicastPublisher, Error, TEXT("Set remote description failed : %s"), *ToString(err));
	});

	PeerConnection->OaOptions.offer_to_receive_video = false;
	PeerConnection->OaOptions.offer_to_receive_audio = false;

	PeerConnection->CreateOffer();

	return true;
}

/* WebSocket Callback
*****************************************************************************/

void UMillicastPublisherComponent::OnConnected()
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Connected"));
	PublishToMillicast();
}

void UMillicastPublisherComponent::OnConnectionError(const FString& Error)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Connection error : %s"), *Error);
}

void UMillicastPublisherComponent::OnClosed(int32 StatusCode,
                                     const FString& Reason,
                                     bool bWasClean)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket Closed"))
}

void UMillicastPublisherComponent::OnMessage(const FString& Msg)
{
	UE_LOG(LogMillicastPublisher, Log, TEXT("Millicast WebSocket new Message : %s"), *Msg);

	TSharedPtr<FJsonObject> ResponseJson;
	auto Reader = TJsonReaderFactory<>::Create(Msg);

	if(FJsonSerializer::Deserialize(Reader, ResponseJson)) {
		FString Type;
		if(!ResponseJson->TryGetStringField("type", Type)) return;

		if(Type == "response") {
			auto DataJson = ResponseJson->GetObjectField("data");
			FString Sdp = DataJson->GetStringField("sdp");
			PeerConnection->SetRemoteDescription(to_string(Sdp));
		}
		else if(Type == "error") {
			// TODO: Parse error and fire an event
		}
		else if(Type == "event") {
			// TODO: Parse broadcaster event and a blueprint event for each
		}
		else {
			// TODO: Unknown answer
		}
	}
}

void UMillicastPublisherComponent::CaptureAndAddTracks()
{
	auto VideoTrack = MillicastMediaSource->StartCapture();
	
	webrtc::RtpTransceiverInit Init;
	Init.direction = webrtc::RtpTransceiverDirection::kSendOnly;
	Init.stream_ids = { "unrealstream" };

	auto result = (*PeerConnection)->AddTransceiver(VideoTrack, Init);

	if (result.ok())
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Add transceiver for video track"));
	}
	else
	{
		UE_LOG(LogMillicastPublisher, Log, TEXT("Couldn't add transceiver for video track : %s"), result.error().message());
	}
}
