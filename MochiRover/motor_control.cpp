#include "motor_control.h"
#include "config.h"

MotorControl motors;

void MotorControl::begin() {
    // New ESP32-core (v3.x) LEDC API binds each pin directly.
    ledcAttach(PIN_MOTOR_L_IN1, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    ledcAttach(PIN_MOTOR_L_IN2, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    ledcAttach(PIN_MOTOR_R_IN3, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
    ledcAttach(PIN_MOTOR_R_IN4, MOTOR_PWM_FREQ, MOTOR_PWM_RES);

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

    int16_t left = constrain(throttle + steering, -255, 255);
    int16_t right = constrain(throttle - steering, -255, 255);

    setLeft(left);
    setRight(right);
    _stopping = false;
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
    // Brake briefly to settle the wheels, then let them coast free.
    setLeft(0);
    setRight(0);
    _throttle = 0;
    _steering = 0;
    _stopping = true;
    _stopAt = millis() + MOTOR_STOP_DELAY_MS;
}

void MotorControl::update() {
    if (_stopping && millis() >= _stopAt) {
        _stopping = false;
    }
}
