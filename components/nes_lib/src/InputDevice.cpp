//
// Created by Kyle Miller on 2/4/19.
//

#include "../include/InputDevice.h"

InputDevice::InputDevice(int num) {
    strobe = false;
    buttonNum = 0;
}

InputDevice::~InputDevice() {

}

uint8_t InputDevice::readController() {
    if (strobe) {
        reset();
    }
    
    bool pressed = true;
    if (buttonNum < 8) {
        pressed = virtualState[buttonNum];
    }
    buttonNum++;
    if (pressed) {
        return 0x1;
    }
    else {
        return 0x0;
    }
}

void InputDevice::writeController(uint8_t byte) {
    buttonNum = 0;
    strobe = ((byte&0x1) != 0);
    reset();
}

void InputDevice::reset() {
    memcpy(virtualState, externState, 8);
    buttonNum = 0;
}

