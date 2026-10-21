# 스탯 코어 강화 수치 설정

현재 구현된 스탯 코어는 최대 3~5단계까지 강화한다. DA의 `Custom / Max Level`이 획득 제한을 정하고,
GE의 `Scalable Float`가 커브 테이블 `Curves/CT_CoreStatLevels`에서 해당 단계의 수치를 읽는다.
수치는 단계별로 직접 편집하며, MMC·CSV 생성·성장 계산 코드는 사용하지 않는다.

## 수치 수정

1. `Curves/CT_CoreStatLevels`를 연다.
2. 코어 이름에 해당하는 행의 값을 수정한다. X는 강화 단계, Y는 GE에 전달할 값이다.
3. 저장한다. 일반 배율은 1.15가 +15%, 0.9가 -10%다. 치명타 행은 0.05가 +5%p다.

| 행 | Max | Lv.1 | Lv.2 | Lv.3 | Lv.4 | Lv.5 |
|---|---:|---:|---:|---:|---:|---:|
| ReinforcedFrame | 5 | 1.15 | 1.30 | 1.45 | 1.60 | 1.75 |
| ImpactDispersionArmor | 5 | 0.90 | 0.80 | 0.70 | 0.60 | 0.50 |
| OverloadOutput | 5 | 1.25 | 1.50 | 1.75 | 2.00 | 2.25 |
| MeleeAmplifier | 5 | 1.20 | 1.40 | 1.60 | 1.80 | 2.00 |
| StableOutputCircuit | 5 | 1.15 | 1.30 | 1.45 | 1.60 | 1.75 |
| EmergencyOutputCircuit | 5 | 1.30 | 1.60 | 1.90 | 2.20 | 2.50 |
| PrecisionAimingCircuit | 5 | 0.05 | 0.10 | 0.15 | 0.20 | 0.25 |
| HighSpeedDrive | 3 | 1.15 | 1.30 | 1.45 | — | — |
| ChargeTime | 3 | 0.80 | 0.60 | 0.40 | — | — |
| EnergyRechargeRate | 3 | 1.25 | 1.50 | 1.75 | — | — |
| EfficientDischargeCircuit | 5 | 1.25 | 1.50 | 1.75 | 2.00 | 2.25 |

과부하 출력기의 받는 피해 증가는 GE에서 모든 단계에 1.15로 고정한다.
안정 출력은 체력 80% 이상, 비상 출력은 체력 30% 이하에서 작동한다.
차지 가속 회로는 ChargeTime과 EnergyRechargeRate 두 행을 사용하며 하나의 코어로 함께 강화된다.

## 새 코어 추가

1. 커브 테이블에 행을 추가하고 사용할 각 단계의 값을 입력한다.
2. GE를 만든다. `Duration Policy=Infinite`, `Stacking Type=None`으로 설정한다.
   여기서 Infinite는 효과의 지속 시간이 무제한이라는 뜻이며 강화 횟수와는 별개다.
3. Modifier에 Attribute와 연산을 지정한다. 일반 배율은 `Multiply (Additive)`, 치명타 추가 확률은 `Add (Base)`다.
4. 수치 계산은 `Scalable Float`, Value는 1, Curve Table은 `CT_CoreStatLevels`, Row Name은 새 행으로 지정한다.
5. DA에 고유한 CoreId, `CoreCategory=Stat`, `UpgradeLimitType=Custom`, `MaxLevel`을 설정하고 GE를 `GrantedEffects`에 연결한다.
6. MaxLevel까지 모든 키가 있는지 확인한다. 커브의 마지막 키는 획득 제한을 대신하지 않는다.

같은 Attribute를 여러 GE가 수정하면 GAS의 Modifier 연산 규칙대로 합산된다.
강화 시 기존 GE의 레벨만 갱신하므로 동일 코어의 GE가 중복 적용되지 않는다.
최대 체력·최대 에너지 증가 시 현재량은 회복하지 않는다.

## 미구현 코어와 설명

전력 흡수·잔여 전력 회수·출력 누적은 아직 실제 동작과 단계별 수치가 정해지지 않아 기존 Custom 1을 유지한다.
능력 해금 코어와 현재 능력형 코어도 기존 Custom 1을 유지한다.
선택 카드에는 신규 획득 또는 현재 → 다음 강화 단계가 표시된다. 카드 순번 /01, /02와 강화 단계는 별개다.

## 카드 설명 작성

DA의 `CoreDescription`에는 `기본 최대 체력 대비 +{HealthBonus}%`처럼 문장을 작성한다.
`Description Values`에는 Token, 지급할 Effect, 해당 Modifier의 Attribute, 숫자 표시 형식을 설정한다.
예를 들어 HealthBonus → 보강 프레임 GE → MaxHealth → Increase Percent로 연결한다.
새 코어는 1단계 값을, 보유 코어는 현재 → 다음 단계 값을 GE에서 직접 읽어 표시한다.
단계가 바뀌어도 같은 값인 조건·페널티에는 비교 화살표를 붙이지 않는다.

| 표시 형식 | GE 값 → 설명 숫자 |
|---|---|
| Number | 그대로 |
| Percent | 0.05 → 5 (치명타 보너스, 체력 조건 등) |
| Increase Percent | 1.15 → 15 (배율 증가) |
| Reduction Percent | 0.9 → 10 (배율 감소) |

퍼센트 기호·%p·기본값 기준 같은 문구는 DA 설명에 작성한다. GE 수치가 바뀌면 설명 숫자도 함께 바뀐다.
이 숫자는 해당 코어의 보너스이며 다른 코어까지 합산한 플레이어 전체 스탯은 아니다.
동적 값이 없는 능력형 코어는 기존 고정 문구를 사용한다.
