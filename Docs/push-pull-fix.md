# 밀기·당기기 공통 이동과 잡기 자세

2026-11-02, master `5107f5b9` 통합.

## 이동과 입력

master의 공통 이동 계산을 사용한다. `PlayerBase.DoMove`가 `SetMoveInput`으로 입력을 전달하고, 조작 중에는 일반 CharacterMovement 갱신을 멈춘다. 플레이어와 상자의 경로·바닥·경사·턱을 함께 검사한 뒤 둘 다 가능한 이동량을 적용한다. 점프·피격·낙하 등으로 조작이 끝나면 원래 CharacterMovement와 속도 제한을 복구한다.

기존 로컬의 물리 속도 직접 제어, 별도의 상자 추종 이동, 자유 물리 제약과 MaxWalkSpeed=0 처리는 제거했다. 렌더링에 쓰는 실제 상자·손 접촉점이 지면 물리 보정과 엇갈리지 않도록 공통 이동은 PostPhysics, 메시의 애니메이션 갱신은 그 뒤에 수행한다. 애니메이션용 GetFollowSpeedX는 공통 이동의 실제 속도를 반환한다.

## 부드러운 접근과 거리

master의 단순 콜리전 표면 조회와 경로 검사를 이용해 목적지를 계산한다. GrabDistance 설정은 캡슐 충돌 여유와 팔의 자연스러운 도달 범위 안으로 제한한다. 원거리에서는 거리에 따라 0.2~0.5초 동안 smoothstep으로 접근하며 매 프레임 경로·바닥을 재검사한다. 접근 중 상자는 움직이지 않고, 캐릭터는 걷기 동작을 사용한다. 접근이 끝나면 공통 이동용 유지 간격을 확정하고 잡기 동작과 IK를 활성화한다. 경로가 막히면 종료한다.

## 양손과 자세

- Robbie_01_AnimBP 출력의 양팔 Two Bone IK를 유지한다. 팔 길이를 늘리지 않고 실제 상자 표면의 접촉점을 따라간다.
- 잡는 높이는 양어깨 평균보다 12cm 아래를 기준으로 상자 범위 안에 제한한다. 양손 폭은 어깨 폭과 상자 깊이에 맞춘다.
- 팔꿈치는 아래로 굽히고, 당기기 손목은 기본 대기 자세 기준으로 보정한다.
- 정지하면 마지막 밀기/당기기 자세를 유지해 미세 속도로 몽타주가 번갈아 바뀌지 않게 한다.
- 밀기 원본의 루트 전진을 고정하고, 당기기 몽타주는 AS_Pull_BoxParallel을 참조한다.

## 검증과 한계

검증은 실제 TNT 메시와 Robbie 메시·AnimBP를 사용한다. TheLastMessage.Gimmick.PushPullDistanceSync는 실제 DoMove 입력을 통해 일반/2배 크기 상자의 좌우 이동, 30/120fps, 접근·양손 접촉·정지 안정성·벽 충돌·해제 복구를 검사한다. 낮은 턱 통과와 점프 시 조작 해제도 검사한다. 통합 검증 로그는 Saved/Logs/MasterIntegratedPushPull.log에 기록한다.

Development Editor 빌드 및 자동 검증을 사용하며, 렌더링된 최종 자세의 시각적 자연스러움과 멀티플레이 실기 검증은 포함하지 않는다.

애니메이션 재생성: Scripts/Animation/FixPushPullPoses.py. 변경 전 에셋 백업: Saved/PushPullPoseBackup, Saved/BoxGripBackup.
