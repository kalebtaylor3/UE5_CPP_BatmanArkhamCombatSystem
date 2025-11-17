// AttackLandAnimNotify.h

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AttackLandAnimNotify.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnNotifiedSignature);

UCLASS()
class ALSV4_CPP_API UAttackLandAnimNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Fires whenever this notify fires in ANY montage */
	static FOnNotifiedSignature OnNotified;

	virtual void Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference) override;
};
