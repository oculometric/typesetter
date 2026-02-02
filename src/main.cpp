#include <chrono>
#include <iostream>
#include <thread>

#include "strn.h"

const char* saturn_ascii = 
" .             "
" .##           "
"  #  ..        "
"     #####-    "
"   #########   "
"   ###.#####.  "
"    ###-####   "
"     .### .    "
"          # -# "
"            ## ";

using namespace STRN;

int main()
{
    NativeRasteriser comp;
    Window* window = new Window("", true, false);
    comp.insertDrawable(window);
    
    Builder b;
    b.beginHorizontalBox();
    b.label("lab1");
    b.label("lab2");
    b.label("lab3");
    b.endHorizontalBox();
    b.artBlock(saturn_ascii, 15);
    b.label("help");
    window->setRoot(b.end());

    constexpr std::chrono::duration<double> ideal_frame_time(1.0 / 60.0);
    
    while (true)
    {
        auto last = std::chrono::high_resolution_clock::now();
        if (comp.update())
            break;

        window->setSize(comp.getSize());
        comp.render();
        comp.present();
    //     STRN::KeyEvent key = comp.getKeyEvent();
    //     while (key.key != 0)
    //     {
    //         if (/*key.key >= 'A' && key.key <= 'Z' && */key.pressed)
    //             window2->setTitle(window2->getTitle() + (char)key.key);
    //         key = comp.getKeyEvent();
    //     }
        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = now - last;        
        auto sleep_duration = ideal_frame_time - duration;
        std::this_thread::sleep_for(sleep_duration); // prevents us from spinning too hard
    }
}