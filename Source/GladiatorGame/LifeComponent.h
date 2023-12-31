// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LifeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FLifeDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLifeChangedDelegate, int, newLife);

UCLASS( ClassGroup=(Custom), Blueprintable, meta=(BlueprintSpawnableComponent) )
class GLADIATORGAME_API ULifeComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Life, meta = (AllowPrivateAccess = "true"))
	int life;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Life, meta = (AllowPrivateAccess = "true"))
	int maxLife = 5;

	FTimerHandle invicibleTimer;

	bool isInvicible;

	UFUNCTION()
	void ResetInvicibility();

public:	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Life, meta = (AllowPrivateAccess = "true"))
	float invicibleCooldown;

	UFUNCTION(BlueprintCallable)
	void Hurt(int damage);
	
	UFUNCTION(BlueprintCallable)
	void Kill();

	void SetLife(int value);

public:
	int GetLife() { return life; }
	int GetMaxLife() { return maxLife; }

	UFUNCTION(BlueprintCallable)
	float GetLifePercent() { return (float)life / (float)maxLife; }

	UPROPERTY(BlueprintAssignable, Category = "Components|Life")
	FLifeChangedDelegate OnLifeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Components|Life")
	FLifeDelegate OnInvicibilityStop;

	UPROPERTY(BlueprintAssignable, Category="Components|Life")
	FLifeDelegate OnHurt;

	UPROPERTY(BlueprintAssignable, Category = "Components|Life")
	FLifeDelegate OnKill;
};
