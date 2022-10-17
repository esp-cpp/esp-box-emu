#include "Object.hpp"
#include <iostream>
#include <cstdint>
#include <string>
#include <cstring>
#include <stdlib.h>
#include <stdarg.h>
using namespace std;

void Object::Error(const char* message, ...){
    char* message_with_ln = (char*)malloc(strlen(message)+2);
    strcpy(message_with_ln, message);
    message_with_ln[strlen(message)+1]   = '\0';
    message_with_ln[strlen(message)] = '\n';
    va_list ap;
    va_start(ap, message_with_ln);
    vfprintf(stderr, message_with_ln, ap);
    va_end(ap);
    exit(EXIT_FAILURE);
}
