/*
  GL_Commdlg
  Copyright (C) 2025 Gao Li

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 *  \file GL_Commdlg.hpp
 *
 *  本头文件包装了一些Windows Commdlg还有Windows Shell中和用户对话框有关的API，使其易用。而且对其没有的功能进行了拓展。
 */


#ifndef __INC_GL_COMMDLG_
#define __INC_GL_COMMDLG_

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>
#include <Shlobj.h>
#include <cstdint>
#include <thread>
#include <memory>
#include <functional>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <mutex>
#include "UTF8toWide.hpp"

// 链接对话框库，若使用非MSCV编译器，请添加编译参数-lcomdlg32 -lshell32
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

    /**
     * @brief 构建文件过滤器字符串（宽字符版）
     * @param filters 过滤器列表，每个元素格式为"描述|过滤模式"
     * @return 符合API要求的宽字符过滤器字符串
     * @throw std::invalid_argument 过滤器格式错误时抛出
     */
    std::wstring buildFilter(const std::vector<std::string>& filters) {
        std::wstring filterStr;

        for (const auto& filter : filters) {
            size_t pipePos = filter.find('|');
            if (pipePos == std::string::npos) {
                throw std::invalid_argument(
                    "Invalid filter format: '" + filter + 
                    "'. Use '描述|过滤模式' (e.g., 'Text Files(*.txt)|*.txt')"
                );
            }

            std::string desc = filter.substr(0, pipePos);
            std::string pattern = filter.substr(pipePos + 1);
            filterStr += utf8ToWide(desc);
            filterStr += L'\0';
            filterStr += utf8ToWide(pattern);
            filterStr += L'\0';
        }

        filterStr += L'\0';
        return filterStr;
    }

    std::wstring FindFontFileLocalMachine(const std::wstring& fontNameSubstring)
    {
        HKEY hKey;
        LONG result;
        DWORD index = 0;
        WCHAR valueName[256];
        DWORD valueNameSize;
        BYTE valueData[1024];
        DWORD valueDataSize;
        DWORD valueType;
        
        result = RegOpenKeyExW(HKEY_LOCAL_MACHINE, 
                            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 
                            0, KEY_READ, &hKey);
        
        if (result != ERROR_SUCCESS) {
            return L"";
        }
        
        std::wstring fontPath;
        
        while (true) {
            valueNameSize = sizeof(valueName) / sizeof(WCHAR);
            valueDataSize = sizeof(valueData);
            
            result = RegEnumValueW(hKey, index, valueName, &valueNameSize, 
                                NULL, &valueType, valueData, &valueDataSize);
            
            if (result != ERROR_SUCCESS) {
                break;
            }
            
            index++;
            
            if (valueType == REG_SZ) {
                std::wstring currentFontName(valueName);
                std::wstring currentFontPath(reinterpret_cast<wchar_t*>(valueData));
                
                size_t pos = currentFontName.find(L" (");
                if (pos != std::wstring::npos) {
                    currentFontName = currentFontName.substr(0, pos);
                }
                
                std::wstring lowerFontName = currentFontName;
                std::wstring lowerSubstring = fontNameSubstring;
                
                for (auto& c : lowerFontName) c = towlower(c);
                for (auto& c : lowerSubstring) c = towlower(c);
                
                if (lowerFontName.find(lowerSubstring) != std::wstring::npos) {
                    
                    if (currentFontPath.find(L':') == std::wstring::npos) {
                        WCHAR windowsDir[MAX_PATH];
                        GetWindowsDirectoryW(windowsDir, MAX_PATH);
                        fontPath = std::wstring(windowsDir) + L"\\Fonts\\" + currentFontPath;
                    } else {
                        fontPath = currentFontPath;
                    }
                    break;
                }
            }
        }
        
        RegCloseKey(hKey);
        return fontPath;
    }

    std::wstring FindFontFileCurrentUser(const std::wstring& fontNameSubstring)
    {
        HKEY hKey;
        LONG result;
        DWORD index = 0;
        WCHAR valueName[256];
        DWORD valueNameSize;
        BYTE valueData[1024];
        DWORD valueDataSize;
        DWORD valueType;
        
        result = RegOpenKeyExW(HKEY_CURRENT_USER, 
                            L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 
                            0, KEY_READ, &hKey);
        
        if (result != ERROR_SUCCESS) {
            return L"";
        }
        
        std::wstring fontPath;
        
        while (true) {
            valueNameSize = sizeof(valueName) / sizeof(WCHAR);
            valueDataSize = sizeof(valueData);
            
            result = RegEnumValueW(hKey, index, valueName, &valueNameSize, 
                                NULL, &valueType, valueData, &valueDataSize);
            
            if (result != ERROR_SUCCESS) {
                break;
            }
            
            index++;
            
            if (valueType == REG_SZ) {
                std::wstring currentFontName(valueName);
                std::wstring currentFontPath(reinterpret_cast<wchar_t*>(valueData));
                
                size_t pos = currentFontName.find(L" (");
                if (pos != std::wstring::npos) {
                    currentFontName = currentFontName.substr(0, pos);
                }
                
                std::wstring lowerFontName = currentFontName;
                std::wstring lowerSubstring = fontNameSubstring;
                
                for (auto& c : lowerFontName) c = towlower(c);
                for (auto& c : lowerSubstring) c = towlower(c);
                
                if (lowerFontName.find(lowerSubstring) != std::wstring::npos) {
                    
                    if (currentFontPath.find(L':') == std::wstring::npos) {
                        WCHAR windowsDir[MAX_PATH];
                        GetWindowsDirectoryW(windowsDir, MAX_PATH);
                        fontPath = std::wstring(windowsDir) + L"\\Fonts\\" + currentFontPath;
                    } else {
                        fontPath = currentFontPath;
                    }
                    break;
                }
            }
        }
        
        RegCloseKey(hKey);
        return fontPath;
    }

    std::wstring FindFontFile(const std::wstring& fontNameSubstring){
        std::wstring try_lm = FindFontFileLocalMachine(fontNameSubstring);
        if(try_lm.empty()){
            try_lm = FindFontFileCurrentUser(fontNameSubstring);
        }
        return try_lm;
    }

}

/**
 * @brief 显示文件打开对话框，让用户选择一个已存在的文件
 * @param filters 文件过滤器列表，每个元素必须遵循"描述|过滤模式"格式：
 *                - 描述：显示在过滤器下拉框中的文本（如"文本文件(*.txt)"）
 *                - 过滤模式：文件匹配规则（如"*.txt"，多模式用分号分隔"*.bmp;*.jpg"）
 *                示例：{"文本文件(*.txt)|*.txt", "所有文件(*.*)|*.*"}
 * @param title 文件打开对话框的窗口标题，若为空则使用默认的标题，即"打开"
 * @param initialDir 初始目录（UTF8编码），为空则使用当前工作目录
 * @param defaultFileName 默认显示的文件名（UTF8编码），为空则不设置
 * @param defaultExt 默认扩展名（无需带点，如"txt"），用户未输入扩展名时自动添加
 * @param parentHWND 文件打开对话框的父窗口句柄
 * @return 选中的文件路径（UTF8编码），用户取消时返回空字符串
 * @throw std::invalid_argument 过滤器格式错误时
 * @throw std::runtime_error 字符串转换失败或对话框调用出错时
 */
std::string getOpenFileName(const std::vector<std::string>& filters,
                           const std::string& title = "",
                           const std::string& initialDir = "",
                           const std::string& defaultFileName = "",
                           const std::string& defaultExt = "",HWND parentHWND = NULL) {
    std::wstring filter = buildFilter(filters);
    std::wstring wtitle = utf8ToWide(title);

    const int MAX_PATH_LEN = 4096;
    std::wstring filePath(MAX_PATH_LEN, L'\0');
    if (!defaultFileName.empty()) {
        std::wstring defaultWide = utf8ToWide(defaultFileName);
        if (defaultWide.size() >= MAX_PATH_LEN) {
            throw std::runtime_error("Default file name is too long");
        }
        wcscpy_s(&filePath[0], MAX_PATH_LEN, defaultWide.c_str());
    }

    std::wstring initialDirWide;
    LPCWSTR initialDirPtr = nullptr;
    if (!initialDir.empty()) {
        initialDirWide = utf8ToWide(initialDir);
        initialDirPtr = initialDirWide.c_str();
    }

    std::wstring defaultExtWide;
    LPCWSTR defaultExtPtr = nullptr;
    if (!defaultExt.empty()) {
        defaultExtWide = utf8ToWide(defaultExt);
        defaultExtPtr = defaultExtWide.c_str();
    }

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = parentHWND;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = &filePath[0];
    ofn.nMaxFile = MAX_PATH_LEN;
    ofn.lpstrInitialDir = initialDirPtr;
    ofn.lpstrDefExt = defaultExtPtr;
    if(!title.empty()) ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST |
                OFN_PATHMUSTEXIST |
                OFN_NOCHANGEDIR |
                OFN_EXPLORER;

    if (!GetOpenFileNameW(&ofn)) {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            throw std::runtime_error("Open file dialog failed: " + std::to_string(err));
        }
        return "";
    }

    filePath.resize(wcslen(filePath.c_str()));
    return wideToUtf8(filePath);
}

/**
 * @brief 显示文件保存对话框，让用户指定文件保存路径
 * @param filters 文件过滤器列表，每个元素必须遵循"描述|过滤模式"格式：
 *                - 描述：显示在过滤器下拉框中的文本（如"文本文件(*.txt)"）
 *                - 过滤模式：文件匹配规则（如"*.txt"，多模式用分号分隔"*.bmp;*.jpg"）
 *                示例：{"文本文件(*.txt)|*.txt", "所有文件(*.*)|*.*"}
 * @param title 文件打开对话框的窗口标题，若为空则使用默认的标题，即"另存为"
 * @param initialDir 初始目录（UTF8编码），为空则使用当前工作目录
 * @param defaultFileName 默认显示的文件名（UTF8编码），为空则不设置
 * @param defaultExt 默认扩展名（无需带点，如"txt"），用户未输入扩展名时自动添加
 * @param parentHWND 文件保存对话框的父窗口句柄
 * @return 选中的文件保存路径（UTF8编码），用户取消时返回空字符串
 * @throw std::invalid_argument 过滤器格式错误时
 * @throw std::runtime_error 字符串转换失败或对话框调用出错时
 */
std::string getSaveFileName(const std::vector<std::string>& filters,
                           const std::string& title = "",
                           const std::string& initialDir = "",
                           const std::string& defaultFileName = "",
                           const std::string& defaultExt = "",HWND parentHWND = NULL) {
    std::wstring filter = buildFilter(filters);
    std::wstring wtitle = utf8ToWide(title);

    const int MAX_PATH_LEN = 4096;
    std::wstring filePath(MAX_PATH_LEN, L'\0');
    if (!defaultFileName.empty()) {
        std::wstring defaultWide = utf8ToWide(defaultFileName);
        if (defaultWide.size() >= MAX_PATH_LEN) {
            throw std::runtime_error("Default file name is too long");
        }
        wcscpy_s(&filePath[0], MAX_PATH_LEN, defaultWide.c_str());
    }

    std::wstring initialDirWide;
    LPCWSTR initialDirPtr = nullptr;
    if (!initialDir.empty()) {
        initialDirWide = utf8ToWide(initialDir);
        initialDirPtr = initialDirWide.c_str();
    }

    std::wstring defaultExtWide;
    LPCWSTR defaultExtPtr = nullptr;
    if (!defaultExt.empty()) {
        defaultExtWide = utf8ToWide(defaultExt);
        defaultExtPtr = defaultExtWide.c_str();
    }

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = parentHWND;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = &filePath[0];
    ofn.nMaxFile = MAX_PATH_LEN;
    ofn.lpstrInitialDir = initialDirPtr;
    ofn.lpstrDefExt = defaultExtPtr;
    if(!title.empty()) ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT |
                OFN_PATHMUSTEXIST |
                OFN_NOCHANGEDIR |
                OFN_EXPLORER;

    if (!GetSaveFileNameW(&ofn)) {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            throw std::runtime_error("Save file dialog failed: " + std::to_string(err));
        }
        return "";
    }

    filePath.resize(wcslen(filePath.c_str()));
    return wideToUtf8(filePath);
}

/**
 * @brief 显示文件打开对话框，让用户选择多个已存在的文件
 * @param filters 文件过滤器列表，每个元素必须遵循"描述|过滤模式"格式：
 *                - 描述：显示在过滤器下拉框中的文本（如"文本文件(*.txt)"）
 *                - 过滤模式：文件匹配规则（如"*.txt"，多模式用分号分隔"*.bmp;*.jpg"）
 *                示例：{"文本文件(*.txt)|*.txt", "所有文件(*.*)|*.*"}
 * @param title 文件打开对话框的窗口标题，若为空则使用默认的标题，即"打开"
 * @param initialDir 初始目录（UTF8编码），为空则使用当前工作目录
 * @param defaultFileName 默认显示的文件名（UTF8编码），为空则不设置
 * @param defaultExt 默认扩展名（无需带点，如"txt"），用户未输入扩展名时自动添加
 * @param parentHWND 文件打开对话框的父窗口句柄
 * @return 选中的文件路径列表（UTF8编码），用户取消时返回空vector
 * @throw std::invalid_argument 过滤器格式错误时
 * @throw std::runtime_error 字符串转换失败或对话框调用出错时
 */
std::vector<std::string> getOpenMultipleFileNames(const std::vector<std::string>& filters,
                           const std::string& title = "",
                           const std::string& initialDir = "",
                           const std::string& defaultFileName = "",
                           const std::string& defaultExt = "",
                           HWND parentHWND = NULL) {
    std::wstring filter = buildFilter(filters);
    std::wstring wtitle = utf8ToWide(title);

    // 为多选文件分配更大的缓冲区（64KB）
    const int BUFFER_SIZE = 65536;
    std::vector<wchar_t> filePathBuffer(BUFFER_SIZE, L'\0');
    
    if (!defaultFileName.empty()) {
        std::wstring defaultWide = utf8ToWide(defaultFileName);
        if (defaultWide.size() * sizeof(wchar_t) >= BUFFER_SIZE) {
            throw std::runtime_error("Default file name is too long");
        }
        wcscpy_s(filePathBuffer.data(), BUFFER_SIZE / sizeof(wchar_t), defaultWide.c_str());
    }

    std::wstring initialDirWide;
    LPCWSTR initialDirPtr = nullptr;
    if (!initialDir.empty()) {
        initialDirWide = utf8ToWide(initialDir);
        initialDirPtr = initialDirWide.c_str();
    }

    std::wstring defaultExtWide;
    LPCWSTR defaultExtPtr = nullptr;
    if (!defaultExt.empty()) {
        defaultExtWide = utf8ToWide(defaultExt);
        defaultExtPtr = defaultExtWide.c_str();
    }

    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = parentHWND;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = filePathBuffer.data();
    ofn.nMaxFile = BUFFER_SIZE;
    ofn.lpstrInitialDir = initialDirPtr;
    ofn.lpstrDefExt = defaultExtPtr;
    if(!title.empty()) ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST |
                OFN_PATHMUSTEXIST |
                OFN_NOCHANGEDIR |
                OFN_EXPLORER |
                OFN_ALLOWMULTISELECT;

    if (!GetOpenFileNameW(&ofn)) {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            throw std::runtime_error("Open file dialog failed: " + std::to_string(err));
        }
        return {};
    }

    std::vector<std::string> selectedFiles;
    
    const wchar_t* ptr = filePathBuffer.data();
    std::wstring directory = ptr;
    ptr += directory.length() + 1;
    
    if (*ptr == L'\0') {
        selectedFiles.push_back(wideToUtf8(directory));
    } else {
        while (*ptr != L'\0') {
            std::wstring filename = ptr;
            std::wstring fullPath = directory + L"\\" + filename;
            selectedFiles.push_back(wideToUtf8(fullPath));
            ptr += filename.length() + 1;
        }
    }

    return selectedFiles;
}

/**
 * @brief 显示目录选择对话框，让用户选择一个目录
 * @param title 目录选择对话框中显示的提示文字
 * @param initialDir 初始目录（UTF8编码），为空则使用当前工作目录
 * @param parentHWND 目录选择对话框的父窗口句柄
 * @return 选中的目录路径（UTF8编码），用户取消时返回空字符串
 * @throw std::runtime_error 字符串转换失败或对话框调用出错时
 * 
 * @note 由于SHBrowseForFolderW不支持自定义窗口标题，所以参数title指的不算是真正意义上的"标题"，应该算提示文字。另外SHBrowseForFolderW默认的窗口标题是"浏览文件夹"
 */
std::string getOpenDirectoryName(const std::string& title = "",
                                const std::string& initialDir = "",
                                HWND parentHWND = NULL) {
    std::wstring wtitle = utf8ToWide(title);
    
    std::wstring initialDirWide;
    if (!initialDir.empty()) {
        initialDirWide = utf8ToWide(initialDir);
    }

    BROWSEINFOW bi = {0};
    bi.hwndOwner = parentHWND;
    if(!title.empty()) bi.lpszTitle = wtitle.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    if (!initialDirWide.empty()) {
        bi.lParam = reinterpret_cast<LPARAM>(initialDirWide.c_str());
        bi.lpfn = [](HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData) -> int {
            if (uMsg == BFFM_INITIALIZED) {
                SendMessageW(hwnd, BFFM_SETSELECTIONW, TRUE, lpData);
            }
            return 0;
        };
    }

    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (pidl == nullptr) {
        return "";
    }

    std::wstring directoryPath(MAX_PATH, L'\0');
    if (!SHGetPathFromIDListW(pidl, &directoryPath[0])) {
        CoTaskMemFree(pidl);
        throw std::runtime_error("Failed to get path from ID list");
    }

    CoTaskMemFree(pidl);
    directoryPath.resize(wcslen(directoryPath.c_str()));
    return wideToUtf8(directoryPath);
}

#ifndef SDL_pixels_h_

struct SDL_Color{
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
};

#endif

/**
 * @brief 显示颜色选择对话框，让用户选择一个颜色
 * 
 * @param selectedColor 用户所选的颜色
 * @param hwndParent 颜色选择对话框的父窗口句柄
 */
void chooseColor(SDL_Color& selectedColor, HWND hwndParent = NULL)
{
    static COLORREF customColors[16] = {0};
    
    CHOOSECOLORW cc = {0};
    cc.lStructSize = sizeof(cc);
    cc.hwndOwner = hwndParent;
    cc.lpCustColors = customColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
    
    COLORREF initialColor = RGB(
        selectedColor.r,
        selectedColor.g,
        selectedColor.b
    );
    cc.rgbResult = initialColor;
	
    if (!ChooseColorW(&cc))
    {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            throw std::runtime_error("Choose color dialog failed: " + std::to_string(err));
        }
    }

    selectedColor.r = GetRValue(cc.rgbResult);
    selectedColor.g = GetGValue(cc.rgbResult);
    selectedColor.b = GetBValue(cc.rgbResult);
    selectedColor.a = 255;
}

struct chooseFontInfo{
    std::string fontFaceName;
    std::string fontPath;
    int fontPointSize;
};

/**
 * @brief 显示字体选择对话框，让用户选择系统上所安装的字体
 * 
 * @param cfi 输出参数，注意，不保证一定可以找到选择的字体的路径，但大概率可以找到。若没找到，fontPath为空字符串。
 * @param hwndParent 颜色选择对话框的父窗口句柄
 */
void chooseFont(chooseFontInfo& cfi, HWND hwndParent = NULL){
    CHOOSEFONTW cf = {0};
    LOGFONTW lf = {0};
    cf.lStructSize = sizeof(CHOOSEFONTW);
    cf.hwndOwner = hwndParent;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_NOVERTFONTS | CF_TTONLY;
    if (!ChooseFontW(&cf)) {
        DWORD err = CommDlgExtendedError();
        if (err != 0) {
            throw std::runtime_error("Choose font dialog failed: " + std::to_string(err));
        }
    }
    cfi.fontFaceName = wideToUtf8(lf.lfFaceName);
    cfi.fontPointSize = cf.iPointSize / 10;
    cfi.fontPath = wideToUtf8(FindFontFile(lf.lfFaceName));
}

#pragma region 非Win32原生对话框
//使用原始的方法轻量级实现一些commdlg.h没有实现的对话框，比如prompt

#define __GCOMMMDLG_IDC_PROMPT  1001  // 提示文本
#define __GCOMMMDLG_IDC_INPUT   1002  // 输入框
#define __GCOMMMDLG_IDOK        1003  // 确定按钮
#define __GCOMMMDLG_IDCANCEL    1004  // 取消按钮

namespace{

    WCHAR* g_inputText = nullptr;
    std::wstring g_defalutContent;
    std::wstring g_message;
    bool g_did_confirm;

    LRESULT CALLBACK PromptDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
        
        static HWND hStaticPrompt = NULL;
        static HWND hEditInput = NULL;
        static HWND hButtonOK = NULL;
        static HWND hButtonCancel = NULL;
        static HBRUSH hDefaultBrush = CreateSolidBrush(RGB(240, 240, 240)); 
        
        switch (msg) {
            case WM_CREATE: {
                
                HFONT hFont = CreateFontW(
                    24, 0, 0, 0, FW_NORMAL, 
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑"
                );
                
                hStaticPrompt = CreateWindowExW(
                    0,
                    L"STATIC",
                    g_message.c_str(),
                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                    20, 20, 260, 25,
                    hDlg,
                    (HMENU)__GCOMMMDLG_IDC_PROMPT,
                    ((LPCREATESTRUCTW)lParam)->hInstance,
                    NULL
                );
                
                hEditInput = CreateWindowExW(
                    WS_EX_CLIENTEDGE,  // 使用3D边框
                    L"EDIT",
                    g_defalutContent.c_str(),
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                    20, 50, 360, 30,
                    hDlg,
                    (HMENU)__GCOMMMDLG_IDC_INPUT,
                    ((LPCREATESTRUCTW)lParam)->hInstance,
                    NULL
                );
                
                hButtonOK = CreateWindowExW(
                    0,
                    L"BUTTON",
                    L"确定",
                    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                    120, 95, 80, 30,
                    hDlg,
                    (HMENU)__GCOMMMDLG_IDOK,
                    ((LPCREATESTRUCTW)lParam)->hInstance,
                    NULL
                );
                
                hButtonCancel = CreateWindowExW(
                    0,
                    L"BUTTON",
                    L"取消",
                    WS_CHILD | WS_VISIBLE,
                    220, 95, 80, 30,
                    hDlg,
                    (HMENU)__GCOMMMDLG_IDCANCEL,
                    ((LPCREATESTRUCTW)lParam)->hInstance,
                    NULL
                );
                
                if (hFont) {
                    SendMessage(hStaticPrompt, WM_SETFONT, (WPARAM)hFont, TRUE);
                    SendMessage(hEditInput, WM_SETFONT, (WPARAM)hFont, TRUE);
                    SendMessage(hButtonOK, WM_SETFONT, (WPARAM)hFont, TRUE);
                    SendMessage(hButtonCancel, WM_SETFONT, (WPARAM)hFont, TRUE);
                }
                
                return 0;
            }

            case WM_SIZE: {
                int clientWidth = LOWORD(lParam);
                int clientHeight = HIWORD(lParam);
                
                if (hStaticPrompt) {
                    SetWindowPos(hStaticPrompt, NULL, 
                        20, 20, 
                        clientWidth - 40, 25, 
                        SWP_NOZORDER);
                }
                
                if (hEditInput) {
                    SetWindowPos(hEditInput, NULL, 
                        20, 55, 
                        clientWidth - 40, 30, 
                        SWP_NOZORDER);
                }
                
                if (hButtonOK && hButtonCancel) {
                    int buttonWidth = 80;
                    int buttonHeight = 30;
                    int buttonY = clientHeight - buttonHeight - 15;
                    int totalButtonWidth = buttonWidth * 2 + 20;
                    int startX = (clientWidth - totalButtonWidth) / 2;
                    
                    SetWindowPos(hButtonOK, NULL, 
                        startX, buttonY, 
                        buttonWidth, buttonHeight, 
                        SWP_NOZORDER);
                    
                    SetWindowPos(hButtonCancel, NULL, 
                        startX + buttonWidth + 20, buttonY, 
                        buttonWidth, buttonHeight, 
                        SWP_NOZORDER);
                }
                return 0;
            }

            case WM_COMMAND: {
                if (LOWORD(wParam) == __GCOMMMDLG_IDOK) {
                    WCHAR buffer[256] = {0};
                    GetDlgItemTextW(hDlg, __GCOMMMDLG_IDC_INPUT, buffer, 256);
                    wcscpy(g_inputText, buffer);
                    g_did_confirm = true;
                    DestroyWindow(hDlg);
                }
                else if (LOWORD(wParam) == __GCOMMMDLG_IDCANCEL) {
                    wcscpy(g_inputText, L"");
                    g_did_confirm = false;
                    DestroyWindow(hDlg);
                }
                return 0;
            }

            case WM_CLOSE:
                wcscpy(g_inputText, L"");
                g_did_confirm = false;
                DestroyWindow(hDlg);
                return 0;

            case WM_CTLCOLOREDIT: {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, RGB(240, 240, 240));
                SetTextColor(hdc,RGB(0, 0, 0));
                return (LRESULT)hDefaultBrush;
            }

            case WM_CTLCOLORSTATIC: {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, RGB(240, 240, 240));
                SetTextColor(hdc,RGB(0, 0, 0));
                return (LRESULT)hDefaultBrush;
            }

            case WM_CTLCOLORBTN: {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, RGB(240, 240, 240));
                SetTextColor(hdc,RGB(0, 0, 0));
                return (LRESULT)hDefaultBrush;
            }

            case WM_DESTROY: {
                PostQuitMessage(0);
                return 0;
            }

            case WM_ERASEBKGND: {
                HDC hdc = (HDC)wParam;
                RECT rect;
                GetClientRect(hDlg, &rect);
                FillRect(hdc, &rect, hDefaultBrush);
                return TRUE;
            }

            default:
                return DefWindowProcW(hDlg, msg, wParam, lParam);
        }
    }
}

/**
 * @brief 显示输入对话框，用于让用户输入一段字符串
 * 
 * @param title 输入对话框的标题
 * @param message 输入对话框内显示的提示文本
 * @param output 输出用户输入的内容
 * @param defaultContent 输入栏内的默认内容
 * @param hParent 输入对话框的父窗口句柄
 * @return 用户是否确认了输入
 */
bool promptDialog(std::string title,std::string message,std::string& output,std::string defaultContent = "",HWND hParent = NULL) {
    
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc   = PromptDialogProc;
    wc.hInstance     = GetModuleHandleW(NULL);
    wc.lpszClassName = L"PromptDialogClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style         = CS_HREDRAW | CS_VREDRAW;

    RegisterClassExW(&wc);

    int targetWidth = GetSystemMetrics(SM_CXSCREEN);
    int targetHeight = GetSystemMetrics(SM_CYSCREEN);

    int x = targetWidth / 2 - 400 / 2,y = targetHeight / 2 - 180 / 2;

    g_inputText = new wchar_t[256];
    g_message = utf8ToWide(message);
    g_defalutContent = utf8ToWide(defaultContent);

    HWND hDlg = CreateWindowExW(
        0,
        L"PromptDialogClass",
        utf8ToWide(title).c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME/* | WS_SIZEBOX*/,
        x, y, 400, 180,
        hParent,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );

    if (hDlg) {
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);

        MSG msg;
        
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    UnregisterClassW(L"PromptDialogClass", GetModuleHandleW(NULL));

    if(!g_did_confirm){
        output = "";
        return false;
    }

    output = wideToUtf8(g_inputText);
    delete[] g_inputText;
    return true;
}

#define __GCOMMMDLG_BTN_START 2000  // 选项按钮起始ID

#define __GCOMMDLG_MSGBOX_BTN_WIDTH 100

namespace {

    std::vector<std::pair<int, std::wstring>> g_options;
    std::wstring g_msgContent;
    std::wstring g_boxTitle;
    int g_selectedId = 0;

    LRESULT CALLBACK MessageBoxDialogProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
        
        static HWND hStaticMsg = NULL;
        static std::vector<HWND> hButtons;
        static HBRUSH hDefaultBrush = CreateSolidBrush(RGB(240, 240, 240));
        static HFONT hFont = NULL;

        switch (msg) {
            case WM_CREATE: {
                
                hFont = CreateFontW(
                    24, 0, 0, 0, FW_NORMAL, 
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑"
                );

                hStaticMsg = CreateWindowExW(
                    0,
                    L"STATIC",
                    g_msgContent.c_str(),
                    WS_CHILD | WS_VISIBLE | SS_LEFT | SS_WORDELLIPSIS,
                    20, 20, 360, 26,
                    hDlg,
                    (HMENU)1001,
                    ((LPCREATESTRUCTW)lParam)->hInstance,
                    NULL
                );

                hButtons.reserve(g_options.size());
                for (size_t i = 0; i < g_options.size(); ++i) {
                    HWND hBtn = CreateWindowExW(
                        0,
                        L"BUTTON",
                        g_options[i].second.c_str(),
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        20, 120, __GCOMMDLG_MSGBOX_BTN_WIDTH, 30,
                        hDlg,
                        (HMENU)(__GCOMMMDLG_BTN_START + i),
                        ((LPCREATESTRUCTW)lParam)->hInstance,
                        NULL
                    );
                    hButtons.push_back(hBtn);
                }

                if (hFont) {
                    SendMessage(hStaticMsg, WM_SETFONT, (WPARAM)hFont, TRUE);
                    for (HWND hBtn : hButtons) {
                        SendMessage(hBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
                    }
                }

                return 0;
            }

            case WM_SIZE: {
                int clientWidth = LOWORD(lParam);
                int clientHeight = HIWORD(lParam);

                if (hStaticMsg) {
                    SetWindowPos(hStaticMsg, NULL,
                        20, 20,
                        clientWidth - 40, 26,
                        SWP_NOZORDER);
                }

                if (!hButtons.empty()) {
                    const int btnWidth = __GCOMMDLG_MSGBOX_BTN_WIDTH;
                    const int btnHeight = 30;
                    const int spacing = 20;
                    int startX = 20;
                    int startY = clientHeight - btnHeight - 20;

                    for (size_t i = 0; i < hButtons.size(); ++i) {
                        SetWindowPos(hButtons[i], NULL,
                            startX + (i % 3) * (btnWidth + spacing), startY - (i / 3) * 40,
                            btnWidth, btnHeight,
                            SWP_NOZORDER);
                    }
                }
                return 0;
            }

            case WM_COMMAND: {
                int btnId = LOWORD(wParam);
                if (btnId >= __GCOMMMDLG_BTN_START && btnId < __GCOMMMDLG_BTN_START + (int)g_options.size()) {
                    size_t index = btnId - __GCOMMMDLG_BTN_START;
                    g_selectedId = g_options[index].first;
                    DestroyWindow(hDlg);
                }
                return 0;
            }

            case WM_CLOSE: {
                g_selectedId = 0;
                DestroyWindow(hDlg);
                return 0;
            }

            case WM_DESTROY: {
                
                if (hFont) {
                    DeleteObject(hFont);
                    hFont = NULL;
                }
                hButtons.clear();
                PostQuitMessage(0);
                return 0;
            }

            case WM_CTLCOLORSTATIC:
            case WM_CTLCOLORBTN: {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, RGB(240, 240, 240));
                SetTextColor(hdc, RGB(0, 0, 0));
                return (LRESULT)hDefaultBrush;
            }

            case WM_ERASEBKGND: {
                HDC hdc = (HDC)wParam;
                RECT rect;
                GetClientRect(hDlg, &rect);
                FillRect(hdc, &rect, hDefaultBrush);
                return TRUE;
            }

            default:
                return DefWindowProcW(hDlg, msg, wParam, lParam);
        }
    }

    LRESULT CALLBACK MessageBoxDialogProc2(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
        static HWND hEditMsg = NULL;
        static std::vector<HWND> hButtons;
        static HBRUSH hDefaultBrush = CreateSolidBrush(RGB(240, 240, 240));
        static HFONT hFont = NULL;
        static HFONT hTitleFont = NULL;
        static int calculatedEditHeight = 0;

        switch (msg) {
            case WM_CREATE: {

                hFont = CreateFontW(
                    18, 0, 0, 0, FW_NORMAL, 
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑"
                );
                
                hTitleFont = CreateFontW(
                    20, 0, 0, 0, FW_SEMIBOLD, 
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑"
                );

                hEditMsg = CreateWindowExW(
                    WS_EX_CLIENTEDGE,
                    L"EDIT",
                    g_msgContent.c_str(),
                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | 
                    ES_WANTRETURN | WS_VSCROLL | ES_LEFT,
                    20, 20, 360, 100,
                    hDlg,
                    (HMENU)1001,
                    ((LPCREATESTRUCTW)lParam)->hInstance,
                    NULL
                );

                if (hFont) {
                    SendMessage(hEditMsg, WM_SETFONT, (WPARAM)hFont, TRUE);
                }

                HDC hdc = GetDC(hEditMsg);
                if (hdc && hFont) {
                    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
                    
                    RECT rcText = {0, 0, 360 - 10, 0};
                    const wchar_t* text = g_msgContent.c_str();
                    
                    DrawTextW(hdc, text, -1, &rcText, DT_CALCRECT | DT_WORDBREAK | DT_LEFT | DT_TOP | DT_EXPANDTABS);
                    
                    calculatedEditHeight = std::min<int>(std::max<int>(rcText.bottom + 20, 60), 300);
                    
                    SelectObject(hdc, hOldFont);
                    ReleaseDC(hEditMsg, hdc);
                    
                    SetWindowPos(hEditMsg, NULL, 20, 20, 360, calculatedEditHeight, SWP_NOZORDER);
                }

                hButtons.reserve(g_options.size());
                const int btnWidth = 100;
                const int btnHeight = 32;
                const int btnSpacing = 15;
                
                int totalBtnWidth = (btnWidth + btnSpacing) * (int)g_options.size() - btnSpacing;
                int startX = (400 - totalBtnWidth) / 2;
                
                for (size_t i = 0; i < g_options.size(); ++i) {
                    HWND hBtn = CreateWindowExW(
                        0,
                        L"BUTTON",
                        g_options[i].second.c_str(),
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        startX + (int)i * (btnWidth + btnSpacing), 
                        20 + calculatedEditHeight + 20,
                        btnWidth, btnHeight,
                        hDlg,
                        (HMENU)(__GCOMMMDLG_BTN_START + i),
                        ((LPCREATESTRUCTW)lParam)->hInstance,
                        NULL
                    );
                    hButtons.push_back(hBtn);
                    
                    if (hTitleFont) {
                        SendMessage(hBtn, WM_SETFONT, (WPARAM)hTitleFont, TRUE);
                    }
                }

                int totalHeight = 20 + calculatedEditHeight + 20 + btnHeight + 40;
                RECT rcWindow;
                GetWindowRect(hDlg, &rcWindow);
                SetWindowPos(hDlg, NULL, 0, 0, 400, totalHeight, 
                            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

                return 0;
            }

            case WM_SIZE: {
                int clientWidth = LOWORD(lParam);
                int clientHeight = HIWORD(lParam);

                if (hEditMsg) {
                    
                    SetWindowPos(hEditMsg, NULL,
                        20, 20,
                        clientWidth - 40, calculatedEditHeight,
                        SWP_NOZORDER);
                }

                if (!hButtons.empty()) {
                    const int btnWidth = 100;
                    const int btnHeight = 32;
                    const int btnSpacing = 15;
                    int totalBtnWidth = (btnWidth + btnSpacing) * (int)hButtons.size() - btnSpacing;
                    int startX = (clientWidth - totalBtnWidth) / 2;
                    int startY = 20 + calculatedEditHeight + 20;

                    for (size_t i = 0; i < hButtons.size(); ++i) {
                        SetWindowPos(hButtons[i], NULL,
                            startX + (int)i * (btnWidth + btnSpacing),
                            startY,
                            btnWidth, btnHeight,
                            SWP_NOZORDER);
                    }
                }
                return 0;
            }

            case WM_COMMAND: {
                int btnId = LOWORD(wParam);
                if (btnId >= __GCOMMMDLG_BTN_START && btnId < __GCOMMMDLG_BTN_START + (int)g_options.size()) {
                    size_t index = btnId - __GCOMMMDLG_BTN_START;
                    g_selectedId = g_options[index].first;
                    DestroyWindow(hDlg);
                }
                return 0;
            }

            case WM_CLOSE: {
                g_selectedId = 0;
                DestroyWindow(hDlg);
                return 0;
            }

            case WM_DESTROY: {
                
                if (hFont) {
                    DeleteObject(hFont);
                    hFont = NULL;
                }
                if (hTitleFont) {
                    DeleteObject(hTitleFont);
                    hTitleFont = NULL;
                }
                hButtons.clear();
                PostQuitMessage(0);
                return 0;
            }

            case WM_CTLCOLOREDIT: {
                
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, RGB(255, 255, 255));
                SetTextColor(hdc, RGB(0, 0, 0));
                
                static HBRUSH hEditBrush = CreateSolidBrush(RGB(255, 255, 255));
                return (LRESULT)hEditBrush;
            }

            case WM_CTLCOLORBTN: {
                HDC hdc = (HDC)wParam;
                SetBkColor(hdc, RGB(240, 240, 240));
                SetTextColor(hdc, RGB(0, 0, 0));
                return (LRESULT)hDefaultBrush;
            }

            case WM_ERASEBKGND: {
                HDC hdc = (HDC)wParam;
                RECT rect;
                GetClientRect(hDlg, &rect);
                FillRect(hdc, &rect, hDefaultBrush);
                return TRUE;
            }

            default:
                return DefWindowProcW(hDlg, msg, wParam, lParam);
        }
    }
}

/**
 * @brief 显示自定义消息对话框，支持多个选项按钮
 * 
 * @param title 对话框标题
 * @param message 对话框内的提示文本
 * @param options 选项集合（键为返回值，值为按钮文本）
 * @param hParent 父窗口句柄
 * @param style 对话框的样式，若为0，则使用STATIC控件来显示提示文本，但这样的话能显示的信息比较少，而且不支持换行。若为1，则使用EDIT控件来显示提示文本，但是这样的话可能会显得不那么好看。
 * @return 选中的选项ID（关闭窗口返回0，要是你传入的options没有元素则返回-1以告知失败）
 */
int messageBox(std::string title, std::string message, const std::vector<std::pair<int, std::string>>& options, HWND hParent = NULL,int style = 0) {

    if(options.empty()) return -1;
    
    g_options.clear();
    for (const auto& opt : options) {
        g_options.emplace_back(opt.first, utf8ToWide(opt.second));
    }
    g_msgContent = utf8ToWide(message);
    g_boxTitle = utf8ToWide(title);
    g_selectedId = 0;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);

    if(style == 1){
        wc.lpfnWndProc = MessageBoxDialogProc2;
    }else{
        wc.lpfnWndProc = MessageBoxDialogProc;
    }

    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"CustomMessageBoxClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    if (!RegisterClassExW(&wc)) {
        return 0;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = (__GCOMMDLG_MSGBOX_BTN_WIDTH + 20) * 3 + 20;
    int windowHeight = 140 + 40 * ((options.size() - 1) / 3);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    HWND hDlg = CreateWindowExW(
        0,
        L"CustomMessageBoxClass",
        g_boxTitle.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME,
        x, y, windowWidth, windowHeight,
        hParent,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );

    if (hDlg) {
        ShowWindow(hDlg, SW_SHOW);
        UpdateWindow(hDlg);

        MSG msg;
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    UnregisterClassW(L"CustomMessageBoxClass", GetModuleHandleW(NULL));
    g_options.clear();

    return g_selectedId;
}

/*
    我接下来会引入一个新的概念：动态对话框
    动态对话框是在传统静态阻塞型对话框的基础上进行创新的新型动态非阻塞型对话框
    调用动态对话框函数会返回一个Interface用与控制创建的动态对话框
    动态对话框的生命周期与返回的Interface一致
    动态对话框由一个单独的线程来维护，线程的维护则由Interface来完成
    也就是说Interface的析构函数必须完成动态对话框的所有资源释放
    Interface类是不可复制的，仅能进行移动操作
*/

class DynamicDialogInterface {
public:
    virtual ~DynamicDialogInterface() = default;
    
    // 禁止复制，允许移动
    DynamicDialogInterface(const DynamicDialogInterface&) = delete;
    DynamicDialogInterface& operator=(const DynamicDialogInterface&) = delete;
    DynamicDialogInterface(DynamicDialogInterface&&) = default;
    DynamicDialogInterface& operator=(DynamicDialogInterface&&) = default;
    
    // 显示对话框
    virtual void Show() = 0;
    
    // 关闭对话框
    virtual void Close() = 0;
    
protected:
    DynamicDialogInterface() = default;
};

namespace{
    // 进度条消息类型
    enum class ProgressBarMessageType {
        SetValue,
        Close
    };

    // 进度条消息结构
    struct ProgressBarMessage {
        ProgressBarMessageType type;
        uint64_t currentValue;
        uint64_t maxValue;
        std::string message;
        
        ProgressBarMessage(ProgressBarMessageType t, uint64_t c = 0, uint64_t m = 0, const std::string& msg = "")
            : type(t), currentValue(c), maxValue(m), message(msg) {}
    };
}

// 动态进度条接口类
class DynamicProgressBar : public DynamicDialogInterface {
private:
    // 对话框数据
    struct DialogData {
        HWND hwnd = nullptr;
        HWND hwndParent = nullptr;
        std::wstring title;
        std::atomic<uint64_t> currentValue{0};
        std::atomic<uint64_t> maxValue{100};
        std::atomic<bool> isFinished{false};
        std::string currentMessage;
        std::mutex dataMutex;
        
        // UI资源
        HFONT hFont = nullptr;
        HBRUSH hBackgroundBrush = nullptr;
        HPEN hBorderPen = nullptr;
        HPEN hProgressPen = nullptr;
        
        void UpdateMessage(const std::string& msg) {
            std::lock_guard<std::mutex> lock(dataMutex);
            currentMessage = msg;
        }
        
        std::string GetMessage() {
            std::lock_guard<std::mutex> lock(dataMutex);
            return currentMessage;
        }
    };
    
    // 线程控制
    std::unique_ptr<std::thread> dialogThread;
    std::atomic<bool> threadRunning{false};
    std::condition_variable messageCV;
    std::mutex messageMutex;
    std::queue<ProgressBarMessage> messageQueue;
    
    // 对话框数据
    std::shared_ptr<DialogData> dialogData;
    
    // 窗口类注册状态
    static bool IsWindowClassRegistered() {
        static std::once_flag registerFlag;
        static bool registered = false;
        
        std::call_once(registerFlag, []() {
            WNDCLASSEXW wc = {0};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.lpszClassName = L"DynamicProgressBarClass";
            
            registered = RegisterClassExW(&wc) != 0;
        });
        
        return registered;
    }
    
    // 窗口过程（静态，通过用户数据获取实例）
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            DialogData* pData = reinterpret_cast<DialogData*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pData));
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        
        DialogData* pData = reinterpret_cast<DialogData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        
        switch (msg) {
            case WM_CREATE: {
                if (!pData) return -1;
                
                pData->hwnd = hwnd;
                
                // 创建字体和画刷
                pData->hFont = CreateFontW(
                    24, 0, 0, 0, FW_NORMAL, 
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑"
                );
                
                pData->hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
                pData->hBorderPen = CreatePen(PS_SOLID, 1, RGB(100, 100, 100));
                pData->hProgressPen = CreatePen(PS_SOLID, 1, RGB(0, 120, 215));
                
                return 0;
            }
            
            case WM_PAINT: {
                if (!pData) break;
                
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                uint64_t current = pData->currentValue.load();
                uint64_t max = pData->maxValue.load();
                std::string message = pData->GetMessage();
                double progress = (max > 0) ? 
                    static_cast<double>(current) / static_cast<double>(max) * 100.0 : 0.0;
                
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int width = clientRect.right - clientRect.left;
                int height = clientRect.bottom - clientRect.top;
                
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
                HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
                
                FillRect(memDC, &clientRect, pData->hBackgroundBrush);
                
                HFONT oldFont = NULL;
                if (pData->hFont) {
                    oldFont = (HFONT)SelectObject(memDC, pData->hFont);
                }
                
                SetTextColor(memDC, RGB(0, 0, 0));
                SetBkMode(memDC, TRANSPARENT);
                
                if (!message.empty()) {
                    std::wstring wmessage = utf8ToWide(message);
                    RECT textRect = {20, 20, clientRect.right - 20, 50};
                    DrawTextW(memDC, wmessage.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
                }
                
                std::wstring progressText = L"进度: " + 
                    std::to_wstring(static_cast<int>(progress)) + L"% (" +
                    std::to_wstring(current) + L" / " + std::to_wstring(max) + L")";
                
                RECT percentRect = {20, 60, clientRect.right - 20, 90};
                DrawTextW(memDC, progressText.c_str(), -1, &percentRect, DT_LEFT | DT_TOP);
                
                RECT progressRect = {20, 100, clientRect.right - 20, 130};
                
                if (progress > 0) {
                    int fillWidth = static_cast<int>(
                        (progressRect.right - progressRect.left) * progress / 100.0);
                    
                    RECT fillRect = progressRect;
                    fillRect.right = progressRect.left + fillWidth;
                    
                    HBRUSH hProgressBrush = CreateSolidBrush(RGB(0, 150, 0));
                    FillRect(memDC, &fillRect, hProgressBrush);
                    DeleteObject(hProgressBrush);
                }
                
                HPEN oldPen = (HPEN)SelectObject(memDC, pData->hBorderPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(HOLLOW_BRUSH));
                Rectangle(memDC, progressRect.left, progressRect.top, 
                        progressRect.right, progressRect.bottom);
                
                if (pData->hFont) {
                    SelectObject(memDC, oldFont);
                }
                SelectObject(memDC, oldPen);
                SelectObject(memDC, oldBrush);
                
                BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
                
                SelectObject(memDC, oldBitmap);
                DeleteObject(memBitmap);
                DeleteDC(memDC);
                
                EndPaint(hwnd, &ps);
                return 0;
            }
            
            case WM_CLOSE:
                //防止用户关闭对话框
                return 0;
                
            case WM_DESTROY:
                if (pData) {
                    pData->isFinished.store(true);
                    
                    // 清理资源
                    if (pData->hFont) DeleteObject(pData->hFont);
                    if (pData->hBackgroundBrush) DeleteObject(pData->hBackgroundBrush);
                    if (pData->hBorderPen) DeleteObject(pData->hBorderPen);
                    if (pData->hProgressPen) DeleteObject(pData->hProgressPen);
                    
                    pData->hwnd = nullptr;
                }
                
                PostQuitMessage(0);
                return 0;
                
            case WM_ERASEBKGND:
                return 1;
                
            default:
                return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    
    void DialogThreadProc() {
        if (!IsWindowClassRegistered()) {
            threadRunning.store(false);
            return;
        }
        
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int windowWidth = 600;
        int windowHeight = 190;
        int x = (screenWidth - windowWidth) / 2;
        int y = (screenHeight - windowHeight) / 2;
        
        HWND hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            L"DynamicProgressBarClass",
            dialogData->title.c_str(),
            WS_POPUP | WS_CAPTION | WS_SYSMENU,
            x, y, windowWidth, windowHeight,
            dialogData->hwndParent,
            nullptr,
            GetModuleHandleW(nullptr),
            dialogData.get()
        );
        
        if (!hwnd) {
            threadRunning.store(false);
            return;
        }
        
        dialogData->hwnd = hwnd;
        
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        
        MSG msg;
        while (threadRunning.load()) {
            
            {
                std::unique_lock<std::mutex> lock(messageMutex);
                messageCV.wait_for(lock, std::chrono::milliseconds(10), 
                    [this]() { return !messageQueue.empty(); });
                
                while (!messageQueue.empty()) {
                    ProgressBarMessage message = messageQueue.front();
                    messageQueue.pop();
                    lock.unlock();
                    
                    ProcessMessage(message);
                    lock.lock();
                }
            }
            
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    threadRunning.store(false);
                    break;
                }
                
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            
            if (!IsWindow(hwnd) || dialogData->isFinished.load()) {
                threadRunning.store(false);
            }
        }
        
        if (IsWindow(hwnd)) {
            DestroyWindow(hwnd);
        }
    }
    
    void ProcessMessage(const ProgressBarMessage& msg) {
        switch (msg.type) {
            case ProgressBarMessageType::SetValue:
                dialogData->currentValue.store(msg.currentValue);
                dialogData->maxValue.store(msg.maxValue);
                dialogData->UpdateMessage(msg.message);
                
                if (dialogData->hwnd && IsWindow(dialogData->hwnd)) {
                    InvalidateRect(dialogData->hwnd, nullptr, TRUE);
                    UpdateWindow(dialogData->hwnd);
                }
                break;
                
            case ProgressBarMessageType::Close:
                dialogData->isFinished.store(true);
                threadRunning.store(false);
                if (dialogData->hwnd && IsWindow(dialogData->hwnd)) {
                    DestroyWindow(dialogData->hwnd);
                }
                break;
        }
    }
    
    void PostMessageToThread(ProgressBarMessageType type, uint64_t current = 0, 
                           uint64_t max = 0, const std::string& message = "") {
        std::lock_guard<std::mutex> lock(messageMutex);
        messageQueue.emplace(type, current, max, message);
        messageCV.notify_one();
    }
    
public:
    
    DynamicProgressBar(const std::string& title, const std::string& initialMessage, HWND hParent = nullptr)
        : dialogData(std::make_shared<DialogData>()) {
        
        dialogData->hwndParent = hParent;
        dialogData->title = utf8ToWide(title);
        dialogData->UpdateMessage(initialMessage);
        
        threadRunning.store(true);
        dialogThread = std::make_unique<std::thread>([this]() {
            DialogThreadProc();
        });
        //dialogThread->detach();
    }
    
    ~DynamicProgressBar() override {
        Close();
        
        if (dialogThread && dialogThread->joinable()) {
            dialogThread->join();
        }
    }
    
    DynamicProgressBar(DynamicProgressBar&& other) noexcept
        : dialogThread(std::move(other.dialogThread))
        , threadRunning(other.threadRunning.load())
        , dialogData(std::move(other.dialogData)) {
    }
    
    DynamicProgressBar& operator=(DynamicProgressBar&& other) noexcept {
        if (this != &other) {
            Close();
            
            if (dialogThread && dialogThread->joinable()) {
                dialogThread->join();
            }
            
            dialogThread = std::move(other.dialogThread);
            threadRunning = other.threadRunning.load();
            dialogData = std::move(other.dialogData);
        }
        return *this;
    }
    
    void SetValue(uint64_t currentValue, uint64_t maxValue, const std::string& message) {
        if (!threadRunning.load() || dialogData->isFinished.load()) {
            return;
        }
        
        PostMessageToThread(ProgressBarMessageType::SetValue, currentValue, maxValue, message);
    }
    
    void GetProgressInfo(uint64_t& current, uint64_t& max, std::string& message, double& progress) {
        if (!dialogData) return;
        
        current = dialogData->currentValue.load();
        max = dialogData->maxValue.load();
        message = dialogData->GetMessage();
        progress = (max > 0) ? 
            static_cast<double>(current) / static_cast<double>(max) * 100.0 : 0.0;
    }
    
    void Show() override {
        if(dialogData){
            ShowWindow(dialogData->hwnd,SW_SHOW);
        }
    }
    
    void Close() override {
        if (threadRunning.load() && !dialogData->isFinished.load()) {
            PostMessageToThread(ProgressBarMessageType::Close);
        }
    }
    
    bool IsFinished() const {
        return dialogData ? dialogData->isFinished.load() : true;
    }
    
    HWND GetWindowHandle() const {
        return dialogData ? dialogData->hwnd : nullptr;
    }
};

/**
 * @brief 创建进度条动态对话框实例
 * 
 * @param title 对话框标题
 * @param initialMessage 初始时对话框内的提示文本
 * @param hParent 父窗口句柄
 * @return 创建的进度条动态对话框实例
 */
DynamicProgressBar CreateDynamicProgressBar(const std::string& title, 
                                           const std::string& initialMessage,
                                           HWND hParent = nullptr) {
    return DynamicProgressBar(title, initialMessage, hParent);
}

namespace{
    // 滑动条消息类型
    enum class SliderMessageType {
        SetValue,
        Close,
        SetRange,
        SetCallback
    };

    // 滑动条回调消息类型
    enum class DynamicSliderCallbackMessageType {
        Dragging,
        Released
    };

    // 滑动条消息结构
    struct SliderMessage {
        SliderMessageType type;
        int value;
        int minValue;
        int maxValue;
        std::function<int(DynamicSliderCallbackMessageType, int)> callback;
        
        SliderMessage(SliderMessageType t, int v = 0, int min = 0, int max = 100, 
                     std::function<int(DynamicSliderCallbackMessageType, int)> cb = nullptr)
            : type(t), value(v), minValue(min), maxValue(max), callback(cb) {}
    };
}

// 动态滑动条接口类
class DynamicSlider : public DynamicDialogInterface {
private:
    // 对话框数据
    struct DialogData {
        DynamicSlider* parentObject;

        HWND hwnd = nullptr;
        HWND hwndParent = nullptr;
        std::wstring title;
        std::atomic<int> currentValue{0};
        std::atomic<int> minValue{0};
        std::atomic<int> maxValue{100};
        std::atomic<bool> isFinished{false};
        std::atomic<bool> isDragging{false};
        std::string currentMessage;
        std::mutex dataMutex;
        
        // 回调函数
        std::function<int(DynamicSliderCallbackMessageType, int)> callback;
        std::mutex callbackMutex;
        
        // UI资源
        HFONT hFont = nullptr;
        HFONT hValueFont = nullptr;
        HBRUSH hBackgroundBrush = nullptr;
        HBRUSH hSliderBgBrush = nullptr;
        HBRUSH hSliderThumbBrush = nullptr;
        HPEN hBorderPen = nullptr;
        HPEN hThumbPen = nullptr;
        
        // 滑条区域
        RECT sliderRect = {0, 0, 0, 0};
        int thumbSize = 20;
        
        void UpdateMessage(const std::string& msg) {
            std::lock_guard<std::mutex> lock(dataMutex);
            currentMessage = msg;
        }
        
        std::string GetMessage() {
            std::lock_guard<std::mutex> lock(dataMutex);
            return currentMessage;
        }
        
        void SetCallback(std::function<int(DynamicSliderCallbackMessageType, int)> cb) {
            std::lock_guard<std::mutex> lock(callbackMutex);
            callback = cb;
        }
        
        int CallCallback(DynamicSliderCallbackMessageType type, int value) {
            std::lock_guard<std::mutex> lock(callbackMutex);
            if (callback) {
                return callback(type, value);
            }else{
                return value;
            }
        }
    };
    
    // 线程控制
    std::unique_ptr<std::thread> dialogThread;
    std::atomic<bool> threadRunning{false};
    std::condition_variable messageCV;
    std::mutex messageMutex;
    std::queue<SliderMessage> messageQueue;
    
    // 对话框数据
    std::shared_ptr<DialogData> dialogData;
    
    // 窗口类注册状态
    static bool IsWindowClassRegistered() {
        static std::once_flag registerFlag;
        static bool registered = false;
        
        std::call_once(registerFlag, []() {
            WNDCLASSEXW wc = {0};
            wc.cbSize = sizeof(WNDCLASSEXW);
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = WindowProc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.lpszClassName = L"DynamicSliderClass";
            
            registered = RegisterClassExW(&wc) != 0;
        });
        
        return registered;
    }
    
    // 窗口过程（静态，通过用户数据获取实例）
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        if (msg == WM_NCCREATE) {
            CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
            DialogData* pData = reinterpret_cast<DialogData*>(pCreate->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pData));
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        
        DialogData* pData = reinterpret_cast<DialogData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        
        switch (msg) {
            case WM_CREATE: {
                if (!pData) return -1;
                
                pData->hwnd = hwnd;
                
                // 创建字体和画刷
                pData->hFont = CreateFontW(
                    24, 0, 0, 0, FW_NORMAL, 
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑"
                );
                
                pData->hValueFont = CreateFontW(
                    24, 0, 0, 0, FW_SEMIBOLD, 
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                    L"微软雅黑"
                );
                
                pData->hBackgroundBrush = CreateSolidBrush(RGB(240, 240, 240));
                pData->hSliderBgBrush = CreateSolidBrush(RGB(200, 200, 200));
                pData->hSliderThumbBrush = CreateSolidBrush(RGB(255, 255, 255));
                pData->hBorderPen = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
                pData->hThumbPen = CreatePen(PS_SOLID, 1, RGB(120, 120, 120));
                
                return 0;
            }
            
            case WM_PAINT: {
                if (!pData) break;
                
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                
                int currentValue = pData->currentValue.load();
                int minValue = pData->minValue.load();
                int maxValue = pData->maxValue.load();
                std::string message = pData->GetMessage();
                
                RECT clientRect;
                GetClientRect(hwnd, &clientRect);
                int width = clientRect.right - clientRect.left;
                int height = clientRect.bottom - clientRect.top;
                
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
                HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);
                
                FillRect(memDC, &clientRect, pData->hBackgroundBrush);
                
                HFONT oldFont = NULL;
                if (pData->hFont) {
                    oldFont = (HFONT)SelectObject(memDC, pData->hFont);
                }
                
                SetTextColor(memDC, RGB(0, 0, 0));
                SetBkMode(memDC, TRANSPARENT);
                
                if (!message.empty()) {
                    std::wstring wmessage = utf8ToWide(message);
                    RECT textRect = {20, 20, clientRect.right - 20, 60};
                    DrawTextW(memDC, wmessage.c_str(), -1, &textRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
                }
                
                std::wstring valueText = L"当前值: " + std::to_wstring(currentValue);
                RECT valueRect = {20, 60, clientRect.right - 20, 90};
                if (pData->hValueFont) {
                    SelectObject(memDC, pData->hValueFont);
                }
                DrawTextW(memDC, valueText.c_str(), -1, &valueRect, DT_LEFT | DT_TOP);
                
                if (pData->hFont && oldFont) {
                    SelectObject(memDC, pData->hFont);
                }
                
                pData->sliderRect.left = 20;
                pData->sliderRect.right = clientRect.right - 20;
                pData->sliderRect.top = 110;
                pData->sliderRect.bottom = 140;
                
                RECT sliderBgRect = pData->sliderRect;
                FillRect(memDC, &sliderBgRect, pData->hSliderBgBrush);
                
                HPEN oldPen = (HPEN)SelectObject(memDC, pData->hBorderPen);
                HBRUSH oldBrush = (HBRUSH)SelectObject(memDC, GetStockObject(HOLLOW_BRUSH));
                Rectangle(memDC, sliderBgRect.left, sliderBgRect.top, 
                        sliderBgRect.right, sliderBgRect.bottom);
                
                int numTicks = 5;
                for (int i = 0; i <= numTicks; i++) {
                    float ratio = static_cast<float>(i) / static_cast<float>(numTicks);
                    int tickX = pData->sliderRect.left + 
                               static_cast<int>(ratio * (pData->sliderRect.right - pData->sliderRect.left));
                    
                    MoveToEx(memDC, tickX, pData->sliderRect.bottom + 2, NULL);
                    LineTo(memDC, tickX, pData->sliderRect.bottom + 8);
                    
                    int tickValue = minValue + static_cast<int>(ratio * (maxValue - minValue));
                    std::wstring tickText = std::to_wstring(tickValue);
                    RECT tickRect = {tickX - 40, pData->sliderRect.bottom + 10, tickX + 40, pData->sliderRect.bottom + 30};
                    DrawTextW(memDC, tickText.c_str(), -1, &tickRect, DT_CENTER | DT_TOP);
                }

                 int thumbX = GetThumbPosition(hwnd);
                int thumbY = (pData->sliderRect.top + pData->sliderRect.bottom) / 2;
                int thumbRadius = pData->thumbSize / 2;
                
                RECT thumbRect = {
                    thumbX,
                    thumbY - thumbRadius - 10,
                    thumbX + pData->thumbSize,
                    thumbY + thumbRadius + 10
                };
                
                FillRect(memDC, &thumbRect, pData->hSliderThumbBrush);

                // SelectObject(memDC, pData->hSliderThumbBrush);
                // SelectObject(memDC, pData->hThumbPen);
                // Ellipse(memDC,thumbRect.left, thumbRect.top, thumbRect.right, thumbRect.bottom);
                
                Rectangle(memDC, thumbRect.left, thumbRect.top, thumbRect.right, thumbRect.bottom);
                
                if (oldFont) {
                    SelectObject(memDC, oldFont);
                }
                SelectObject(memDC, oldPen);
                SelectObject(memDC, oldBrush);
                
                BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
                
                SelectObject(memDC, oldBitmap);
                DeleteObject(memBitmap);
                DeleteDC(memDC);
                
                EndPaint(hwnd, &ps);
                return 0;
            }
            
            case WM_LBUTTONDOWN: {
                if (!pData) break;
                
                int mouseX = GET_X_LPARAM(lParam);
                int mouseY = GET_Y_LPARAM(lParam);
                
                // 检查是否点击在滑块上
                int thumbX = GetThumbPosition(hwnd);
                int thumbY = (pData->sliderRect.top + pData->sliderRect.bottom) / 2;
                int thumbRadius = pData->thumbSize / 2;
                
                RECT thumbRect = {
                    thumbX,
                    thumbY - thumbRadius,
                    thumbX + pData->thumbSize,
                    thumbY + thumbRadius
                };
                
                // 检查是否点击在滑条区域内
                POINT pt = {mouseX, mouseY};
                if (PtInRect(&thumbRect, pt) || 
                    (mouseY >= pData->sliderRect.top && mouseY <= pData->sliderRect.bottom &&
                     mouseX >= pData->sliderRect.left && mouseX <= pData->sliderRect.right)) {
                    
                    pData->isDragging.store(true);
                    SetCapture(hwnd);
                    UpdateValueFromMouse(hwnd, mouseX);
                    InvalidateRect(hwnd, nullptr, TRUE);
                    return 0;
                }
                break;
            }
            
            case WM_MOUSEMOVE: {
                if (!pData) break;
                
                if (pData->isDragging.load()) {
                    int mouseX = GET_X_LPARAM(lParam);
                    UpdateValueFromMouse(hwnd, mouseX);
                    InvalidateRect(hwnd, nullptr, TRUE);
                    return 0;
                }
                break;
            }
            
            case WM_LBUTTONUP: {
                if (!pData) break;
                
                if (pData->isDragging.load()) {
                    pData->isDragging.store(false);
                    ReleaseCapture();
                    
                    // 触发释放回调
                    pData->currentValue.store(pData->CallCallback(DynamicSliderCallbackMessageType::Released, 
                                       pData->currentValue.load()));
                    
                    return 0;
                }
                break;
            }
            
            case WM_CLOSE:
                if(pData){
                    pData->parentObject->PostMessageToThread(SliderMessageType::Close);
                }
                return 0;
                
            case WM_DESTROY:
                if (pData) {
                    pData->isFinished.store(true);
                    
                    // 清理资源
                    if (pData->hFont) DeleteObject(pData->hFont);
                    if (pData->hValueFont) DeleteObject(pData->hValueFont);
                    if (pData->hBackgroundBrush) DeleteObject(pData->hBackgroundBrush);
                    if (pData->hSliderBgBrush) DeleteObject(pData->hSliderBgBrush);
                    if (pData->hSliderThumbBrush) DeleteObject(pData->hSliderThumbBrush);
                    if (pData->hBorderPen) DeleteObject(pData->hBorderPen);
                    if (pData->hThumbPen) DeleteObject(pData->hThumbPen);
                    
                    pData->hwnd = nullptr;
                }
                
                PostQuitMessage(0);
                return 0;
                
            case WM_ERASEBKGND:
                return 1;
                
            default:
                return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    
    // 辅助函数：获取滑块位置
    static int GetThumbPosition(HWND hwnd) {
        DialogData* pData = reinterpret_cast<DialogData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!pData) return 0;
        
        int value = pData->currentValue.load();
        int min = pData->minValue.load();
        int max = pData->maxValue.load();
        
        if (max <= min) return pData->sliderRect.left;
        
        float ratio = static_cast<float>(value - min) / static_cast<float>(max - min);
        int range = pData->sliderRect.right - pData->sliderRect.left - pData->thumbSize;
        return pData->sliderRect.left + static_cast<int>(ratio * range);
    }
    
    // 辅助函数：根据鼠标位置更新值
    static void UpdateValueFromMouse(HWND hwnd, int mouseX) {

        DialogData* pData = reinterpret_cast<DialogData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!pData) return;
        
        RECT sliderRect = pData->sliderRect;
        int thumbSize = pData->thumbSize;
        
        // 确保鼠标在滑条区域内
        mouseX = std::max<LONG>(sliderRect.left, std::min<LONG>(sliderRect.right - thumbSize, mouseX));
        
        int range = sliderRect.right - sliderRect.left - thumbSize;
        if (range <= 0) return;
        
        float ratio = static_cast<float>(mouseX - sliderRect.left) / static_cast<float>(range);
        
        int minValue = pData->minValue.load();
        int maxValue = pData->maxValue.load();
        int newValue = minValue + static_cast<int>(ratio * (maxValue - minValue));
        
        // 确保值在范围内
        newValue = std::max(minValue, std::min(maxValue, newValue));
        
        // 更新值
        newValue = pData->CallCallback(DynamicSliderCallbackMessageType::Dragging,newValue);
        pData->currentValue.store(newValue);
    }
    
    void DialogThreadProc() {
        if (!IsWindowClassRegistered()) {
            threadRunning.store(false);
            return;
        }
        
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);
        int windowWidth = 600;
        int windowHeight = 230;
        int x = (screenWidth - windowWidth) / 2;
        int y = (screenHeight - windowHeight) / 2;
        
        HWND hwnd = CreateWindowExW(
            WS_EX_DLGMODALFRAME,
            L"DynamicSliderClass",
            dialogData->title.c_str(),
            WS_POPUP | WS_CAPTION | WS_SYSMENU,
            x, y, windowWidth, windowHeight,
            dialogData->hwndParent,
            nullptr,
            GetModuleHandleW(nullptr),
            dialogData.get()
        );
        
        if (!hwnd) {
            threadRunning.store(false);
            return;
        }
        
        dialogData->hwnd = hwnd;
        
        ShowWindow(hwnd, SW_SHOW);
        UpdateWindow(hwnd);
        
        MSG msg;
        while (threadRunning.load()) {
            
            {
                std::unique_lock<std::mutex> lock(messageMutex);
                messageCV.wait_for(lock, std::chrono::milliseconds(10), 
                    [this]() { return !messageQueue.empty(); });
                
                while (!messageQueue.empty()) {
                    SliderMessage message = messageQueue.front();
                    messageQueue.pop();
                    lock.unlock();
                    
                    ProcessMessage(message);
                    lock.lock();
                }
            }
            
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) {
                    threadRunning.store(false);
                    break;
                }
                
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            
            if (!IsWindow(hwnd) || dialogData->isFinished.load()) {
                threadRunning.store(false);
            }
        }
        
        if (IsWindow(hwnd)) {
            DestroyWindow(hwnd);
        }
    }
    
    void ProcessMessage(const SliderMessage& msg) {
        switch (msg.type) {
            case SliderMessageType::SetValue:
                dialogData->currentValue.store(msg.value);
                
                if (dialogData->hwnd && IsWindow(dialogData->hwnd)) {
                    InvalidateRect(dialogData->hwnd, nullptr, TRUE);
                    UpdateWindow(dialogData->hwnd);
                }
                break;
                
            case SliderMessageType::SetRange:
            {
                dialogData->minValue.store(msg.minValue);
                dialogData->maxValue.store(msg.maxValue);
                
                // 确保当前值在范围内
                int current = dialogData->currentValue.load();
                if (current < msg.minValue) dialogData->currentValue.store(msg.minValue);
                if (current > msg.maxValue) dialogData->currentValue.store(msg.maxValue);
                
                if (dialogData->hwnd && IsWindow(dialogData->hwnd)) {
                    InvalidateRect(dialogData->hwnd, nullptr, TRUE);
                    UpdateWindow(dialogData->hwnd);
                }
                break;
            }   
            case SliderMessageType::SetCallback:
                dialogData->SetCallback(msg.callback);
                break;
                
            case SliderMessageType::Close:
                dialogData->isFinished.store(true);
                threadRunning.store(false);
                if (dialogData->hwnd && IsWindow(dialogData->hwnd)) {
                    DestroyWindow(dialogData->hwnd);
                }
                break;
        }
    }
    
    void PostMessageToThread(SliderMessageType type, int value = 0, 
                           int minValue = 0, int maxValue = 100,
                           std::function<int(DynamicSliderCallbackMessageType, int)> callback = nullptr) {
        std::lock_guard<std::mutex> lock(messageMutex);
        messageQueue.emplace(type, value, minValue, maxValue, callback);
        messageCV.notify_one();
    }
    
public:
    
    DynamicSlider(const std::string& title, const std::string& initialMessage, 
                 int minValue, int maxValue, int initialValue,
                 std::function<int(DynamicSliderCallbackMessageType, int)> callback,
                 HWND hParent = nullptr)
        : dialogData(std::make_shared<DialogData>()) {
        
        dialogData->parentObject = this;
        dialogData->hwndParent = hParent;
        dialogData->title = utf8ToWide(title);
        dialogData->UpdateMessage(initialMessage);
        dialogData->minValue.store(minValue);
        dialogData->maxValue.store(maxValue);
        dialogData->currentValue.store(initialValue);
        dialogData->SetCallback(callback);
        
        threadRunning.store(true);
        dialogThread = std::make_unique<std::thread>([this]() {
            DialogThreadProc();
        });
    }
    
    ~DynamicSlider() override {
        Close();
        
        if (dialogThread && dialogThread->joinable()) {
            dialogThread->join();
        }
    }
    
    DynamicSlider(DynamicSlider&& other) noexcept
        : dialogThread(std::move(other.dialogThread))
        , threadRunning(other.threadRunning.load())
        , dialogData(std::move(other.dialogData)) {
    }
    
    DynamicSlider& operator=(DynamicSlider&& other) noexcept {
        if (this != &other) {
            Close();
            
            if (dialogThread && dialogThread->joinable()) {
                dialogThread->join();
            }
            
            dialogThread = std::move(other.dialogThread);
            threadRunning = other.threadRunning.load();
            dialogData = std::move(other.dialogData);
        }
        return *this;
    }
    
    void SetValue(int value) {
        if (!threadRunning.load() || dialogData->isFinished.load()) {
            return;
        }
        
        PostMessageToThread(SliderMessageType::SetValue, value);
    }
    
    void SetRange(int minValue, int maxValue) {
        if (!threadRunning.load() || dialogData->isFinished.load()) {
            return;
        }
        
        PostMessageToThread(SliderMessageType::SetRange, 0, minValue, maxValue);
    }
    
    void SetCallback(std::function<int(DynamicSliderCallbackMessageType, int)> callback) {
        if (!threadRunning.load() || dialogData->isFinished.load()) {
            return;
        }
        
        PostMessageToThread(SliderMessageType::SetCallback, 0, 0, 0, callback);
    }
    
    void GetSliderInfo(int& current, int& min, int& max, std::string& message) {
        if (!dialogData) return;
        
        current = dialogData->currentValue.load();
        min = dialogData->minValue.load();
        max = dialogData->maxValue.load();
        message = dialogData->GetMessage();
    }
    
    void Show() override {
        if(dialogData){
            ShowWindow(dialogData->hwnd, SW_SHOW);
        }
    }
    
    void Close() override {
        if (threadRunning.load() && !dialogData->isFinished.load()) {
            PostMessageToThread(SliderMessageType::Close);
        }
    }
    
    bool IsFinished() const {
        return dialogData ? dialogData->isFinished.load() : true;
    }
    
    HWND GetWindowHandle() const {
        return dialogData ? dialogData->hwnd : nullptr;
    }
    
    bool IsDragging() const {
        return dialogData ? dialogData->isDragging.load() : false;
    }
};

/**
 * @brief 创建滑动条动态对话框实例
 * 
 * @param title 对话框标题
 * @param initialMessage 初始时对话框内的提示文本
 * @param minValue 滑动条最小值
 * @param maxValue 滑动条最大值
 * @param callbackOnValueChange 值改变时的回调函数，参数为事件类型和当前值
 * @param hParent 父窗口句柄
 * @return 创建的滑动条动态对话框实例
 */
DynamicSlider CreateDynamicSlider(const std::string& title, 
                                 const std::string& initialMessage,
                                 int minValue,
                                 int maxValue,
                                 int initialValue,
                                 std::function<int(DynamicSliderCallbackMessageType, int)> callbackOnValueChange,
                                 HWND hParent = nullptr) {
    return DynamicSlider(title, initialMessage, minValue, maxValue, initialValue, callbackOnValueChange, hParent);
}

#endif
