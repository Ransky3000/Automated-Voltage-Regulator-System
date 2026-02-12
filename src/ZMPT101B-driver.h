#ifndef ZMPT101B_driver_h
#define ZMPT101B_driver_h

#include <Arduino.h>

#define DEFAULT_FREQUENCY 50.0f
#define DEFAULT_SENSITIVITY 500.0f

#if defined(AVR)
	#define ADC_SCALE 1023.0f
	#define VREF 5.0f
#elif defined(ESP8266)
	#define ADC_SCALE 1023.0
	#define VREF 3.3
#elif defined(ESP32)
	#define ADC_SCALE 4095.0
	#define VREF 3.3
#endif

class ZMPT101B
{
public:
	ZMPT101B (uint8_t pin, uint16_t frequency = DEFAULT_FREQUENCY);
	void     setSensitivity(float value);
    
    // BLOCKING (Original)
    // freezes execution for (~17ms * loopCount)
    float    getRmsVoltage(uint8_t loopCount = 1);

    // NON-BLOCKING (Responsive)
    // Call update() as fast as possible in loop()
    void     update();             
    float    getVoltage();         // Returns latest calculated RMS
    float    getZeroPoint();       // Returns current DC offset (Non-blocking filter)

private:
	uint8_t  pin;
	uint32_t period;
	float 	 sensitivity = DEFAULT_SENSITIVITY;
    
    // Helper for blocking
    int      getZeroPointBlocking();

    // Non-Blocking State
    unsigned long lastSampleTime = 0;
    unsigned long periodStartTime = 0;
    float    zeroPoint = 512.0f;   // Floating point for filter precision
    double   sumSquares = 0;       // Use double to prevent overflow
    uint16_t sampleCount = 0;
    float    rmsVoltage = 0.0f;
};

#endif