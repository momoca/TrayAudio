#include <iostream>
#include <windows.h>
#include <shellapi.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <cmath>
#include <chrono>
#include <thread>
#include <gdiplus.h>
#include <vector>
#include <comdef.h> // 用于错误代码解释

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ole32.lib")

using namespace Gdiplus;
using namespace std;

// 全局变量
#define WM_TRAYICON (WM_USER + 1)
HMENU hMenu;
HINSTANCE hInst;
NOTIFYICONDATA nid1 = {};
UINT WM_TASKBAR = 0;
IAudioCaptureClient *pCaptureClient = nullptr;
IAudioClient *pAudioClient = nullptr;
WAVEFORMATEX *pwfx = nullptr;
HICON previousIcon1 = nullptr;
vector<float> waveform1(16, 0.0f);

// 定义任务栏图标的菜单项ID
#define ID_TRAY_EXIT 1001

// 初始化COM并获取默认音频设备
bool initAudioDevice(IAudioClient **pAudioClient, WAVEFORMATEX **pwfx)
{
    HRESULT hr;
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;

    hr = CoInitialize(NULL);
    if (FAILED(hr))
    {
        cerr << "Failed to initialize COM." << endl;
        return false;
    }

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&pEnumerator);
    if (FAILED(hr))
    {
        cerr << "Failed to create device enumerator." << endl;
        return false;
    }

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr))
    {
        cerr << "Failed to get default audio endpoint." << endl;
        return false;
    }

    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)pAudioClient);
    if (FAILED(hr))
    {
        cerr << "Failed to activate audio client." << endl;
        return false;
    }

    hr = (*pAudioClient)->GetMixFormat(pwfx);
    if (FAILED(hr))
    {
        cerr << "Failed to get mix format." << endl;
        return false;
    }

    return true;
}

// 添加托盘图标
bool AddTrayIcon(HWND hWnd, NOTIFYICONDATA &nid, UINT uID, const char *tip)
{
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = uID;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON; // 确保此处是 WM_TRAYICON
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strcpy_s(nid.szTip, tip);

    if (!Shell_NotifyIcon(NIM_ADD, &nid))
    {
        cerr << "Failed to add tray icon: " << GetLastError() << endl;
        return false;
    }
    return true;
}

// 更新托盘图标
void UpdateTrayIcon(NOTIFYICONDATA &nid, HICON &previousIcon, HICON hIcon)
{
    // 仅在图标不同的情况下更新
    if (previousIcon != hIcon)
    {
        nid.hIcon = hIcon;
        Shell_NotifyIcon(NIM_MODIFY, &nid);
        if (previousIcon)
        {
            DestroyIcon(previousIcon);
        }
        previousIcon = hIcon;
    }
}

// 删除托盘图标
void RemoveTrayIcon(NOTIFYICONDATA &nid, HICON &previousIcon)
{
    if (!Shell_NotifyIcon(NIM_DELETE, &nid))
    {
        cerr << "Failed to remove tray icon: " << GetLastError() << endl;
    }
    if (previousIcon)
    {
        DestroyIcon(previousIcon);
    }
}

// 处理任务栏图标的右键菜单
void ShowContextMenu(HWND hWnd)
{
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hWnd);
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
}

// 窗口过程
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP)
        {
            ShowContextMenu(hWnd);
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT)
        {
            RemoveTrayIcon(nid1, previousIcon1);
            PostQuitMessage(0);
        }
        break;
    case WM_DESTROY:
        RemoveTrayIcon(nid1, previousIcon1);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// 创建不同深度的灰色
Color GetGrayColor(float level)
{
    const BYTE MIN_GRAY_VALUE = static_cast<BYTE>(0.3f * 255); // 30% 的灰度值
    BYTE gray = static_cast<BYTE>(255 * (1.0f - level));       // 数值越高，颜色越深
    if (gray < MIN_GRAY_VALUE)
    {
        gray = MIN_GRAY_VALUE;
    }
    return Color(255, gray, gray, gray);
}

// 创建动态图标
HICON CreateDynamicIcon(const vector<float> &waveform)
{
    Bitmap bitmap(16, 16);
    Graphics graphics(&bitmap);
    graphics.Clear(Color(255, 68, 68, 68));

    for (size_t i = 0; i < waveform.size(); ++i)
    {
        float level = waveform[i];
        int height = static_cast<int>(level * 16);
        SolidBrush brush(GetGrayColor(level));
        graphics.FillRectangle(&brush, static_cast<REAL>(i), static_cast<REAL>(16 - height), 1.0f, static_cast<REAL>(height));
    }

    HICON hIcon;
    bitmap.GetHICON(&hIcon);
    return hIcon;
}

// 更新波形数据
void UpdateWaveform(vector<float> &waveform, const vector<float> &newValues)
{
    waveform = newValues;
}

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // 初始化GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    hInst = hInstance;
    WM_TASKBAR = RegisterWindowMessageA("TaskbarCreated");

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = TEXT("TaskbarIconClass");
    if (!RegisterClass(&wc))
    {
        cerr << "Failed to register window class: " << GetLastError() << endl;
        return 1;
    }

    HWND hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        TEXT("Taskbar Icon"),
        0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        HWND_MESSAGE,
        NULL,
        hInstance,
        NULL);

    if (!hWnd)
    {
        cerr << "Failed to create window: " << GetLastError() << endl;
        return 1;
    }

    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, "Exit");
    if (!AddTrayIcon(hWnd, nid1, 1, "TrayAudio"))
    {
        return 1;
    }

    if (!initAudioDevice(&pAudioClient, &pwfx))
    {
        return 1;
    }

    HRESULT hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, pwfx, NULL);
    if (FAILED(hr))
    {
        cerr << "Failed to initialize audio client: " << _com_error(hr).ErrorMessage() << endl;
        return 1;
    }

    hr = pAudioClient->Start();
    if (FAILED(hr))
    {
        cerr << "Failed to start audio client: " << _com_error(hr).ErrorMessage() << endl;
        return 1;
    }

    hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void **)&pCaptureClient);
    if (FAILED(hr))
    {
        cerr << "Failed to get capture client: " << _com_error(hr).ErrorMessage() << endl;
        return 1;
    }

    MSG msg;
    bool keepRunning = true;
    while (keepRunning)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                keepRunning = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        UINT32 packetLength = 0;
        hr = pCaptureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr))
        {
            cerr << "Failed to get next packet size: " << _com_error(hr).ErrorMessage() << endl;
            break;
        }

        if (packetLength > 0)
        {
            BYTE *pData;
            UINT32 numFramesAvailable;
            DWORD flags;

            hr = pCaptureClient->GetBuffer(&pData, &numFramesAvailable, &flags, NULL, NULL);
            if (SUCCEEDED(hr))
            {
                float *ptr = reinterpret_cast<float *>(pData);
                vector<float> newWaveform1(16, 0.0f);

                for (size_t i = 0; i < 16; ++i)
                {
                    newWaveform1[i] = max(0.0f, abs(ptr[i % numFramesAvailable]));
                }

                pCaptureClient->ReleaseBuffer(numFramesAvailable);

                // 更新波形数据
                UpdateWaveform(waveform1, newWaveform1);

                // 更新图标
                HICON hIcon1 = CreateDynamicIcon(waveform1);
                UpdateTrayIcon(nid1, previousIcon1, hIcon1);

                // 等待一段时间
                Sleep(50);
            }
            else
            {
                cerr << "Failed to get buffer: " << _com_error(hr).ErrorMessage() << endl;
            }
        }
        else
        {
            Sleep(50);
        }
    }
    DestroyMenu(hMenu);
    pAudioClient->Stop();
    CoUninitialize();
    GdiplusShutdown(gdiplusToken);

    return 0;
}
