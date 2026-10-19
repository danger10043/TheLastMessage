# 보스 활성 코어

저장 위치: `/Game/_ExternalAssets/CustomMesh/BossActiveCore`

## 에셋

- `SK_BossActiveCore`: 25본의 리지드 스킨 스켈레탈 메시. 높이 약 236cm. 피벗은 바닥 중앙.
- `SK_BossActiveCore_Skeleton`: 공통 스켈레톤.
- `AN_BossActiveCore_Spawn`: 2.2초. 지하에서 상승 → 지지대 펼침 → 코어 활성화.
- `AN_BossActiveCore_Destroy`: 2.4초. 피격 진동 → 결정 12조각 분리 → 지지대 붕괴 → 결정 및 에너지 소멸. 금속 잔해는 남음.
- `AN_BossActiveCore_Retract`: 2.4초. 코어 축소 → 지지대 접힘 → 지하 복귀.
- `M_BossCore_*`: 금속, 푸른 발광, 반투명 결정, 주황 경고등 등 7종.
- `L_BossActiveCore_Preview`: 최종 활성 자세와 조명·카메라가 배치된 확인용 레벨. 기존 게임 레벨과 별개입니다.

## 블루프린트 연결

1. Actor에 Skeletal Mesh Component를 추가하고 `SK_BossActiveCore`를 지정합니다. Actor 원점은 바닥 높이에 둡니다.
2. Animation Mode는 `Use Animation Asset`으로 설정합니다. 초기 Animation Asset을 `AN_BossActiveCore_Spawn`으로 지정하고 Looping을 끕니다. 스폰 시 첫 프레임부터 지하 상태가 보이도록 초기 애니메이션을 미리 지정합니다.
3. 등장 완료 후 마지막 자세를 유지합니다. 별도 AnimBP를 사용한다면 Spawn의 마지막 프레임을 활성 대기 자세로 사용합니다.
4. 보스 공격으로 파괴가 결정되면 기존 재생을 중단하고 `Play Animation(AN_BossActiveCore_Destroy, false)`을 호출합니다. 피격 판정을 끄고, 종료 후 잔해 유지 또는 Actor 제거를 선택합니다.
5. 제한시간이 끝나면 `Play Animation(AN_BossActiveCore_Retract, false)`를 호출하고 2.4초 후 Actor를 제거합니다.
6. 파괴와 복귀가 동시에 실행되지 않도록 상태값(등장/활성/파괴/복귀)으로 분기합니다.

루트 모션은 비활성화되어 있습니다. 고정된 `root` 아래의 `platform_lift` 본이 지하 이동을 담당하므로 Actor 위치를 함께 내리지 않습니다.

공격 판정에는 Actor의 별도 Sphere Collision을 권장합니다. `core_float` 본 위치를 기준으로 반경 약 43cm로 설정하고, 보스 공격에만 반응하도록 게임의 충돌 채널에 연결합니다. 스킨 메시의 물리 시뮬레이션은 사용하지 않습니다.

이 작업은 메시·스켈레톤·애니메이션·머터리얼 제작입니다. 기존 보스의 피해 처리, 타이머, 실드 해제 로직은 수정하지 않았습니다. 바닥 파편·먼지·연기·사운드는 별도 VFX/SFX로 추가할 수 있습니다. 메시가 바닥을 실제로 변형시키지는 않습니다.

## 외형 조정

- `M_BossCore_Cyan`, `M_BossCore_Crystal`의 `CycleSeconds`: 하늘색/파란색 왕복 주기. 기본 4초.
- `GlowStrength`: 발광 강도. 화면의 번짐은 레벨의 Bloom/노출 설정에도 영향을 받습니다.
- `M_BossCore_Crystal`의 `Opacity`: 기본 0.20. 여러 결정 면을 겹쳐 보는 구조라 투명도를 낮춰 내부를 드러냈습니다.
- `Tint`, `BlueTint`: 색상.

## 소스와 확인 자료

- `SK_BossActiveCore.glb`: 메시, 본, 3개 애니메이션이 포함된 재사용 가능한 원본.
- `BossActiveCore_SourcePreview.png`, `BossActiveCore_AnimationContactSheet.png`, `BossActiveCore_Animations.gif`: 원본 지오메트리의 간이 렌더링. Unreal 최종 라이팅과 차이가 있습니다.
- `source_report.json`, `unreal_report.json`, `animation_validation.json`: 제작 및 Unreal 재로딩/포즈 확인 기록.
- `UnrealPreview/`: Unreal에서 렌더링한 자세별 이미지.

컨셉 아트를 바탕으로 절차적으로 모델링한 게임용 버전입니다. 아트 이미지의 표면 스크래치나 바닥 파편 텍스처는 포함하지 않습니다.
