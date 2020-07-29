// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LiveLinkSourceFactory.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "FreeDLiveLinkSourceFactory.generated.h"

/**
 * 
 */
UCLASS()
class FREEDLIVELINK_API UFreeDLiveLinkSourceFactory : public ULiveLinkSourceFactory
{
public:
    GENERATED_BODY()

    virtual FText GetSourceDisplayName() const override;
    virtual FText GetSourceTooltip() const override;

    virtual EMenuType GetMenuType() const override { return EMenuType::SubPanel; }
    virtual TSharedPtr<SWidget> BuildCreationPanel(FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const override;
    TSharedPtr<ILiveLinkSource> CreateSource(const FString& ConnectionString) const override;
private:
    void OnOkClicked(FIPv4Endpoint Endpoint, FOnLiveLinkSourceCreated OnLiveLinkSourceCreated) const;
};
