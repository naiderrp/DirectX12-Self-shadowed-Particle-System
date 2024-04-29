#pragma once
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib") // fixes the unresolved external symbol _IID_ID3D12Device error
                                   // or add #include <initguid.h> before including d3d12.h

#include <dxgidebug.h>
#include <D3D12SDKLayers.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <chrono>

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
#if defined(CreateWindow)
#undef CreateWindow
#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>

/*
    When using runtime compiled HLSL shaders using any of the D3DCompiler functions, do not 
    forget to link against the D3Dcompiler_47.lib library and copy the D3dcompiler_47.dll 
    to the same folder as the binary executable when distributing your project.
    
    A redistributable version of the D3dcompiler_47.dll file can be found in the 
        Windows 10 SDK installation folder at C:\Program Files (x86)\Windows Kits\10\Redist\D3D\.

    For more information, refer to the MSDN blog post at: 
        https://blogs.msdn.microsoft.com/chuckw/2012/05/07/hlsl-fxc-and-d3dcompile/
*/

#include <d3dcompiler.h>

#include <DirectXMath.h>

#include "d3dx12.h"
#include <string>

#include <fstream>

