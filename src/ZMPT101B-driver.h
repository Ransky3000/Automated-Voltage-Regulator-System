#ifndef ZMPT101B_DRIVER_H
#define ZMPT101B_DRIVER_H

#include <Arduino.h>

class ZMPT101B {
    public:
        ZMPT101B(uint8_t pin);
        float getVoltageAC();
        void calibrate();
    private:
        uint8_t _pin;
        float _sensitivity;
};

#endif
