#include <iostream>
#include <unordered_set>
#include <math.h>
#include <Windows.h>
#include "Offset.h"
#include "Config.h"
#include "MinHook.h"

#define PI 3.1415926535
namespace DIA4A
{
	struct Vector2D
	{
		float x = 0.f;
		float y = 0.f;
	};

	float CalculateUnits(float m_flX, float m_flY)
	{
		return std::sqrtf((m_flX * m_flX) + (m_flY * m_flY));
	}

	float DistanceInUnits(Vector2D m_pBasePosition, Vector2D m_pTargetPosition)
	{
		return CalculateUnits(m_pTargetPosition.x - m_pBasePosition.x, m_pTargetPosition.y - m_pBasePosition.y);
	}

	namespace Viewport
	{
		bool m_bIsCentered = false;

		int PixelsPerUnit()
		{
			return 50;
		}

		int SCREEN_RESOLUTION_WIDTH()
		{
			HWND m_pExaltWindow = FindWindowA(NULL, "RotMGExalt");
			if (!m_pExaltWindow)
			{
				return 1920;
			}

			RECT m_pWindowRect;
			GetWindowRect(m_pExaltWindow, &m_pWindowRect);
			return (m_pWindowRect.right - m_pWindowRect.left);
		}

		int SCREEN_RESOLUTION_HEIGHT()
		{
			HWND m_pExaltWindow = FindWindowA(NULL, "RotMGExalt");
			if (!m_pExaltWindow)
			{
				return 1080;
			}

			RECT m_pWindowRect;
			GetWindowRect(m_pExaltWindow, &m_pWindowRect);
			return (m_pWindowRect.bottom - m_pWindowRect.top);
		}

		int LOCAL_MIDDLE_X()
		{
			return int(SCREEN_RESOLUTION_WIDTH() / 2.4615);
		}

		float LOCAL_MIDDLE_Y()
		{
			if (!m_bIsCentered)
			{
				return int(SCREEN_RESOLUTION_HEIGHT() / 2.0571);
			}

			return int(SCREEN_RESOLUTION_HEIGHT() / 1.5652);
		}

		float LOCAL_START_X()
		{
			return LOCAL_MIDDLE_X() - (PixelsPerUnit() / 2);
		}

		float LOCAL_START_Y()
		{
			return LOCAL_MIDDLE_Y() - (PixelsPerUnit() / 2);
		}

		float LOCAL_END_X()
		{
			return LOCAL_MIDDLE_X() + (PixelsPerUnit() / 2);
		}

		float LOCAL_END_Y()
		{
			return LOCAL_MIDDLE_Y() + (PixelsPerUnit() / 2);
		}
	}

	namespace Exalt
	{
		void* m_pLocalPlayerAddress = nullptr;

		struct Entity
		{
			static Entity* LocalPlayer()
			{
				return (Entity*)m_pLocalPlayerAddress;
			}

			bool IsLocalPlayer()
			{
				return *(bool*)(DWORD64(this) + LOCALPLAYER_OFFSET);
			}

			Vector2D& RealOrigin()
			{
				return *(Vector2D*)(DWORD64(this) + REALORIGIN_OFFSET);
			}

			DWORD64 DormancyInfo()
			{
				return *(DWORD64*)(DWORD64(this) + DORMANCYINFO_OFFSET);
			}

			int EntityType()
			{
				return *(int*)(DWORD64(this) + ENTITYTYPE_OFFSET);
			}

			float ScreenRotation()
			{
				return *(float*)(DWORD64(this) + SCREENROTATION_OFFSET);
			}

			int& MaxHealth()
			{
				return *(int*)(DWORD64(this) + MAXHEALTH_OFFSET);
			}

			int& Health()
			{
				return *(int*)(DWORD64(this) + HEALTH_OFFSET);
			}

			bool IsAlive()
			{
				return (Health() > 0 && DormancyInfo() != 0);
			}

			bool IsActive()
			{
				return (DormancyInfo() != 0i64);
			}
			
			bool IsEnemy()
			{
				return EntityType() == 7;
			}

			static Vector2D WorldToScreen(Vector2D m_pPoint, bool m_bCentered = true)
			{
				Entity* m_pLocalPlayer = Entity::LocalPlayer();

				if (!m_pLocalPlayer)
				{
					return Vector2D{ 0,0 };
				}

				int m_nPixelsPer = Viewport::PixelsPerUnit();

				float m_flXDelta = (m_pPoint.x - m_pLocalPlayer->RealOrigin().x);
				float m_flYDelta = (m_pPoint.y - m_pLocalPlayer->RealOrigin().y);

				float m_flEntityX = Viewport::LOCAL_START_X() + (m_flXDelta * m_nPixelsPer);
				float m_flEntityY = Viewport::LOCAL_START_Y() + (m_flYDelta * m_nPixelsPer);

				float m_flEntityNonRotatedCenterX = m_flEntityX + (m_nPixelsPer / 2);
				float m_flEntityNonRotatedCenterY = m_flEntityY + (m_nPixelsPer / 2);

				double m_dRotation = -(m_pLocalPlayer->ScreenRotation() == 0 ? 0 : ((360 - m_pLocalPlayer->ScreenRotation()) / (180 / PI)));
				float m_flAtanResult = std::atan2(Viewport::LOCAL_MIDDLE_Y() - m_flEntityNonRotatedCenterY, Viewport::LOCAL_MIDDLE_X() - m_flEntityNonRotatedCenterX);
				double m_dAngle = ((m_flAtanResult + PI + m_dRotation));

				float m_flAbsoluteDeltaPixels = (CalculateUnits(m_flXDelta, m_flYDelta) * m_nPixelsPer);

				float m_flXModifier = m_flAbsoluteDeltaPixels * std::cos(m_dAngle);
				float m_flYModifier = m_flAbsoluteDeltaPixels * std::sin(m_dAngle);

				if (m_bCentered)
				{
					return Vector2D{ Viewport::LOCAL_MIDDLE_X() + m_flXModifier, Viewport::LOCAL_MIDDLE_Y() + m_flYModifier };
				}

				return Vector2D{ Viewport::LOCAL_START_X() + m_flXModifier, Viewport::LOCAL_START_Y() + m_flYModifier };
			}

			Vector2D WorldToScreen(bool m_bCentered = true)
			{
				return WorldToScreen(RealOrigin(), m_bCentered);
			}
		};

		std::unordered_set<Entity*> m_pEnemyEntityList;
	}

	namespace WinApi
	{
		INPUT ConstructKeyBoardInput(WORD m_wVirtualKey, DWORD m_dwFlags)
		{
			INPUT m_inpInput;
			m_inpInput.type = INPUT_KEYBOARD;
			m_inpInput.ki.wVk = m_wVirtualKey;
			m_inpInput.ki.dwFlags = m_dwFlags;
			m_inpInput.ki.wScan = MapVirtualKeyExA(m_wVirtualKey, 0, GetKeyboardLayout(GetCurrentThreadId()));
			m_inpInput.ki.time = 0;
			m_inpInput.ki.dwExtraInfo = 0;

			return m_inpInput;
		}

		void ForceKeyDown(DWORD m_dwKey)
		{
			SendInput(1, &ConstructKeyBoardInput(m_dwKey, KEYEVENTF_SCANCODE), sizeof(INPUT));
		}

		void ForceKeyUp(DWORD m_dwKey)
		{
			SendInput(1, &ConstructKeyBoardInput(m_dwKey, KEYEVENTF_KEYUP), sizeof(INPUT));
		}

		void HoldKeyDown(DWORD m_dwKey, DWORD m_dwTimeMs = 1)
		{
			ForceKeyDown(m_dwKey);
			Sleep(m_dwTimeMs);
			ForceKeyUp(m_dwKey);
		}

		INPUT ConstructMouseMoveInput(int x, int y)
		{
			INPUT m_pMoveMouse;
			m_pMoveMouse.type = INPUT_MOUSE;
			m_pMoveMouse.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
			m_pMoveMouse.mi.dx = x * double(65535.0f / double(GetSystemMetrics(SM_CXSCREEN) - 1));
			m_pMoveMouse.mi.dy = y * double(65535.0f / double(GetSystemMetrics(SM_CYSCREEN) - 1));
			m_pMoveMouse.mi.mouseData = 0;
			m_pMoveMouse.mi.time = 0;
			m_pMoveMouse.mi.dwExtraInfo = 0;
			return m_pMoveMouse;
		}

		void MoveMouse(int x, int y)
		{
			SendInput(1, &ConstructMouseMoveInput(x, y), sizeof(INPUT));
		}

		INPUT ConstructMousePressInput(DWORD m_dwFlags)
		{
			INPUT m_pMoveMouse;
			m_pMoveMouse.type = INPUT_MOUSE;
			m_pMoveMouse.mi.dwFlags = m_dwFlags;
			m_pMoveMouse.mi.mouseData = 0;
			m_pMoveMouse.mi.time = 0;
			m_pMoveMouse.mi.dwExtraInfo = 0;
			return m_pMoveMouse;
		}

		void ForceMouseDown(bool m_bLeftClick = true)
		{
			SendInput(1, &ConstructMousePressInput(m_bLeftClick ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_RIGHTDOWN), sizeof(INPUT));
		}

		void ForceMouseUp(bool m_bLeftClick = true)
		{
			SendInput(1, &ConstructMousePressInput(m_bLeftClick ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_RIGHTUP), sizeof(INPUT));
		}

		void PressMouse(bool m_bLeftClick = true, DWORD m_dwTimeMs = 1)
		{
			ForceMouseDown(m_bLeftClick);
			Sleep(m_dwTimeMs);
			ForceMouseUp(m_bLeftClick);
		}
	}

	namespace Detours
	{
		void* CacheEntityOriginal = nullptr;
		void* CacheEntityDetour(void* a1)
		{
			if (a1)
			{
				DIA4A::Exalt::Entity* m_pEntity = (DIA4A::Exalt::Entity*)a1;

				if (m_pEntity)
				{
					if (m_pEntity->IsLocalPlayer())
					{
						Exalt::m_pLocalPlayerAddress = a1;
						Exalt::m_pEnemyEntityList.clear();
					}
					else if (m_pEntity->IsEnemy())
					{
						if (Exalt::m_pEnemyEntityList.find(m_pEntity) == Exalt::m_pEnemyEntityList.end())
						{
							Exalt::m_pEnemyEntityList.emplace(m_pEntity);
						}
					}
				}
			}

			return ((void* (__stdcall*)(void*))(CacheEntityOriginal))(a1);
		}

		void* TrySetIntOriginal = nullptr;
		void* TrySetIntDetour(void* a1, bool m_bValue)
		{
			if (a1)
			{
				if (*(int*)(DWORD64(a1) + 0x10) == 10)
				{
					if (std::memcmp((const char*)(DWORD64(a1) + 0x14), "o\0f\0f\0s\0e\0t\0M\0o\0d\0e\0", 20) == 0)
					{
						Viewport::m_bIsCentered = m_bValue;
					}
				}
			}

			return ((void* (__stdcall*)(void*, bool))(TrySetIntOriginal))(a1, m_bValue);
		}
	}

	namespace Threads
	{
		void AutoNexusThread()
		{
			while (true)
			{
				if (Exalt::m_pLocalPlayerAddress)
				{
					Exalt::Entity* m_pLocalPlayer = Exalt::Entity::LocalPlayer();
					int m_nHealthPercentage = int(((float)m_pLocalPlayer->Health() / (float)m_pLocalPlayer->MaxHealth()) * 100.f);

					if (m_nHealthPercentage < AUTONEXUS_PERCENT)
					{
						if (GetForegroundWindow() != FindWindowA(NULL, "RotMGExalt"))
						{
							SetForegroundWindow(FindWindowA(NULL, "RotMGExalt"));
							Sleep(50);
						}
						WinApi::HoldKeyDown(NEXUS_KEY, 35);
					}
				}

				Sleep(5);
			}
		}

		void AutoAimThread()
		{
			while (true)
			{
				if (Exalt::m_pLocalPlayerAddress && GetAsyncKeyState(AUTOAIM_KEY) && GetForegroundWindow() == FindWindowA(NULL, "RotMGExalt"))
				{
					Exalt::Entity* m_pLocalPlayer = Exalt::Entity::LocalPlayer();

					POINT m_pCursorPosition;
					GetCursorPos(&m_pCursorPosition);
					ScreenToClient(FindWindowA(NULL, "RotMGExalt"), &m_pCursorPosition);

					float m_flClosest = 10000.f;
					Exalt::Entity* m_pBestTarget = nullptr;

					for (Exalt::Entity* m_pEntity : Exalt::m_pEnemyEntityList)
					{
						if (!m_pEntity->IsActive())
						{
							Exalt::m_pEnemyEntityList.erase(m_pEntity);
							continue;
						}

						if (DistanceInUnits(m_pEntity->RealOrigin(), m_pLocalPlayer->RealOrigin()) > 15.f)
						{
							continue;
						}

						float m_flDelta = DistanceInUnits(m_pEntity->WorldToScreen(), Vector2D{ (float)m_pCursorPosition.x, (float)m_pCursorPosition.y });
						if (m_flDelta < m_flClosest)
						{
							m_flClosest = m_flDelta;
							m_pBestTarget = m_pEntity;
						}
					}

					if (m_pBestTarget)
					{
						Vector2D m_pScreenPosition = m_pBestTarget->WorldToScreen();
						WinApi::MoveMouse(m_pScreenPosition.x, m_pScreenPosition.y);
						WinApi::PressMouse(true, 10);
					}
				}

				Sleep(5);
			}
		}

		void MainThread()
		{
			if (MH_Initialize() != MH_OK)
			{
				return;
			}

			void* m_pCacheEntityFunction = (void*)(DWORD64(GetModuleHandleA("GameAssembly.dll")) + 0x13627E0);
			if (MH_CreateHook(m_pCacheEntityFunction, Detours::CacheEntityDetour, &Detours::CacheEntityOriginal) != MH_OK)
			{
				MessageBoxA(NULL, "Faild To Detour CacheEntity, Perhaps Outdated Offset?", "DIA4A::Exalt::Lite", MB_OK);
				return;
			}

			void* m_pTrySetIntFunction = (void*)(DWORD64(GetModuleHandleA("GameAssembly.dll")) + 0xBEAE20);
			if (MH_CreateHook(m_pTrySetIntFunction, Detours::TrySetIntDetour, &Detours::TrySetIntOriginal) != MH_OK)
			{
				MessageBoxA(NULL, "Faild To Detour TrySetInt, Perhaps Outdated Offset?", "DIA4A::Exalt::Lite", MB_OK);
				return;
			}

			if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
			{
				MessageBoxA(NULL, "Faild To Activate MinHook Hooks", "DIA4A::Exalt::Lite", MB_OK);
				return;
			}

			CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)DIA4A::Threads::AutoAimThread, NULL, NULL, NULL);
			CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)DIA4A::Threads::AutoNexusThread, NULL, NULL, NULL);
		}
	}
}


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD dwReason, LPVOID lpReserved)
{
	BOOL bReturnValue = TRUE;
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)DIA4A::Threads::MainThread, NULL, NULL, NULL);
		break;
	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return bReturnValue;
}