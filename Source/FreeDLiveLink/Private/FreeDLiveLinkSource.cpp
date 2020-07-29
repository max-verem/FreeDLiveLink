#include "FreeDLiveLinkSource.h"

#include "ILiveLinkClient.h"
#include "LiveLinkTypes.h"
#include "Roles/LiveLinkTransformRole.h"
#include "Roles/LiveLinkTransformTypes.h"
#include "Roles/LiveLinkCameraRole.h"
#include "Roles/LiveLinkCameraTypes.h"

#include "Async/Async.h"
#include "Common/UdpSocketBuilder.h"
#include "HAL/RunnableThread.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#include "FreeD.h"

#define LOCTEXT_NAMESPACE "FFreeDLiveLinkSource"

#define RECV_BUFFER_SIZE 1024 * 1024

FFreeDLiveLinkSource::FFreeDLiveLinkSource(FIPv4Endpoint InEndpoint)
: Socket(nullptr)
, Stopping(false)
, Thread(nullptr)
, WaitTime(FTimespan::FromMilliseconds(10))
{
	// defaults
	DeviceEndpoint = InEndpoint;

	SourceStatus = LOCTEXT("SourceStatus_DeviceNotFound", "Device Not Found");
	SourceType = LOCTEXT("FreedLiveLinkSource", "Free-D LiveLink");
	SourceMachineName = LOCTEXT("FreeDLiveLinkSourceMachineName", "localhost");

	//setup socket
	if (DeviceEndpoint.Address.IsMulticastAddress())
	{
		Socket = FUdpSocketBuilder(TEXT("FREEDSOCKET"))
			.AsNonBlocking()
			.AsReusable()
			.BoundToPort(DeviceEndpoint.Port)
			.WithReceiveBufferSize(RECV_BUFFER_SIZE)

			.BoundToAddress(FIPv4Address::Any)
			.JoinedToGroup(DeviceEndpoint.Address)
			.WithMulticastLoopback()
			.WithMulticastTtl(2);
					
	}
	else
	{
		Socket = FUdpSocketBuilder(TEXT("FREEDSOCKET"))
			.AsNonBlocking()
			.AsReusable()
			.BoundToAddress(DeviceEndpoint.Address)
			.BoundToPort(DeviceEndpoint.Port)
			.WithReceiveBufferSize(RECV_BUFFER_SIZE);
	}

	RecvBuffer.SetNumUninitialized(RECV_BUFFER_SIZE);

	if ((Socket != nullptr) && (Socket->GetSocketType() == SOCKTYPE_Datagram))
	{
		SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

		Start();

		SourceStatus = LOCTEXT("SourceStatus_Receiving", "Receiving");
	}
}

FFreeDLiveLinkSource::~FFreeDLiveLinkSource()
{
	Stop();
	if (Thread != nullptr)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	if (Socket != nullptr)
	{
		Socket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
	}
}

void FFreeDLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}


bool FFreeDLiveLinkSource::IsSourceStillValid() const
{
	// Source is valid if we have a valid thread and socket
	bool bIsSourceValid = !Stopping && Thread != nullptr && Socket != nullptr;
	return bIsSourceValid;
}


bool FFreeDLiveLinkSource::RequestSourceShutdown()
{
	Stop();

	return true;
}
// FRunnable interface

void FFreeDLiveLinkSource::Start()
{
	ThreadName = "FREED UDP Receiver ";
	ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
	Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
}

void FFreeDLiveLinkSource::Stop()
{
	Stopping = true;
}

uint32 FFreeDLiveLinkSource::Run()
{
	TSharedRef<FInternetAddr> Sender = SocketSubsystem->CreateInternetAddr();
	
	while (!Stopping)
	{
		if (Socket->Wait(ESocketWaitConditions::WaitForRead, WaitTime))
		{
			uint32 Size;

			while (Socket->HasPendingData(Size))
			{
				int32 Read = 0;

				if (Socket->RecvFrom(RecvBuffer.GetData(), RecvBuffer.Num(), Read, *Sender))
				{
					if (Read > 0)
					{
						TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ReceivedData = MakeShareable(new TArray<uint8>());
						ReceivedData->SetNumUninitialized(Read);
						memcpy(ReceivedData->GetData(), RecvBuffer.GetData(), Read);
						AsyncTask(ENamedThreads::GameThread, [this, ReceivedData]() { HandleReceivedData(ReceivedData); });
					}
				}
			}
		}
	}
	return 0;
}

/* basic quaternion type- scalar part is last element in array    */
typedef double q_type[4];

/* for accessing the elements of q_type and q_vec_type   */
#define Q_X    0
#define Q_Y    1
#define Q_Z    2
#define Q_W    3

/*****************************************************************************
 *
    q_from_euler - converts 3 euler angles (in radians) to a quaternion

   angles are in radians;  Assumes roll is rotation about X, pitch
   is rotation about Y, yaw is about Z.  Assumes order of
   yaw, pitch, roll applied as follows:

       p' = roll( pitch( yaw(p) ) )

      See comments for q_euler_to_col_matrix for more on this.
 *
 *****************************************************************************/
static void q_from_euler(q_type destQuat, double yaw, double pitch, double roll)
{
    double  cosYaw, sinYaw, cosPitch, sinPitch, cosRoll, sinRoll;
    double  half_roll, half_pitch, half_yaw;


    /* put angles into radians and divide by two, since all angles in formula
     *  are (angle/2)
     */

    half_yaw = yaw / 2.0;
    half_pitch = pitch / 2.0;
    half_roll = roll / 2.0;

    cosYaw = cos(half_yaw);
    sinYaw = sin(half_yaw);

    cosPitch = cos(half_pitch);
    sinPitch = sin(half_pitch);

    cosRoll = cos(half_roll);
    sinRoll = sin(half_roll);


    destQuat[Q_X] = sinRoll * cosPitch * cosYaw - cosRoll * sinPitch * sinYaw;
    destQuat[Q_Y] = cosRoll * sinPitch * cosYaw + sinRoll * cosPitch * sinYaw;
    destQuat[Q_Z] = cosRoll * cosPitch * sinYaw - sinRoll * sinPitch * cosYaw;

    destQuat[Q_W] = cosRoll * cosPitch * cosYaw + sinRoll * sinPitch * sinYaw;

}  /* q_from_euler */


void FFreeDLiveLinkSource::HandleReceivedData(TSharedPtr<TArray<uint8>, ESPMode::ThreadSafe> ReceivedData)
{
    int i = 0;
    uint8 buf[1024];
    FreeD_D1_t data;

    for (uint8& Byte : *ReceivedData.Get())
    {
        buf[i++] = Byte;
    }

    if (FreeD_D1_unpack(buf, i, &data))
        return;

    FVector tLocation = FVector(data.X / 10.0, data.Y / 10.0, data.Z / 10.0);

    q_type d_quat;
    q_from_euler(d_quat,
        PI * data.Pan / 180.0,
        PI * data.Tilt / 180.0,
        PI * data.Roll / 180.0
        );
    FQuat tQuat;
    tQuat.X = d_quat[Q_X];
    tQuat.Y = d_quat[Q_Y];
    tQuat.Z = d_quat[Q_Z];
    tQuat.W = d_quat[Q_W];

    FVector tScale = FVector(1.0, 1.0, 1.0);
    FTransform tTransform = FTransform(tQuat.Inverse(), tLocation, tScale);

    FString s = FString::FromInt(data.ID);
    s += "@";
    s += DeviceEndpoint.ToString();
    FName SubjectName(s);

    FLiveLinkStaticDataStruct TransformStaticDataStruct = FLiveLinkStaticDataStruct(FLiveLinkTransformStaticData::StaticStruct());
    FLiveLinkTransformStaticData& TransformStaticData = *TransformStaticDataStruct.Cast<FLiveLinkTransformStaticData>();
    Client->PushSubjectStaticData_AnyThread({ SourceGuid, SubjectName }, ULiveLinkTransformRole::StaticClass(), MoveTemp(TransformStaticDataStruct));

    FLiveLinkFrameDataStruct TransformFrameDataStruct = FLiveLinkFrameDataStruct(FLiveLinkTransformFrameData::StaticStruct());
    FLiveLinkTransformFrameData& TransformFrameData = *TransformFrameDataStruct.Cast<FLiveLinkTransformFrameData>();
    TransformFrameData.Transform = tTransform;
    Client->PushSubjectFrameData_AnyThread({ SourceGuid, SubjectName }, MoveTemp(TransformFrameDataStruct));

#if 0
    /*
        FLiveLinkCameraFrameData
        https://docs.unrealengine.com/en-US/API/Runtime/LiveLinkInterface/Roles/FLiveLinkCameraFrameData/index.html
    */

    FLiveLinkFrameDataStruct CameraFrameDataStruct = FLiveLinkFrameDataStruct(FLiveLinkCameraFrameData::StaticStruct());
    FLiveLinkCameraFrameData& CameraFrameData = *CameraFrameDataStruct.Cast<FLiveLinkCameraFrameData>();
    CameraFrameData.Transform = tTransform;
    Client->PushSubjectFrameData_AnyThread({ SourceGuid, "CAMERA" }, MoveTemp(CameraFrameDataStruct));
#endif

}

#undef LOCTEXT_NAMESPACE
