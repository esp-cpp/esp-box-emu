#pragma once
#include "common.hpp"
using namespace std;

class InterruptManager:public Object{
    private:
        bool nmi;
    public:
        InterruptManager();
        bool HasNmi();
        void SetNmi();
        void ClearNmi();
};