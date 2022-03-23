// MADE BY BLUEFIRE1337

#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <random>
#include <stdint.h>
#include "def.h"
#include <list>

#include "SDK/Paladins_classes.hpp"
#include "Overlay.h"
#include <thread>
#include "Mhyprot/mhyprot.hpp"
#include "skCrypter.h"

#include "Gamepad.h"
#pragma comment(lib, "Xinput.lib")

bool tracersEnabled = false;
bool aimbotEnabled = true;
bool boxESPEnabled = false;
bool hpESPEnabled = false;
bool thirdPerson = true;
bool glowEnabled = true;
bool noRecoil = true;
bool noSpread = true;
bool nameESP = false;
bool smoothing = true;
bool lockWhenClose = true;
float smoothingValue = 0.1f; // from 0-1
bool isPredictionAim = false;
bool useNvidiaOverlay = true; // change this if you have problems with overlay
bool overrideFOV = true;
float deafultFOV = 106.f; //106


struct Vec2
{
public:
	float x;
	float y;
};

void ESPLoop();
void RecoilLoop();

uint64_t process_base = 0;

LONG WINAPI SimplestCrashHandler(EXCEPTION_POINTERS* ExceptionInfo)
{
	std::cout << skCrypt("[!!] Crash at addr 0x") << ExceptionInfo->ExceptionRecord->ExceptionAddress << skCrypt(" by 0x") << std::hex << ExceptionInfo->ExceptionRecord->ExceptionCode << std::endl;

	return EXCEPTION_EXECUTE_HANDLER;
}

bool rendering = true;
int frame = 0;
FOverlay* g_overlay;

namespace math
{
	// Constants that we need for maths stuff
#define Const_URotation180        32768
#define Const_PI                  3.14159265358979323
#define Const_RadToUnrRot         10430.3783504704527
#define Const_UnrRotToRad         0.00009587379924285
#define Const_URotationToRadians  Const_PI / Const_URotation180 

	int ClampYaw(int angle) {
		static const auto max = Const_URotation180 * 2;

		while(angle > max)
		{
			angle -= max;
		}

		while(angle < 0) {
			angle += max;
		}
		return angle;
	}

	int ClampPitch(int angle) {
		if(angle > 16000) {
			angle = 16000;
		}
		if(angle < -16000) {
			angle = -16000;
		}
		return angle;
	}


	float VectorMagnitude(Vec2 Vec) {
		return sqrt((Vec.x * Vec.x) + (Vec.y * Vec.y));
	}

	void Normalize(Vec2& v)
	{
		float size = VectorMagnitude(v);

		if(!size)
		{
			v.x = v.y = 1;
		}
		else
		{
			v.x /= size;
			v.y /= size;
		}
	}


	void GetAxes(FRotator R, FVector& X, FVector& Y, FVector& Z)
	{
		X = RotationToVector(R);
		X = X.Normalize();
		R.Yaw += 16384;
		FRotator R2 = R;
		R2.Pitch = 0.f;
		Y = RotationToVector(R2);
		Y = Y.Normalize();
		Y.Z = 0.f;
		R.Yaw -= 16384;
		R.Pitch += 16384;
		Z = RotationToVector(R);
		Z = Z.Normalize();
	}

	FVector GetAngleTo(FVector TargetVec, FVector OriginVec)
	{
		FVector Diff;
		Diff.X = TargetVec.X - OriginVec.X;
		Diff.Y = TargetVec.Y - OriginVec.Y;
		Diff.Z = TargetVec.Z - OriginVec.Z;

		return Diff;
	}

	float GetDistance(FVector to, FVector from) {
		float deltaX = to.X - from.X;
		float deltaY = to.Y - from.Y;
		float deltaZ = to.Z - from.Z;

		return (float)sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
	}

	float GetCrosshairDistance(float Xx, float Yy, float xX, float yY)
	{
		return sqrt((yY - Yy) * (yY - Yy) + (xX - Xx) * (xX - Xx));
	}
}

UEngine CurrentUEngine;
ULocalPlayer CurrentLocalPlayer;
APlayerController CurrentController;
APawn CurrentAcknowledgedPawn;
ACamera CurrentCamera;
AWorldInfo CurrentWorldInfo;
APawn CurrentPawnList;

int CurrentHealth;
float CurrentFOV;
int CurrentLoopFrame = 0;

bool MainAddress() {
#define NAMEOF(name) #name // USE FOR DEBUGGING
#define CHECKVAL(_name)		\
	/*printf("%s : 0x%llX\n", NAMEOF(_name), _name.data);*/ \
	if(!_name.IsValid()) {	\
		return false;			\
	}							\

	//kinda dirty defines, use for debugging
#define PRINTVAL(_name) printf(skCrypt("%s : 0x%llX\n"), NAMEOF(_name), _name);
#define PRINTVALD(_name) printf(skCrypt("%s : %d\n"), NAMEOF(_name), _name);
#define PRINTVALF(_name) printf(skCrypt("%s : %f\n"), NAMEOF(_name), _name);

#define BREAK(_name) 	if(!_name.IsValid()) {	\
		continue;			\
	}
	CurrentLoopFrame++;

	CurrentUEngine = GetUEngine(process_base);
	CHECKVAL(CurrentUEngine);

	CurrentLocalPlayer = CurrentUEngine.GetLocalPlayer();
	CHECKVAL(CurrentLocalPlayer);

	CurrentController = CurrentLocalPlayer.GetController();
	CHECKVAL(CurrentController);

	CurrentAcknowledgedPawn = CurrentController.GetAcknowledgedPawn();
	CHECKVAL(CurrentAcknowledgedPawn);

	CurrentCamera = CurrentController.GetCamera();
	CHECKVAL(CurrentCamera);

	CurrentWorldInfo = CurrentController.GetWorldInfo();
	CHECKVAL(CurrentWorldInfo);

	CurrentPawnList = CurrentWorldInfo.GetPawnList();
	CHECKVAL(CurrentPawnList);

	CurrentHealth = CurrentAcknowledgedPawn.GetHealth();

	CurrentFOV = CurrentCamera.GetDeafultFov() * CurrentController.GetFovMultiplier();

	CurrentAcknowledgedPawn.GetWeapon().SetPerspective(thirdPerson);
	CurrentAcknowledgedPawn.SetGlowhack(glowEnabled);
	return true;
}

float ScreenCenterX;
float ScreenCenterY;

bool Locked;
APawn LockedPawn;
// W2S (WorldToScreen) is used to map the 3D game's world coordinates to 2D screen space coordinates.
bool W2S(FVector target, Vec2& dst, FRotator myRot, FVector myLoc, float fov)
{
	FVector AxisX, AxisY, AxisZ, Delta, Transformed;
	math::GetAxes(myRot, AxisX, AxisY, AxisZ);

	Delta = target - myLoc;
	Transformed.X = Delta.Dot(AxisY);
	Transformed.Y = Delta.Dot(AxisZ);
	Transformed.Z = Delta.Dot(AxisX);

	if(Transformed.Z < 1.00f)
		Transformed.Z = 1.00f;

	dst.x = ScreenCenterX + Transformed.X * (ScreenCenterX / tan(fov * Const_PI / 360.0f)) / Transformed.Z;
	dst.y = ScreenCenterY + -Transformed.Y * (ScreenCenterX / tan(fov * Const_PI / 360.0f)) / Transformed.Z;
	return true; // I WANT TO SEE PPL BEHIND ME

	if(dst.x >= 0.0f && dst.x <= FOverlay::ScreenWidth)
	{
		if(dst.y >= 0.0f && dst.y <= FOverlay::ScreenHeight)
		{
			return true;
		}
	}
	return false;
}

void HackTick() {
	if(MainAddress()) { // 0-1ms
		ESPLoop();
	}
	else {
		Sleep(100);
	}
}

float speed = 7000.0f;
void CallAimbot() {
	if(!LockedPawn.IsValid()) return;

	int Hp = LockedPawn.GetHealth();

	if(Hp > 1)
	{
		FRotator AimRotation = FRotator();

		bool isPawnVisible = LockedPawn.GetMesh().IsVisible(CurrentWorldInfo.GetTimeSeconds());

		if(isPawnVisible)
		{
			FVector TargetLocation = LockedPawn.GetLocation();
			if(isPredictionAim) {
				auto currentProjectiles = CurrentAcknowledgedPawn.GetWeapon().GetProjectiles();

				if(currentProjectiles.Length() > 0)
				{
					speed = currentProjectiles.GetById(0).GetSpeed();
				}

				FVector TargetVelocity = LockedPawn.GetVelocity();
				float TravelTime = math::GetDistance(CurrentAcknowledgedPawn.GetLocation(), TargetLocation) / speed;
				TargetLocation = {
					(TargetLocation.X + TargetVelocity.X * TravelTime),
					(TargetLocation.Y + TargetVelocity.Y * TravelTime),
					 TargetLocation.Z
				};
			}

			FVector RealLocation = CurrentCamera.GetRealLocation();

			AimAtVector(TargetLocation + FVector(0, 0, LockedPawn.GetEyeHeight()), RealLocation, AimRotation);

			AimRotation.Yaw = math::ClampYaw(AimRotation.Yaw);
			AimRotation.Pitch = math::ClampPitch(AimRotation.Pitch);

			if(smoothing) { // idk kinda spagetti to me, but it works
				FRotator currentRotation = CurrentController.GetRotation();
				currentRotation.Roll = 0;

				auto diff = currentRotation - AimRotation;

				auto realDiff = diff;
				auto a = math::ClampYaw(currentRotation.Yaw);
				auto b = math::ClampYaw(AimRotation.Yaw);
				const auto Full360 = Const_URotation180 * 2;

				auto dist1 = -(a - b + Full360) % Full360;
				auto dist2 = (b - a + Full360) % Full360;

				auto dist = dist1;
				if(abs(dist2) < abs(dist1)) {
					dist = dist2;
				}

				auto smoothAmount = smoothingValue;

				if(lockWhenClose && abs(dist) + abs(diff.Pitch) < Const_URotation180 / 100) {
					smoothAmount = 1;
				}

				diff.Yaw = (int)(dist * smoothAmount);
				diff.Pitch = (int)(diff.Pitch * smoothAmount);
				AimRotation = currentRotation + diff;
			}
			CurrentController.SetRotation(AimRotation);
		}
	}
	else
	{
		Locked = false;
		LockedPawn = APawn{};
		return;
	}
}

void RecoilLoop() {
	if(!CurrentAcknowledgedPawn.IsValid()) return;
	auto wep = CurrentAcknowledgedPawn.GetWeapon();
	if(!wep.IsValid()) return;
	wep.NoRecoil(noRecoil);
	wep.NoSpread(noSpread);
	if(overrideFOV) {
		CurrentCamera.SetDefaultFOV(deafultFOV);
	}
}

// Default constructor
Gamepad::Gamepad() {}

// Overloaded constructor
Gamepad::Gamepad(int a_iIndex)
{
	// Set gamepad index
	m_iGamepadIndex = a_iIndex - 1;
}

// Return gamepad state
XINPUT_STATE Gamepad::GetState()
{
	// Temporary XINPUT_STATE to return
	XINPUT_STATE GamepadState;

	// Zero memory
	ZeroMemory(&GamepadState, sizeof(XINPUT_STATE));

	// Get the state of the gamepad
	XInputGetState(m_iGamepadIndex, &GamepadState);

	// Return the gamepad state
	return GamepadState;
}

// Return gamepad index
int Gamepad::GetIndex()
{
	return m_iGamepadIndex;
}

bool Gamepad::Connected()
{
	// Zero memory
	ZeroMemory(&m_State, sizeof(XINPUT_STATE));

	// Get the state of the gamepad
	DWORD Result = XInputGetState(m_iGamepadIndex, &m_State);

	if (Result == ERROR_SUCCESS)
		return true;  // The gamepad is connected
	else
		return false; // The gamepad is not connected
}

// Update gamepad state
void Gamepad::Update()
{
	m_State = GetState(); // Obtain current gamepad state
}

// Return value of right trigger
float Gamepad::RightTrigger()
{
	// Obtain value of right trigger
	BYTE Trigger = m_State.Gamepad.bRightTrigger;

	if (Trigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD)
		return Trigger / 255.0f;

	return 0.0f; // Trigger was not pressed

}



void ESPLoop() {
	APawn CurrentPawn = CurrentPawnList;
	int players = 0;

	bool AimbotKey=false;
	
	Gamepad Gamepad;

	if (Gamepad.Connected() && Gamepad.RightTrigger()>0.0f)
		{
		AimbotKey = true;
		Gamepad.Update();
		}

	bool isAimbotActive = aimbotEnabled && AimbotKey; //GetAsyncKeyState

	if(!isAimbotActive || !LockedPawn.IsValid() || !LockedPawn.GetMesh().IsVisible(CurrentWorldInfo.GetTimeSeconds())) {
		LockedPawn = APawn{};
		Locked = false;
	}

	float ClosestDistance = 999999.0f;

	auto currentTeamIndex = CurrentAcknowledgedPawn.GetPlayerReplicationInfo().GetTeamInfo().GetTeamIndex();

	while(CurrentPawn.IsValid())
	{
		APawn nextPawn = CurrentPawn.GetNextPawn();
		if(!CurrentPawn.IsValid())
		{
			CurrentPawn = nextPawn;
			continue;
		}

		int Hp = CurrentPawn.GetHealth();
		players++;
		auto repInfo = CurrentPawn.GetPlayerReplicationInfo();
		if(!repInfo.IsValid()) {
			CurrentPawn = nextPawn;
			continue;
		}

		auto teamInfo = repInfo.GetTeamInfo();
		if(!teamInfo.IsValid()) {
			CurrentPawn = nextPawn;
			continue;
		}

		auto teamIndex = teamInfo.GetTeamIndex();

		//teamIndex != currentTeamIndex to aim at teammates
		if(teamIndex == currentTeamIndex ||
			CurrentPawn == CurrentAcknowledgedPawn ||
			Hp < 1 || Hp > 10000)
		{
			CurrentPawn = nextPawn;
			continue;
		}

		auto mesh = CurrentPawn.GetMesh();
		if(!mesh.IsValid()) {
			CurrentPawn = nextPawn;
			continue;
		}

		FBoxSphereBounds PlayerHitbox = mesh.GetBounds();

		Vec2 smin, smax, pos;

		FVector min = PlayerHitbox.Origin - PlayerHitbox.BoxExtent;
		FVector max = PlayerHitbox.Origin + PlayerHitbox.BoxExtent;
		FVector pawnPos = CurrentPawn.GetLocation();

		FRotator playerRotation = CurrentController.GetRotation();
		FVector camLocation = CurrentCamera.GetRealLocation();

		bool isPawnVisible = mesh.IsVisible(CurrentWorldInfo.GetTimeSeconds());
		auto posw2s = W2S(pawnPos, pos, playerRotation, camLocation, CurrentFOV);

		// TRACERS
		auto tracerDistance = (camLocation - pawnPos).Size() / 50.f;
		float red = max(0, 250 - tracerDistance) / 255.f;
		Vec2 normalizedHead;
		normalizedHead.x = pos.x - ScreenCenterX;
		normalizedHead.y = pos.y - ScreenCenterY;
		math::Normalize(normalizedHead);
		const float offsetCircle = 10.0f;

		auto tracerColor = D2D1::ColorF(isPawnVisible ? 0.f : red, min(250, tracerDistance) / 255.f, isPawnVisible ? red : 0);
		if(LockedPawn.data == CurrentPawn.data) {
			tracerColor = D2D1::ColorF(1.0f, 0, 1.0f);
		}

		if(tracersEnabled) {
			g_overlay->draw_line(ScreenCenterX + normalizedHead.x * offsetCircle, ScreenCenterY + normalizedHead.y * offsetCircle, pos.x, pos.y, tracerColor);
		}

		try {
			if(W2S(min, smin, playerRotation, camLocation, CurrentFOV) &&
				W2S(max, smax, playerRotation, camLocation, CurrentFOV) &&
				posw2s)
			{

				float flWidth = fabs((smax.y - smin.y) / 4);

				//BOX
				if(boxESPEnabled) {
					g_overlay->draw_line(pos.x - flWidth, smin.y, pos.x + flWidth, smin.y, tracerColor); // bottom
					g_overlay->draw_line(pos.x - flWidth, smax.y, pos.x + flWidth, smax.y, tracerColor); // up
					g_overlay->draw_line(pos.x - flWidth, smin.y, pos.x - flWidth, smax.y, tracerColor); // left
					g_overlay->draw_line(pos.x + flWidth, smin.y, pos.x + flWidth, smax.y, tracerColor); // right
				}

				//HP
				if(hpESPEnabled) {
					auto maxHP = CurrentPawn.GetMaxHealth();
					//auto procentage = Hp * 100 / maxHP;
					g_overlay->draw_text(pos.x + flWidth + 10, smax.y, D2D1::ColorF(1.f, 1.f, 0), skCrypt("%d / %d"), Hp, maxHP);
				}

				if(nameESP) {
					auto str = repInfo.GetName().ToWString();
					g_overlay->draw_text(pos.x + flWidth + 10, smax.y + (hpESPEnabled ? 10 : 0), D2D1::ColorF(1.f, 1.f, 0), skCrypt("%S"), str.c_str());
				}

				if(isAimbotActive && isPawnVisible && !Locked) {
					Vec2 headPos;
					if(W2S(pawnPos, headPos, playerRotation, camLocation, CurrentFOV))
					{
						//TODO SETTING
						float aimfov = 30.0f; //TODO FIX THIS
						float cx = headPos.x;
						float cy = headPos.y;
						float radiusx_ = aimfov * (ScreenCenterX / CurrentFOV);
						float radiusy_ = aimfov * (ScreenCenterY / CurrentFOV);
						float crosshairDistance = math::GetCrosshairDistance(cx, cy, ScreenCenterX, ScreenCenterY);
						if(tracerDistance < 30.f || cx >= ScreenCenterX - radiusx_ && cx <= ScreenCenterX + radiusx_ && cy >= ScreenCenterY - radiusy_ && cy <= ScreenCenterY + radiusy_)
						{
							if(crosshairDistance < ClosestDistance)
							{
								ClosestDistance = crosshairDistance;
								LockedPawn = CurrentPawn;
							}
						}
						CurrentPawn = nextPawn;
						continue;
					}
				}
			}
			CurrentPawn = nextPawn;
		}
		catch(const std::exception&) {
			CurrentPawn = nextPawn;
			continue;
		}
	}

	if(LockedPawn.IsValid())
	{
		Locked = true;
		CallAimbot();
	}

}

static void render(FOverlay* overlay)
{
	while(rendering)
	{
		overlay->begin_scene();

		overlay->clear_scene();
		frame++;

		HackTick();
		if(frame % 60 == 0) { // once a second (given a 60hz updaterate) because the results are cached 
			RecoilLoop();
		}

		//	overlay->draw_text(200, 200, D2D1::ColorF(1.f, 1.f, 0), "render %d", frame
		overlay->end_scene();
	}
}

void exiting() {
	std::cout << skCrypt("Exiting");

	rendering = false;
	g_overlay->begin_scene();

	g_overlay->clear_scene();

	g_overlay->end_scene();
}

static void _init(FOverlay* overlay)
{
	// Initialize the window
	if(!overlay->window_init(useNvidiaOverlay))
		return;

	// D2D Failed to initialize?
	if(!overlay->init_d2d())
		return;

	// render loop
	std::thread r(render, overlay);

	r.join(); // threading

	overlay->d2d_shutdown();

	return;
}

int main()
{
	//SetConsoleTitle(skCrypt("BLUEFIRE1337's Paladins Cheat V5"));
	SetUnhandledExceptionFilter(SimplestCrashHandler);
	//initTrace();

	system(skCrypt("sc stop mhyprot2")); // RELOAD DRIVER JUST IN CASE
	system(skCrypt("CLS")); // CLEAR

	auto process_name = skCrypt("Paladins.exe");
	auto process_id = GetProcessId(process_name);
	if(!process_id)
	{
		printf(skCrypt("[!] process \"%s\ was not found\n"), process_name);
		return -1;
	}

	printf(skCrypt("[+] %s (%d)\n"), process_name, process_id);

	//
	// initialize its service, etc
	//
	if(!mhyprot::init())
	{
		printf(skCrypt("[!] failed to initialize vulnerable driver\n"));
		return -1;
	}

	if(!mhyprot::driver_impl::driver_init(
		false, // print debug
		false // print seedmap
	))
	{
		printf(skCrypt("[!] failed to initialize driver properly\n"));
		mhyprot::unload();
		return -1;
	}
	process_base = GetProcessBase(process_id);
	if(!process_base) {
		printf(skCrypt("[!] failed to get baseadress\n"));
		mhyprot::unload();
		return -1;
	}

	//printf("[+] Game Base is 0x%llX\n", process_base);
	IMAGE_DOS_HEADER dos_header = read<IMAGE_DOS_HEADER>(process_base);
	printf(skCrypt("[+] Game header Magic is 0x%llX\n"), dos_header.e_magic);
	if(dos_header.e_magic != 0x5A4D) {
		printf(skCrypt("[!] Game header Magic should be 0x5A4D\n"));
	}
	MainAddress();

	RECT desktop;
	// Get a handle to the desktop window
	const HWND hDesktop = GetDesktopWindow();
	// Get the size of screen to the variable desktop
	GetWindowRect(hDesktop, &desktop);
	HDC monitor = GetDC(hDesktop);

	int current = GetDeviceCaps(monitor, VERTRES);
	int total = GetDeviceCaps(monitor, DESKTOPVERTRES);

	FOverlay::ScreenWidth = (desktop.right - desktop.left) * total / current;
	FOverlay::ScreenHeight = (desktop.bottom - desktop.top) * total / current;

	ScreenCenterX = FOverlay::ScreenWidth / 2.f;
	ScreenCenterY = FOverlay::ScreenHeight / 2.f;
	g_overlay = { 0 };
	_init(g_overlay);
}
