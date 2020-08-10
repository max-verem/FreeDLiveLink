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

    memset(current_data, 0, sizeof(current_data));
    memset(current_cnt, 0, sizeof(current_cnt));

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
    Stopping = false;

    if (Thread == nullptr)
    {
        ThreadName = "FREED UDP Receiver ";
        ThreadName.AppendInt(FAsyncThreadIndex::GetNext());
        Thread = FRunnableThread::Create(this, *ThreadName, 128 * 1024, TPri_AboveNormal, FPlatformAffinity::GetPoolThreadMask());
    }
}

void FFreeDLiveLinkSource::Stop()
{
    Stopping = true;

    if (Thread != nullptr)
    {
        Thread->WaitForCompletion();
        Thread = nullptr;
    }
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
                        FreeD_D1_t data;

                        if (!FreeD_D1_unpack(RecvBuffer.GetData(), Read, &data))
                        {
                            FScopeLock lock(&currentLock);
                            current_data[data.ID] = data;
                            current_cnt[data.ID]++;
                        }
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

void FFreeDLiveLinkSource::UpdateData(FreeD_D1_t& data)
{
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
}

void FFreeDLiveLinkSource::Update()
{
    int i;

    for (i = 0; i < 256; i++)
    {
        FreeD_D1_t data;

        if (current_cnt[i])
        {
            FScopeLock lock(&currentLock);
            data = current_data[i];
        }
        else
            continue;

        UpdateData(data);
    }
}
#undef LOCTEXT_NAMESPACE
