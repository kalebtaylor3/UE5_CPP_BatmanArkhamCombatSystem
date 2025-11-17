// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/HitEndAnimNotify.h"
#include "Character/CombatSystem.h"
#include "AI/BaseEnemyAI.h"

void UHitEndAnimNotify::Notify(USkeletalMeshComponent* MeshComp,
    UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp)
    {
        if (ABaseEnemyAI* AI = Cast<ABaseEnemyAI>(MeshComp->GetOwner()))
        {
            AI->bCanMove = true;
        }

        if (UCombatSystem* Player = Cast<UCombatSystem>(MeshComp->GetOwner()))
        {
            Player->Character->SetMovementState(EALSMovementState::Grounded);
            Player->Character->SetOverlayState(EALSOverlayState::Combat);
        }
    }
}