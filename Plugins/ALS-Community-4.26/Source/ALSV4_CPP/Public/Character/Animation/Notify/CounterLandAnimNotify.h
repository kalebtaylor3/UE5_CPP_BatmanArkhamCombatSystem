// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "CounterLandAnimNotify.generated.h"

/**
 * 
 */
DECLARE_MULTICAST_DELEGATE(FOnNotifiedSignature);
UCLASS()
class ALSV4_CPP_API UCounterLandAnimNotify : public UAnimNotify
{
	GENERATED_BODY()
	
public:
	static FOnNotifiedSignature OnNotified;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
