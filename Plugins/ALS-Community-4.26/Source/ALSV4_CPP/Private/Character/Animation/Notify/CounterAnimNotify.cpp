// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Animation/Notify/CounterAnimNotify.h"

FOnNotifiedSignature UCounterAnimNotify::OnNotified;

void UCounterAnimNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// broadcast to all listeners
	OnNotified.Broadcast();

	// always call super last
	Super::Notify(MeshComp, Animation, EventReference);
}
