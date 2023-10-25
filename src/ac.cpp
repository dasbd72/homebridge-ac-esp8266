#include "ac.h"

#include <ArduinoJson.h>
#include <DHT.h>
#include <EEPROM.h>
#include <WebSocketsServer.h>

Ac::Ac() {
    // Default Settings
    currentTemperature = 0;
    currentHumidity = 0;
    targetMode = "off";
    targetFanSpeed = "auto";
    targetTemperature = 23;
    verticalSwing = true;
    horizontalSwing = true;
    quietMode = false;
    powerfulMode = false;
}

void Ac::begin() {
    EEPROM.begin(512);

    webSocket.begin();
    webSocket.onEvent(std::bind(&Ac::webSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

    if (mode == DAIKIN) {
        Serial.println("RUNNING IN DAIKIN MODE");
        daikin.begin();
    } else if (mode == PANASONIC) {
        Serial.println("RUNNING IN PANASONIC MODE");
        panasonic.begin();
        panasonic.setModel(kPanasonicRkr);
    } else if (mode == HITACHI) {
        Serial.println("RUNNING IN HITACHI MODE");
        hitachi.begin();
    }

    // restore settings
    restore();

    // start DHT
    dht.begin();

    // load initial weather
    this->getWeather();
}

void Ac::loop() {
    webSocket.loop();

    unsigned long currentMillis = millis();

    if (currentMillis - loopLastRun >= 30000) {
        loopLastRun = currentMillis;
        this->getWeather();
        this->broadcast();
    }
}

void Ac::webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\r\n", num);
            break;
        case WStype_CONNECTED: {
            Serial.printf("[%u] Connected from url: %s\r\n", num, payload);
            // broadcast current settings
            this->broadcast();
            break;
        }
        case WStype_TEXT: {
            // send the payload to the ac handler
            this->incomingRequest((char*)&payload[0]);
            break;
        }
        case WStype_PING:
            // Serial.printf("[%u] Got Ping!\r\n", num);
            break;
        case WStype_PONG:
            // Serial.printf("[%u] Got Pong!\r\n", num);
            break;
        default:
            Serial.printf("Invalid WStype [%d]\r\n", type);
            break;
    }
}

void Ac::getWeather() {
    float humidity = dht.readHumidity();
    float temp = dht.readTemperature(false);

    if (isnan(humidity) || isnan(temp)) {
        Serial.println("Failed to read from DHT sensor!");
        return;
    } else {
        currentTemperature = temp;
        currentHumidity = humidity;
    }
}

String Ac::toJson() {
    DynamicJsonDocument doc(1024);

    doc["currentTemperature"] = currentTemperature;
    doc["currentHumidity"] = currentHumidity;
    doc["targetMode"] = targetMode;
    doc["targetFanSpeed"] = targetFanSpeed;
    doc["targetTemperature"] = targetTemperature;
    doc["verticalSwing"] = verticalSwing;
    doc["horizontalSwing"] = horizontalSwing;
    doc["quietMode"] = quietMode;
    doc["powerfulMode"] = powerfulMode;

    String res;
    serializeJson(doc, res);

    return res;
}

void Ac::broadcast() {
    String res = toJson();
    webSocket.broadcastTXT(res);
}

void Ac::incomingRequest(String payload) {
    Serial.println(payload);
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);

    /* Get and Set Target State */
    if (doc.containsKey("targetMode")) {
        setTargetMode(doc["targetMode"]);
    }

    /* Get and Set Fan Speed */
    if (doc.containsKey("targetFanSpeed")) {
        setTargetFanSpeed(doc["targetFanSpeed"]);
    }

    /* Get and Set Target Temperature */
    if (doc.containsKey("targetTemperature")) {
        setTemperature(doc["targetTemperature"]);
    }

    /* Other Settings */
    if (doc.containsKey("verticalSwing")) {
        setVerticalSwing(doc["verticalSwing"]);
    }

    if (doc.containsKey("horizontalSwing")) {
        setHorizontalSwing(doc["horizontalSwing"]);
    }

    if (doc.containsKey("quietMode")) {
        setQuietMode(doc["quietMode"]);
    }

    if (doc.containsKey("powerfulMode")) {
        setPowerfulMode(doc["powerfulMode"]);
    }

    send();
}

void Ac::send() {
    // flash LED ON
    digitalWrite(LED_BUILTIN, LOW);

    // send the IR signal
    if (mode == DAIKIN) {
#if SEND_DAIKIN
        daikin.send();
#endif  // SEND_DAIKIN
    } else if (mode == PANASONIC) {
#if SEND_PANASONIC_AC
        Serial.println(panasonic.toString());
        panasonic.send();
#endif  // SEND_PANASONIC_AC
    } else if (mode == HITACHI) {
#if SEND_HITACHI_AC
        hitachi.send();
#endif  // SEND_HITACHI_AC
    }

    // flash LED OFF
    digitalWrite(LED_BUILTIN, HIGH);

    // broadcast update
    broadcast();

    // save
    save();
}

void Ac::setTargetMode(String value) {
    value.toLowerCase();

    if (value == "off") {
        if (mode == DAIKIN) {
            daikin.off();
        } else if (mode == PANASONIC) {
            panasonic.off();
        } else if (mode == HITACHI) {
            hitachi.off();
        }
    } else if (value == "cool") {
        if (mode == DAIKIN) {
            daikin.on();
            daikin.setMode(DAIKIN_COOL);
        } else if (mode == PANASONIC) {
            panasonic.on();
            panasonic.setMode(kPanasonicAcCool);
        } else if (mode == HITACHI) {
            hitachi.on();
            hitachi.setMode(kHitachiAcCool);
        }
    } else if (value == "heat") {
        if (mode == DAIKIN) {
            daikin.on();
            daikin.setMode(DAIKIN_HEAT);
        } else if (mode == PANASONIC) {
            panasonic.on();
            panasonic.setMode(kPanasonicAcHeat);
        } else if (mode == HITACHI) {
            hitachi.on();
            hitachi.setMode(kHitachiAcHeat);
        }
    } else if (value == "fan") {
        if (mode == DAIKIN) {
            daikin.on();
            daikin.setMode(DAIKIN_FAN);
        } else if (mode == PANASONIC) {
            panasonic.on();
            panasonic.setMode(kPanasonicAcFan);
        } else if (mode == HITACHI) {
            hitachi.on();
            hitachi.setMode(kHitachiAcFan);
        }
    } else if (value == "auto") {
        if (mode == DAIKIN) {
            daikin.on();
            daikin.setMode(DAIKIN_AUTO);
        } else if (mode == PANASONIC) {
            panasonic.on();
            panasonic.setMode(kPanasonicAcAuto);
        } else if (mode == HITACHI) {
            hitachi.on();
            hitachi.setMode(kHitachiAcAuto);
        }
    } else if (value == "dry") {
        if (mode == DAIKIN) {
            daikin.on();
            daikin.setMode(DAIKIN_DRY);
        } else if (mode == PANASONIC) {
            panasonic.on();
            panasonic.setMode(kPanasonicAcDry);
        } else if (mode == HITACHI) {
            hitachi.on();
            hitachi.setMode(kHitachiAcDry);
        }
    } else {
        Serial.println("WARNING: No Valid Mode Passed. Turning Off.");
        if (mode == DAIKIN) {
            daikin.off();
            value = "off";
        } else if (mode == PANASONIC) {
            panasonic.off();
            value = "off";
        } else if (mode == HITACHI) {
            hitachi.off();
            value = "off";
        }
    }

    if (value != targetMode) {
        Serial.print("Target Mode Changed: ");
        Serial.println(value);
        targetMode = value;
    }
}

void Ac::setTargetFanSpeed(String value) {
    value.toLowerCase();

    if (value == "auto") {
        if (mode == DAIKIN) {
            daikin.setFan(DAIKIN_FAN_AUTO);
        } else if (mode == PANASONIC) {
            panasonic.setFan(kPanasonicAcFanAuto);
        } else if (mode == HITACHI) {
            hitachi.setFan(kHitachiAcFanAuto);
        }
    } else if (value == "min") {
        if (mode == DAIKIN) {
            daikin.setFan(DAIKIN_FAN_MIN);
        } else if (mode == PANASONIC) {
            panasonic.setFan(kPanasonicAcFanMin);
        } else if (mode == HITACHI) {
            hitachi.setFan(kHitachiAcFanLow);
        }
    } else if (value == "max") {
        if (mode == DAIKIN) {
            daikin.setFan(DAIKIN_FAN_MAX);
        } else if (mode == PANASONIC) {
            panasonic.setFan(kPanasonicAcFanMax);
        } else if (mode == HITACHI) {
            hitachi.setFan(kHitachiAcFanHigh);
        }
    } else {
        if (mode == DAIKIN) {
            daikin.setFan(DAIKIN_FAN_AUTO);
        } else if (mode == PANASONIC) {
            panasonic.setFan(kPanasonicAcFanAuto);
        } else if (mode == HITACHI) {
            hitachi.setFan(kHitachiAcFanAuto);
        }
        value = "auto";
        Serial.println("WARNING: No Valid Fan Speed Passed. Setting to Auto.");
    }

    if (value != targetFanSpeed) {
        Serial.print("Target Fan Speed: ");
        Serial.println(value);
        targetFanSpeed = value;
        // set(S_FAN, targetFanSpeed);
    }
}

void Ac::setTemperature(int value) {
    if (mode == DAIKIN) {
        daikin.setTemp(value);
    } else if (mode == PANASONIC) {
        panasonic.setTemp(value);
    } else if (mode == HITACHI) {
        hitachi.setTemp(value);
    }
    Serial.print("Target Temperature: ");
    Serial.println(value);
    targetTemperature = value;
}

void Ac::setVerticalSwing(bool value) {
    if (mode == DAIKIN) {
        daikin.setSwingVertical(value);
    } else if (mode == PANASONIC) {
        panasonic.setSwingVertical(value ? kPanasonicAcSwingVAuto : kPanasonicAcSwingVHighest);
    } else if (mode == HITACHI) {
        hitachi.setSwingVertical(value);
    }
    if (value != verticalSwing) {
        Serial.print("Verticle Swing: ");
        Serial.println(value);
        verticalSwing = value;
        set(S_VS, (verticalSwing) ? 1 : 0);
    }
}

void Ac::setHorizontalSwing(bool value) {
    if (mode == DAIKIN) {
        daikin.setSwingHorizontal(value);
    } else if (mode == PANASONIC) {
        panasonic.setSwingHorizontal(value ? kPanasonicAcSwingHAuto : kPanasonicAcSwingHMiddle);
    } else if (mode == HITACHI) {
        hitachi.setSwingHorizontal(value);
    }
    if (value != horizontalSwing) {
        Serial.print("Horizontal Swing: ");
        Serial.println(value);
        horizontalSwing = value;
        set(S_HS, (horizontalSwing) ? 1 : 0);
    }
}

void Ac::setQuietMode(bool value) {
    if (mode == DAIKIN) {
        daikin.setQuiet(value);
    } else if (mode == PANASONIC) {
        panasonic.setQuiet(value);
    } else if (mode == HITACHI) {
    }
    if (value != quietMode) {
        Serial.print("Quiet Mode: ");
        Serial.println(value);
        quietMode = value;
        set(S_QM, (quietMode) ? 1 : 0);
    }

    if (value) {
        // cannot be powerful and quiet
        setPowerfulMode(false);
    }
}

void Ac::setPowerfulMode(bool value) {
    if (mode == DAIKIN) {
        daikin.setPowerful(value);
    } else if (mode == PANASONIC) {
        panasonic.setPowerful(value);
    } else if (mode == HITACHI) {
    }
    if (value != powerfulMode) {
        Serial.print("Powerful Mode: ");
        Serial.println(value);
        powerfulMode = value;
        set(S_PM, (powerfulMode) ? 1 : 0);
    }

    if (value) {
        // cannot be quiet and powerful
        setQuietMode(false);
    }
}

// Set a setting value to EEPROM
void Ac::set(int location, int value) {
    Serial.printf("Setting %d to %d\n", location, value);
    EEPROM.write(location, value);
    dirty = true;
}

// Loads a setting value from EEPROM
int Ac::load(int location) {
    int value = EEPROM.read(location);
    Serial.printf("Setting %d is equal to %d\n", location, value);
    return value;
}

// Saves the settings to EEPROM
void Ac::save() {
    if (dirty) {
        Serial.println("Saving to EEPROM");
        EEPROM.commit();
        dirty = false;
    }
}

// Restores Settings from EEPROM
void Ac::restore() {
    verticalSwing = (load(S_VS) == 1) ? true : false;
    horizontalSwing = (load(S_HS) == 1) ? true : false;
    quietMode = (load(S_QM) == 1) ? true : false;
    powerfulMode = (load(S_PM) == 1) ? true : false;

    setTargetMode(targetMode);
    setTargetFanSpeed(targetFanSpeed);
    setVerticalSwing(verticalSwing);
    setHorizontalSwing(horizontalSwing);
    setQuietMode(quietMode);
    setPowerfulMode(powerfulMode);
}