#include "JoyPad.hpp"
#include "Gui.hpp"

Gui::Gui(JoyPad* joy_pad){
    this->joy_pad = joy_pad;
    assert(joy_pad!=NULL);
    // init buttons
    for(int i=0; i<BUTTON_KIND_CNT; i++){
        this->button_state[i] = false;
    }
    this->quit  = false;
    // TODO: Init renderer
    // create render buffer TODO: how can we do this?
    // this->image = (Pixel*)malloc(this->SCREEN_WIDTH*this->SCREEN_HEIGHT*sizeof(Pixel));
}

void Gui::Update(){
    /*
    SDL_UpdateTexture(this->texture, NULL, this->image, this->SCREEN_WIDTH * sizeof(Pixel));
    SDL_RenderCopy(this->renderer, this->texture, NULL, NULL);
    SDL_RenderPresent(this->renderer);
    */
    // TODO: send the updated frame to the screen
}

bool Gui::IsQuit(){
    return this->quit;
}

void Gui::PollEvents(){
    /*
    SDL_Event e;
    while (SDL_PollEvent(&e)){
        if (e.type == SDL_QUIT){
            this->quit = true;
        }
        if(e.type==SDL_KEYDOWN){
            this->HandleKeyDown(&e);
            break;
        }
        if(e.type==SDL_KEYUP){
            this->HandleKeyUp(&e);
            break;
        }
    }
    */
    this->joy_pad->UpdateButtonState(this->button_state);
}

void Gui::SetPixel(int x, int y, Pixel pixel){
    // TODO: update the api so that we handle more than a pixel
    // this->image[x+y*this->SCREEN_WIDTH] = pixel;
}

BUTTON_KIND Gui::SdlScancode2KeyCode(void *e){
    /*
    switch (e->key.keysym.sym){
        case SDLK_a:
            return BUTTON_A_KIND;
        case SDLK_b:
            return BUTTON_B_KIND;
        case SDLK_SPACE:
            return BUTTON_SELECT_KIND;
        case SDLK_RETURN:
            return BUTTON_START_KIND;
        case SDLK_UP:
            return BUTTON_UP_KIND;
        case SDLK_DOWN:
            return BUTTON_DOWN_KIND;
        case SDLK_LEFT:
            return BUTTON_LEFT_KIND;
        case SDLK_RIGHT:
            return BUTTON_RIGHT_KIND;
        default:
            return NOT_BUTTON_KIND;
    }
    */
    return NOT_BUTTON_KIND;
}

void Gui::HandleKeyUp(void *e){
    /*
    BUTTON_KIND button_kind;
    button_kind = this->SdlScancode2KeyCode(e);
    if(button_kind==NOT_BUTTON_KIND){
        return;
    }
    this->button_state[button_kind] = false;
    */
}

void Gui::HandleKeyDown(void *e){
    /*
    BUTTON_KIND button_kind;
    button_kind = this->SdlScancode2KeyCode(e);
    if(button_kind==NOT_BUTTON_KIND){
        return;
    }
    this->button_state[button_kind] = true;
    */
}
