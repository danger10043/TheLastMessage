# BP_DirectionalStairs

에셋: `/Game/Main/Blueprints/Gimmick/BP_DirectionalStairs`

`ADirectionalStairs`를 부모로 사용하는 배치용 BP입니다. LI_14의 계단 12개에 적용했습니다.

- 원본 메시의 위쪽은 로컬 -X, 아래쪽은 로컬 +X입니다. 반대 방향 계단은 Yaw 180도로 배치합니다.
- 위쪽 끝의 아래에서 +X 방향으로 걷거나, 아래에서 위로 점프할 때 해당 캐릭터 캡슐이 이 계단 액터를 이동 중 무시합니다. 위에서 착지할 때는 계단을 밟습니다.
- 계단 위에서 기존 하향 점프 입력(아래 입력 + 점프)을 사용하면 캐릭터가 계단 아래로 내려갑니다. 캡슐 전체가 계단 아래를 벗어나면 충돌을 복구합니다.
- 그래플링이 시작되거나 계단을 벗어나면 예외를 해제합니다. 한 캐릭터의 예외가 다른 캐릭터에 영향을 주지 않습니다.
- 숨겨진 `SolidRamp` 메시의 충돌은 항상 켜져 있습니다. 그래플링, Visibility 원거리 판정, BossBullet을 포함한 모든 채널을 차단합니다.
- BP 액터 전체를 회전/스케일하세요. 개별 StairMesh/SolidRamp 상대 변형은 판정 형상과 어긋날 수 있습니다.
- Smooth ramp가 캐릭터 이동을 담당하고 StairMesh는 시각적 계단입니다. 이 액터에는 기존 SoftCollision 프리셋을 사용하지 마세요.

검증: `TheLastMessage.Gimmick.DirectionalStairs` 자동화 테스트(통과 방향, 아래에서 점프, 위에서 착지 차단, 하향 점프 및 충돌 복구, 훅 제외, 단순/복잡 무기 충돌).
