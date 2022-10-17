#pragma once
#include "common.hpp"
using namespace std;

class JoyPad:public Object{
    private:
        bool button_state[BUTTON_KIND_CNT];
        bool ready = false;
        int idx = 0;
    public:
        JoyPad();
        void UpdateButtonState(const bool *button_state_buff);
        void Write(uint8_t value);
        uint8_t Read();
};
