#include "ZMPT101B-driver.h"

/// @brief ZMPT101B constructor
/// @param pin analog pin that ZMPT101B connected to.
/// @param frequency AC system frequency
ZMPT101B::ZMPT101B(uint8_t pin, uint16_t frequency)
{
	this->pin = pin;
	period = 1000000 / frequency;
	pinMode(pin, INPUT);
    periodStartTime = micros();
}

/// @brief Set sensitivity
/// @param value Sensitivity value
void ZMPT101B::setSensitivity(float value)
{
	sensitivity = value;
}

/// @brief Non-blocking update function
/// Call this as fast as possible in loop()
void ZMPT101B::update() 
{
    unsigned long now = micros();
    
    // Sample every 1ms (1000us) -> 1kHz sampling rate
    // This is sufficient for 50/60Hz and leaves time for other tasks
    if (now - lastSampleTime >= 1000) {
        lastSampleTime = now;
        
        int raw = analogRead(pin);
        
        // Continuous DC Removal (Low Pass Filter)
        // Keeps the Zero Point accurate even if it drifts.
        // alpha = 0.005 -> Very slow, stable adaptation
        zeroPoint = (0.995f * zeroPoint) + (0.005f * raw);
        
        float centered = raw - zeroPoint;
        
        // Accumulate squares
        sumSquares += (centered * centered);
        sampleCount++;
    }
    
    // End of AC Period? (e.g., 16.6ms for 60Hz)
    if (now - periodStartTime >= period) {
        if (sampleCount > 0) {
            // Calculate RMS
            float rms = sqrt(sumSquares / sampleCount);
            
            // Map to Voltage using the User's formula
            // RMS * (Volts per step) * Sensitivity
            rmsVoltage = (rms / ADC_SCALE) * VREF * sensitivity;
        }
        
        // Reset for next period
        sumSquares = 0;
        sampleCount = 0;
        periodStartTime = now;
    }
}


/// @brief Get the latest calculated RMS voltage (NON-BLOCKING)
/// @return RMS Voltage
float ZMPT101B::getVoltage()
{
	return rmsVoltage;
}

/// @brief Get the current Zero Point (DC Offset)
/// @return Zero Point
float ZMPT101B::getZeroPoint()
{
    return zeroPoint;
}

// ================================================================
// BLOCKING METHODS (Original/Legacy)
// ================================================================

/// @brief Calculate zero point (BLOCKING)
/// @return zero / center value
int ZMPT101B::getZeroPointBlocking()
{
	uint32_t Vsum = 0;
	uint32_t measurements_count = 0;
	uint32_t t_start = micros();

	while (micros() - t_start < period)
	{
		Vsum += analogRead(pin);
		measurements_count++;
	}

	return Vsum / measurements_count;
}

/// @brief Calculate RMS voltage (BLOCKING)
/// This function blocks execution!
/// @param loopCount Loop count to calculate
/// @return RMS Voltage
float ZMPT101B::getRmsVoltage(uint8_t loopCount)
{
	double readingVoltage = 0.0f;

	for (uint8_t i = 0; i < loopCount; i++)
	{
		int zero = this->getZeroPointBlocking();

		int32_t Vnow = 0;
		uint32_t Vsum = 0;
		uint32_t measurements_count = 0;
		uint32_t t_start = micros();

		while (micros() - t_start < period)
		{
			Vnow = analogRead(pin) - zero;
			Vsum += (Vnow * Vnow);
			measurements_count++;
		}

		readingVoltage += sqrt(Vsum / measurements_count) / ADC_SCALE * VREF * sensitivity;
	}

	return readingVoltage / loopCount;
}