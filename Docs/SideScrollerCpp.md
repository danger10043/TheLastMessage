# 사이드 스크롤러 C++ 구현

UE 5.8.2의 BP 사이드 스크롤러를 기준으로 캐릭터, 입력, 카메라, 리스폰과 게임 모드를 C++로 옮겼습니다.

## 플레이

프로젝트의 기본 시작 맵은 `Test_Jihong/SideScroller/Lvl_SideScrolling`입니다. 이 레벨에서 Play를 누릅니다.

개발·테스트 콘텐츠는 `Test_Jihong`에서 작업합니다. `Main`에는 사용자가 완성본으로 올리도록 요청한 콘텐츠만 둡니다. 이 규칙은 프로젝트 루트의 `AGENTS.md`에도 기록되어 있습니다.

| 조작 | 키 |
| --- | --- |
| 좌우 이동 | A / D, 게임패드 왼쪽 스틱 |
| 점프 / 이중 점프 / 벽 점프 | Space, 게임패드 아래쪽 버튼 |
| 발판 아래로 내려가기 | S + Space, 스틱 아래 + 점프 |
| 상호작용 | F, 게임패드 왼쪽 버튼 |

## 수정할 코드

- `Source/TheLastMessage/Private/Character/Player/RobotPlayerCharacter.cpp`: 이동, 점프, 코요테 타임, 벽 점프, 발판 충돌, 상호작용.
- `Source/TheLastMessage/Private/Camera/SideScrollingCameraManager.cpp`: 카메라 위치, 높이 추적, 보간, 이동 범위.
- `Source/TheLastMessage/Private/Framework/SideScrollingPlayerController.cpp`: Enhanced Input, 터치 UI, 리스폰.
- `Source/TheLastMessage/Private/Framework/SideScrollingGameMode.cpp`: 기본 클래스, 플레이어 시작 위치, 픽업 카운터, 로컬 플레이어 생성.

캐릭터 생성자의 이동 설정은 원본 BP의 값을 사용합니다. 점프 속도는 525, 중력 배율은 1.75, 이동 속도는 500, 최대 가속도는 1500입니다. 코요테 타임은 0.16초, 벽 점프 입력 제한은 0.3초입니다. 카메라 거리는 1000, FOV는 65, X 범위는 -1000에서 9000입니다.

## 남아 있는 BP와 에셋

`BP_RobotPlayerCharacter`는 C++ `RobotPlayerCharacter`를 상속합니다. 메시, 애니메이션, 입력 액션을 연결하고 기존 템플릿의 발판·터치 인터페이스 호출을 C++ 함수로 전달합니다. 이동·점프·키 바인딩은 C++에서 실행합니다.

카메라·컨트롤러 BP에는 동작 그래프가 없으며 설정만 담습니다. 게임 모드 BP에는 픽업 인터페이스를 C++에 전달하는 연결만 있습니다. 애니메이션 상태 머신, UI, 발판·스위치·픽업 및 NPC는 템플릿 에셋을 사용합니다.

원본 `Content/_ExternalAssets/ThirdPerson/Variant_SideScroller` 에셋은 비교 대상으로 유지됩니다. 새 테스트 레벨은 원본의 외부 액터를 포함한 일반 레벨입니다. 메시·재질·애니메이션, 공용 BP 인터페이스와 NPC는 `_ExternalAssets`의 기존 에셋에 의존하므로 해당 폴더가 프로젝트에 있어야 합니다.

## 검증 도구

에디터의 Automation에서 `TheLastMessage.SideScroller`를 실행할 수 있습니다. 원본 이동 설정 비교와 실제 Play 입력·점프·카메라·발판 인터페이스·리스폰 검사를 포함합니다.

2026-10-14 검증: Development Editor 및 Windows Development 게임 빌드 성공. `TemplateSettings`와 `PlayIntegration` 모두 통과했습니다. 카메라 비교는 원본 BP와 40프레임의 시점 이동 결과를 비교하며, 자동 검사는 화면 렌더링 없이 실행했습니다.

`Test_Jihong/SideScroller`로 이동한 뒤에도 두 검사를 통과했습니다. 에셋 15개의 패키지 참조를 갱신했으며, 픽업 BP도 다시 컴파일해 이전 `Main/SideScroller` 경로가 남지 않았음을 확인했습니다.

`TheLastMessageEditor`는 에셋 연결 및 검사 전용 모듈이며 게임 빌드에는 포함되지 않습니다. `SideScrollerMigration` 명령은 빈 `Test_Jihong/SideScroller` 폴더에만 생성하므로 이미 완성된 에셋을 덮어쓰지 않습니다.
