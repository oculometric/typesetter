#include <chrono>
#include <iostream>
#include <thread>
#include <cstring>

#include <strn.h>

#include "editor.h"
#include "../resource.h"

using namespace STRN;
using namespace std;

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
extern char _binary_iapetus_png_start[];
extern char _binary_iapetus_png_end[];
#endif

int main()
{
#if defined(_WIN32)
#if defined(GUI)
    FreeConsole();
#else
    SetConsoleTitle(L"IAPETUS");
#endif
#endif
    try
    {
#if defined(GUI)
        NativeRasteriser comp(false);
#if defined(_WIN32)
        const HRSRC res = FindResource(nullptr, MAKEINTRESOURCE(IDB_PNG1), L"PNG");
        const DWORD size = SizeofResource(nullptr, res);
        const HGLOBAL data = LoadResource(nullptr, res);
        vector<uint8_t> data_array(size);
        memcpy(data_array.data(), data, size);
#else
        vector<uint8_t> data_array(_binary_iapetus_png_end - _binary_iapetus_png_start);
        memcpy(data_array.data(), _binary_iapetus_png_start, data_array.size());
#endif
        comp.setWindowIcon(data_array);
        comp.setWindowTitle("IAPETUS");
        comp.setScaleFactor(2.0f);
#else
        TerminalRasteriser comp;
#endif

        comp.setPalette(Palette{
            DEFAULT_COLOUR,
            DEFAULT_INVERTED,
            FG_BLACK | BG_LIGHT_GREY
            });
        EditorDrawable* e = new EditorDrawable();
        comp.insertDrawable(e);

        constexpr std::chrono::duration<double> ideal_frame_time(1.0 / 60.0);
    
        while (true)
        {
            auto last = std::chrono::high_resolution_clock::now();
            if (comp.update())
                break;
            static Vec2 last_size = Vec2{ 0, 0 };
            if (comp.getSize() != last_size)
            {
                last_size = comp.getSize();
                e->setSize(last_size);
                e->setPosition({ 0, 0 });
                e->updateLines();
            }
            comp.render();
            comp.present();
            KeyEvent key = comp.getKeyEvent();
            while (key.key != 0)
            {
                e->keyEvent(key);
                key = comp.getKeyEvent();
            }
            unsigned int chr = comp.getCharEvent();
            while (chr != 0)
            {
                e->textEvent(chr);
                chr = comp.getCharEvent();
            }
#if defined(GUI)
            comp.setDistortion(e->getDistortion());
#endif
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration = now - last;        
            auto sleep_duration = ideal_frame_time - duration;
            std::this_thread::sleep_for(sleep_duration); // prevents us from spinning too hard
        }
    } catch (exception e)
    {
#if defined(_WIN32)
        const size_t WCHARBUF = 100;
        const char *szSource = e.what();
        wchar_t  wszDest[WCHARBUF];
        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, szSource, -1, wszDest, WCHARBUF);
        MessageBox(NULL, wszDest, NULL, 0);
#endif
    }
}