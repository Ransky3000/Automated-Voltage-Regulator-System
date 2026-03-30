#ifndef ACS712_DRIVER_H
#define ACS712_DRIVER_H

#include <Arduino.h>

class ACS712 {
public:
    /**
     * @brief Constructor
     * @param pin The analog pin connected to the sensor's VOUT
     * @param voltageReference The Arduino voltage reference (usually 5.0 or 3.3)
     * @param adcResolution The ADC resolution (usually 1023 for 10-bit)
     */
    ACS712(int pin, float voltageReference = 5.0, int adcResolution = 1023);

    /**
     * @brief Initialize the sensor (sets pinMode)
     */
    void begin();

    /**
     * @brief Set the sensitivity of the sensor.
     * Common values:
     * - ACS712-05B: 0.185 V/A
     * - ACS712-20A: 0.100 V/A
     * - ACS712-30A: 0.066 V/A
     * @param sensitivity Volts per Ampere (V/A)
     */
    void setSensitivity(float sensitivity);

    /**
     * @brief Calibrate the zero point offset.
     * Ensure no current is flowing through the sensor when calling this.
     * @return The calculated zero point ADC value.
     */
    float calibrate();

    /**
     * @brief Read DC Current
     * @return Current in Amperes
     */
    float readCurrentDC();

    /**
     * @brief Read AC Current (RMS) using Zero-Crossing Detection.
     * 
     * Instead of assuming a fixed frequency (e.g. 60Hz), this method
     * detects the actual waveform zero-crossings to determine the
     * real AC period. This makes it accurate even when the generator
     * RPM fluctuates and the frequency drifts (e.g. 50–70Hz).
     * 
     * @param cycles Number of complete AC cycles to average (default 2).
     *               More cycles = more accurate but longer blocking time.
     *               1 cycle ≈ 17ms at 60Hz, 2 cycles ≈ 33ms.
     * @return RMS Current in Amperes. Returns 0 if no AC signal detected.
     */
    float readCurrentAC(int cycles = 2);

    // --- Non-Blocking API ---

    /**
     * @brief Update routine, call this in your loop() as fast as possible.
     * @return true if a new sample set is adequate and a new current value is calculated.
     */
    bool update();

    /**
     * @brief Get the last calculated DC Current (from update).
     * @return Current in Amperes
     */
    float getAmps();

    // ------------------------

    /**
     * @brief Get the currently set zero point
     * @return Zero point ADC value
     */
    float getZeroPoint();

    /**
     * @brief Manually set the zero point (e.g. from EEPROM)
     * @param zeroPoint The ADC value to use as zero
     */
    void setZeroPoint(float zeroPoint);

    /**
     * @brief Get the current sensitivity
     * @return Sensitivity in V/A
     */
    float getSensitivity();

private:
    int _pin;
    float _voltageReference;
    int _adcResolution;
    float _sensitivity;
    float _zeroPoint;
    
    float adcToVoltage(float adcValue);

    // Non-blocking state
    unsigned long _lastSampleTime;
    long _accumulator;
    int _sampleCount;
    float _lastAmps;
};

#endif
