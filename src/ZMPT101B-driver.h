#ifndef ZMPT101B_DRIVER_H
#define ZMPT101B_DRIVER_H

#include <Arduino.h>

class ZMPT101B {
    public:
        ZMPT101B(uint8_t pin);
        void setSensitivity(float sensitivity);
        void setZeroPoint(int zeroPoint);
        
        // Core (Non-Blocking)
        void update(); 
        float getVoltageAC(); 
        
        int calibrateZeroPoint(); 

    private:
        uint8_t _pin;
        float _sensitivity;
        int _zeroPoint;
        
        // Non-blocking state
        uint32_t _lastSampleTime;
        const uint32_t _sampleInterval = 1000; // 1ms = 1000Hz sampling
        unsigned long _sumSquares;
        unsigned int _sampleCount;
        float _lastRMSVoltage;
};

#endif
