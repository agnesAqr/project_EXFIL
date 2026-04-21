// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Project_EXFILCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

UCLASS(abstract)
class AProject_EXFILCharacter : public ACharacter
{
	GENERATED_BODY()

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:

	
	AProject_EXFILCharacter();	

protected:

	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	
	void Move(const FInputActionValue& Value);

	
	void Look(const FInputActionValue& Value);

public:

	
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

public:

	
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

