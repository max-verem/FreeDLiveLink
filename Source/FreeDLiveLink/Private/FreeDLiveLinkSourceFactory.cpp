// Fill out your copyright notice in the Description page of Project Settings.

#include "FreeDLiveLinkSourceFactory.h"
#include "FreeDLiveLinkSource.h"
#include "FreeDLiveLink.h"

#define LOCTEXT_NAMESPACE "UFreeDLiveLinkSourceFactory"

FText UFreeDLiveLinkSourceFactory::GetSourceDisplayName() const
{
    return LOCTEXT("SourceDisplayName", "FREED LiveLink");
}

FText UFreeDLiveLinkSourceFactory::GetSourceTooltip() const
{
    return LOCTEXT("SourceTooltip", "Creates a connection to a FREED UDP Stream");
}

TSharedPtr<SWidget> UFreeDLiveLinkSourceFactory::BuildCreationPanel(FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
    return SNew(FreeDLiveLinkSourceEditor)
        .OnOkClicked(FreeDLiveLinkSourceEditor::FOnOkClicked::CreateUObject(this, &UFreeDLiveLinkSourceFactory::OnOkClicked, InOnLiveLinkSourceCreated));
}

TSharedPtr<ILiveLinkSource> UFreeDLiveLinkSourceFactory::CreateSource(const FString& InConnectionString) const
{
    FIPv4Endpoint DeviceEndPoint;
    if (!FIPv4Endpoint::Parse(InConnectionString, DeviceEndPoint))
    {
        return TSharedPtr<ILiveLinkSource>();
    }

    return MakeShared<FFreeDLiveLinkSource>(DeviceEndPoint);
}

void UFreeDLiveLinkSourceFactory::OnOkClicked(FIPv4Endpoint InEndpoint, FOnLiveLinkSourceCreated InOnLiveLinkSourceCreated) const
{
    InOnLiveLinkSourceCreated.ExecuteIfBound(MakeShared<FFreeDLiveLinkSource>(InEndpoint), InEndpoint.ToString());
}

#undef LOCTEXT_NAMESPACE
