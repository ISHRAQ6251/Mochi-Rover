#pragma once

#include <Arduino.h>

// DRV8833 dual-channel motor driver with LEDC PWM on all four inputs.
//
// Wiring:
//   Motor A (left):  IN1 = GPIO47, IN2 = GPIO14
//   Motor B (right): IN3 = GPIO21, IN4 = GPIO42
//
// All four pins are re-purposed as LEDC outputs in begin().

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

    // Re-apply the last throttle/steering after reverse or trim changes.
    void reapply();

    // Bring-up self-test: drive exactly one motor input directly, bypassing
    // trim, reverse and the drive watchdog, so a single GPIO can be isolated.
    //   index: 0=L_IN1, 1=L_IN2, 2=R_IN3, 3=R_IN4
    //   value: -255..0..255 (sign chooses which side of the half-bridge, but
    //          with the opposite input held low either sign spins the motor)
    // Auto-coasts after MOTOR_TEST_MS. Returns the LEDC duty read back from the
    // pin, so a caller can tell "pin never attached" (0) from "driven fine".
    uint32_t testPin(uint8_t index, int16_t value);

    // Coast both motors.
    void stop();

    // Call frequently. Implements the drive watchdog: if no drive/stop command
    // has been received within DRIVE_WATCHDOG_MS the motors coast, so the rover
    // never keeps running after the app disconnects or is closed.
    void update();

    int16_t throttle() const { return _throttle; }
    int16_t steering() const { return _steering; }

private:
    void setPin(uint8_t inA, uint8_t inB, int16_t speed);

    int16_t _throttle = 0;
    int16_t _steering = 0;
    uint32_t _lastCmdAt = 0;
    int8_t _testIndex = -1;      // active per-pin test, -1 = none
    uint32_t _testAt = 0;        // when the per-pin test was started
};

extern MotorControl motors;
