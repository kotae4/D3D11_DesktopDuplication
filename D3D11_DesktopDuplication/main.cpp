/*
* HELPFUL LINKS:
* ALL OF MSDN DOCUMENTATION
* https://github.com/roman380/DuplicationAndMediaFoundation/blob/master/WinDesktopDup.cpp
* https://github.com/ocornut/imgui/wiki/Image-Loading-and-Displaying-Examples#Example-for-DirectX11-users
* https://github.com/ra1nty/DXcam/tree/main/dxcam/core
* NOTE:
* This started as a copy of the example d3d11_win32 backend that comes with imgui
* Everything new is cobbled together from the above links (and probably more that I closed out of before bookmarking, sorry)
*/

#include "main.h"
#include <stdio.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <dxgi1_2.h>

#include <map>
#include <vector>
#include <string>
#include <format>

#include "gui-helpers.h"

// Data
static ID3D11Device* g_pd3dDevice = NULL;
static ID3D11DeviceContext* g_pd3dDeviceContext = NULL;
static IDXGISwapChain* g_pSwapChain = NULL;
static ID3D11RenderTargetView* g_mainRenderTargetView = NULL;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static std::vector<IDXGIAdapter1*> g_Adapters;
static std::vector<std::string> g_AdapterDescriptions;
static int g_SelectedAdapterIndex = 0;
static std::vector<IDXGIOutput1*> g_Outputs;
static std::vector<std::string> g_OutputDescriptions;
static int g_SelectedOutputIndex = 0;
static IDXGIOutputDuplication* g_pDuplication = NULL;
static ID3D11Texture2D* DesktopFrameCopyTex = NULL;
static ID3D11ShaderResourceView* DesktopFrameCopySRView = NULL;

bool PrepareDesktopFrameCopyTexture(IDXGIOutput1* output, ID3D11Device* device)
{
    if (DesktopFrameCopyTex != NULL)
    {
        DesktopFrameCopyTex->Release();
        DesktopFrameCopyTex = NULL;
    }

    HRESULT result = 0;
    DXGI_OUTPUT_DESC outputDesc = { 0 };
    result = output->GetDesc(&outputDesc);
    if (FAILED(result))
    {
        printf("[WARN] Failed to get description for output\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = { 0 };
    desc.Width = outputDesc.DesktopCoordinates.right - outputDesc.DesktopCoordinates.left;
    desc.Height = outputDesc.DesktopCoordinates.bottom - outputDesc.DesktopCoordinates.top;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    result = device->CreateTexture2D(&desc, NULL, &DesktopFrameCopyTex);
    if (FAILED(result))
    {
        printf("[WARN] Failed to create DesktopFrameCopy Texture [%x]\n", result);
        return false;
    }

    return true;
}

bool LoadAdapters()
{
    printf("Loading adapters\n");
    HRESULT result = 0;
    IDXGIFactory1* factory = NULL;
    result = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory);
    if (FAILED(result))
    {
        return false;
    }
    UINT index = 0;
    IDXGIAdapter1* pAdapter;
    DXGI_ADAPTER_DESC1 desc;
    while (result != DXGI_ERROR_NOT_FOUND)
    {
        result = factory->EnumAdapters1(index, &pAdapter);
        index++;
        if (FAILED(result))
        {
            break;
        }
        else if (result != DXGI_ERROR_NOT_FOUND)
        {
            result = pAdapter->GetDesc1(&desc);
            if (FAILED(result))
            {
                pAdapter->Release();
                printf("[WARN] Failed to get description for adapter at index %d\n", index-1);
                continue;
            }
            size_t stringLength = 0;
            char derp[128];
            int error = wcstombs_s(&stringLength, derp, 128, desc.Description, 128);
            if (error != 0)
            {
                pAdapter->Release();
                printf("[WARN] Failed to convert adapter description to char (%S)[%d]\n", desc.Description, error);
                continue;
            }
            g_Adapters.push_back(pAdapter);
            g_AdapterDescriptions.push_back(std::format("[{}] {} ({} MB VRAM)", index-1, derp, desc.DedicatedVideoMemory / 1024 / 1024));
        }
    }
    if (FAILED(result))
    {
        printf("Failed to load adapters\n");
        return false;
    }
    printf("Done loading adapters\n");
    return true;
}

void CleanupAdapters()
{
    printf("Cleaning up adapters\n");
    for (IDXGIAdapter1* adapter : g_Adapters)
    {
        adapter->Release();
    }
    g_Adapters.clear();
    g_AdapterDescriptions.clear();
    g_SelectedAdapterIndex = 0;
    printf("Done cleaning up adapters\n");
}

bool LoadOutputs(IDXGIAdapter1* adapter)
{
    printf("Loading outputs for adapter %d\n", g_SelectedAdapterIndex);
    HRESULT result = 0;
    UINT index = 0;
    IDXGIOutput* pOutput;
    IDXGIOutput1* pUpgradedOutput;
    DXGI_OUTPUT_DESC desc;
    MONITORINFOEX monitorDesc = { 0 };
    while (result != DXGI_ERROR_NOT_FOUND)
    {
        result = adapter->EnumOutputs(index, &pOutput);
        index++;
        if (FAILED(result))
        {
            break;
        }
        else if (result != DXGI_ERROR_NOT_FOUND)
        {
            result = pOutput->GetDesc(&desc);
            if ((FAILED(result)) || (desc.Monitor == INVALID_HANDLE_VALUE))
            {
                pOutput->Release();
                printf("[WARN] Failed to get description for output at index %d\n", index-1);
                continue;
            }
            monitorDesc.cbSize = sizeof(MONITORINFOEX);
            BOOL success = GetMonitorInfo(desc.Monitor, &monitorDesc);
            if (success == 0)
            {
                pOutput->Release();
                int lastErrorCode = GetLastError();
                printf("[WARN] Failed to get monitor info for output at index %d (monitor: %d)[%d : %d]\n", index-1, desc.Monitor, success, lastErrorCode);
                continue;
            }
            size_t stringLength = 0;
            char derpDevice[32];
            char derpMonitor[32];
            int error = wcstombs_s(&stringLength, derpDevice, 32, desc.DeviceName, 32);
            if (error != 0)
            {
                pOutput->Release();
                printf("[WARN] Failed to convert output description to char (%S)[%d]\n", desc.DeviceName, error);
                continue;
            }
            error = wcstombs_s(&stringLength, derpMonitor, 32, monitorDesc.szDevice, 32);
            if (error != 0)
            {
                pOutput->Release();
                printf("[WARN] Failed to convert monitor description to char (%S)[%d]\n", monitorDesc.szDevice, error);
                continue;
            }

            result = pOutput->QueryInterface(__uuidof(IDXGIOutput1), (void**)&pUpgradedOutput);
            if (FAILED(result))
            {
                pOutput->Release();
                printf("[WARN] Failed to upgrade the output\n");
                continue;
            }
            pOutput->Release();
            pOutput = 0;
            g_Outputs.push_back(pUpgradedOutput);
            g_OutputDescriptions.push_back(std::format("[{}] {} (Monitor: {})", index-1, derpDevice, derpMonitor));
        }
    }
    if (FAILED(result))
    {
        printf("Failed to load outputs\n");
        return false;
    }
    printf("Done loading outputs\n");
    return true;
}

void CleanupOutputs()
{
    printf("Cleaning up outputs\n");
    for (IDXGIOutput* output : g_Outputs)
    {
        output->Release();
    }
    g_Outputs.clear();
    g_OutputDescriptions.clear();
    g_SelectedOutputIndex = 0;
    printf("Done cleaning up outputs\n");
}

void ReAcquireDuplicationInterface()
{
    if ((g_Outputs.size() <= g_SelectedOutputIndex) || (g_pd3dDevice == NULL))
        return;

    if (g_pDuplication != NULL)
        g_pDuplication->Release();

    HRESULT result = g_Outputs[g_SelectedOutputIndex]->DuplicateOutput(g_pd3dDevice, &g_pDuplication);
    if (FAILED(result))
    {
        printf("Failed to get duplication interface [%d]\n", result);
        g_pDuplication = NULL;
    }
}

bool CaptureDesktopFrame()
{
    HRESULT result = 0;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = { 0 };
    IDXGIResource* frameResource = NULL;
    ID3D11Texture2D* frameTexture = NULL;
    result = g_pDuplication->AcquireNextFrame(0, &frameInfo, &frameResource);
    if (result == DXGI_ERROR_ACCESS_LOST)
    {
        g_pDuplication->Release();
        ReAcquireDuplicationInterface();
    }
    else if (result == DXGI_ERROR_WAIT_TIMEOUT)
    {
        // the desktop hasn't updated yet, there is no new data to be captured.
        return true;
    }
    else if (FAILED(result))
    {
        printf("[WARN] Failed to acquire next desktop frame [%x]\n", result);
        return false;
    }
    result = frameResource->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&frameTexture);
    frameResource->Release();
    frameResource = NULL;

    if ((g_pd3dDeviceContext != NULL) && (frameTexture != NULL) && (DesktopFrameCopyTex != NULL))
    {
        g_pd3dDeviceContext->CopyResource(DesktopFrameCopyTex, frameTexture);
    }
}

// Main code
int main(int, char**)
{
    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"ImGui Example", NULL };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName,
        L"Dear ImGui DirectX11 Example",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1280, 800,
        NULL,
        NULL,
        wc.hInstance,
        NULL);

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // do our first-time init
    if (LoadAdapters() == false)
        printf("[INIT] Failed to load adapters\n");

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            ImGui::Begin("Hello, world!");

            ImGui::Text("This is some useful text.");
            
            if (g_AdapterDescriptions.size() > 0)
            {
                if (CustomListBoxInt("Adapter", &g_SelectedAdapterIndex, g_AdapterDescriptions))
                {
                    CleanupOutputs();
                    LoadOutputs(g_Adapters[g_SelectedAdapterIndex]);
                }

                if (g_OutputDescriptions.size() > 0)
                {
                    if (CustomListBoxInt("Output", &g_SelectedOutputIndex, g_OutputDescriptions))
                    {
                        // do the duplication api stuff
                        if (PrepareDesktopFrameCopyTexture(g_Outputs[g_SelectedOutputIndex], g_pd3dDevice) == false)
                        {
                            printf("[ERROR] Could not prepare DesktopFrameCopy Texture\n");
                        }
                        else if (DesktopFrameCopyTex != NULL)
                        {
                            if (DesktopFrameCopySRView != NULL)
                            {
                                DesktopFrameCopySRView->Release();
                                DesktopFrameCopySRView = NULL;
                            }
                            printf("Creating ShaderResourceView for the DesktopFrameCopyTex\n");
                            HRESULT result = g_pd3dDevice->CreateShaderResourceView(DesktopFrameCopyTex, NULL, &DesktopFrameCopySRView);
                            if (FAILED(result))
                            {
                                DesktopFrameCopyTex->Release();
                                DesktopFrameCopyTex = NULL;
                                printf("Could not prepare DesktopFromCopy ShaderResourceView\n");
                            }
                            else
                            {
                                ReAcquireDuplicationInterface();
                            }
                        }
                    }
                }
                else
                {
                    if (ImGui::Button("Reload outputs"))
                    {
                        CleanupOutputs();
                        LoadOutputs(g_Adapters[g_SelectedAdapterIndex]);
                    }
                }
            }
            else
            {
                if (ImGui::Button("Reload adapters"))
                {
                    CleanupOutputs();
                    CleanupAdapters();
                    LoadAdapters();
                }
            }

            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

            if (g_pDuplication != NULL)
            {
                if (CaptureDesktopFrame() == false)
                {
                    printf("[WARN] Failed to capture desktop frame\n");
                }
                if (DesktopFrameCopySRView != NULL)
                {
                    ImVec2 windowSize = ImGui::GetWindowSize();
                    ImGui::Image((void*)DesktopFrameCopySRView, ImVec2(windowSize.x, windowSize.y - 120.f));
                }
                g_pDuplication->ReleaseFrame();
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, NULL);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Cleanup
    CleanupAdapters();
    CleanupOutputs();
    if (g_pDuplication != NULL)
    {
        g_pDuplication->ReleaseFrame();
        g_pDuplication->Release();
        g_pDuplication = NULL;
    }
    if (DesktopFrameCopySRView != NULL)
    {
        DesktopFrameCopySRView->Release();
        DesktopFrameCopySRView = NULL;
    }
    if (DesktopFrameCopyTex != NULL)
    {
        DesktopFrameCopyTex->Release();
        DesktopFrameCopyTex = NULL;
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = NULL; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = NULL; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = NULL; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = NULL; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_pd3dDevice != NULL && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
