#include <chrono>
#include <iostream>
#include <thread>

#include <strn.h>

#include "editor.h"

using namespace STRN;
using namespace std;

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

int main()
{
    FreeConsole();
    NativeRasteriser comp;
    comp.setPalette(Palette{
        DEFAULT_COLOUR,
        DEFAULT_INVERTED,
        FG_BLACK | BG_LIGHT_GREY
        });
    comp.setScaleFactor(2.0f);
    EditorDrawable* e = new EditorDrawable();
    comp.insertDrawable(e);

    constexpr std::chrono::duration<double> ideal_frame_time(1.0 / 24.0);
    
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
        comp.setDistortion(e->getDistortion());
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = now - last;        
        auto sleep_duration = ideal_frame_time - duration;
        std::this_thread::sleep_for(sleep_duration); // prevents us from spinning too hard
    }
}