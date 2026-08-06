#pragma once

#include <Arduino.h>

// DRV8833 dual-channel motor driver with LEDC PWM on all four inputs.
//
// Wiring (user-confirmed):
//   Motor A (left):  IN1 = GPIO3, IN2 = GPIO1
//   Motor B (right): IN3 = GPIO13, IN4 = GPIO12
//
// Note: GPIO1/GPIO3 are also U0TXD/U0RXD. Serial.begin() must therefore never
// be called on UART0. All four pins are re-purposed as LEDC outputs in
// begin() after boot.

class MotorControl {
public:
    void begin();

    // Drive using arcade-style differential mixing.
    //   throttle: -255 (full reverse) .. 0 (stop) .. +255 (full forward)
    //   steering: -255 (hard left) .. 0 (straight) .. +255 (hard right)
    void drive(int16_t throttle, int16_t steering);

    // Set each motor directly. Positive = forward, negative = reverse.
    void setLeft(int16_t speed);
    void setRight(int16_t speed);

    // Brake (both inputs driven) then coast after a short delay.
    void stop();

    void update();   // call frequently; handles coast timer

    int16_t throttle() const { return _throttle; }
    int16_t steering() const { return _steering; }

private:
    void setPin(uint8_t inA, uint8_t inB, int16_t speed);

    int16_t _throttle = 0;
    int16_t _steering = 0;
    bool    _stopping = false;
    uint32_t _stopAt = 0;
};

extern MotorControl motors;
