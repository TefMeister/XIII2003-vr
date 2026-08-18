// proxy/camera_hook.cpp
//
// Inline hook on APlayerController::eventPlayerCalcView(AActor*&, FVector&,
// FRotator&) in Engine.dll. That function outputs the render camera's rotation
// each frame; we call the original, then add a synthetic yaw sweep to the out
// FRotator so head-look is visible headset-free.
//
// Self-contained: resolve the target by its (exported) decorated name, then a
// minimal 5-byte-jmp inline hook with a hand-built trampoline. The target's
// prologue is verified at runtime before patching, so a version mismatch fails
// safe instead of crashing.

#include <windows.h>
#include <cstdint>
#include <cstring>
#include "pose_math.h"
#include "vr_host.h"

// Unreal FRotator: three int32 in declaration order Pitch, Yaw, Roll.
// A full revolution is 65536 units.
struct FRotator { int32_t Pitch; int32_t Yaw; int32_t Roll; };

// thiscall emulated as __fastcall: this in ECX (1st param), EDX unused (2nd),
// the three reference args arrive on the stack.
typedef void(__fastcall* PlayerCalcView_t)(void* self, void* edx,
                                           void** ViewActor, void* CameraLocation,
                                           FRotator* CameraRotation);

static const char* kMangled =
    "?eventPlayerCalcView@APlayerController@@QAEXAAPAVAActor@@AAVFVector@@AAVFRotator@@@Z";

// eventPlayerCalcView prologue we relocate: `sub esp,1C; mov eax,[esp+20]`.
// No relative operands, so it copies verbatim into the trampoline.
static const uint8_t kExpectedPrologue[] = { 0x83, 0xEC, 0x1C, 0x8B, 0x44, 0x24, 0x20 };
static const size_t  kStealLen = sizeof(kExpectedPrologue);  // 7

static PlayerCalcView_t s_trampoline    = nullptr;
static LONG             s_sweep         = 0;   // accumulating synthetic yaw
static bool             s_sweepEnabled  = false;
static bool             s_liveHmd       = false;
static const LONG       kSweepStep      = 150; // ~0.8 deg/frame (150/65536*360)

static void Log(const char* m) {
    OutputDebugStringA("[xiii-camera] "); OutputDebugStringA(m); OutputDebugStringA("\n");
}

// Read camera-override config from the game's XIII.ini (next to XIII.exe):
//   [VR] CameraSyntheticSweep = 1  -> hardcoded yaw sweep (bypasses pose_math)
//   [VR] CameraLiveHmd        = 1  -> drive the view from VrHostGetHeadPose()
//                                     through the full pose_math pipeline
// Both default 0 (passthrough). LiveHmd takes precedence if both are set.
static void ReadConfigFromIni() {
    char exe[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, exe, MAX_PATH)) return;
    char* slash = strrchr(exe, '\\');
    if (!slash) return;
    lstrcpyA(slash + 1, "XIII.ini");
    s_sweepEnabled = GetPrivateProfileIntA("VR", "CameraSyntheticSweep", 0, exe) != 0;
    s_liveHmd      = GetPrivateProfileIntA("VR", "CameraLiveHmd", 0, exe) != 0;
}

static void __fastcall Hook_PlayerCalcView(void* self, void* edx, void** ViewActor,
                                           void* CameraLocation,
                                           FRotator* CameraRotation) {
    s_trampoline(self, edx, ViewActor, CameraLocation, CameraRotation);
    if (!CameraRotation) return;
    if (s_liveHmd) {
        // Full pose pipeline: HMD quaternion -> euler radians -> rotator units.
        float qx, qy, qz, qw;
        if (VrHostGetHeadPose(&qx, &qy, &qz, &qw)) {
            EulerRadians e = QuaternionToEuler(Quaternion{qx, qy, qz, qw});
            // Yaw wraps mod 65536, so the normalized [0,65536) value adds right.
            // Negated: OpenVR's yaw winds opposite to Unreal's, so the raw value
            // mirrors head-look (turn left, game pans right -- confirmed on HW).
            CameraRotation->Yaw += RadiansToUnrealRotatorUnits(-e.yaw);
            // Pitch is signed and small; convert directly (no wrap-normalize).
            CameraRotation->Pitch +=
                (int32_t)(e.pitch / 6.28318530717958647692f * 65536.0f);
        }
    } else if (s_sweepEnabled) {
        CameraRotation->Yaw += InterlockedAdd(&s_sweep, kSweepStep);
    }
}

bool InstallCameraHook() {
    ReadConfigFromIni();

    HMODULE hEngine = GetModuleHandleA("Engine.dll");
    if (!hEngine) { Log("Engine.dll not loaded"); return false; }

    BYTE* target = (BYTE*)GetProcAddress(hEngine, kMangled);
    if (!target) { Log("eventPlayerCalcView export not found"); return false; }

    // Fail safe if the prologue is not what we relocate.
    for (size_t i = 0; i < kStealLen; ++i)
        if (target[i] != kExpectedPrologue[i]) { Log("prologue mismatch; not hooking"); return false; }

    // Trampoline: [stolen bytes][jmp target+kStealLen]
    BYTE* tramp = (BYTE*)VirtualAlloc(nullptr, kStealLen + 5, MEM_COMMIT | MEM_RESERVE,
                                      PAGE_EXECUTE_READWRITE);
    if (!tramp) { Log("trampoline alloc failed"); return false; }
    memcpy(tramp, target, kStealLen);
    tramp[kStealLen] = 0xE9;  // jmp rel32
    *(int32_t*)(tramp + kStealLen + 1) =
        (int32_t)((target + kStealLen) - (tramp + kStealLen + 5));
    s_trampoline = (PlayerCalcView_t)tramp;

    // Patch target entry: jmp Hook (+ nop padding to kStealLen).
    DWORD oldProt = 0;
    if (!VirtualProtect(target, kStealLen, PAGE_EXECUTE_READWRITE, &oldProt)) {
        Log("VirtualProtect failed"); return false;
    }
    target[0] = 0xE9;
    *(int32_t*)(target + 1) = (int32_t)((BYTE*)&Hook_PlayerCalcView - (target + 5));
    for (size_t i = 5; i < kStealLen; ++i) target[i] = 0x90;  // nop
    DWORD tmp = 0;
    VirtualProtect(target, kStealLen, oldProt, &tmp);
    FlushInstructionCache(GetCurrentProcess(), target, kStealLen);

    Log(s_liveHmd      ? "eventPlayerCalcView hooked (LiveHmd via pose_math)"
        : s_sweepEnabled ? "eventPlayerCalcView hooked (synthetic sweep ON)"
                         : "eventPlayerCalcView hooked (passthrough)");
    return true;
}
