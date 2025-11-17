// Fill out your copyright notice in the Description page of Project Settings.


#include "AI/BaseEnemyAI.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"              // for FNavPathSharedPtr
#include "Navigation/PathFollowingComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h" // for SimpleMoveToLocation (optional)
#include "AI/HitEndAnimNotify.h"         // our new notify
#include "Components/SkeletalMeshComponent.h"
#include <Kismet/GameplayStatics.h>
#include <Character/Animation/Notify/AttackLandEnemyAnimNotify.h>
#include "Character/CombatSystem.h"
const FName ABaseEnemyAI::PelvisBoneName(TEXT("pelvis"));


void ABaseEnemyAI::BeginPlay()
{
    Super::BeginPlay();
    InitAnimNotifies();
    CacheAIRefs();
}

void ABaseEnemyAI::CacheAIRefs()
{
    CachedAIController = Cast<AAIController>(GetController());

    BrainComp = nullptr;
    BTComp = nullptr;

    if (CachedAIController)
    {
        BrainComp = CachedAIController->GetBrainComponent();
        BTComp = Cast<UBehaviorTreeComponent>(BrainComp);
    }
}


void ABaseEnemyAI::StopAILogic(const FString& Reason)
{
    if (bAILogicStopped) return;
    bAILogicStopped = true;

    // Stop BT/brain first so tasks/services/timers stop ticking
    if (BrainComp)
    {
        BrainComp->StopLogic(Reason);
    }

    // Hard stop any movement
    if (CachedAIController)
    {
        CachedAIController->StopMovement();
    }

    // Example: disable movement/collision if you're “dead”
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->DisableMovement();
    }
}


void ABaseEnemyAI::GetHit(const FVector& HitFromLocation, int32 PlayerComboCount)
{
    if (bAttacking)
    {
        bAttacking = false;
        OnAttackComplete.Broadcast(this);
    }

    // 1) reset our “combo window” timer
    GetWorld()->GetTimerManager().ClearTimer(HitResetTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(
        HitResetTimerHandle,
        this,
        &ABaseEnemyAI::ResetHitTimes,
        HitResetDelay,
        false
    );

    // 2) increment hit‐count
    HitTimes++;

    // 3) play impact montage
    if (HitMontage)
        Replicated_PlayMontage(HitMontage, 1.0f);

    // 4) figure how many hits we should require before ragdoll
    int32 RequiredHits = 3;
    if (PlayerComboCount >= 10)
        RequiredHits = 1;
    else if (PlayerComboCount >= 5)
        RequiredHits = 2;

    // 5) non‐lethal
    if (HitTimes < RequiredHits)
    {
        SetMovementState(EALSMovementState::Attacking);
        LaunchCharacter((GetActorLocation() - HitFromLocation).GetSafeNormal() * KnockbackStrength +
            FVector(0, 0, KnockbackUpStrength),
            true, true);

        // recover in 0.5s
        GetWorld()->GetTimerManager().SetTimer(
            RecoveryTimer,
            this,
            &ABaseEnemyAI::Recover,
            0.5f,
            false
        );
    }
    else
    {
        RagdollStart();
        FTimerHandle Th;
        GetWorld()->GetTimerManager().SetTimer(
            Th, this, &ABaseEnemyAI::ApplyDeferredRagdollImpulse, 0.01f, false
        );
        GetWorld()->GetTimerManager().SetTimer(
            RagdollEndTimerHandle, this, &ABaseEnemyAI::EndRagDoll, RagdollDuration, false
        );
        HitTimes = 0;
    }
}

void ABaseEnemyAI::ResetHitTimes()
{
    HitTimes = 0;
}

void ABaseEnemyAI::Recover()
{
    SetMovementState(EALSMovementState::Grounded);
}

void ABaseEnemyAI::ApplyDeferredRagdollImpulse()
{
    if (USkeletalMeshComponent* Skel = GetMesh())
    {
        // ensure all bodies simulate & have gravity
        Skel->SetAllBodiesBelowSimulatePhysics(PelvisBoneName, true, false);
        Skel->SetSimulatePhysics(true);
        Skel->WakeAllRigidBodies();
        Skel->SetEnableGravity(true);

        // apply the impulse right at the pelvis bone
        Skel->AddImpulseAtLocation(PendingImpulse * 80, Skel->GetBoneLocation(PelvisBoneName), PelvisBoneName);

        // clear out so we don’t reapply
        PendingImpulse = FVector::ZeroVector;
    }
}

void ABaseEnemyAI::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // if we can’t move, bail
    if (!bCanMove)
        return;

    const bool ShouldShow = CanPlayerCounter();
    if (ShouldShow)
    {
        if (!CounterEffectComponent && CounterEffectTemplate)
        {
            CounterEffectComponent = UGameplayStatics::SpawnEmitterAttached(
                CounterEffectTemplate,
                GetMesh(),
                TEXT("headSocket"),
                FVector::ZeroVector,
                FRotator::ZeroRotator,    // no rotation tweak anymore
                EAttachLocation::SnapToTarget,
                true                      // auto destroy
            );
            // **apply your scale offset here**
            CounterEffectComponent->SetRelativeScale3D(CounterEffectScale);
        }
    }
    else if (CounterEffectComponent)
    {
        CounterEffectComponent->DeactivateSystem();
        CounterEffectComponent->DestroyComponent();
        CounterEffectComponent = nullptr;
    }


    // if we’re doing a normal MoveToPosition
    if (bMovingToTarget)
    {
        FVector Current = GetActorLocation();
        FVector Dir = MoveTarget - Current;
        Dir.Z = 0.f;

        const float DistSq = Dir.SizeSquared();
        if (DistSq <= FMath::Square(AcceptanceRadius))
        {
            bMovingToTarget = false;
        }
        else
        {
            Dir.Normalize();
            AddMovementInput(Dir, 1.f);
        }

        // don’t fall through into the attack code
        return;
    }

    // if we’re in the “approach + attack” phase
    if (bIsApproaching)
    {

        //if approaching they can be countered.. there for we need to add a particle effect (cascade particle system) that attached to a socket above the head called "headSocket" and basically when approaching and the player can counter this needs to be visible and active. if not make it go away. get me??

        APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
        if (!Player)
        {
            bIsApproaching = false;
            return;
        }

        FVector ToPlayer = (Player->GetActorLocation() - GetActorLocation());
        ToPlayer.Z = 0.f;
        float DistSq2 = ToPlayer.SizeSquared();

        // still out of range? keep approaching
        if (DistSq2 > FMath::Square(AttackDistance))
        {
            AddMovementInput(ToPlayer.GetSafeNormal(), 1.f);
        }
        else
        {
            // we’re in range → attack!
            bIsApproaching = false;
            PerformAttackTurn();
        }
    }
}

void ABaseEnemyAI::HandleAttackCompleteNotify()
{
    if (bAttacking)
    {
        bAttacking = false;
        OnAttackComplete.Broadcast(this);
    }
}


void ABaseEnemyAI::StartApproachAttack()
{
    if (!bCanMove) return;
    bIsApproaching = true;
    bMovingToTarget = false;      // cancel any previous move
    SetMovementState(EALSMovementState::Grounded);
}

// BaseEnemyAI.cpp additions
void ABaseEnemyAI::MoveToPosition(const FVector& TargetPos)
{

    if (!bCanMove)
        return;

    // 1) record target
    MoveTarget = TargetPos;
    bMovingToTarget = true;

    // 2) switch into ALS walk/run state
    SetMovementState(EALSMovementState::Grounded);
}

void ABaseEnemyAI::PerformAttackTurn()
{
    // play one of AttackMontages, then on end notify the group to advance turn
    if (AttackMontages.Num())
    {
        bAttacking = true;
        UAnimMontage* M = AttackMontages[FMath::RandRange(0, AttackMontages.Num() - 1)];
        Replicated_PlayMontage(M, 1.f);
        // bind to montage end, then call OwningGroup->AdvanceAttackTurn()
    }
}

void ABaseEnemyAI::InitAnimNotifies()
{
    UAttackLandEnemyAnimNotify::OnNotified.AddUObject(this, &ABaseEnemyAI::AttackLanded);
}

void ABaseEnemyAI::AttackLanded()
{
    //here we need to check and see if we are within range of the player if we are cast to the player and call the ReceiveHit
    // 2) Range check
    APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Pawn) return;

    float DistSq = FVector::DistSquaredXY(Pawn->GetActorLocation(), GetActorLocation());
    if (DistSq > FMath::Square(AttackDistance))
    {
        return;
    }

    // 3) Pull off the UCombatSystem component
    if (UCombatSystem* CombatComp = Pawn->FindComponentByClass<UCombatSystem>())
    {
        // call your "receive hit" method on the component
        CombatComp->ReceiveHit(GetActorLocation());
        GetWorld()->GetFirstPlayerController()->PlayerCameraManager->StartCameraShake(CameraShake, 35.0f);
    }
}

void ABaseEnemyAI::EndRagDoll()
{
    if(bAttacking)
        OwningGroup->RequestNextAttacker();

    //// stop ragdoll
    //RagdollEnd();

    //// re‐enable locomotion
    //bCanMove = true;
    //Recover();

    //// let the group know we’re back up
    //NotifyRagdollFinished();

    //// also fire the attack-complete so the turn cycle continues
    //HandleAttackCompleteNotify();
}

void ABaseEnemyAI::NotifyRagdollFinished()
{
    if (OwningGroup)
    {
        OwningGroup->HandleMemberRecovered(this);
    }
}

bool ABaseEnemyAI::CanPlayerCounter() const
{
    // TODO: replace with your real “counter window” logic
    // e.g. return bIsApproaching && (some timer < counterWindow);

    if (bIsApproaching || bAttacking)
        return true;
    else
        return false;
}



