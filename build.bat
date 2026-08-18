@echo off
REM Rebuilds the XIII VR mod D3DDrv.dll proxy (32-bit) with MSVC.
REM Output: %~dp0proxy\D3DDrv.dll  -> copy into the game's system\ folder
REM (keep the stock render device there as D3DDrv_Original.dll; the proxy
REM chain-loads it). Requires VS2022 Build Tools; adjust the vcvars32 path
REM if your edition/install location differs.
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat" >nul
if errorlevel 1 (
  echo vcvars32.bat not found -- fix the path above for this machine.
  exit /b 1
)
cd /d "%~dp0proxy"
cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /LD ^
  dllmain.cpp frame_capture.cpp camera_hook.cpp vr_host.cpp openxr_host.cpp steamvr_host.cpp shutdown_hook.cpp ..\pose_math\pose_math.cpp ^
  /I..\third_party\openxr\include /I..\third_party\openvr\headers /I..\pose_math ^
  /Fe:D3DDrv.dll ^
  /link /DEF:proxy.def /DELAYLOAD:openxr_loader.dll /DELAYLOAD:openvr_api.dll ^
  d3d11.lib dxgi.lib delayimp.lib user32.lib ^
  ..\third_party\openvr\lib\openvr_api.lib ..\third_party\openxr\lib\openxr_loader.lib
exit /b %errorlevel%
