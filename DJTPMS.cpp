#include "DJTPMS.h"

DJTPMS::DJTPMS() {
    reset();
}

void DJTPMS::reset() {
    memset(mac, 0, 6);
    memset(macStr, 0, 18);
    memset(macStrRaw, 0, 13);
    temperatureRaw = 0;
    voltageRaw = 0;
    pressureRaw = 0;
    flags = 0;
    lastUpdateTime = 0;
    valid = false;
    macSet = false;
}

void DJTPMS::clearMAC() {
    memset(mac, 0, 6);
    memset(macStr, 0, 18);
    memset(macStrRaw, 0, 13);
    macSet = false;
}

uint8_t DJTPMS::hexToNibble(char c) const {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

void DJTPMS::updateMacStrings() {
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    sprintf(macStrRaw, "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool DJTPMS::setMAC(const std::string& macString) {
    return setMAC(macString.c_str());
}

bool DJTPMS::setMAC(const char* macString) {
    if (!macString || macString[0] == '\0') {
        clearMAC();
        return false;
    }

    size_t hexCount = 0;
    for (const char* p = macString; *p; p++) {
        char c = *p;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
            hexCount++;
        }
    }

    if (hexCount != 12) {
        return false;
    }

    size_t byteIdx = 0;
    uint8_t nibbleCount = 0;
    uint8_t currentByte = 0;

    for (const char* p = macString; *p && byteIdx < 6; p++) {
        char c = *p;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
            currentByte = (currentByte << 4) | hexToNibble(c);
            nibbleCount++;
            if (nibbleCount == 2) {
                mac[byteIdx++] = currentByte;
                currentByte = 0;
                nibbleCount = 0;
            }
        }
    }

    updateMacStrings();
    macSet = true;
    return true;
}

void DJTPMS::setMAC(const uint8_t* macBytes) {
    memcpy(mac, macBytes, 6);
    updateMacStrings();
    macSet = true;
}

bool DJTPMS::update(const std::string& hexString) {
    return update(hexString.c_str());
}

bool DJTPMS::update(const char* hexString) {
    size_t hexLen = 0;
    for (const char* p = hexString; *p; p++) {
        char c = *p;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
            hexLen++;
        }
    }

    if (hexLen < TPMS_MIN_PACKET_LENGTH * 2) {
        return false;
    }

    size_t byteLen = hexLen / 2;
    uint8_t* buffer = (uint8_t*)malloc(byteLen);
    if (!buffer) return false;

    size_t byteIdx = 0;
    uint8_t nibbleCount = 0;
    uint8_t currentByte = 0;

    for (const char* p = hexString; *p && byteIdx < byteLen; p++) {
        char c = *p;
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
            currentByte = (currentByte << 4) | hexToNibble(c);
            nibbleCount++;
            if (nibbleCount == 2) {
                buffer[byteIdx++] = currentByte;
                currentByte = 0;
                nibbleCount = 0;
            }
        }
    }

    bool result = update(buffer, byteLen);
    free(buffer);
    return result;
}

bool DJTPMS::update(const uint8_t* data, size_t length) {
    if (length < TPMS_MIN_PACKET_LENGTH) {
        return false;
    }

    // If MAC is pre-configured, verify packet MAC matches
    if (macSet) {
        if (memcmp(&data[length - 6], mac, 6) != 0) {
            return false;
        }
    }
    // If already valid (MAC learned from previous packet), verify match
    else if (valid) {
        if (memcmp(&data[length - 6], mac, 6) != 0) {
            return false;
        }
    }

    // Calculate data offset (skip prefix bytes)
    size_t offset = length - TPMS_MIN_PACKET_LENGTH;

    // Packet structure (from offset):
    // [0]: Voltage (raw / 10 = volts)
    // [1]: Temperature (direct Celsius)
    // [2]: Pressure high byte (16-bit big-endian)
    // [3]: Pressure low byte
    // [4]: Unknown
    // [5]: Unknown
    // [6-11]: MAC Address

    voltageRaw = data[offset + 0];
    temperatureRaw = data[offset + 1];
    pressureRaw = ((uint16_t)data[offset + 2] << 8) | data[offset + 3];

    // Extract MAC (last 6 bytes) - only update if not pre-configured
    if (!macSet) {
        memcpy(mac, &data[length - 6], 6);
        updateMacStrings();
    }

    lastUpdateTime = millis();
    valid = true;

    return true;
}

float DJTPMS::getTemperature() const {
    return (float)temperatureRaw;
}

float DJTPMS::getTemperatureF() const {
    return (temperatureRaw * 9.0f / 5.0f) + 32.0f;
}

float DJTPMS::getPressureKPA() const {
    if (pressureRaw <= PRESSURE_OFFSET) return 0.0f;
    return (float)(pressureRaw - PRESSURE_OFFSET);
}

float DJTPMS::getPressureBar() const {
    return getPressureKPA() * KPA_TO_BAR;
}

float DJTPMS::getPressurePSI() const {
    return getPressureKPA() * KPA_TO_PSI;
}

float DJTPMS::getVoltage() const {
    return (float)voltageRaw / VOLTAGE_DIVISOR;
}

unsigned long DJTPMS::getTimeSinceUpdate() const {
    if (lastUpdateTime == 0) return ULONG_MAX;
    return millis() - lastUpdateTime;
}

bool DJTPMS::isStale(unsigned long timeoutMs) const {
    if (!valid || lastUpdateTime == 0) return true;
    return getTimeSinceUpdate() > timeoutMs;
}

void DJTPMS::printDebug() const {
    Serial.println("=== DJTPMS ===");
    Serial.printf("MAC:      %s\n", macStr);
    Serial.printf("MAC Set:  %s\n", macSet ? "Yes" : "No (auto)");
    Serial.printf("Valid:    %s\n", valid ? "Yes" : "No");
    Serial.printf("Temp:     %.1f C (%.1f F)\n", getTemperature(), getTemperatureF());
    Serial.printf("Pressure: %.1f PSI (%.1f kPa, %.2f bar)\n", 
                  getPressurePSI(), getPressureKPA(), getPressureBar());
    Serial.printf("Voltage:  %.2f V\n", getVoltage());
    Serial.printf("Raw:      V=%d T=%d P=%d\n", voltageRaw, temperatureRaw, pressureRaw);
    Serial.printf("Age:      %lu ms\n", getTimeSinceUpdate());
    Serial.println("==============");
}