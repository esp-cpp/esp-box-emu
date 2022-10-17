#include "JoyPad.hpp"

JoyPad::JoyPad(){
    for(int i=0; i<BUTTON_KIND_CNT; i++){
        this->button_state[i] = false;
    }
}

void JoyPad::UpdateButtonState(const bool *button_state_buff){
    assert(button_state_buff!=NULL);
    for(int i=0; i<BUTTON_KIND_CNT; i++){
        this->button_state[i] = button_state_buff[i];
    }
}

void JoyPad::Write(uint8_t value){
    if(value==0x01){
        this->ready = true;
    }else if(this->ready&&value==0x00){
        this->ready = false;
        this->idx = 0;
    }
}

uint8_t JoyPad::Read(){
    if(this->idx==BUTTON_KIND_CNT){
        this->idx = 0;
    }
    if(this->button_state[this->idx]){
        this->button_state[this->idx] = false;
        this->idx++;
        return 1;
    }else{
        this->idx++;
        return 0;
    }
}