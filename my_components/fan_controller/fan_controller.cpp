#include "fan_controller.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace fan_controller {

static const char *const TAG = "fan_controller";

void FanController::setup() {
  if (low_led_ != nullptr && high_led_ != nullptr) {
    update_from_leds();
  } else {
    this->state = false;
    this->speed = 0;
  }
}

void FanController::loop() {
  switch (operation_state_) {
    case OperationState::IDLE:
      if (!operation_queue_.empty() && current_operation_ == nullptr) {
        start_next_operation();
      }
      break;
      
    case OperationState::BUTTON_PRESS:
      if (millis() - last_action_time_ >= BUTTON_PRESS_DURATION) {
        if (current_operation_->type == OperationType::POWER) {
          power_output_->turn_off();
        } else {
          speed_output_->turn_off();
        }
        operation_state_ = OperationState::BUTTON_RELEASE;
        last_action_time_ = millis();
      }
      break;

    case OperationState::BUTTON_RELEASE:
      if (millis() - last_action_time_ >= BUTTON_RELEASE_DURATION) {
        if (current_operation_->type == OperationType::POWER) {
          operation_state_ = OperationState::IDLE;
          if (handle_operation_result()) {
            delete current_operation_;
            current_operation_ = nullptr;
          }
        } else {
          operation_state_ = OperationState::AWAIT_CHANGE;
          operation_start_time_ = millis();
        }
      }
      break;

    case OperationState::AWAIT_CHANGE:
      if (handle_operation_result()) {
        operation_state_ = OperationState::IDLE;
        delete current_operation_;
        current_operation_ = nullptr;
      } else if (millis() - operation_start_time_ >= SPEED_CHANGE_TIMEOUT) {
        ESP_LOGW(TAG, "Operation timeout");
        operation_state_ = OperationState::IDLE;
        delete current_operation_;
        current_operation_ = nullptr;
      }
      break;

    default:
      break;
  }
}

void FanController::control(const fan::FanCall &call) {
  bool state_change = false;
  bool speed_change = false;
  
  if (call.get_state().has_value()) {
    bool new_state = *call.get_state();
    if (new_state != this->state) {
      queue_operation(OperationType::POWER);
      this->state = new_state;
      state_change = true;
    }
  }

  if (call.get_speed().has_value()) {
    uint8_t new_speed = *call.get_speed();
    if (new_speed != this->speed) {
      if (this->state || state_change) {
        queue_operation(OperationType::SPEED, new_speed);
      }
      this->speed = new_speed;
      speed_change = true;
    }
  }
  
  this->publish_state();
}

void FanController::update_from_leds() {
  if (low_led_ == nullptr || high_led_ == nullptr) {
    ESP_LOGW(TAG, "LED sensors not configured");
    return;
  }

  bool low = low_led_->state;
  bool high = high_led_->state;
  
  if (high && low) {
    this->speed = 3;
  } else if (high) {
    this->speed = 2;
  } else if (low) {
    this->speed = 1;
  } else {
    this->speed = 0;
  }
  
  this->state = (this->speed > 0);
  this->publish_state();
}

void FanController::queue_operation(OperationType type, uint8_t value) {
  operation_queue_.push(Operation(type, value));
}

void FanController::start_next_operation() {
  if (operation_queue_.empty() || operation_state_ != OperationState::IDLE || current_operation_ != nullptr) {
    return;
  }

  current_operation_ = new Operation(operation_queue_.front());
  operation_queue_.pop();
  
  output::BinaryOutput* output = (current_operation_->type == OperationType::POWER) ?
                                power_output_ : speed_output_;
  
  if (output == nullptr) {
    ESP_LOGW(TAG, "Output not configured for operation type %d",
             static_cast<int>(current_operation_->type));
    delete current_operation_;
    current_operation_ = nullptr;
    return;
  }

  ESP_LOGD(TAG, "Starting operation type %d value %u",
           static_cast<int>(current_operation_->type), current_operation_->target_value);
           
  operation_state_ = OperationState::BUTTON_PRESS;
  operation_attempts_ = 0;
  last_action_time_ = millis();
  output->turn_on();
}

bool FanController::handle_operation_result() {
  if (current_operation_->type == OperationType::POWER) {
    return true;  // Power operations always succeed after button press
  }
  
  // For speed operations, check if we've reached target speed
  uint8_t current = current_speed();
  if (current == current_operation_->target_value) {
    return true;
  }
  
  // If not at target speed, try another button press if attempts remain
  if (operation_attempts_ < MAX_SPEED_ATTEMPTS) {
    operation_state_ = OperationState::BUTTON_PRESS;
    speed_output_->turn_on();
    last_action_time_ = millis();
    operation_attempts_++;
    return false;
  }
  
  ESP_LOGW(TAG, "Failed to reach target speed %u after %u attempts",
           current_operation_->target_value, operation_attempts_);
  return true;  // Give up after max attempts
}

uint8_t FanController::current_speed() {
  if (low_led_ == nullptr || high_led_ == nullptr) {
    ESP_LOGW(TAG, "LED sensors not configured");
    return 0;
  }

  bool low = low_led_->state;
  bool high = high_led_->state;
  
  if (high && low) return 3;
  if (high) return 2;
  if (low) return 1;
  return 0;
}

}  // namespace fan_controller
}  // namespace esphome