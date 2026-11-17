#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GuardAnimationCommandlet.generated.h"

/**
 * 가드 애니메이션 FBX(Claude outputs/AnimSource/A_PlayerGuard_*.fbx)를 Robbie 스켈레톤으로 임포트하고
 * Start → Loop(반복) → End 섹션의 AM_PlayerGuard 몽타주를 만든다. 다시 실행하면 같은 에셋을 갱신한다.
 * 실행: UnrealEditor-Cmd.exe <uproject> -run=GuardAnimation
 */
UCLASS()
class UGuardAnimationCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    virtual int32 Main(const FString& Params) override;
};
