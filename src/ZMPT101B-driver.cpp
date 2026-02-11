#include "ZMPT101B-driver.h"

ZMPT101B::ZMPT101B(uint8_t pin) {
    _pin = pin;
    _sensitivity = 1.0; 
    _zeroPoint = 512; // Default mid-point for 10-bit ADC
    _lastSampleTime = 0;
    _sumSquares = 0;
    _sampleCount = 0;
    _lastRMSVoltage = 0.0;
}

void ZMPT101B::setSensitivity(float sensitivity) {
    _sensitivity = sensitivity;
}

void ZMPT101B::setZeroPoint(int zeroPoint) {
    _zeroPoint = zeroPoint;
}

void ZMPT101B::update() {
    if (micros() - _lastSampleTime >= _sampleInterval) {
        _lastSampleTime = micros();
        
        int raw = analogRead(_pin);
        int centered = raw - _zeroPoint;
        _sumSquares += (unsigned long)(centered * centered);
        _sampleCount++;
        
        // 20ms window (50Hz cycle) => 20 samples at 1ms interval
        if (_sampleCount >= 20) {
            float rms = sqrt((float)_sumSquares / _sampleCount);
             // Convert to Voltage (0-5V range, scaled)
             // This is a rough estimation, sensitivity handles the real scaling
            float voltage = (rms * 5.0) / 1023.0;
            
            _lastRMSVoltage = voltage * _sensitivity;
            
            // Reset buffer
            _sumSquares = 0;
            _sampleCount = 0;
        }
    }
}

float ZMPT101B::getVoltageAC() {
    return _lastRMSVoltage;
}

int ZMPT101B::calibrateZeroPoint() {
    long sum = 0;
    for(int i=0; i<100; i++) {
        sum += analogRead(_pin);
        delay(2);
    }
    _zeroPoint = sum / 100;
    return _zeroPoint;
}
