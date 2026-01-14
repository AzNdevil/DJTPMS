/**
 * @file DJTPMS.h
 * @brief Simplified Arduino library for decoding DJ TPMS Bluetooth sensor data
 */

#ifndef DJTPMS_H
#define DJTPMS_H

#include <Arduino.h>
#include <string>

// Conversion constants
#define KPA_TO_PSI 0.145038f
#define KPA_TO_BAR 0.01f
#define PRESSURE_OFFSET 101
#define VOLTAGE_DIVISOR 10.0f

// Packet constants
#define TPMS_MIN_PACKET_LENGTH 12

class DJTPMS {
public:
    // Raw sensor data (public for direct access)
    uint8_t mac[6];
    uint8_t temperatureRaw;
    uint8_t voltageRaw;
    uint16_t pressureRaw;      // 16-bit pressure
    uint8_t flags;
    unsigned long lastUpdateTime;
    bool valid;
    bool macSet;

    // MAC address strings (auto-updated on decode or setMAC)
    char macStr[18];
    char macStrRaw[13];

    DJTPMS();

    bool setMAC(const char* macString);
    bool setMAC(const std::string& macString);
    void setMAC(const uint8_t* macBytes);

    bool update(const char* hexString);
    bool update(const std::string& hexString);
    bool update(const uint8_t* data, size_t length);

    float getTemperature() const;
    float getTemperatureF() const;
    float getPressureKPA() const;
    float getPressureBar() const;
    float getPressurePSI() const;
    float getVoltage() const;

    unsigned long getTimeSinceUpdate() const;
    bool isStale(unsigned long timeoutMs = 60000) const;

    void reset();
    void clearMAC();
    void printDebug() const;

private:
    uint8_t hexToNibble(char c) const;
    void updateMacStrings();
};

#endif