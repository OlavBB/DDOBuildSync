#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <thread>
#include <atomic>
#include "config.h"
#include "git_manager.h"

constexpr UINT WM_APP_LOG        = WM_APP + 1;
constexpr UINT WM_APP_GIT_DONE   = WM_APP + 2;
constexpr UINT WM_APP_DDO_EXITED = WM_APP + 3;

// Control IDs
enum {
    ID_BTN_LAUNCH     = 101,
    ID_BTN_PULL       = 102,
    ID_BTN_PUSH       = 103,
    ID_BTN_SETUP      = 104,
    ID_EDIT_LOG       = 201,
    ID_STATIC_FOLDER  = 301,
    ID_STATIC_REPO    = 302,
    ID_STATIC_STATUS  = 303,
    ID_CHK_AUTOPUSH   = 401,
    ID_CHK_AUTOPULL   = 402,
};

class MainWindow {
public:
    bool Create(HINSTANCE hInstance);
    HWND GetHwnd() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    void OnCreate();
    void OnCommand(WPARAM wParam);
    void OnDestroy();

    // GUI creation
    void CreateControls();
    void UpdateStatusLabels();
    void SetStatus(const std::wstring& status);

    // Log output
    void AppendLog(const std::string& text);

    // Actions
    void OnLaunchDDOBuilder();
    void OnPull();
    void OnPush();
    void OnSetup();

    // Run git operation on background thread
    void RunAsync(std::function<void()> work);

    // DDO Builder process monitoring
    void MonitorDDOBuilder(HANDLE hProcess);

    // First-run setup dialog
    bool RunSetupDialog();

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInstance = nullptr;

    // Controls
    HWND m_btnLaunch = nullptr;
    HWND m_btnPull   = nullptr;
    HWND m_btnPush   = nullptr;
    HWND m_btnSetup  = nullptr;
    HWND m_editLog   = nullptr;
    HWND m_lblFolder = nullptr;
    HWND m_lblRepo   = nullptr;
    HWND m_lblStatus = nullptr;
    HWND m_chkAutoPush = nullptr;
    HWND m_chkAutoPull = nullptr;

    ConfigManager m_configMgr;
    GitManager m_gitMgr;

    std::atomic<bool> m_busy{false};
    std::atomic<bool> m_ddoRunning{false};
    std::thread m_workerThread;
    std::thread m_monitorThread;
};
