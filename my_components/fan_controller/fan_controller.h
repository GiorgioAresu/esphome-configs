#pragma once

#include "esphome.h"
#include "esphome/components/fan/fan.h"
#include "esphome/components/output/binary_output.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <queue>

namespace esphome {
namespace fan_controller {

class FanController;  // Forward declaration

// Operation types and states
enum class OperationType {
  POWER,
  SPEED
};

struct Operation {
  OperationType type;
  uint8_t target_value;  // For speed operations
  
  Operation(OperationType t, uint8_t val = 0) 
    : type(t), target_value(val) {}
};

enum class OperationState {
  IDLE,
  BUTTON_PRESS,
  BUTTON_RELEASE,
  AWAIT_CHANGE
};

class FanController : public Component, public fan::Fan {
 public:
  FanController() : Fan() {}
  
  fan::FanTraits get_traits() override {
    auto traits = fan::FanTraits();
    traits.set_speed(true);
    traits.set_supported_speed_count(3);
    return traits;
  }

  void setup() override;
  void loop() override;
  void control(const fan::FanCall &call) override;
  void update_from_leds();
  
  void set_power_output(output::BinaryOutput *output) { power_output_ = output; }
  void set_speed_output(output::BinaryOutput *output) { speed_output_ = output; }
  void set_low_led(binary_sensor::BinarySensor *sensor) { low_led_ = sensor; }
  void set_high_led(binary_sensor::BinarySensor *sensor) { high_led_ = sensor; }
  
 protected:
  void queue_operation(OperationType type, uint8_t value = 0);
  void start_next_operation();
  bool handle_operation_result();
  uint8_t current_speed();

  // Hardware interfaces
  output::BinaryOutput *power_output_{nullptr};
  output::BinaryOutput *speed_output_{nullptr};
  binary_sensor::BinarySensor *low_led_{nullptr};
  binary_sensor::BinarySensor *high_led_{nullptr};

  // Timing constants
  static const uint32_t BUTTON_PRESS_DURATION = 100;    // Duration to hold button
  static const uint32_t BUTTON_RELEASE_DURATION = 100;  // Wait after release
  static const uint32_t SPEED_CHANGE_TIMEOUT = 3000;    // Max time to wait for speed change
  static const uint8_t MAX_SPEED_ATTEMPTS = 4;          // Max attempts to change speed

  // Operation queue and state tracking
  std::queue<Operation> operation_queue_{};
  OperationState operation_state_{OperationState::IDLE};
  uint32_t last_action_time_{0};
  uint32_t operation_start_time_{0};
  uint8_t operation_attempts_{0};
  Operation* current_operation_{nullptr};

  // Wait for leds to update before trying to power on to a certain speed
  bool wait_for_leds_{false};
  uint32_t led_wait_start_{0};
  uint8_t previous_led_speed_{0};
  uint8_t target_speed_{0};
  static const uint32_t LED_STABILIZE_TIME = 500;
};

}  // namespace fan_controller
}  // namespace esphome