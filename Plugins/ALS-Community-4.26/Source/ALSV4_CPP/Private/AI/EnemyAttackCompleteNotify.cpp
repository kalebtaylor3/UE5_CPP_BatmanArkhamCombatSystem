// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/EnemyAttackCompleteNotify.h"
#include <AI/BaseEnemyAI.h>

void UEnemyAttackCompleteNotify::Notify(USkeletalMeshComponent* Mesh, UAnimSequenceBase*, const FAnimNotifyEventReference&)
{
    if (Mesh)
    {
        if (ABaseEnemyAI* AI = Cast<ABaseEnemyAI>(Mesh->GetOwner()))
        {
            AI->HandleAttackCompleteNotify();
        }
    }
}

