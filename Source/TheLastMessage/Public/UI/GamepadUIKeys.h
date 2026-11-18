#pragma once

#include "InputCoreTypes.h"

/** 메뉴에서 게임패드 버튼을 키보드 동작과 같게 처리하기 위한 판별 함수(Xbox 배치 기준). */
namespace GamepadUI
{
	/** B: Esc와 같은 뒤로/닫기. */
	inline bool IsBack(const FKey& Key) { return Key == EKeys::Gamepad_FaceButton_Right; }
	/** A: 확인. */
	inline bool IsAccept(const FKey& Key) { return Key == EKeys::Gamepad_FaceButton_Bottom; }
	/** 십자키·왼쪽 스틱 좌우. 왼쪽 -1, 오른쪽 +1, 그 밖 0. */
	inline int32 HorizontalStep(const FKey& Key)
	{
		if (Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left) return -1;
		if (Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right) return 1;
		return 0;
	}
	/** 십자키·왼쪽 스틱 상하. 위 -1, 아래 +1, 그 밖 0. */
	inline int32 VerticalStep(const FKey& Key)
	{
		if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up) return -1;
		if (Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down) return 1;
		return 0;
	}
	/** LB -1, RB +1: 탭 전환. */
	inline int32 ShoulderStep(const FKey& Key)
	{
		if (Key == EKeys::Gamepad_LeftShoulder) return -1;
		if (Key == EKeys::Gamepad_RightShoulder) return 1;
		return 0;
	}
}
