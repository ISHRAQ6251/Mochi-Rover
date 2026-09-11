#include "motor_control.h"
#include "config.h"
#include "settings.h"

MotorControl motors;

// Order matches the `index` argument of testPin(): L_IN1, L_IN2, R_IN3, R_IN4.
static const uint8_t kMotorPins[4] = {
    PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, PIN_MOTOR_R_IN3, PIN_MOTOR_R_IN4
};

void MotorControl::begin() {
    // New ESP32-core (v3.x) LEDC API. The Arduino core derives the timer as
    // timer = (channel / 2) % 4, so channels 0..3 use timers 0..1 only. The
    // camera XCLK uses native-IDF channel 5 = timer 2. Do NOT use channel 4:
    // it also maps to timer 2 and sharing it with the camera breaks that
    // motor's reverse output (GPIO42) after esp_camera_init().
    bool okA = ledcAttachChannel(PIN_MOTOR_L_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 0);
    bool okB = ledcAttachChannel(PIN_MOTOR_L_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 1);
    bool okC = ledcAttachChannel(PIN_MOTOR_R_IN3, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 2);
    bool okD = ledcAttachChannel(PIN_MOTOR_R_IN4, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 3);
    Serial.printf("motors: LEDC attach L1=%d L2=%d R1=%d R2=%d\n", okA, okB, okC, okD);

    ledcWrite(PIN_MOTOR_L_IN1, 0);
    ledcWrite(PIN_MOTOR_L_IN2, 0);
    ledcWrite(PIN_MOTOR_R_IN3, 0);
    ledcWrite(PIN_MOTOR_R_IN4, 0);
}

void MotorControl::drive(int16_t throttle, int16_t steering) {
    throttle = constrain(throttle, -255, 255);
    steering = constrain(steering, -255, 255);

    _throttle = throttle;
    _steering = steering;
    _lastCmdAt = millis();
    _testIndex = -1;  // a real drive command cancels any bring-up pin test

    int16_t left = constrain(throttle + steering, -255, 255);
    int16_t right = constrain(throttle - steering, -255, 255);

    setLeft(left);
    setRight(right);
}

static int16_t scaleTrim(int16_t speed, uint8_t trim) {
    if (speed == 0) return 0;
    int32_t scaled = (int32_t)speed * constrain((int)trim, MOTOR_TRIM_MIN, MOTOR_TRIM_MAX) / 100;
    if (scaled == 0) scaled = (speed > 0) ? 1 : -1;
    return (int16_t)constrain(scaled, -255, 255);
}

void MotorControl::setLeft(int16_t speed) {
    speed = scaleTrim(speed, settings.data.trimLeft);
    if (settings.data.reverseLeft) speed = -speed;
    setPin(PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, speed);
}

void MotorControl::setRight(int16_t speed) {
    speed = scaleTrim(speed, settings.data.trimRight);
    if (settings.data.reverseRight) speed = -speed;
    setPin(PIN_MOTOR_R_IN3, PIN_MOTOR_R_IN4, speed);
}

void MotorControl::reapply() {
    if (_throttle != 0 || _steering != 0) drive(_throttle, _steering);
}

uint32_t MotorControl::testPin(uint8_t index, int16_t value) {
    if (index > 3) return 0;
    stop();  // clear any normal drive so the test is unambiguous

    value = constrain(value, -255, 255);
    for (uint8_t i = 0; i < 4; i++) ledcWrite(kMotorPins[i], 0);

    if (value != 0) {
        ledcWrite(kMotorPins[index], (value < 0) ? -value : value);
        _testIndex = (int8_t)index;
        _testAt = millis();
    } else {
        _testIndex = -1;
    }
    // ledcRead() returns 0 when the pin was never attached to an LEDC channel,
    // which distinguishes a firmware/attach failure from a wiring one.
    return ledcRead(kMotorPins[index]);
}

void MotorControl::setPin(uint8_t inA, uint8_t inB, int16_t speed) {
    speed = constrain(speed, -255, 255);
    if (speed > 0) {
        ledcWrite(inA, speed);
        ledcWrite(inB, 0);
    } else if (speed < 0) {
        ledcWrite(inA, 0);
        ledcWrite(inB, -speed);
    } else {
        ledcWrite(inA, 0);
        ledcWrite(inB, 0);
    }
}

void MotorControl::stop() {
    setLeft(0);
    setRight(0);
    _throttle = 0;
    _steering = 0;
    _lastCmdAt = millis();
    _testIndex = -1;
}

void MotorControl::update() {
    // Per-pin bring-up test auto-coasts so a forgotten test cannot run away.
    if (_testIndex >= 0 && (millis() - _testAt > MOTOR_TEST_MS)) {
        for (uint8_t i = 0; i < 4; i++) ledcWrite(kMotorPins[i], 0);
        _testIndex = -1;
    }

    // Drive watchdog: a held button keeps a 300 ms heartbeat coming from the
    // UI, so any longer silence means the client is gone -> coast the motors.
    if ((_throttle != 0 || _steering != 0) &&
        _lastCmdAt && (millis() - _lastCmdAt > DRIVE_WATCHDOG_MS)) {
        stop();
    }
}
