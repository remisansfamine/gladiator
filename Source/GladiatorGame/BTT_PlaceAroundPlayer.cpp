// Fill out your copyright notice in the Description page of Project Settings.


#include "BTT_PlaceAroundPlayer.h"
#include "AIC_Enemy.h"
#include "PlayerCharacter.h"
#include "EnemyCharacter.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Math/UnrealMathUtility.h"
#include "BTD_CheckPlacing.h"

UBTT_PlaceAroundPlayer::UBTT_PlaceAroundPlayer(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = TEXT("Place Around Player");
}

FVector ProjectPointOnNavigableLocation(FVector desiredLocation, APawn* enemyPawn)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(enemyPawn->GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("NavSys Failed"));
		return FVector::ZeroVector;
	}

	const FNavAgentProperties& AgentProps = enemyPawn->GetNavAgentPropertiesRef();
	FNavLocation target;

	NavSys->ProjectPointToNavigation(desiredLocation, target, INVALID_NAVEXTENT, &AgentProps);

	return target.Location;
}


FVector UBTT_PlaceAroundPlayer::GetPointRadiusOnNavigableLocation(FVector originLocation, float radius, APawn* enemyPawn)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(enemyPawn->GetWorld());
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("NavSys Failed"));
	}
	FNavLocation target;

	NavSys->GetRandomReachablePointInRadius(originLocation, radius, target);
	UE_LOG(LogTemp, Warning, TEXT("target location = (%f,%f,%f)"), target.Location.X, target.Location.Y, target.Location.Z);

	return target.Location;
}

FVector GetRandomPointInSemiTorus(float radiusMin, float radiusMax, FVector unitAxisB)
{

	float randomAngle = FMath::RandRange(0.f, 6.28f);

	float cX = FMath::Sin(randomAngle);
	float cY = FMath::Cos(randomAngle);

	FVector ringPos = FVector(cX, cY, 0);
	ringPos *= FMath::RandRange(radiusMin, radiusMax);

	FVector result = (ringPos);
	result.Y = FMath::Abs(result.Y);

	FVector unitAxisA(0, 1, 0);
	FQuat quaternionResult = FQuat::FindBetweenVectors(unitAxisA, unitAxisB);
	
	return quaternionResult.RotateVector(result);

	//return result;
}

EBTNodeResult::Type UBTT_PlaceAroundPlayer::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	float safePlayerDistanceMin = OwnerComp.GetBlackboardComponent()->GetValueAsFloat("safePlayerDistanceMin");
	float safePlayerDistanceMax = OwnerComp.GetBlackboardComponent()->GetValueAsFloat("safePlayerDistanceMax");

	const AAIController* cont = OwnerComp.GetAIOwner();

	APawn* enemyPawn = cont->GetPawn();

	const AEnemyCharacter* enemyCharacter = Cast<AEnemyCharacter>(enemyPawn);
	AAIController* enemyController = Cast<AAIController>(enemyCharacter->GetController());

	const APlayerCharacter* playerCharacter = Cast<APlayerCharacter>(OwnerComp.GetBlackboardComponent()->GetValueAsObject("PlayerActor"));

	FVector enemyLocation = enemyPawn->GetActorLocation();
	FVector playerLocation = playerCharacter->GetActorLocation();
	FVector playerEnemyDir = enemyLocation - playerLocation;
	playerEnemyDir.Normalize();

	bool result = false;
	int iteration = 0;
	FVector projectedLocation;

	while (!result && iteration < 10)
	{
		iteration++;

		FVector randomLocation = GetRandomPointInSemiTorus(safePlayerDistanceMin,
			safePlayerDistanceMax, playerEnemyDir);

		projectedLocation = ProjectPointOnNavigableLocation(randomLocation + playerLocation, enemyPawn);

		float dist = FVector::Dist(projectedLocation, playerLocation);
		if (dist < safePlayerDistanceMin || dist > safePlayerDistanceMax)
			continue;

		if (checkIfPawnEnemyIsFront(projectedLocation, playerLocation, enemyPawn))
			continue;
		
		if (checkIfPawnIsInSphere(enemyCharacter->wantedRoomRadius, projectedLocation, enemyPawn))
			continue;

		result = true;
	}

	enemyController->MoveToLocation(projectedLocation);

	OwnerComp.GetBlackboardComponent()->SetValueAsEnum("MovingState", 3);
	OwnerComp.GetBlackboardComponent()->SetValueAsVector("currentTarget", projectedLocation);

	return EBTNodeResult::Succeeded;

}
