#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <process.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ID_BTN_START            1001
#define ID_BTN_STOP             1002
#define ID_BTN_REFRESH          1003
#define ID_BTN_UPLOAD           1004
#define ID_BTN_DOWNLOAD         1005
#define ID_BTN_BROWSE_UPLOAD    1006
#define ID_TIMER_REFRESH        2001
#define WM_APP_TRANSFER_RESULT  (WM_APP + 1)

#define TIMER_INTERVAL_MS       2000
#define MAX_LOG_BYTES           (200 * 1024)

typedef struct {
    HWND hwnd;
    HWND hStatus;
    HWND hBtnStart;
    HWND hBtnStop;
    HWND hBtnRefresh;
    HWND hServerFiles;
    HWND hClientFiles;
    HWND hClientActivity;
    HWND hLogView;
    HWND hThroughput;
    HWND hError;
    HWND hLblUploadLocal;
    HWND hLblUploadRemote;
    HWND hLblDownloadRemote;
    HWND hLblDownloadLocal;
    HWND hUploadLocal;
    HWND hUploadRemote;
    HWND hDownloadRemote;
    HWND hDownloadLocal;
    HWND hBtnUpload;
    HWND hBtnDownload;
    HWND hBtnBrowse;
    HFONT hFont;
    HANDLE hServerProcess;
    char baseDir[MAX_PATH];
    char clientDir[MAX_PATH];
} AppState;

typedef struct {
    AppState* app;
    BOOL upload;
    char localPath[MAX_PATH];
    char remoteName[256];
} TransferTask;

static const char* SERVER_EXE_NAME = "tftp_server.exe";
static const char* LOG_RELATIVE_PATH = "logs\\tftp_server.log";
static const char* SERVER_ROOT_RELATIVE = "tftp_root";
static const char* CLIENT_CACHE_DIR = "client_workspace";

static void set_control_font(HWND control, HFONT font) {
    SendMessage(control, WM_SETFONT, (WPARAM)font, TRUE);
}

static void ensure_directories(AppState* state) {
    char path[MAX_PATH];

    PathCombineA(path, state->baseDir, "logs");
    CreateDirectoryA(path, NULL);

    PathCombineA(path, state->baseDir, SERVER_ROOT_RELATIVE);
    CreateDirectoryA(path, NULL);

    PathCombineA(state->clientDir, state->baseDir, CLIENT_CACHE_DIR);
    CreateDirectoryA(state->clientDir, NULL);
}

static void update_server_status(AppState* state) {
    if (!state) {
        return;
    }

    if (state->hServerProcess == NULL) {
        SetWindowTextA(state->hStatus, "Server not running");
        EnableWindow(state->hBtnStart, TRUE);
        EnableWindow(state->hBtnStop, FALSE);
        return;
    }

    DWORD exit_code = 0;
    if (GetExitCodeProcess(state->hServerProcess, &exit_code) && exit_code == STILL_ACTIVE) {
        SetWindowTextA(state->hStatus, "Server running");
        EnableWindow(state->hBtnStart, FALSE);
        EnableWindow(state->hBtnStop, TRUE);
    } else {
        CloseHandle(state->hServerProcess);
        state->hServerProcess = NULL;
        SetWindowTextA(state->hStatus, "Server stopped");
        EnableWindow(state->hBtnStart, TRUE);
        EnableWindow(state->hBtnStop, FALSE);
    }
}

static void populate_listview_columns(HWND list, const char* col1, int w1, const char* col2, int w2) {
    LVCOLUMNA col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH;

    col.pszText = (LPSTR)col1;
    col.cx = w1;
    ListView_InsertColumn(list, 0, &col);

    col.pszText = (LPSTR)col2;
    col.cx = w2;
    ListView_InsertColumn(list, 1, &col);
}

static void populate_activity_columns(HWND list) {
    LVCOLUMNA col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH;

    col.pszText = "Client";
    col.cx = 160;
    ListView_InsertColumn(list, 0, &col);

    col.pszText = "Event";
    col.cx = 520;
    ListView_InsertColumn(list, 1, &col);
}

static void refresh_server_files(AppState* state) {
    if (!state) {
        return;
    }
    ListView_DeleteAllItems(state->hServerFiles);

    char search_path[MAX_PATH];
    PathCombineA(search_path, state->baseDir, SERVER_ROOT_RELATIVE);
    PathAppendA(search_path, "*.*");

    WIN32_FIND_DATAA data;
    HANDLE h_find = FindFirstFileA(search_path, &data);
    if (h_find == INVALID_HANDLE_VALUE) {
        return;
    }

    int index = 0;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
    ULONGLONG size = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    char size_text[64];
    StringCchPrintfA(size_text, ARRAYSIZE(size_text), "%I64u", size);

        LVITEMA item = {0};
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.pszText = data.cFileName;
        ListView_InsertItem(state->hServerFiles, &item);
        ListView_SetItemText(state->hServerFiles, index, 1, size_text);
        ++index;
    } while (FindNextFileA(h_find, &data));

    FindClose(h_find);
}

static void refresh_client_files(AppState* state) {
    if (!state) {
        return;
    }
    ListView_DeleteAllItems(state->hClientFiles);

    char search_path[MAX_PATH];
    StringCchCopyA(search_path, MAX_PATH, state->clientDir);
    PathAppendA(search_path, "*.*");

    WIN32_FIND_DATAA data;
    HANDLE h_find = FindFirstFileA(search_path, &data);
    if (h_find == INVALID_HANDLE_VALUE) {
        return;
    }

    int index = 0;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            continue;
        }
    ULONGLONG size = ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
    char size_text[64];
    StringCchPrintfA(size_text, ARRAYSIZE(size_text), "%I64u", size);

        FILETIME local_ft;
        FileTimeToLocalFileTime(&data.ftLastWriteTime, &local_ft);
        SYSTEMTIME st;
        FileTimeToSystemTime(&local_ft, &st);
        char time_buf[64];
        snprintf(time_buf, sizeof(time_buf), "%04d-%02d-%02d %02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);

        LVITEMA item = {0};
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.pszText = data.cFileName;
        ListView_InsertItem(state->hClientFiles, &item);
        ListView_SetItemText(state->hClientFiles, index, 1, size_text);
        ListView_SetItemText(state->hClientFiles, index, 2, time_buf);
        ++index;
    } while (FindNextFileA(h_find, &data));

    FindClose(h_find);
}

static void refresh_client_activity(AppState* state, const char* log_data) {
    if (!state) {
        return;
    }

    ListView_DeleteAllItems(state->hClientActivity);
    if (!log_data) {
        return;
    }

    char* copy = _strdup(log_data);
    if (!copy) {
        return;
    }

    size_t entry_index = 0;
    char* context = NULL;
    for (char* line = strtok_s(copy, "\r\n", &context);
         line != NULL && entry_index < 128;
         line = strtok_s(NULL, "\r\n", &context)) {
        char* client_ptr = strstr(line, "Client ");
        if (!client_ptr) {
            continue;
        }
        client_ptr += 7;
        char* comma = strchr(client_ptr, ',');
        if (!comma) {
            continue;
        }
        *comma = '\0';
        char* event_ptr = comma + 1;
        while (*event_ptr == ' ') {
            ++event_ptr;
        }
        if (*event_ptr == '\0') {
            continue;
        }

        LVITEMA item = {0};
        item.mask = LVIF_TEXT;
        item.iItem = (int)entry_index;
        item.pszText = client_ptr;
        ListView_InsertItem(state->hClientActivity, &item);
        ListView_SetItemText(state->hClientActivity, (int)entry_index, 1, event_ptr);
        ++entry_index;
    }

    free(copy);
}

static void update_throughput_and_errors(AppState* state, const char* log_data) {
    if (!state) {
        return;
    }
    const char* last_throughput = NULL;
    const char* last_error = NULL;

    if (log_data) {
        const char* cursor = log_data;
        while (*cursor) {
            const char* line_start = cursor;
            const char* newline = strpbrk(cursor, "\r\n");
            if (!newline) {
                newline = cursor + strlen(cursor);
            }
            if (strstr(line_start, "Transfer statistics:") != NULL) {
                last_throughput = line_start;
            }
            if (strstr(line_start, "[ERROR]") != NULL) {
                last_error = line_start;
            }
            if (*newline == '\0') {
                break;
            }
            cursor = (*newline == '\r' && *(newline + 1) == '\n') ? newline + 2 : newline + 1;
        }
    }

    if (last_throughput) {
        size_t len = strcspn(last_throughput, "\r\n");
        char buffer[256];
        if (len >= sizeof(buffer)) {
            len = sizeof(buffer) - 1;
        }
        memcpy(buffer, last_throughput, len);
        buffer[len] = '\0';
        SetWindowTextA(state->hThroughput, buffer);
    } else {
        SetWindowTextA(state->hThroughput, "Throughput: No data");
    }

    if (last_error) {
        size_t len = strcspn(last_error, "\r\n");
        char buffer[256];
        if (len >= sizeof(buffer)) {
            len = sizeof(buffer) - 1;
        }
        memcpy(buffer, last_error, len);
        buffer[len] = '\0';
        SetWindowTextA(state->hError, buffer);
    } else {
        SetWindowTextA(state->hError, "Recent errors: None");
    }
}

static void update_log_view(AppState* state) {
    if (!state) {
        return;
    }

    char log_path[MAX_PATH];
    PathCombineA(log_path, state->baseDir, LOG_RELATIVE_PATH);

    HANDLE h_file = CreateFileA(log_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        SetWindowTextA(state->hLogView, "Log file not available, please ensure the server has written logs.");
        update_throughput_and_errors(state, NULL);
        refresh_client_activity(state, NULL);
        return;
    }

    LARGE_INTEGER size;
    if (!GetFileSizeEx(h_file, &size)) {
        CloseHandle(h_file);
        return;
    }

    ULONGLONG read_bytes = (ULONGLONG)size.QuadPart;
    if (read_bytes > MAX_LOG_BYTES) {
        LONG move = (LONG)(read_bytes - MAX_LOG_BYTES);
        SetFilePointer(h_file, move, NULL, FILE_BEGIN);
        read_bytes = MAX_LOG_BYTES;
    } else {
        SetFilePointer(h_file, 0, NULL, FILE_BEGIN);
    }

    char* buffer = (char*)malloc((size_t)read_bytes + 1);
    if (!buffer) {
        CloseHandle(h_file);
        return;
    }

    DWORD actually_read = 0;
    if (!ReadFile(h_file, buffer, (DWORD)read_bytes, &actually_read, NULL)) {
        free(buffer);
        CloseHandle(h_file);
        return;
    }
    buffer[actually_read] = '\0';

    SetWindowTextA(state->hLogView, buffer);
    update_throughput_and_errors(state, buffer);
    refresh_client_activity(state, buffer);

    free(buffer);
    CloseHandle(h_file);
}

static void stop_server(AppState* state) {
    if (!state || !state->hServerProcess) {
        return;
    }
    TerminateProcess(state->hServerProcess, 0);
    CloseHandle(state->hServerProcess);
    state->hServerProcess = NULL;
    update_server_status(state);
}

static void start_server(AppState* state) {
    if (!state) {
        return;
    }
    if (state->hServerProcess) {
        SetWindowTextA(state->hError, "Server already running.");
        return;
    }

    char exe_path[MAX_PATH];
    PathCombineA(exe_path, state->baseDir, SERVER_EXE_NAME);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    BOOL created = CreateProcessA(exe_path, NULL, NULL, NULL, FALSE,
                                  CREATE_NEW_CONSOLE, NULL, state->baseDir, &si, &pi);
    if (!created) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to start server: %lu", GetLastError());
        SetWindowTextA(state->hError, msg);
        return;
    }

    CloseHandle(pi.hThread);
    state->hServerProcess = pi.hProcess;
    update_server_status(state);
    SetWindowTextA(state->hError, "Server started.");
}

static void browse_upload_file(AppState* state) {
    if (!state) {
        return;
    }
    OPENFILENAMEA ofn = {0};
    char buffer[MAX_PATH] = {0};

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = state->hwnd;
    ofn.lpstrFilter = "All Files\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        SetWindowTextA(state->hUploadLocal, buffer);
        const char* filename = PathFindFileNameA(buffer);
        if (filename && *filename) {
            SetWindowTextA(state->hUploadRemote, filename);
        }
    }
}

static unsigned __stdcall transfer_thread_proc(void* param);

static void run_transfer_task(AppState* state, BOOL upload) {
    if (!state) {
        return;
    }

    TransferTask* task = (TransferTask*)calloc(1, sizeof(TransferTask));
    if (!task) {
        return;
    }
    task->app = state;
    task->upload = upload;

    GetWindowTextA(upload ? state->hUploadLocal : state->hDownloadLocal,
                   task->localPath, sizeof(task->localPath));
    GetWindowTextA(upload ? state->hUploadRemote : state->hDownloadRemote,
                   task->remoteName, sizeof(task->remoteName));

    if (upload && strlen(task->localPath) == 0) {
        SetWindowTextA(state->hError, "Please select a local file to upload.");
        free(task);
        return;
    }
    if (strlen(task->remoteName) == 0) {
        SetWindowTextA(state->hError, upload ? "Please enter the filename to upload to the server." : "Please enter the filename to download from the server.");
        free(task);
        return;
    }
    if (!upload && strlen(task->localPath) == 0) {
        StringCchCopyA(task->localPath, MAX_PATH, task->remoteName);
    }

    uintptr_t handle = _beginthreadex(NULL, 0, transfer_thread_proc, task, 0, NULL);

    if (handle) {
        CloseHandle((HANDLE)handle);
    }
}

static unsigned __stdcall transfer_thread_proc(void* param) {
    TransferTask* work = (TransferTask*)param;
    if (!work || !work->app) {
        free(work);
        return 0;
    }

    AppState* app = work->app;

    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        PostMessage(app->hwnd, WM_APP_TRANSFER_RESULT, 0, (LPARAM)_strdup("创建管道失败"));
        free(work);
        return 0;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    char command[1024];
    if (work->upload) {
        const char* remote = work->remoteName[0] ? work->remoteName : PathFindFileNameA(work->localPath);
        snprintf(command, sizeof(command), "tftp -i 127.0.0.1 put \"%s\" \"%s\"", work->localPath, remote);
    } else {
        snprintf(command, sizeof(command), "tftp -i 127.0.0.1 get \"%s\" \"%s\"", work->remoteName, work->localPath);
    }

    char cmd_line[1200];
    snprintf(cmd_line, sizeof(cmd_line), "cmd /c %s", command);

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;

    char working_dir[MAX_PATH];
    if (work->upload) {
        StringCchCopyA(working_dir, MAX_PATH, app->baseDir);
    } else {
        StringCchCopyA(working_dir, MAX_PATH, app->clientDir);
    }

    BOOL launched = CreateProcessA(NULL, cmd_line, NULL, NULL, TRUE,
                                   CREATE_NO_WINDOW, NULL, working_dir, &si, &pi);
    CloseHandle(write_pipe);

    if (!launched) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Failed to start command: %lu", GetLastError());
        PostMessage(app->hwnd, WM_APP_TRANSFER_RESULT, 0, (LPARAM)_strdup(msg));
        CloseHandle(read_pipe);
        free(work);
        return 0;
    }

    char* output = (char*)malloc(1024);
    size_t capacity = 1024;
    size_t used = 0;
    if (output) {
        output[0] = '\0';
    }

    DWORD read = 0;
    char buffer[256];
    while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &read, NULL) && read > 0) {
        buffer[read] = '\0';
        if (!output) {
            continue;
        }
        if (used + read + 1 > capacity) {
            size_t new_capacity = capacity * 2 + read + 1;
            char* resized = (char*)realloc(output, new_capacity);
            if (!resized) {
                free(output);
                output = NULL;
                continue;
            }
            output = resized;
            capacity = new_capacity;
        }
        memcpy(output + used, buffer, read + 1);
        used += read;
    }

    CloseHandle(read_pipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    const char* action = work->upload ? "Upload" : "Download";
    char summary[256];
    snprintf(summary, sizeof(summary), "%s command completed with exit code: %lu\r\n%s\r\n", action, exit_code, command);

    size_t final_len = strlen(summary) + (output ? strlen(output) : 0);
    char* final_msg = (char*)malloc(final_len + 1);
    if (!final_msg) {
        final_msg = _strdup(summary);
    } else {
        StringCchCopyA(final_msg, final_len + 1, summary);
        if (output) {
            StringCchCatA(final_msg, final_len + 1, output);
        }
    }

    if (output) {
        free(output);
    }

    PostMessage(app->hwnd, WM_APP_TRANSFER_RESULT, 0, (LPARAM)final_msg);
    free(work);
    return 0;
}

static void layout_controls(AppState* state, int width, int height) {
    if (!state) {
        return;
    }

    int margin = 12;
    int button_width = 100;
    int control_height = 26;
    int top_height = control_height;

    MoveWindow(state->hStatus, margin, margin, width - margin * 3 - button_width * 2, control_height, TRUE);
    int start_x = width - margin * 2 - button_width * 2;
    int stop_x = width - margin - button_width;
    MoveWindow(state->hBtnStart, start_x, margin, button_width, control_height, TRUE);
    MoveWindow(state->hBtnStop, stop_x, margin, button_width, control_height, TRUE);
    MoveWindow(state->hBtnRefresh, start_x, margin + control_height + 6, button_width, control_height, TRUE);

    int list_top = margin * 2 + top_height + control_height;
    int list_height = 220;
    int left_width = (width - margin * 3) / 2;
    int right_width = width - left_width - margin * 3;

    MoveWindow(state->hServerFiles, margin, list_top, left_width, list_height, TRUE);
    MoveWindow(state->hClientFiles, margin * 2 + left_width, list_top, right_width, list_height, TRUE);

    int transfer_top = list_top + list_height + margin;
    int label_width = 110;
    int edit_width = left_width;

    MoveWindow(state->hLblUploadLocal, margin, transfer_top, label_width, control_height, TRUE);
    MoveWindow(state->hUploadLocal, margin + label_width + 4, transfer_top, edit_width, control_height, TRUE);
    MoveWindow(state->hBtnBrowse, margin + label_width + edit_width + 12, transfer_top, 80, control_height, TRUE);

    MoveWindow(state->hLblUploadRemote, margin, transfer_top + control_height + 6, label_width, control_height, TRUE);
    MoveWindow(state->hUploadRemote, margin + label_width + 4, transfer_top + control_height + 6, edit_width, control_height, TRUE);
    MoveWindow(state->hBtnUpload, margin + label_width + edit_width + 12, transfer_top + control_height + 6, 80, control_height, TRUE);

    MoveWindow(state->hLblDownloadRemote, margin, transfer_top + control_height * 2 + 12, label_width, control_height, TRUE);
    MoveWindow(state->hDownloadRemote, margin + label_width + 4, transfer_top + control_height * 2 + 12, edit_width, control_height, TRUE);

    MoveWindow(state->hLblDownloadLocal, margin, transfer_top + control_height * 3 + 18, label_width, control_height, TRUE);
    MoveWindow(state->hDownloadLocal, margin + label_width + 4, transfer_top + control_height * 3 + 18, edit_width, control_height, TRUE);
    MoveWindow(state->hBtnDownload, margin + label_width + edit_width + 12, transfer_top + control_height * 3 + 18, 80, control_height, TRUE);

    int activity_top = transfer_top + control_height * 4 + margin + 20;
    int activity_height = 180;
    MoveWindow(state->hClientActivity, margin, activity_top, width - margin * 2, activity_height, TRUE);

    int log_top = activity_top + activity_height + margin;
    int log_height = height - log_top - control_height * 2 - margin * 2;
    if (log_height < 120) {
        log_height = 120;
    }

    MoveWindow(state->hLogView, margin, log_top, width - margin * 2, log_height, TRUE);
    MoveWindow(state->hThroughput, margin, log_top + log_height + margin, width - margin * 2, control_height, TRUE);
    MoveWindow(state->hError, margin, log_top + log_height + margin + control_height + 4, width - margin * 2, control_height, TRUE);
}

static void create_controls(AppState* state) {
    state->hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

    state->hStatus = CreateWindowExA(0, "STATIC", "Server not running", WS_CHILD | WS_VISIBLE,
                                     0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hStatus, state->hFont);

    state->hBtnStart = CreateWindowExA(0, "BUTTON", "Start Server", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                       0, 0, 0, 0, state->hwnd, (HMENU)ID_BTN_START, NULL, NULL);
    set_control_font(state->hBtnStart, state->hFont);

    state->hBtnStop = CreateWindowExA(0, "BUTTON", "Stop Server", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                      0, 0, 0, 0, state->hwnd, (HMENU)ID_BTN_STOP, NULL, NULL);
    set_control_font(state->hBtnStop, state->hFont);

    state->hBtnRefresh = CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                         0, 0, 0, 0, state->hwnd, (HMENU)ID_BTN_REFRESH, NULL, NULL);
    set_control_font(state->hBtnRefresh, state->hFont);

    state->hServerFiles = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                                          WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                          0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    ListView_SetExtendedListViewStyle(state->hServerFiles, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    set_control_font(state->hServerFiles, state->hFont);
    populate_listview_columns(state->hServerFiles, "Server Files", 220, "Size (Bytes)", 140);

    state->hClientFiles = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                                          WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                          0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    ListView_SetExtendedListViewStyle(state->hClientFiles, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    set_control_font(state->hClientFiles, state->hFont);
    LVCOLUMNA client_cols[3] = {0};
    client_cols[0].mask = LVCF_TEXT | LVCF_WIDTH;
    client_cols[0].pszText = "Client Cache Files";
    client_cols[0].cx = 180;
    ListView_InsertColumn(state->hClientFiles, 0, &client_cols[0]);
    client_cols[1].pszText = "Size (Bytes)";
    client_cols[1].cx = 120;
    ListView_InsertColumn(state->hClientFiles, 1, &client_cols[1]);
    client_cols[2].pszText = "Last Modified";
    client_cols[2].cx = 160;
    ListView_InsertColumn(state->hClientFiles, 2, &client_cols[2]);

    state->hClientActivity = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
                                             WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                             0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    ListView_SetExtendedListViewStyle(state->hClientActivity, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    set_control_font(state->hClientActivity, state->hFont);
    populate_activity_columns(state->hClientActivity);

    state->hLblUploadLocal = CreateWindowExA(0, "STATIC", "Local Upload File:", WS_CHILD | WS_VISIBLE,
                                             0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hLblUploadLocal, state->hFont);

    state->hLblUploadRemote = CreateWindowExA(0, "STATIC", "Uploaded Filename:", WS_CHILD | WS_VISIBLE,
                                              0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hLblUploadRemote, state->hFont);

    state->hLblDownloadRemote = CreateWindowExA(0, "STATIC", "Server Filename:", WS_CHILD | WS_VISIBLE,
                                                0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hLblDownloadRemote, state->hFont);

    state->hLblDownloadLocal = CreateWindowExA(0, "STATIC", "Save to Client:", WS_CHILD | WS_VISIBLE,
                                               0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hLblDownloadLocal, state->hFont);

    state->hUploadLocal = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                          0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hUploadLocal, state->hFont);

    state->hUploadRemote = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hUploadRemote, state->hFont);

    state->hDownloadRemote = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                             0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hDownloadRemote, state->hFont);

    state->hDownloadLocal = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                            0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hDownloadLocal, state->hFont);

    state->hBtnBrowse = CreateWindowExA(0, "BUTTON", "Browse", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                        0, 0, 0, 0, state->hwnd, (HMENU)ID_BTN_BROWSE_UPLOAD, NULL, NULL);
    set_control_font(state->hBtnBrowse, state->hFont);

    state->hBtnUpload = CreateWindowExA(0, "BUTTON", "Upload", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                        0, 0, 0, 0, state->hwnd, (HMENU)ID_BTN_UPLOAD, NULL, NULL);
    set_control_font(state->hBtnUpload, state->hFont);

    state->hBtnDownload = CreateWindowExA(0, "BUTTON", "Download", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                          0, 0, 0, 0, state->hwnd, (HMENU)ID_BTN_DOWNLOAD, NULL, NULL);
    set_control_font(state->hBtnDownload, state->hFont);

    state->hLogView = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_READONLY | WS_VSCROLL,
                                      0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hLogView, state->hFont);

    state->hThroughput = CreateWindowExA(0, "STATIC", "Throughput: N/A", WS_CHILD | WS_VISIBLE,
                                         0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hThroughput, state->hFont);

    state->hError = CreateWindowExA(0, "STATIC", "Recent Errors: None", WS_CHILD | WS_VISIBLE,
                                    0, 0, 0, 0, state->hwnd, NULL, NULL, NULL);
    set_control_font(state->hError, state->hFont);
}

static LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) {
    AppState* state = (AppState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE: {
            AppState* new_state = (AppState*)calloc(1, sizeof(AppState));
            if (!new_state) {
                return -1;
            }
            new_state->hwnd = hwnd;
            GetModuleFileNameA(NULL, new_state->baseDir, MAX_PATH);
            PathRemoveFileSpecA(new_state->baseDir);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)new_state);
            state = new_state;

            SetCurrentDirectoryA(new_state->baseDir);
            ensure_directories(new_state);

            create_controls(new_state);
            RECT rc;
            GetClientRect(hwnd, &rc);
            layout_controls(new_state, rc.right - rc.left, rc.bottom - rc.top);

            SetTimer(hwnd, ID_TIMER_REFRESH, TIMER_INTERVAL_MS, NULL);
            update_server_status(new_state);
            refresh_server_files(new_state);
            refresh_client_files(new_state);
            update_log_view(new_state);
            return 0;
        }
        case WM_SIZE: {
            if (state) {
                int width = LOWORD(l_param);
                int height = HIWORD(l_param);
                layout_controls(state, width, height);
            }
            return 0;
        }
        case WM_COMMAND: {
            if (!state) {
                break;
            }
            switch (LOWORD(w_param)) {
                case ID_BTN_START:
                    start_server(state);
                    break;
                case ID_BTN_STOP:
                    stop_server(state);
                    break;
                case ID_BTN_REFRESH:
                    refresh_server_files(state);
                    refresh_client_files(state);
                    update_log_view(state);
                    update_server_status(state);
                    break;
                case ID_BTN_BROWSE_UPLOAD:
                    browse_upload_file(state);
                    break;
                case ID_BTN_UPLOAD:
                    run_transfer_task(state, TRUE);
                    break;
                case ID_BTN_DOWNLOAD:
                    run_transfer_task(state, FALSE);
                    break;
            }
            return 0;
        }
        case WM_TIMER: {
            if (state && w_param == ID_TIMER_REFRESH) {
                update_server_status(state);
                refresh_server_files(state);
                refresh_client_files(state);
                update_log_view(state);
            }
            return 0;
        }
        case WM_APP_TRANSFER_RESULT: {
            if (state && l_param) {
                SetWindowTextA(state->hError, (const char*)l_param);
                update_log_view(state);
                refresh_server_files(state);
                refresh_client_files(state);
                free((void*)l_param);
            }
            return 0;
        }
        case WM_DESTROY: {
            if (state) {
                KillTimer(hwnd, ID_TIMER_REFRESH);
                stop_server(state);
                free(state);
                SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
            }
            PostQuitMessage(0);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, w_param, l_param);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd_line, int cmd_show) {
    (void)prev;
    (void)cmd_line;

    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = main_wnd_proc;
    wc.hInstance = instance;
    wc.lpszClassName = "TftpGuiMainWnd";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class", "Error", MB_ICONERROR);
        return 0;
    }

    HWND hwnd = CreateWindowExA(0, wc.lpszClassName, "TFTP Experiment Monitoring Panel",
                                WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
                                NULL, NULL, instance, NULL);
    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create window", "Error", MB_ICONERROR);
        return 0;
    }

    ShowWindow(hwnd, cmd_show);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
