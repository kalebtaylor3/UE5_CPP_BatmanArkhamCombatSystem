// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AttackLandEnemyAnimNotify.generated.h"

/**
 * 
 */

DECLARE_MULTICAST_DELEGATE(FOnNotifiedSignatureOne);

UCLASS()
class ALSV4_CPP_API UAttackLandEnemyAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Fires whenever this notify fires in ANY montage */
	static FOnNotifiedSignatureOne OnNotified;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	
};
