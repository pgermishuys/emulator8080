#include <iostream>
#include "window.h"
#include "emulator8080.h"

int main(){
    Emulator8080 emulator;
    emulator.Initialize();
    while(true) {
        emulator.AdvanceEmulationStep();
    }
}
