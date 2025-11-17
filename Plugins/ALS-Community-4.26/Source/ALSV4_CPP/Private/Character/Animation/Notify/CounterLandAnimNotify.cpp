// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Animation/Notify/CounterLandAnimNotify.h"

FOnNotifiedSignature UCounterLandAnimNotify::OnNotified;

void UCounterLandAnimNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	// broadcast to all listeners
	OnNotified.Broadcast();

	// always call super last
	Super::Notify(MeshComp, Animation, EventReference);
}