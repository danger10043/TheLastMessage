#pragma once
#include "Animation/AnimMontage.h"

namespace WeaponAttackAnimation
{
// A release marker splits an existing attack, so normal playback must cross it.
inline FName GetContinuation(const UAnimMontage* Montage, FName Section, FName Release)
{
    if (!Montage || Release.IsNone() || Section == Release) return NAME_None;
    const int32 Index = Montage->GetSectionIndex(Section);
    return Index != INDEX_NONE && Index + 1 < Montage->GetNumSections()
        && Montage->GetSectionName(Index + 1) == Release ? Release : NAME_None;
}
}
