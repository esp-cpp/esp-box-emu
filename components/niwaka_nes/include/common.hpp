#pragma once
using namespace std;
#include <iostream>
#include <cstdint>
#include <thread>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <cassert>
#include <mutex>
#include <vector>
#include <condition_variable>
//#include <emscripten.h>
#include <memory>
#include "Object.hpp"
#define WIDTH 256
#define HEIGHT 240
enum BUTTON_KIND {BUTTON_A_KIND, BUTTON_B_KIND, BUTTON_SELECT_KIND, BUTTON_START_KIND, BUTTON_UP_KIND, BUTTON_DOWN_KIND, BUTTON_LEFT_KIND, BUTTON_RIGHT_KIND, BUTTON_KIND_CNT, NOT_BUTTON_KIND};

typedef struct _Pixel{
    uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
}Pixel;
