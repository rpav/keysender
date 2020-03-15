#include <windows.h>
#include <chrono>
#include <thread>
#include <iostream>
#include "workwindow.hpp"

BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
    if (!IsWindowVisible(hWnd) || !IsWindowEnabled(hWnd) || GetWindowTextLengthA(hWnd) == 0)
        return TRUE;
    std::vector<HWND> &hWnds =
        *reinterpret_cast<std::vector<HWND> *>(lParam);
    hWnds.push_back(hWnd);
    return TRUE;
}

BOOL CALLBACK EnumChildProc(HWND hWnd, LPARAM lParam)
{
    if (!IsWindowVisible(hWnd) || !IsWindowEnabled(hWnd))
        return TRUE;
    std::vector<HWND> &chWnds =
        *reinterpret_cast<std::vector<HWND> *>(lParam);
    chWnds.push_back(hWnd);
    return TRUE;
}

Napi::Object windowGetter(HWND hWnd, Napi::Env *env)
{
    Napi::Object window = Napi::Object::New(*env);
    window["handle"] = HandleToLong(hWnd);
    int titleLenght = GetWindowTextLengthA(hWnd);
    if (titleLenght > 0)
    {
        std::vector<wchar_t> title(titleLenght + 1);
        GetWindowTextW(hWnd, &title[0], title.size());
        title.pop_back();
        window["title"] = Napi::Buffer<wchar_t>::Copy(*env, title.data(), title.size());
    }
    else
        window["title"] = NULL;
    std::vector<wchar_t> className(256);
    GetClassNameW(hWnd, &className[0], className.size());
    className.resize(std::distance(className.begin(), std::search_n(className.begin(), className.end(), 2, 0)));
    className.shrink_to_fit();
    window["className"] = Napi::Buffer<wchar_t>::Copy(*env, className.data(), className.size());
    return window;
}

Napi::Value getWindow(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() == 0)
    {
        std::vector<HWND> hWnds;
        EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&hWnds));
        Napi::Array windows = Napi::Array::New(env);
        for (const HWND &hWnd : hWnds)
            windows[windows.Length()] = windowGetter(hWnd, &env);
        return windows;
    }
    if (info.Length() != 2)
        Napi::Error::New(env, "Expected 0 or 2 arguments: Buffer||NULL, Buffer||NULL")
            .ThrowAsJavaScriptException();
    return Napi::Number::New(env, HandleToLong(FindWindowW(
                                      info[1].IsNull() ? NULL : LPCWSTR(std::u16string(Napi::Buffer<char16_t>(env, info[1]).Data()).data()),
                                      info[0].IsNull() ? NULL : LPCWSTR(std::u16string(Napi::Buffer<char16_t>(env, info[0]).Data()).data()))));
}

Napi::Value getWindowChild(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() == 1 && info[0].IsNumber())
    {
        std::vector<HWND> chWnds;
        EnumChildWindows((HWND)info[0].As<Napi::Number>().Int64Value(), EnumChildProc, reinterpret_cast<LPARAM>(&chWnds));
        Napi::Array children = Napi::Array::New(env);
        if (!chWnds.empty())
            for (const HWND &hWnd : chWnds)
                children[children.Length()] = windowGetter(hWnd, &env);
        return children;
    }
    if (info.Length() != 3 || !info[0].IsNumber() || !info[1].IsBuffer() || !info[2].IsBuffer())
        Napi::Error::New(env, "Expected 1 or 3 arguments: Number, Buffer||NULL, Buffer||NULL")
            .ThrowAsJavaScriptException();
    return Napi::Number::New(env, HandleToLong(FindWindowExW((HWND)info[0].As<Napi::Number>().Int64Value(), NULL,
                                                             info[1].IsNull() ? NULL : LPCWSTR(std::u16string(Napi::Buffer<char16_t>(env, info[1]).Data()).data()),
                                                             info[2].IsNull() ? NULL : LPCWSTR(std::u16string(Napi::Buffer<char16_t>(env, info[2]).Data()).data()))));
}

void sleep(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    if (info.Length() != 1 || !info[0].IsNumber())
        Napi::Error::New(env, "Expected 1 argument: Number")
            .ThrowAsJavaScriptException();
    std::this_thread::sleep_for(std::chrono::milliseconds(info[0].As<Napi::Number>().Int32Value()));
}

Napi::Value Workwindow::capture(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    RECT area;
    int16_t width, height;
    if (info.Length() == 0)
    {
        GetClientRect(hWnd, &area);
        width = area.right - area.left;
        height = area.bottom - area.top;
    }
    else if (info.Length() != 4 || !info[0].IsNumber() || !info[1].IsNumber() || !info[2].IsNumber() || !info[3].IsNumber())
        Napi::Error::New(env, "Expected 4 arguments: Number, Number, Number, Number")
            .ThrowAsJavaScriptException();
    else
    {
        width = info[2].As<Napi::Number>().Int32Value();
        height = info[3].As<Napi::Number>().Int32Value();
        area.left = info[0].As<Napi::Number>().Int32Value();
        area.top = info[1].As<Napi::Number>().Int32Value();
        area.right = area.left + width;
        area.bottom = area.top + height;
    }
    const uint32_t size = width * height * 4;
    HDC context = GetDC(hWnd);
    BITMAPINFO bi;
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = -height;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = 0;
    bi.bmiHeader.biXPelsPerMeter = 0;
    bi.bmiHeader.biYPelsPerMeter = 0;
    bi.bmiHeader.biClrUsed = 0;
    bi.bmiHeader.biClrImportant = 0;
    uint8_t *pixels;
    HDC memDC = CreateCompatibleDC(context);
    HBITMAP section = CreateDIBSection(context, &bi, DIB_RGB_COLORS, (void **)&pixels, 0, 0);
    DeleteObject(SelectObject(memDC, section));
    BitBlt(memDC, 0, 0, width, height, context, area.left, area.top, SRCCOPY);
    DeleteDC(memDC);
    for (uint32_t i = 0; i < size; i += 4)
        std::swap(pixels[i], pixels[i + 2]);
    Napi::Object returnValue = Napi::Object::New(env);
    returnValue["data"] = Napi::Buffer<uint8_t>::Copy(env, pixels, size);
    returnValue["width"] = width;
    returnValue["height"] = height;
    DeleteObject(section);
    return returnValue;
}

void Workwindow::setWorkwindow(const Napi::CallbackInfo &info, const Napi::Value &value)
{
    Napi::Env env = info.Env();
    if (info.Length() != 1 || !info[0].IsNumber())
        Napi::Error::New(env, "Expected 1 argument: Number")
            .ThrowAsJavaScriptException();
    hWnd = (HWND)info[0].As<Napi::Number>().Int64Value();
};

Napi::Value Workwindow::getWorkwindow(const Napi::CallbackInfo &info)
{
    Napi::Env env = info.Env();
    return windowGetter(hWnd, &env);
};

Napi::Value Workwindow::isForeground(const Napi::CallbackInfo &info)
{
    return Napi::Boolean::New(info.Env(), hWnd == GetForegroundWindow());
};

Napi::Value Workwindow::isOpen(const Napi::CallbackInfo &info)
{
    return Napi::Boolean::New(info.Env(), IsWindow(hWnd));
};

void Workwindow::setForeground(const Napi::CallbackInfo &info)
{
    SetForegroundWindow(hWnd);
};