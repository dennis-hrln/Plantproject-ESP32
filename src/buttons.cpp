/**
 * buttons.cpp - Button handling implementation
 */

#include "buttons.h"
#include "config.h"
#include "leds.h"
#include "sensor.h"
#include "watering.h"
#include "storage.h"

// =============================================================================
// INTERNAL HELPERS
// =============================================================================

/**
 * Get GPIO pin for button ID.
 */
static uint8_t get_button_pin(ButtonId button) {
    switch (button) {
        case BTN_MAIN:      return PIN_BTN_MAIN;
        case BTN_CAL_WET:   return PIN_BTN_CAL_WET;
        case BTN_CAL_DRY:   return PIN_BTN_CAL_DRY;
        default:            return 0;
    }
}

/**
 * Read button with debouncing.
 */
static bool read_button_debounced(uint8_t pin) {
    if (digitalRead(pin) == LOW) {  // Pressed (pull-up)
        delay(BTN_DEBOUNCE_MS);
        return (digitalRead(pin) == LOW);  // Still pressed
    }
    return false;
}

// =============================================================================
// INITIALIZATION
// =============================================================================

void buttons_init() {
    pinMode(PIN_BTN_MAIN, INPUT_PULLUP);
    pinMode(PIN_BTN_CAL_WET, INPUT_PULLUP);
    pinMode(PIN_BTN_CAL_DRY, INPUT_PULLUP);
}

// =============================================================================
// READING FUNCTIONS
// =============================================================================

bool button_is_pressed(ButtonId button) {
    uint8_t pin = get_button_pin(button);
    if (pin == 0) return false;
    
    return read_button_debounced(pin);
}

ButtonState button_check() {
    ButtonState state = {BTN_NONE, BTN_EVENT_NONE, 0};
    
    if (button_is_pressed(BTN_MAIN)) {
        state.button = BTN_MAIN;
        state.event = BTN_EVENT_SHORT_PRESS;  // Will be updated on release
    } else if (button_is_pressed(BTN_CAL_WET)) {
        state.button = BTN_CAL_WET;
        state.event = BTN_EVENT_SHORT_PRESS;
    } else if (button_is_pressed(BTN_CAL_DRY)) {
        state.button = BTN_CAL_DRY;
        state.event = BTN_EVENT_SHORT_PRESS;
    }
    
    return state;
}

ButtonState button_wait_event(uint32_t timeout_ms) {
    ButtonState state = {BTN_NONE, BTN_EVENT_NONE, 0};
    uint32_t start = millis();
    
    // Wait for a button to be pressed
    while (true) {
        if (timeout_ms > 0 && (millis() - start) > timeout_ms) {
            return state;  // Timeout
        }
        
        state = button_check();
        if (state.button != BTN_NONE) {
            break;  // Button detected
        }
        delay(10);
    }
    
    // Button is pressed, measure hold duration
    uint32_t press_start = millis();
    
    while (button_is_pressed(state.button)) {
        state.press_duration_ms = millis() - press_start;
        
        // Provide feedback when reaching long press threshold
        if (state.press_duration_ms >= BTN_LONG_PRESS_MS && 
            state.event != BTN_EVENT_LONG_PRESS) {
            state.event = BTN_EVENT_LONG_PRESS;
            // Brief LED feedback that long press registered
            led_green_on();
            delay(50);
            led_green_off();
        }
        
        delay(10);
    }
    
    // Button released
    if (state.event != BTN_EVENT_LONG_PRESS) {
        state.event = BTN_EVENT_SHORT_PRESS;
    }
    
    return state;
}

// =============================================================================
// LONG PRESS DETECTION
// =============================================================================

bool button_wait_long_press(ButtonId button) {
    uint32_t start = millis();
    
    while (button_is_pressed(button)) {
        if ((millis() - start) >= BTN_LONG_PRESS_MS) {
            return true;  // Long press achieved
        }
        delay(10);
    }
    
    return false;  // Released too soon
}

uint32_t button_wait_release(ButtonId button) {
    uint32_t start = millis();
    
    while (button_is_pressed(button)) {
        delay(10);
    }
    
    return millis() - start;
}

// =============================================================================
// COMBINATION DETECTION
// =============================================================================

bool buttons_calibration_combo_pressed() {
    // Both calibration buttons must be pressed
    return button_is_pressed(BTN_CAL_WET) && button_is_pressed(BTN_CAL_DRY);
}

bool buttons_full_combo_pressed() {
    // All three buttons pressed
    return button_is_pressed(BTN_MAIN) && 
           button_is_pressed(BTN_CAL_WET) && 
           button_is_pressed(BTN_CAL_DRY);
}

// =============================================================================
// HIGH-LEVEL INTERACTION HANDLER
// =============================================================================

/**
 * Handle calibration mode.
 * Entry requires both calibration buttons pressed + main button long press.
 * Then release and long-press individual calibration button.
 * 
 * SAFETY: Requires 3-button combo held for 2+ seconds, then individual 
 * calibration button long-press. Accidental calibration is virtually impossible.
 */
static void handle_calibration_mode() {
    #ifdef DEBUG_SERIAL
    Serial.println("Entered calibration mode");
    #endif
    
    // Confirm entering calibration mode with distinctive pattern
    led_show_calibration_confirm();
    
    // Wait for all buttons to be released
    while (button_is_pressed(BTN_MAIN) || 
           button_is_pressed(BTN_CAL_WET) || 
           button_is_pressed(BTN_CAL_DRY)) {
        delay(10);
    }
    
    // Indicate waiting for calibration selection
    // Green LED breathing/slow blink to show we're in calibration mode
    uint32_t start = millis();
    bool led_state = false;
    
    // Now wait for individual calibration button long press
    // Timeout after 10 seconds
    while ((millis() - start) < 10000) {
        // Blink green slowly to indicate waiting
        if (((millis() - start) / 500) % 2 != led_state) {
            led_state = !led_state;
            if (led_state) led_green_on();
            else led_green_off();
        }
        
        if (button_is_pressed(BTN_CAL_WET)) {
            led_green_off();
            #ifdef DEBUG_SERIAL
            Serial.println("Wet calibration button detected, waiting for long press...");
            #endif
            
            // Wait for long press with visual feedback
            uint32_t press_start = millis();
            while (button_is_pressed(BTN_CAL_WET)) {
                uint32_t held = millis() - press_start;
                
                // Progressive LED feedback
                if (held > BTN_LONG_PRESS_MS) {
                    // Long press achieved - perform calibration
                    led_green_on();
                    uint16_t raw = sensor_calibrate_wet();
                    
                    #ifdef DEBUG_SERIAL
                    Serial.print("Wet calibration complete. Raw value: ");
                    Serial.println(raw);
                    #endif
                    
                    // Wait for release
                    while (button_is_pressed(BTN_CAL_WET)) delay(10);
                    
                    led_green_off();
                    led_show_success();
                    return;
                }
                
                // Visual progress indicator
                if ((held / 200) % 2) led_green_on();
                else led_green_off();
                
                delay(10);
            }
            // Released too soon
            led_green_off();
        }
        
        if (button_is_pressed(BTN_CAL_DRY)) {
            led_green_off();
            #ifdef DEBUG_SERIAL
            Serial.println("Dry calibration button detected, waiting for long press...");
            #endif
            
            // Wait for long press with visual feedback
            uint32_t press_start = millis();
            while (button_is_pressed(BTN_CAL_DRY)) {
                uint32_t held = millis() - press_start;
                
                // Progressive LED feedback
                if (held > BTN_LONG_PRESS_MS) {
                    // Long press achieved - perform calibration
                    led_red_on();
                    uint16_t raw = sensor_calibrate_dry();
                    
                    #ifdef DEBUG_SERIAL
                    Serial.print("Dry calibration complete. Raw value: ");
                    Serial.println(raw);
                    #endif
                    
                    // Wait for release
                    while (button_is_pressed(BTN_CAL_DRY)) delay(10);
                    
                    led_red_off();
                    led_show_success();
                    return;
                }
                
                // Visual progress indicator (red for dry)
                if ((held / 200) % 2) led_red_on();
                else led_red_off();
                
                delay(10);
            }
            // Released too soon
            led_red_off();
        }
        
        delay(10);
    }
    
    // Timeout - exit without calibration
    led_green_off();
    led_show_error();
    
    #ifdef DEBUG_SERIAL
    Serial.println("Calibration mode timeout - no action taken");
    #endif
}

/**
 * Handle optimal humidity adjustment.
 * Entry: Main + WET calibration button held together.
 * Then short presses on WET (increase) or DRY (decrease) adjust value.
 * 
 * UI: 
 * - Shows current value via LED flashes
 * - Green blink = increase (+5%)
 * - Red blink = decrease (-5%)
 * - Auto-saves after 5 seconds of no input
 */
static void handle_humidity_adjustment() {
    uint8_t current = storage_get_optimal_humidity();
    
    #ifdef DEBUG_SERIAL
    Serial.print("Entering humidity adjustment. Current: ");
    Serial.print(current);
    Serial.println("%");
    #endif
    
    // Wait for buttons to be released first
    while (button_is_pressed(BTN_MAIN) || button_is_pressed(BTN_CAL_WET)) {
        delay(10);
    }
    
    // Indicate current value
    delay(500);
    led_display_number(current);
    delay(500);
    
    // Wait for adjustment presses (timeout 5 seconds between presses)
    uint32_t last_press = millis();
    
    while ((millis() - last_press) < 5000) {
        if (button_is_pressed(BTN_CAL_WET)) {
            // Increase humidity by 5%
            if (current <= 95) {
                current += 5;
                #ifdef DEBUG_SERIAL
                Serial.print("Humidity +5% = ");
                Serial.println(current);
                #endif
            }
            led_green_blink(1, 150);
            last_press = millis();
            button_wait_release(BTN_CAL_WET);
        }
        
        if (button_is_pressed(BTN_CAL_DRY)) {
            // Decrease humidity by 5%
            if (current >= 5) {
                current -= 5;
                #ifdef DEBUG_SERIAL
                Serial.print("Humidity -5% = ");
                Serial.println(current);
                #endif
            }
            led_red_blink(1, 150);
            last_press = millis();
            button_wait_release(BTN_CAL_DRY);
        }
        
        delay(10);
    }
    
    // Save new value
    storage_set_optimal_humidity(current);
    
    #ifdef DEBUG_SERIAL
    Serial.print("Humidity adjustment saved: ");
    Serial.print(current);
    Serial.println("%");
    #endif
    
    // Display final value and confirm save
    delay(300);
    led_display_number(current);
    delay(500);
    led_show_success();
}

void buttons_handle_interaction() {
    #ifdef DEBUG_SERIAL
    Serial.println("Button wake - handling interaction");
    #endif
    
    // Initial debounce
    delay(BTN_DEBOUNCE_MS);
    
    // Check for special combinations first
    
    // Full combo (all 3 buttons) → calibration mode entry
    // SAFETY: Requires all 3 buttons held for 2+ seconds
    if (buttons_full_combo_pressed()) {
        #ifdef DEBUG_SERIAL
        Serial.println("Full combo detected, waiting for long press...");
        #endif
        
        // Wait for long press while holding
        uint32_t start = millis();
        
        // Visual feedback during hold
        while (buttons_full_combo_pressed() && 
               (millis() - start) < BTN_LONG_PRESS_MS) {
            // Both LEDs pulse to indicate combo detected
            if (((millis() - start) / 100) % 2) {
                led_green_on();
                led_red_on();
            } else {
                led_green_off();
                led_red_off();
            }
            delay(10);
        }
        
        leds_all_off();
        
        if ((millis() - start) >= BTN_LONG_PRESS_MS && buttons_full_combo_pressed()) {
            handle_calibration_mode();
            return;
        }
    }
    
    // Main + WET calibration → humidity adjustment
    // SAFETY: Requires 2-button combo + long press
    if (button_is_pressed(BTN_MAIN) && button_is_pressed(BTN_CAL_WET)) {
        #ifdef DEBUG_SERIAL
        Serial.println("Main + Wet combo detected, waiting for long press...");
        #endif
        
        uint32_t start = millis();
        
        // Visual feedback
        while (button_is_pressed(BTN_MAIN) && button_is_pressed(BTN_CAL_WET) &&
               (millis() - start) < BTN_LONG_PRESS_MS) {
            if (((millis() - start) / 150) % 2) {
                led_green_on();
            } else {
                led_green_off();
            }
            delay(10);
        }
        
        led_green_off();
        
        if ((millis() - start) >= BTN_LONG_PRESS_MS) {
            handle_humidity_adjustment();
            return;
        }
    }
    
    // Single button actions
    ButtonState state = button_wait_event(5000);  // 5 second timeout
    
    #ifdef DEBUG_SERIAL
    Serial.print("Button event: btn=");
    Serial.print(state.button);
    Serial.print(" event=");
    Serial.print(state.event);
    Serial.print(" duration=");
    Serial.println(state.press_duration_ms);
    #endif
    
    switch (state.button) {
        case BTN_MAIN:
            if (state.event == BTN_EVENT_LONG_PRESS) {
                // Long press → manual watering
                #ifdef DEBUG_SERIAL
                Serial.println("Manual watering requested");
                #endif
                
                // Brief confirmation before watering
                led_green_blink(2, 100);
                
                WateringResult result = watering_manual(false);
                
                #ifdef DEBUG_SERIAL
                Serial.print("Watering result: ");
                Serial.println((int)result);
                #endif
                
                if (result == WATER_OK) {
                    led_show_success();
                } else if (result == WATER_TOO_SOON) {
                    // Indicate time remaining with red blinks
                    led_red_blink(3, 200);
                } else if (result == WATER_BATTERY_LOW) {
                    led_show_battery_critical();
                } else {
                    led_show_error();
                }
            } else if (state.event == BTN_EVENT_SHORT_PRESS) {
                // Short press → display humidity
                #ifdef DEBUG_SERIAL
                uint8_t h = sensor_read_humidity_percent();
                Serial.print("Displaying humidity: ");
                Serial.print(h);
                Serial.println("%");
                #endif
                
                led_display_humidity();
            }
            break;
            
        case BTN_CAL_WET:
        case BTN_CAL_DRY:
            // Single press on calibration buttons without combo does nothing
            // (prevents accidental calibration)
            // Feedback: single red blink to indicate "no action"
            led_red_blink(1, 100);
            
            #ifdef DEBUG_SERIAL
            Serial.println("Calibration button pressed alone - no action (safety)");
            #endif
            break;
            
        case BTN_NONE:
            // Timeout or no button - just return to sleep
            #ifdef DEBUG_SERIAL
            Serial.println("No button event detected");
            #endif
            break;
            
        default:
            break;
    }
}
