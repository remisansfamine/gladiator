// Copyright Epic Games, Inc. All Rights Reserved.

#include "GladiatorGameCharacter.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "LifeComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

AGladiatorGameCharacter::AGladiatorGameCharacter()
	: Super()
{
	bUseControllerRotationRoll = bUseControllerRotationPitch = bUseControllerRotationYaw = false;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("PawnIgnoreCam"));

	GetMesh()->SetCollisionProfileName(TEXT("CharacterMeshIgnoreCam"));
	GetMesh()->SetVectorParameterValueOnMaterials("FlickerColor", FVector(0.f, 0.f, 0.f));

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 250.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	cameraBoomComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	cameraBoomComp->SetupAttachment(RootComponent);
	cameraBoomComp->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	cameraBoomComp->bUsePawnControlRotation = true; // Rotate the arm based on the controller
	DeactivateCamera();

	// Create a follow camera
	followCameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	followCameraComp->SetupAttachment(cameraBoomComp, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	followCameraComp->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	hammer = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Hammer"));
	hammer->SetupAttachment(GetMesh(), TEXT("WeaponPoint"));

	attackCollider = CreateDefaultSubobject<USphereComponent>(TEXT("WeaponCollider"));
	attackCollider->SetupAttachment(hammer, TEXT("ColliderSocket"));
	attackCollider->SetGenerateOverlapEvents(true);

	shield = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Shield"));
	shield->SetupAttachment(GetMesh(), TEXT("DualWeaponPoint"));

	healthComponent = CreateDefaultSubobject<ULifeComponent>(TEXT("LifeComp"));
}

void AGladiatorGameCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (attackCollider)
	{
		attackCollider->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		attackCollider->OnComponentBeginOverlap.AddDynamic(this, &AGladiatorGameCharacter::OverlapCallback);
	}

	if (healthComponent)
	{
		healthComponent->OnHurt.AddDynamic(this, &AGladiatorGameCharacter::OnHurt);
		healthComponent->OnKill.AddDynamic(this, &AGladiatorGameCharacter::OnDeath);
		healthComponent->OnInvicibilityStop.AddDynamic(this, &AGladiatorGameCharacter::OnInvicibilityStop);
	}
}

void AGladiatorGameCharacter::OverlapCallback(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OverlappedComp || !OtherActor || OtherActor == this)
		return;

	AGladiatorGameCharacter* other = Cast<AGladiatorGameCharacter>(OtherActor);

	if (other)
		other->TakeDamage(1, GetActorLocation());
}

void AGladiatorGameCharacter::TakeDamage(int damage, const FVector& senderPosition)
{
	if (characterState != ECharacterState::DEFENDING)
	{
		healthComponent->Hurt(1);
		return;
	}

	DefendOff();

	FVector senderDirection = (senderPosition - GetActorLocation()).GetSafeNormal();

	if (FVector::DotProduct(senderDirection, GetActorForwardVector()) > 0.25f)
	{
		setCameraShake(camShake, 1.f);
		return;
	}

	healthComponent->Hurt(1);
}

void AGladiatorGameCharacter::SetAttackState(bool attacking)
{
	if (hammer)
		hammer->SetVectorParameterValueOnMaterials("FlickerColor", attacking * FVector(0.9f, 0.f, 0.f));

	attackCollider->SetCollisionEnabled(attacking ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AGladiatorGameCharacter::ActivateCamera() 
{ 
	cameraBoomComp->Activate();
}

void AGladiatorGameCharacter::DeactivateCamera() 
{ 
	cameraBoomComp->Deactivate();
}

void AGladiatorGameCharacter::OnInvicibilityStop()
{
	GetMesh()->SetVectorParameterValueOnMaterials("FlickerColor", FVector(0.f, 0.f, 0.f));
}

void AGladiatorGameCharacter::setCameraShake(const TSubclassOf<UCameraShakeBase>& shakeClass, float scale)
{
	if (shakeClass)
		GetWorld()->GetFirstPlayerController()->PlayerCameraManager->StartCameraShake(shakeClass, scale);
}

void AGladiatorGameCharacter::OnHurt()
{
	GetMesh()->SetVectorParameterValueOnMaterials("FlickerColor", FVector(1.f, 0.f, 0.f));

	setCameraShake(camShake, 0.75f);
}

void AGladiatorGameCharacter::OnDeath()
{
	characterState = ECharacterState::DEAD;

	setCameraShake(camShake, 1.25f);

	SetAttackState(false);
	SetState(ECharacterState::IDLE);

	GetCharacterMovement()->Deactivate();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetSimulatePhysics(false);
	GetCapsuleComponent()->SetEnableGravity(false);

	GetMesh()->SetCollisionProfileName(TEXT("RagdollIgnoreCam"));
	GetMesh()->SetSimulatePhysics(true);

	hammer->DetachFromParent(true);
	hammer->SetSimulatePhysics(true);
	hammer->SetCollisionProfileName(TEXT("Props"));

	shield->DetachFromParent(true);
	shield->SetSimulatePhysics(true);
	shield->SetCollisionProfileName(TEXT("Props"));
}

void AGladiatorGameCharacter::SetState(ECharacterState state)
{
	characterState = state;

	if (OnStateChanged.IsBound())
		OnStateChanged.Broadcast(state);
}

void AGladiatorGameCharacter::Attack()
{
	if (!canAttack())
		return;

	SetState(ECharacterState::ATTACKING);
}

void AGladiatorGameCharacter::DefendOn()
{
	SetState(ECharacterState::DEFENDING);
}

void AGladiatorGameCharacter::DefendOff()
{
	SetState(ECharacterState::IDLE);
}

void AGladiatorGameCharacter::Idle()
{
	SetState(ECharacterState::IDLE);
}

void AGladiatorGameCharacter::MoveForward(float Value)
{
	Move(EAxis::X, Value);
}

void AGladiatorGameCharacter::MoveRight(float Value)
{
	Move(EAxis::Y, Value);
}

void AGladiatorGameCharacter::Move(EAxis::Type axis, float value)
{
	if (!canMove() || !Controller || value == 0.0f )
		return;

	// find out which way is right
	const FRotator rot = Controller->GetControlRotation();
	const FRotator yawRot(0, rot.Yaw, 0);

	// get right vector and add movement in that direction
	const FVector dir = FRotationMatrix(yawRot).GetUnitAxis(axis);
	Move(dir, value);
}

void AGladiatorGameCharacter::Move(const FVector& direction, float value)
{
	if (!canMove() || value == 0.0f)
		return;

	AddMovementInput(direction, value);
}

AGladiatorGameCharacter* AGladiatorGameCharacter::GetOtherGladiator(float minDistance, float maxDistance)
{
	float minDistSquared = minDistance * minDistance;
	float maxDistSquared = maxDistance * maxDistance;

	FVector currentLocation = GetActorLocation();

	TArray<AActor*> gladiators;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGladiatorGameCharacter::StaticClass(), gladiators);

	TArray<AGladiatorGameCharacter*> validGladiators;
	for (AActor* actor : gladiators)
	{
		if (actor == this)
			continue;
		
		float distSquared = GetSquaredDistanceTo(actor);

		if (GetSquaredDistanceTo(actor) < minDistSquared || GetSquaredDistanceTo(actor) > maxDistSquared)
			continue;

		AGladiatorGameCharacter* gladiator = Cast<AGladiatorGameCharacter>(actor);

		if (!gladiator->isAlive())
			continue;

		validGladiators.Add(gladiator);
	}

	if (validGladiators.Num() == 0)
		return nullptr;

	validGladiators.Sort([this](const AGladiatorGameCharacter& A, const AGladiatorGameCharacter& B)
	{
		float distA = GetDistanceTo(&A);
		float distB = GetDistanceTo(&B);
		return distA < distB;
	});

	return validGladiators[0];
}

void AGladiatorGameCharacter::LookAtTarget(AActor* target, float lookSpeed)
{
	FVector toLookAt = target->GetActorLocation() - GetActorLocation();
	FRotator lookAtRot = toLookAt.Rotation();

	FRotator initialActorRot = GetActorRotation(), actorLookAt = initialActorRot;
	FRotator initialControllerRot = Controller->GetControlRotation(), controllerLookAt = initialControllerRot;

	controllerLookAt.Yaw = actorLookAt.Yaw = lookAtRot.Yaw;

	actorLookAt = UKismetMathLibrary::RInterpTo(initialActorRot, actorLookAt, GetWorld()->GetDeltaSeconds(), lookSpeed);
	controllerLookAt = UKismetMathLibrary::RInterpTo(controllerLookAt, controllerLookAt, GetWorld()->GetDeltaSeconds(), lookSpeed);

	SetActorRotation(actorLookAt);
	Controller->SetControlRotation(controllerLookAt);
}