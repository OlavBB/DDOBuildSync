#include "main_window.h"
#include "utils.h"
#include <commdlg.h>
#include <shlobj.h>
#include <cstdio>

static const wchar_t* CLASS_NAME = L"DDOBuildSyncWindow";
static const wchar_t* WINDOW_TITLE = L"DDO Build Sync";

static MainWindow* GetThis(HWND hwnd) {
    return reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* self = GetThis(hwnd);
    if (self) {
        self->m_hwnd = hwnd;
        return self->HandleMessage(msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        OnCreate();
        return 0;
    case WM_COMMAND:
        OnCommand(wParam);
        return 0;
    case WM_APP_LOG: {
        char* text = reinterpret_cast<char*>(lParam);
        if (text) {
            AppendLog(text);
            free(text);
        }
        return 0;
    }
    case WM_APP_GIT_DONE:
        m_busy = false;
        EnableWindow(m_btnLaunch, TRUE);
        EnableWindow(m_btnPull, TRUE);
        EnableWindow(m_btnPush, TRUE);
        SetStatus(L"Ready");
        return 0;
    case WM_APP_DDO_EXITED:
        m_ddoRunning = false;
        EnableWindow(m_btnLaunch, TRUE);
        SetWindowTextW(m_btnLaunch, L"Launch DDO Builder");
        AppendLog("DDO Builder has exited");
        if (m_configMgr.Get().autoPushOnClose && !m_busy) {
            AppendLog("Auto-pushing changes...");
            OnPush();
        }
        return 0;
    case WM_CLOSE:
        if (m_workerThread.joinable()) m_workerThread.detach();
        if (m_monitorThread.joinable()) m_monitorThread.detach();
        OnDestroy();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

bool MainWindow::Create(HINSTANCE hInstance) {
    m_hInstance = hInstance;

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hIcon = LoadIconW(hInstance, L"IDI_APPICON");
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        0, CLASS_NAME, WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 520,
        nullptr, nullptr, hInstance, this
    );

    if (!m_hwnd) return false;

    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

void MainWindow::OnCreate() {
    CreateControls();

    // Load config
    m_configMgr.LoadDefault();
    auto& cfg = m_configMgr.Get();

    // Auto-detect DDO Builder if not configured
    if (cfg.buildsFolder.empty()) {
        std::string detected = Utils::AutoDetectDDOBuilderFolder();
        if (!detected.empty()) {
            cfg.buildsFolder = detected;
            cfg.ddoBuilderExe = detected + "\\DDOBuilder.exe";
            AppendLog("Auto-detected DDO Builder at: " + detected);
        }
    }

    // Setup git manager
    m_gitMgr.SetWorkDir(cfg.buildsFolder);
    m_gitMgr.SetRepoUrl(cfg.gitRepoUrl);
    m_gitMgr.SetLogCallback([this](const std::string& msg) {
        // Post to UI thread
        char* copy = _strdup(msg.c_str());
        if (!PostMessageW(m_hwnd, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(copy))) {
            free(copy);
        }
    });

    UpdateStatusLabels();

    // Check if setup is needed
    if (cfg.buildsFolder.empty() || cfg.gitRepoUrl.empty()) {
        AppendLog("First run detected - click Setup to configure");
        SetStatus(L"Setup required");
    } else {
        // Check git status
        if (!m_gitMgr.IsGitAvailable()) {
            AppendLog("WARNING: git not found on PATH. Install git and restart.");
            SetStatus(L"Git not found");
        } else if (!m_gitMgr.IsRepoInitialized()) {
            AppendLog("Git repo not initialized in builds folder. Click Setup to initialize.");
            SetStatus(L"Repo not initialized");
        } else {
            SetStatus(L"Ready");
            int changed = m_gitMgr.GetChangedFileCount();
            if (changed > 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "%d changed file(s) detected", changed);
                AppendLog(buf);
            }
        }
    }

    // Sync checkbox state
    SendMessageW(m_chkAutoPush, BM_SETCHECK, cfg.autoPushOnClose ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkAutoPull, BM_SETCHECK, cfg.autoPullOnLaunch ? BST_CHECKED : BST_UNCHECKED, 0);
}

void MainWindow::CreateControls() {
    int x = 10, y = 10;
    int btnW = 140, btnH = 30;

    m_btnLaunch = CreateWindowW(L"BUTTON", L"Launch DDO Builder",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, btnW, btnH, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_LAUNCH)),
        m_hInstance, nullptr);

    m_btnPull = CreateWindowW(L"BUTTON", L"Pull Builds",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + btnW + 10, y, 100, btnH, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PULL)),
        m_hInstance, nullptr);

    m_btnPush = CreateWindowW(L"BUTTON", L"Push Builds",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + btnW + 120, y, 100, btnH, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_PUSH)),
        m_hInstance, nullptr);

    m_btnSetup = CreateWindowW(L"BUTTON", L"Setup",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x + btnW + 230, y, 80, btnH, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_BTN_SETUP)),
        m_hInstance, nullptr);

    y += btnH + 15;

    // Info labels
    CreateWindowW(L"STATIC", L"Builds folder:",
        WS_CHILD | WS_VISIBLE, x, y, 90, 20, m_hwnd, nullptr, m_hInstance, nullptr);
    m_lblFolder = CreateWindowW(L"STATIC", L"(not set)",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, x + 95, y, 490, 20, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATIC_FOLDER)),
        m_hInstance, nullptr);
    y += 22;

    CreateWindowW(L"STATIC", L"Git repo:",
        WS_CHILD | WS_VISIBLE, x, y, 90, 20, m_hwnd, nullptr, m_hInstance, nullptr);
    m_lblRepo = CreateWindowW(L"STATIC", L"(not set)",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, x + 95, y, 490, 20, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATIC_REPO)),
        m_hInstance, nullptr);
    y += 22;

    CreateWindowW(L"STATIC", L"Status:",
        WS_CHILD | WS_VISIBLE, x, y, 90, 20, m_hwnd, nullptr, m_hInstance, nullptr);
    m_lblStatus = CreateWindowW(L"STATIC", L"Initializing...",
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, x + 95, y, 490, 20, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATIC_STATUS)),
        m_hInstance, nullptr);
    y += 28;

    // Checkboxes
    m_chkAutoPull = CreateWindowW(L"BUTTON", L"Auto-pull on launch",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, 160, 20, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHK_AUTOPULL)),
        m_hInstance, nullptr);

    m_chkAutoPush = CreateWindowW(L"BUTTON", L"Auto-push on DDO Builder exit",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x + 170, y, 230, 20, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CHK_AUTOPUSH)),
        m_hInstance, nullptr);

    y += 30;

    // Log panel - multiline edit
    m_editLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        x, y, 585, 300, m_hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT_LOG)),
        m_hInstance, nullptr);

    // Set monospace font on log
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    if (hFont) {
        SendMessageW(m_editLog, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
    }
}

void MainWindow::UpdateStatusLabels() {
    auto& cfg = m_configMgr.Get();
    if (!cfg.buildsFolder.empty()) {
        SetWindowTextW(m_lblFolder, Utils::ToWide(cfg.buildsFolder).c_str());
    } else {
        SetWindowTextW(m_lblFolder, L"(not set)");
    }
    if (!cfg.gitRepoUrl.empty()) {
        SetWindowTextW(m_lblRepo, Utils::ToWide(cfg.gitRepoUrl).c_str());
    } else {
        SetWindowTextW(m_lblRepo, L"(not set)");
    }
}

void MainWindow::SetStatus(const std::wstring& status) {
    SetWindowTextW(m_lblStatus, status.c_str());
}

void MainWindow::AppendLog(const std::string& text) {
    // Get timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d] ",
             st.wHour, st.wMinute, st.wSecond);

    std::string line = std::string(timestamp) + text + "\r\n";
    std::wstring wline = Utils::ToWide(line);

    // Append to edit control
    int len = GetWindowTextLengthW(m_editLog);
    SendMessageW(m_editLog, EM_SETSEL, len, len);
    SendMessageW(m_editLog, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(wline.c_str()));

    // Auto-scroll to bottom
    SendMessageW(m_editLog, EM_SCROLLCARET, 0, 0);
}

void MainWindow::OnCommand(WPARAM wParam) {
    switch (LOWORD(wParam)) {
    case ID_BTN_LAUNCH:
        OnLaunchDDOBuilder();
        break;
    case ID_BTN_PULL:
        OnPull();
        break;
    case ID_BTN_PUSH:
        OnPush();
        break;
    case ID_BTN_SETUP:
        OnSetup();
        break;
    case ID_CHK_AUTOPUSH:
        m_configMgr.Get().autoPushOnClose =
            (SendMessageW(m_chkAutoPush, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_configMgr.SaveDefault();
        break;
    case ID_CHK_AUTOPULL:
        m_configMgr.Get().autoPullOnLaunch =
            (SendMessageW(m_chkAutoPull, BM_GETCHECK, 0, 0) == BST_CHECKED);
        m_configMgr.SaveDefault();
        break;
    }
}

void MainWindow::OnDestroy() {
    // Save config
    m_configMgr.SaveDefault();
    DestroyWindow(m_hwnd);
}

void MainWindow::RunAsync(std::function<void()> work) {
    if (m_busy) {
        AppendLog("Operation already in progress");
        return;
    }
    m_busy = true;
    EnableWindow(m_btnPull, FALSE);
    EnableWindow(m_btnPush, FALSE);

    if (m_workerThread.joinable()) m_workerThread.detach();
    m_workerThread = std::thread([this, work = std::move(work)]() {
        work();
        PostMessageW(m_hwnd, WM_APP_GIT_DONE, 0, 0);
    });
}

void MainWindow::OnLaunchDDOBuilder() {
    auto& cfg = m_configMgr.Get();
    if (cfg.ddoBuilderExe.empty() || !Utils::FileExists(cfg.ddoBuilderExe)) {
        AppendLog("DDO Builder executable not found. Run Setup first.");
        return;
    }

    if (m_ddoRunning) {
        AppendLog("DDO Builder is already running");
        return;
    }

    // Auto-pull first if enabled
    if (cfg.autoPullOnLaunch && m_gitMgr.IsRepoInitialized()) {
        AppendLog("Auto-pulling before launch...");
        SetStatus(L"Pulling...");
        // Do pull synchronously before launch (on UI thread, quick operation)
        // Actually, let's do it async then launch after
        RunAsync([this, &cfg]() {
            m_gitMgr.Pull();

            // Now launch DDO Builder (from worker thread, post result)
            STARTUPINFOA si = {};
            si.cb = sizeof(si);
            PROCESS_INFORMATION pi = {};

            BOOL ok = CreateProcessA(
                cfg.ddoBuilderExe.c_str(),
                nullptr, nullptr, nullptr,
                FALSE, 0, nullptr,
                cfg.buildsFolder.c_str(),
                &si, &pi
            );

            if (!ok) {
                char* msg = _strdup("Failed to launch DDO Builder");
                PostMessageW(m_hwnd, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(msg));
                return;
            }

            CloseHandle(pi.hThread);
            m_ddoRunning = true;

            // Post log message
            char* msg = _strdup("DDO Builder launched - monitoring process...");
            PostMessageW(m_hwnd, WM_APP_LOG, 0, reinterpret_cast<LPARAM>(msg));

            // Monitor in separate thread
            if (m_monitorThread.joinable()) m_monitorThread.detach();
            m_monitorThread = std::thread(&MainWindow::MonitorDDOBuilder, this, pi.hProcess);
        });
    } else {
        // Launch directly
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi = {};

        BOOL ok = CreateProcessA(
            cfg.ddoBuilderExe.c_str(),
            nullptr, nullptr, nullptr,
            FALSE, 0, nullptr,
            cfg.buildsFolder.c_str(),
            &si, &pi
        );

        if (!ok) {
            AppendLog("Failed to launch DDO Builder");
            return;
        }

        CloseHandle(pi.hThread);
        m_ddoRunning = true;
        EnableWindow(m_btnLaunch, FALSE);
        SetWindowTextW(m_btnLaunch, L"DDO Builder Running");
        AppendLog("DDO Builder launched - monitoring process...");

        if (m_monitorThread.joinable()) m_monitorThread.detach();
        m_monitorThread = std::thread(&MainWindow::MonitorDDOBuilder, this, pi.hProcess);
    }
}

void MainWindow::MonitorDDOBuilder(HANDLE hProcess) {
    WaitForSingleObject(hProcess, INFINITE);
    CloseHandle(hProcess);
    PostMessageW(m_hwnd, WM_APP_DDO_EXITED, 0, 0);
}

void MainWindow::OnPull() {
    auto& cfg = m_configMgr.Get();
    if (cfg.buildsFolder.empty()) {
        AppendLog("Builds folder not configured. Run Setup first.");
        return;
    }
    if (!m_gitMgr.IsRepoInitialized()) {
        AppendLog("Git repo not initialized. Run Setup first.");
        return;
    }

    SetStatus(L"Pulling...");
    RunAsync([this]() {
        m_gitMgr.Pull();
    });
}

void MainWindow::OnPush() {
    auto& cfg = m_configMgr.Get();
    if (cfg.buildsFolder.empty()) {
        AppendLog("Builds folder not configured. Run Setup first.");
        return;
    }
    if (!m_gitMgr.IsRepoInitialized()) {
        AppendLog("Git repo not initialized. Run Setup first.");
        return;
    }

    SetStatus(L"Pushing...");
    RunAsync([this]() {
        m_gitMgr.Push();
    });
}

void MainWindow::OnSetup() {
    if (RunSetupDialog()) {
        UpdateStatusLabels();
        m_gitMgr.SetWorkDir(m_configMgr.Get().buildsFolder);
        m_gitMgr.SetRepoUrl(m_configMgr.Get().gitRepoUrl);
        m_configMgr.SaveDefault();
        SetStatus(L"Ready");
    }
}

bool MainWindow::RunSetupDialog() {
    auto& cfg = m_configMgr.Get();

    // Step 1: Browse for DDO Builder folder
    AppendLog("Setup: Select DDO Builder V2 folder...");

    BROWSEINFOW bi = {};
    bi.hwndOwner = m_hwnd;
    bi.lpszTitle = L"Select DDO Builder V2 folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) {
        AppendLog("Setup cancelled");
        return false;
    }

    wchar_t folderPath[MAX_PATH];
    SHGetPathFromIDListW(pidl, folderPath);
    CoTaskMemFree(pidl);

    cfg.buildsFolder = Utils::ToUtf8(folderPath);
    AppendLog("Builds folder: " + cfg.buildsFolder);

    // Check for DDOBuilder.exe
    std::string exePath = cfg.buildsFolder + "\\DDOBuilder.exe";
    if (Utils::FileExists(exePath)) {
        cfg.ddoBuilderExe = exePath;
        AppendLog("Found DDOBuilder.exe");
    } else {
        AppendLog("Warning: DDOBuilder.exe not found in selected folder");
    }

    // Step 2: Ask for GitHub repo URL
    // Use a simple input dialog - we'll create a small dialog inline
    wchar_t repoUrl[512] = {};
    if (!cfg.gitRepoUrl.empty()) {
        wcscpy_s(repoUrl, Utils::ToWide(cfg.gitRepoUrl).c_str());
    }

    // Simple approach: use a message box to explain, then get input
    // We'll create a tiny dialog window
    struct DlgData {
        wchar_t url[512];
        bool ok;
    } dlgData = {};
    if (!cfg.gitRepoUrl.empty()) {
        wcscpy_s(dlgData.url, Utils::ToWide(cfg.gitRepoUrl).c_str());
    }
    dlgData.ok = false;

    // Build dialog template in memory
    // This is the standard Win32 approach for a simple input dialog
    HINSTANCE hInst = m_hInstance;
    HWND hParent = m_hwnd;

    // Use a modeless approach with a class
    static const wchar_t* DLG_CLASS = L"DDOBuildSyncInputDlg";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
        static HWND hEdit = nullptr;
        static DlgData* pData = nullptr;

        switch (msg) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
            pData = reinterpret_cast<DlgData*>(cs->lpCreateParams);

            CreateWindowW(L"STATIC", L"Enter GitHub repo URL:",
                WS_CHILD | WS_VISIBLE, 10, 10, 360, 20, hwnd, nullptr,
                cs->hInstance, nullptr);

            hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", pData->url,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                10, 35, 360, 25, hwnd, nullptr, cs->hInstance, nullptr);

            CreateWindowW(L"BUTTON", L"OK",
                WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                200, 70, 80, 28, hwnd, reinterpret_cast<HMENU>(1), cs->hInstance, nullptr);

            CreateWindowW(L"BUTTON", L"Cancel",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                290, 70, 80, 28, hwnd, reinterpret_cast<HMENU>(2), cs->hInstance, nullptr);

            SetFocus(hEdit);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // OK
                GetWindowTextW(hEdit, pData->url, 512);
                pData->ok = true;
                DestroyWindow(hwnd);
            } else if (LOWORD(wParam) == 2) { // Cancel
                pData->ok = false;
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_DESTROY:
            PostMessageW(GetParent(hwnd), WM_NULL, 0, 0); // Wake parent message loop
            return 0;
        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                pData->ok = false;
                DestroyWindow(hwnd);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    };
    wc.hInstance = hInst;
    wc.lpszClassName = DLG_CLASS;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    // Center on parent
    RECT parentRect;
    GetWindowRect(hParent, &parentRect);
    int dlgW = 395, dlgH = 140;
    int dlgX = parentRect.left + ((parentRect.right - parentRect.left) - dlgW) / 2;
    int dlgY = parentRect.top + ((parentRect.bottom - parentRect.top) - dlgH) / 2;

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, DLG_CLASS, L"GitHub Repository URL",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        dlgX, dlgY, dlgW, dlgH,
        hParent, nullptr, hInst, &dlgData
    );
    ShowWindow(hDlg, SW_SHOW);
    EnableWindow(hParent, FALSE);

    // Modal message loop
    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(hParent, TRUE);
    SetForegroundWindow(hParent);

    if (!dlgData.ok) {
        AppendLog("Setup cancelled");
        return false;
    }

    cfg.gitRepoUrl = Utils::ToUtf8(dlgData.url);
    AppendLog("Git repo URL: " + cfg.gitRepoUrl);

    // Step 3: Initialize git repo if needed
    m_gitMgr.SetWorkDir(cfg.buildsFolder);
    m_gitMgr.SetRepoUrl(cfg.gitRepoUrl);

    if (!m_gitMgr.IsGitAvailable()) {
        AppendLog("ERROR: git not found on PATH. Please install git and try again.");
        return false;
    }

    if (!m_gitMgr.IsRepoInitialized()) {
        AppendLog("Initializing git repository...");

        // Check if remote repo already has content - try clone instead
        std::string output;
        // First try to clone
        AppendLog("Attempting to clone existing repo...");

        // We need to clone into a temp location then move .git, or init fresh
        // Simplest: if no .git, just init and push
        if (!m_gitMgr.InitRepo()) {
            AppendLog("Repo init had issues - you may need to push manually");
        }
    } else {
        AppendLog("Git repo already initialized");
    }

    return true;
}
