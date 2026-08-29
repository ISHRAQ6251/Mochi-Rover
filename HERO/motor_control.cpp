#include "motor_control.h"
#include "config.h"

MotorControl motors;

void MotorControl::begin() {
    // New ESP32-core (v3.x) LEDC API. Channels are pinned explicitly (1..4);
    // the camera's XCLK uses the native-IDF LEDC channel 5 / timer 2, so the
    // two never collide.
    ledcAttachChannel(PIN_MOTOR_L_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 1);
    ledcAttachChannel(PIN_MOTOR_L_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 2);
    ledcAttachChannel(PIN_MOTOR_R_IN3, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 3);
    ledcAttachChannel(PIN_MOTOR_R_IN4, MOTOR_PWM_FREQ, MOTOR_PWM_RES, 4);

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

    int16_t left = constrain(throttle + steering, -255, 255);
    int16_t right = constrain(throttle - steering, -255, 255);

    setLeft(left);
    setRight(right);
}

void MotorControl::setLeft(int16_t speed) {
    setPin(PIN_MOTOR_L_IN1, PIN_MOTOR_L_IN2, speed);
}

void MotorControl::setRight(int16_t speed) {
    setPin(PIN_MOTOR_R_IN3, PIN_MOTOR_R_IN4, speed);
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
}

void MotorControl::update() {
    // Drive watchdog: a held button keeps a 300 ms heartbeat coming from the
    // UI, so any longer silence means the client is gone -> coast the motors.
    if ((_throttle != 0 || _steering != 0) &&
        _lastCmdAt && (millis() - _lastCmdAt > DRIVE_WATCHDOG_MS)) {
        stop();
    }
}
