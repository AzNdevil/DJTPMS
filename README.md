// TPMS Configuration
#define TPMS_NUM_SENSORS 4

// Known TPMS sensor MACs
const char* sensorMACs[TPMS_NUM_SENSORS] = {
    "C817F5B1285F",   // Front Left
    "C817F5B1270F",   // Front Right
    "C817F5B1271B",   // Rear Left (auto-detect)
    "0C3D5E4EA08B"    // Rear Right (auto-detect)
};

DJTPMS tpmsSensors[TPMS_NUM_SENSORS];

typedef struct tpms_data {
    uint16_t pressurePSI_x10;    // 2 bytes - round(PSI * 10) (0-100+ PSI range)
    int8_t temperatureC;       // 1 byte - signed Celsius (-128 to 127)
    uint8_t voltage_x10;       // 1 byte - Voltage * 10 (0-25.5V range)
    uint8_t status;            // 1 byte - bit flags
} tpms_data;

// =================================================================
// TPMS Functions
// =================================================================
void tpmsSetup() {
    // Initialize sensors with known MACs
    for (int i = 0; i < TPMS_NUM_SENSORS; i++) {
        if (strcmp(sensorMACs[i], "") != 0) {
            tpmsSensors[i].setMAC(sensorMACs[i]);
            tpmsSensors[i].macSet = 1;
            ESP_LOGI("tpmsSetup", "Sensor %d: MAC %s", i, sensorMACs[i]);
        }
    }
}

void processTPMSData(const std::string& hexData) {
    DJTPMS tempSensor;
    if (!tempSensor.update(hexData)) {
        ESP_LOGW("processTPMSData", "processTPMSData failed: %s", hexData.c_str());
        return;
    }

    // Check if MAC matches any existing sensor
    for (int i = 0; i < TPMS_NUM_SENSORS; i++) {
        if (tpmsSensors[i].macSet && strcmp(tpmsSensors[i].macStr, tempSensor.macStr) == 0) {
            // Matches existing sensor
            tpmsSensors[i] = tempSensor;
            tpmsSensors[i].valid = 1;
            tpmsSensors[i].macSet = 1;
            ESP_LOGI("processTPMSData", "Updated sensor %d", i);
            tpmsSensors[i].printDebug();
            return;
        }
    }

    // Doesn't match existing - find first empty slot
    for (int i = 0; i < TPMS_NUM_SENSORS; i++) {
        if (!tpmsSensors[i].macSet) {
            tpmsSensors[i] = tempSensor;
            tpmsSensors[i].valid = 1;
            tpmsSensors[i].macSet = 1;
            ESP_LOGI("processTPMSData", "New sensor %d", i);
            tpmsSensors[i].printDebug();
            return;
        }
    }

    ESP_LOGW("processTPMSData", "No available slot for new sensor");
}

void updateTPMSData(tpms_data* tpmsArray, int arraySize) {
    for (int i = 0; i < arraySize && i < TPMS_NUM_SENSORS; i++) {
        tpmsArray[i].pressurePSI_x10 = round(tpmsSensors[i].getPressurePSI() * 10);
        tpmsArray[i].temperatureC = (int8_t)tpmsSensors[i].getTemperature();
        tpmsArray[i].voltage_x10 = tpmsSensors[i].voltageRaw;
        tpmsArray[i].status = tpmsSensors[i].valid ? 0x01 : 0x00;
    }
}

void checkStaleTPMSSensors(unsigned long staleTimeout) {
    for (int i = 0; i < TPMS_NUM_SENSORS; i++) {
        if (tpmsSensors[i].valid && tpmsSensors[i].isStale(staleTimeout)) {
            ESP_LOGI("checkStaleTPMSSensors", "Sensor %d timed out, invalidating...", i);
            tpmsSensors[i].valid = 0;
        }
    }
}

// ------------------------------------------------------------------
// Nimble-Arduino for TPMS BLE
// ------------------------------------------------------------------
NimBLEScan* pBLEScan;
static constexpr uint32_t scanTimeMs = 0;

void handleTPMSAdvertisement(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (!advertisedDevice->haveName()) return;
    
    const std::string& advName = advertisedDevice->getName();
    if (advName != "DJTPMS") return;
    
    std::string mfgDataRaw = advertisedDevice->getManufacturerData();
    std::string mfgData = NimBLEUtils::dataToHexString(
        reinterpret_cast<const uint8_t*>(mfgDataRaw.data()), 
        mfgDataRaw.length()
    );
    ESP_LOGI("bleCallback", "DJTPMS data: %s", mfgData.c_str());
    processTPMSData(mfgData);
}

class scanCallbacks : public NimBLEScanCallbacks {
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override {
        handleTPMSAdvertisement(advertisedDevice);
    }

    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        handleTPMSAdvertisement(advertisedDevice);
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        ESP_LOGW("bleCallback onScanEnd", "Scan ended reason = %d; restarting scan\n", reason);
        NimBLEDevice::getScan()->start(scanTimeMs, false, true);
    }
} scanCallbacks;

void setup() {
  tpmsSetup();
  
  NimBLEDevice::init("");
  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setScanCallbacks(&scanCallbacks, false);
  pBLEScan->setActiveScan(true);
  pBLEScan->setMaxResults(0);
  pBLEScan->start(scanTimeMs, false, true);
  ESP_LOGI("setup", "TPMS Scanning...");
}

void loop() {
  delay(1);
}
