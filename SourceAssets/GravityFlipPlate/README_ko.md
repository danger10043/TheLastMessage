# 중력 반전 발판 3D 에셋

제공한 정면·측면·상면 이미지를 참고해 만든 실제 입체 모델입니다. 이미지의 자동 3D 복원본이 아니라, 형태를 해석해 제작한 모델입니다.

## 파일

- `SM_GravityFlipPlate.fbx`: 언리얼용 정적 메시와 `UCX_SM_GravityFlipPlate_00` 단순 충돌체.
- `GravityFlipPlate.blend`: 텍스처를 내장한 Blender 편집 원본. 조명과 카메라는 미리보기용입니다.
- `Textures/`: 2048 × 1024 PNG 텍스처 4장.
- `Preview_Perspective.png`, `Preview_Top.png`, `Preview_Front.png`: 실제 모델 렌더링.
- `build_report.json`: Blender 모델 크기와 폴리곤 수.
- `fbx_validation_report.json`: FBX 재불러오기 검증 결과. 크기, UV, 재질 슬롯, 충돌체, 좌표와 탄젠트, 삼각형 면적을 검사했습니다.
- `Import_Into_Unreal.py`: FBX와 같은 폴더에 둔 뒤 언리얼 Python 환경에서 실행하는 임포트 도우미. **현재 환경에서 언리얼 실행이 승인되지 않아 실행 검증하지 못했습니다.**
- `unreal_import_report.json`: 언리얼 자동 임포트가 성공했을 때 생성되는 검증 결과.

## 언리얼 프로젝트 에셋

자동 임포트 스크립트의 대상은 `/Game/Test_Jiho/Gimmick/Plate/GravityFlipPlate_Art`입니다.

- `SM_GravityFlipPlate`: 발판 메시. 약 240 × 203 × 34cm, 바닥 중앙 피벗, Z 위쪽.
- `M_GravityFlipPlate`: PBR 기본 재질.
- `MI_GravityFlipPlate`: 기본 청색 발광 재질 인스턴스.
- `T_GravityFlipPlate_*`: 색상, ORM, 노멀, 발광 마스크.

기존 기믹 블루프린트, 레벨, 소스 코드를 바꾸지 않고 아트 에셋만 추가합니다.

## 기존 GravityFlipPlate에 연결하기

1. 기존 `GravityFlipPlate` 기반 블루프린트를 열고 Static Mesh 컴포넌트를 추가하거나 기존 메시 컴포넌트를 선택합니다.
2. Static Mesh에 `SM_GravityFlipPlate`, Material에 `MI_GravityFlipPlate`를 지정합니다.
3. SceneRoot 기준 상대 위치 `(0, 0, 0)`, 회전 `(0, 0, 0)`, 스케일 `(1, 1, 1)`부터 확인합니다.
4. 메시 충돌은 발을 받치는 용도입니다. 중력 전환 감지는 기존 `PlateTrigger`가 담당합니다. 메시 자체의 Generate Overlap Events는 끄고 기존 트리거를 유지합니다.
5. 현재 C++ 트리거 크기는 반경 `(100, 100, 30)cm`, 중심 `(0, 0, 30)cm`입니다. 발판 전체 폭을 감지하려면 레벨 설계에 맞춰 트리거 크기를 조절합니다.

블루프린트에서 Dynamic Material Instance를 만들면 `EmissiveStrength` 스칼라(기본 3.0)와 `GlowColor` 벡터를 조절할 수 있습니다. 비활성 시 강도 0, 활성 시 3 정도를 출발점으로 사용할 수 있습니다. Bloom 표현은 프로젝트의 포스트 프로세스 설정에 따릅니다.

이 에셋에는 새로운 중력 반전 로직이나 애니메이션을 포함하지 않습니다. 기존 기믹 구현에 시각 에셋으로 연결합니다.

## FBX를 다른 프로젝트에 직접 가져올 때

- Static Mesh로 임포트, Skeletal Mesh 끄기, Import Uniform Scale 1.0.
- Import Materials / Import Textures 끄기. FBX는 복잡한 PBR 재질 그래프를 그대로 전달하지 않습니다.
- Import Collision 켜기. 제공된 UCX 충돌체를 사용하고 자동 생성 충돌은 끕니다.
- Import Normals and Tangents 사용. UV0는 표면 텍스처용, UV1은 비중첩 라이트맵용입니다.
- Light Map Coordinate Index 1, Light Map Resolution 128부터 시작합니다.
- 약 9만 삼각형의 상세 모델이므로 지원 플랫폼에서 Nanite 사용을 권장합니다. 모바일용 별도 LOD 최적화는 포함하지 않았습니다.

재질 연결:

| 텍스처 | 설정 | 연결 |
|---|---|---|
| BaseColor | sRGB 켜기 | RGB → Base Color |
| ORM | sRGB 끄기, Masks 압축 | R → AO, G → Roughness, B → Metallic |
| Normal | sRGB 끄기, Normalmap 압축, **Flip Green Channel 켜기** | RGB → Normal |
| EmissiveMask | sRGB 끄기, Masks 압축 | R × GlowColor × EmissiveStrength → Emissive Color |

GlowColor 기본값은 선형 RGB `(0.003, 0.20, 1.0)`입니다. Blender용 노멀은 OpenGL 방식이므로 언리얼에서 녹색 채널 반전이 필요합니다.

## 재생성

프로젝트 `Scripts/create_gravity_flip_plate.py`를 Blender 백그라운드 모드로 실행하면 모델과 텍스처, 미리보기와 편집 원본을 재생성합니다.

`Scripts/import_gravity_flip_plate.py` 또는 동봉된 `Import_Into_Unreal.py`는 언리얼 Python 환경에서 실행합니다. Python Editor Script Plugin이 필요합니다. Tools → Execute Python Script에서 실행할 수 있습니다. 대상 프로젝트의 새 아트 폴더에만 에셋을 저장합니다. 임포트 도우미는 아직 언리얼에서 실행 검증되지 않았으므로 수동 임포트 방법도 함께 제공합니다.

`unreal_import_report.json`은 메시 크기, UV 개수, 재질 슬롯 수, 단순 충돌체 수, Nanite 활성화를 검사한 기록입니다. 실제 캐릭터 보행과 중력 반전 플레이 검증은 별도입니다.
