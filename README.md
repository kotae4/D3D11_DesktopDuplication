# D3D11_DesktopDuplication

Quick and dirty example of using the Desktop Duplication API to grab the desktop image and then rendering it with imgui.<br>
Started with the example_win32_directx11 project that comes with imgui and then cobbled the rest together using other people's work.<br>

## Helpful Links

First 6 can be summarized as "all of MSDN documentation" but these particular pages were the most relevant.
* https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nn-dxgi1_2-idxgioutputduplication
* https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutput1-duplicateoutput
* https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutputduplication-acquirenextframe
* https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createtexture2d
* https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11device-createshaderresourceview
* https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource
* https://github.com/ra1nty/DXcam/tree/main/dxcam/core
* https://github.com/roman380/DuplicationAndMediaFoundation/blob/master/WinDesktopDup.cpp
* https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#Example-for-DirectX11-users
* (and probably more that I closed out of before bookmarking, sorry)

## Building

You need winsdk installed in order for the d3d stuff to be present.<br>
You also need a device that supports most of the d3d feature levels.

## Running

You have to click 'Reload Outputs' button, I am a lazy programmer.<br>
You then have to click and select one of the outputs even though a default output is selected. It isn't actually selected until you click on it. Again, bad programmer here.<br>
After you do that it should start rendering the desktop image directly below the FPS text.<br>

The console window will have some debug messages. It might spam you, that's not good.<br>

**There might be resource leaks so don't leave this running 24/7.**<br>
*What are CComPtrs? Sounds like 'good programmer' things, so it's not for me.*