#pragma once
#include "common.hpp"
using namespace std;

class JoyPad;

class Gui:public Object{
    private:
        bool quit;
        JoyPad* joy_pad;
        int SCREEN_WIDTH = WIDTH;
        int SCREEN_HEIGHT = HEIGHT;
        void HandleKeyUp(void *e);
        void HandleKeyDown(void *e);
        BUTTON_KIND SdlScancode2KeyCode(void *e);
        bool button_state[BUTTON_KIND_CNT];
        // Pixel* image; // TODO: replace
    public:
        Gui(JoyPad* joy_pad);
        void Update();
        void PollEvents();
        bool IsQuit();
        void SetPixel(int x, int y, Pixel pixel);
};
