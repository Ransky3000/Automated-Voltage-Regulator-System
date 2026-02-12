#ifndef MACHINE_H
#define MACHINE_H

#include <Arduino.h>

class Machine {
public:
    Machine();
    void begin();
    void update(); // Main state machine logic

private:
    // Add state variables here
};

#endif
