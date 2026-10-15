// Fill out your copyright notice in the Description page of Project Settings.


#include "Weapon/WeaponBase.h"
#include "DataAsset/WeaponDataAsset.h"

UWeaponBase::UWeaponBase()
{}

bool UWeaponBase::Initialize(UWeaponDataAsset* InWeaponData)
{
	if (!IsValid(InWeaponData))
	{
		return false;
	}

	WeaponData = InWeaponData;
	return true;
}
