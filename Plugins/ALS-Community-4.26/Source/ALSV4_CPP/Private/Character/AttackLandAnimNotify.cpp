// AttackLandAnimNotify.cpp

#include "Character/AttackLandAnimNotify.h"
#include <Character/ALSBaseCharacter.h>

// define the static delegate
FOnNotifiedSignature UAttackLandAnimNotify::OnNotified;

void UAttackLandAnimNotify::Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	// broadcast to all listeners
	OnNotified.Broadcast();

	// always call super last
	Super::Notify(MeshComp, Animation, EventReference);
}
