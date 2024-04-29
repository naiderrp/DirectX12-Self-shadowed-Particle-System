#pragma once

namespace Util
{
    static bool RunningApp = true;
}

struct Win32Application
{
    static LRESULT CALLBACK WndProc(HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam)

    {
        switch (msg)
        {
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                if (MessageBox(0, L"Are you sure you want to exit?",
                    L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    DestroyWindow(hwnd);
                }
            }
            return 0;

        case WM_DESTROY: // x button on top right corner of window was pressed
            Util::RunningApp = false;
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd,
            msg,
            wParam,
            lParam);
    }

	void Initialize(HINSTANCE hInstance, int ShowWnd)
	{
        if (mFullscreen)
        {
            HMONITOR hmon = MonitorFromWindow(hwnd,
                MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(hmon, &mi);

            Width = mi.rcMonitor.right - mi.rcMonitor.left;
            Height = mi.rcMonitor.bottom - mi.rcMonitor.top;
        }

        WNDCLASSEX wc;

        wc.cbSize = sizeof(WNDCLASSEX);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = Win32Application::WndProc;
        wc.cbClsExtra = NULL;
        wc.cbWndExtra = NULL;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
        wc.lpszMenuName = NULL;
        wc.lpszClassName = WindowName;
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

        if (!RegisterClassEx(&wc))
        {
            MessageBox(NULL, L"Error registering class", L"Error", MB_OK | MB_ICONERROR);
        }

        hwnd = CreateWindowEx(NULL,
            WindowName,
            WindowTitle,
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            Width, Height,
            NULL,
            NULL,
            hInstance,
            NULL);

        if (!hwnd)
        {
            MessageBox(NULL, L"Error creating window",
                L"Error", MB_OK | MB_ICONERROR);
        }

        if (mFullscreen)
        {
            SetWindowLong(hwnd, GWL_STYLE, 0);
        }

        ShowWindow(hwnd, ShowWnd);
        UpdateWindow(hwnd);
	}

    void UpdateFPS()
    {
        ++frameCount;

        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - fpsTimerStart).count();

        if (elapsed >= 1000) {
            double fps = static_cast<double>(frameCount) / (elapsed / 1000.0);

            std::wstring windowTitle = L"FPS: " + std::to_wstring(fps);
            SetWindowText(hwnd, windowTitle.c_str());

            frameCount = 0;
            fpsTimerStart = now;
        }
    }

public:
	HWND hwnd = NULL;

	LPCTSTR WindowName = L"App";
	LPCTSTR WindowTitle = L"Window";

	int Width = 800;
	int Height = 600;
	float m_aspectRatio = static_cast<float>(Width) / static_cast<float>(Height);

	bool mFullscreen = false;

    int frameCount = 0;
    std::chrono::time_point<std::chrono::steady_clock> fpsTimerStart;
};