#include "InterruptManager.hpp"

InterruptManager::InterruptManager(){
    this->nmi = false;
}

void InterruptManager::ClearNmi(){
    this->nmi = false;
}

void InterruptManager::SetNmi(){
    this->nmi = true;
}

bool InterruptManager::HasNmi(){
    return this->nmi;
}