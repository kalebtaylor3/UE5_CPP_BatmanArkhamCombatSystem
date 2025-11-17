// EnemyGroupController.cpp

#include "AI/EnemyGroupController.h"
#include "AI/EnemyGroup.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "AI/BaseEnemyAI.h"
#include "EngineUtils.h"
#include "TimerManager.h"  // for ClearAllTimersForObject

AEnemyGroupController::AEnemyGroupController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AEnemyGroupController::BeginPlay()
{
    Super::BeginPlay();

    // Auto-find all groups in the level
    for (TActorIterator<AEnemyGroup> It(GetWorld()); It; ++It)
    {
        AEnemyGroup* G = *It;
        if (G)
        {
            Groups.Add(G);
            G->OwningController = this;  // if you expose that pointer in EnemyGroup
        }
    }
}

void AEnemyGroupController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    MergeGroupsIfNeeded();
}

void AEnemyGroupController::NotifyGroupEngaged(AEnemyGroup* EngagedGroup)
{
    // You could immediately attempt merges here too
    MergeGroupsIfNeeded();
}

void AEnemyGroupController::MergeGroupsIfNeeded()
{
    const float MergeDistanceSq = FMath::Square(1000.f); // tweak as needed

    for (int32 i = 0; i < Groups.Num(); ++i)
    {
        AEnemyGroup* A = Groups[i];
        if (!A || !A->bIsEngaged) continue;

        for (int32 j = i + 1; j < Groups.Num(); ++j)
        {
            AEnemyGroup* B = Groups[j];
            if (!B || !B->bIsEngaged) continue;

            // If their centroids are close enough, merge B into A
            if ((A->GetActorLocation() - B->GetActorLocation()).SizeSquared() <= MergeDistanceSq)
            {
                MergeGroupInto(A, B);
                // restart scanning since Groups changed
                return;
            }
        }
    }
}

void AEnemyGroupController::MergeGroupInto(AEnemyGroup* A, AEnemyGroup* B)
{
    if (!A || !B || A == B) return;

    // 1) Stop B from ever firing its own queue again:
    B->Disengage();
    B->SetActorTickEnabled(false);
    GetWorld()->GetTimerManager().ClearAllTimersForObject(B);

    // 2) Move B’s soldiers into A
    for (ABaseEnemyAI* E : B->Members)
    {
        if (!E) continue;
        E->OnAttackComplete.RemoveAll(B);
        E->bIsApproaching = false;
        E->bMovingToTarget = false;
        E->bAttacking = false;
        E->bCanMove = true;
        E->OwningGroup = A;
        A->Members.AddUnique(E);
    }
    B->Members.Empty();

    // 3) Rebuild A’s formation
    A->RebuildFormationOffsets();

    // 4) Clean up B
    Groups.Remove(B);
    B->Destroy();
}

