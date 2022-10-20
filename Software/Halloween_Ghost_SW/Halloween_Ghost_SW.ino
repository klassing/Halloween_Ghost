/* 
    Author: Ryan Klassing 
    Date: 10/18/22
    Version: */ #define SW_Version "v0.1" /*
    Description:
        This is intended to run a small ESP32 based PCB that looks
        like a ghost, for simple/fun Halloween decotrations.  The
        ESP32 controls 2x "eye" LEDs (for the ghost's eyes) as well
        as an additional 8x "ambiance" LEDs (for glowing effects) around
        the ghost.  The board also contains 2x I2S amplifiers + Speakers,
        a USB Type C (for programming / debugging), and can be operated
        from a battery range of ~1.6V (startup, then can operate down to ~1.0V)
        up to ~5.2V.

    License: see "LICENSE" file
*/

/* ------------ [START] Include necessary libraries -------------- */
    #include <FastLED.h>        // Tested with v3.5.0 - https://github.com/FastLED/FastLED/releases/tag/3.5.0
    #include <cmdArduino.h>     // Custom Forked library from Ryan K - https://github.com/klassing/cmdArduino
    #include <cmdChar.h>        // Custom library from Ryan K - https://github.com/klassing/arduino_libraries
    #include <WiFi.h>           // Included in ESP32 Arduino Core - Tested with v2.0.5 - https://github.com/espressif/arduino-esp32/releases/tag/2.0.5
    #include <esp_bt.h>         // Included in ESP32 Arduino Core - Tested with v2.0.5 - https://github.com/espressif/arduino-esp32/releases/tag/2.0.5
    #include <Button2.h>        // Tested with v2.0.3 - https://github.com/LennartHennigs/Button2/releases/tag/2.0.3
/* -------------- [END] Include necessary libraries -------------- */

/* ------------ [START] HW Configuration Setup -------------- */
    /* Pin Configurations */
    #define I2S_BCK_PIN 5
    #define I2S_LRCK_PIN 18
    #define I2S_DOUT_PIN 23
    #define AMP_LEFT_EN_PIN 19
    #define AMP_RIGHT_EN_PIN 21
    #define LED_PWR_EN_PIN 25
    #define LED_DATA_PIN 26
    #define LEFT_TOUCH_PIN 15
    #define RIGHT_TOUCH_PIN 14

    /* LED Configurations */
    #define LED_TYPE WS2811
    #define LED_COLOR_ORDER GRB
    #define LED_QTY 10
    #define LED_RIGHT_EYE_POS 0     //Right Eye LED position in the LED array
    #define LED_LEFT_EYE_POS 1      //Left Eye LED position in the LED array
    #define LED_PER_START_POS 2     //Starting array position for the peripheral LEDs
    #define LED_MAX_BRIGHTNESS 96  //Maximum allowed brightness for the LEDs
    #define LED_FPS 240              //# of times to push an update to the LED color/brightness per second
    CRGB LED_ARR[LED_QTY];          //global LED array

    /* LED Settings */
    #define LED_EYE_BREATH_MIN 20
    #define LED_EYE_BREATH_MAX 120
    #define LED_EYE_BREATH_SPEED 3//1.5
    static float LED_EYE_BREATH_DELTA = (LED_EYE_BREATH_MAX - LED_EYE_BREATH_MIN) / 2.35040238;
    CHSV LED_EYE_COLOR;
    #define LED_DEFAULT_EYE_COLOR CRGB::Red

    /* Sleep / CPU configurations */
    #define CPU_SLEEP_FREQ_MHZ 40   //Set CPU to 40MHz before going to sleep
    #define CPU_RUN_FREQ_MHZ 80     //Set CPU to 80MHz while running

/* -------------- [END] HW Configuration Setup -------------- */

/* ------------ [START] Debug compile options -------------- */
    #define LOG_DEBUG true          //true = logging printed to terminal, false = no logging
/* -------------- [END] Debug compile options -------------- */

/* ------------ [START] Serial Terminal Configuration -------------- */
    #define SERIAL_BAUD 115200
/* -------------- [END] Serial Terminal Configuration -------------- */

/* ---------- [START] Button Configuration -------------- */
    Button2 left_hand_btn;          //Button for pressing the ghost's left hand
    Button2 right_hand_btn;         //Button for pressing the ghost's right hand
/* ------------ [End] Button Configuration -------------- */

/* ------------ [START] Define Function Prototypes -------------- */
    /* General Protoypes */
    void pin_config();                          //Function to initialize HW config
    void print_welcome_message();               //Function to print a welcome message with the SW version
    void check_wakeup_reason();                 //Fucntion to check the wake-up reason for this boot cycle

    /* Power Management Prototypes */
    void power_state_handler();                 //Handler function to execute various power state management tasks
    void disableWiFi();                         //Function to disable WiFi for power savings
    void disableBT();                           //Function to disable BT for power savings

    /* LED Management Prototypes */
    void led_handler();                         //Handler function to execute various LED management tasks
    void set_initial_eye();                     //Function to set the default Ghost eye color
    void calculate_eye_fade();                  //Function to calculate the Ghost's eye brightness

    /* Input Button Management Prototypes */
    void button_handler();                      //Handler function to execute various input button management tasks
    void button_init();                         //Function to initialize the button configurations
    void button_left_pressed(Button2& btn);     //Callback function to be executed when left is pressed/held
    void button_left_long_click(Button2& btn);  //Callback function to be executed when left is long clicked
    void button_right_click(Button2& btn);      //Callback function to be executed when right is clicked
    void button_right_long_click(Button2& btn); //Callback function to be executed when right is long clicked

    /* Debug / Printing Prototypes */
    void time_print(String message);            //Function to pre-pend a message with the current CPU running timestamp
    void time_println(String message);          //Function to pre-pend a message with the current CPU running timestamp, with CRLF
    void log(String message);                   //Function to log debug messages during development
    void logln(String message);                 //Function to log debug messages during development, with CRLF
/* -------------- [END] Define Function Prototypes -------------- */

void setup() {
    /* Initialize the Serial Terminal */
    Serial.begin(SERIAL_BAUD);

    /* Disable BT/WiFi for power savings */
    disableWiFi();
    disableBT();

    /* Initialize the HW pins / buttons */
    pin_config();
    button_init();

    /* Initialize the Ghost Eye color */
    set_initial_eye_color();

    /* Print Welcome Message */
    print_welcome_message();

    /* Check the wakeup reason */
    check_wakeup_reason();

    /* Create LED array / Set master brightness */
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, LED_COLOR_ORDER>(LED_ARR, LED_QTY).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(LED_MAX_BRIGHTNESS);

}

void loop() {
    /* Handle LED tasks */
    led_handler();

    /* Handle input tasks */
    button_handler();

    /* Handle the power states */
    power_state_handler();
}

/* Function to initialize HW config */
void pin_config() {
    /* Set the pin modes (output/input) */
    pinMode(I2S_BCK_PIN, INPUT_PULLDOWN);         //Disabled for now
    pinMode(I2S_LRCK_PIN, INPUT_PULLDOWN);        //Disabled for now
    pinMode(I2S_DOUT_PIN, INPUT_PULLDOWN);        //Disabled for now
    pinMode(AMP_LEFT_EN_PIN, INPUT_PULLDOWN);     //Disabled for now
    pinMode(AMP_RIGHT_EN_PIN, INPUT_PULLDOWN);    //Disabled for now
    pinMode(LED_PWR_EN_PIN, OUTPUT);

    /* Set the default state for output pins */
    digitalWrite(LED_PWR_EN_PIN, HIGH);
}

/* Function to print a welcome message with the SW version */
void print_welcome_message() {
    time_println("***************************");
    time_println("***** Halloween Ghost *****");
    time_print("*****     SW: "); Serial.print(SW_Version); Serial.println("    *****");
    time_println("***************************");
}

/* Fucntion to check the wake-up reason for this boot cycle */
void check_wakeup_reason() {
   esp_sleep_wakeup_cause_t wake_up_source;

   wake_up_source = esp_sleep_get_wakeup_cause();

   switch(wake_up_source){
      case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wake-up from external signal with RTC_IO"); break;
      case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wake-up from external signal with RTC_CNTL"); break;
      case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wake up caused by a timer"); break;
      case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wake up caused by a touchpad"); break;
      default : Serial.printf("Wake up not caused by Deep Sleep: %d\n",wake_up_source); break;
   }
}

/* Function to disable WiFi for power savings */
void disableWiFi() {
    logln("Disabling WiFi for power saving");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

/* Function to disable BT for power savings */
void disableBT() {
    logln("Disabling BT for power saving");
    btStop();
}

/* Handler function to execute various LED management tasks */
void led_handler() {
    /* Calculate the eye fade */
    calculate_eye_fade();

    /* push LED data */
    FastLED.show();

}

/* Handler function to execute various input button management tasks */
void button_handler() {
    left_hand_btn.loop();
    right_hand_btn.loop();

    /* Temporary work around */
    if (left_hand_btn.isPressed()) {button_left_pressed(left_hand_btn);}
}

/* Function to initialize the button configurations */
void button_init() {
    /* Configure Left Hand */
    left_hand_btn.begin(LEFT_TOUCH_PIN, INPUT, false, false);       //(pin, mode, isCapacitive, activeLow)
    //left_hand_btn.setPressedHandler(button_left_pressed);
    left_hand_btn.setLongClickHandler(button_left_long_click);
    left_hand_btn.setLongClickDetectedRetriggerable(true);

    /* Configure Right Hand */
    right_hand_btn.begin(RIGHT_TOUCH_PIN, INPUT, false, false);       //(pin, mode, isCapacitive, activeLow)
    right_hand_btn.setClickHandler(button_right_click);
    right_hand_btn.setLongClickHandler(button_right_long_click);
}

/* Callback function to be executed when left is clicked */
void button_left_pressed(Button2& btn) {
    EVERY_N_MILLISECONDS (50) {
        logln("Left Hand - Pressed: Incrementing Eye Hue -> " + String(LED_EYE_COLOR.h));

        /* Increment the hue */
        LED_EYE_COLOR.h++;
    }
}

/* Callback function to be executed when left is long clicked */
void button_left_long_click(Button2& btn) {
    logln("Left Hand - Long Click: Incrementing Eye Hue -> " + String(LED_EYE_COLOR.h));

    /* Increment the hue */
    LED_EYE_COLOR.h++;
}

/* Callback function to be executed when right is clicked */
void button_right_click(Button2& btn) {
    logln("Right Hand - Click: No Action");
}

/* Callback function to be executed when right is long clicked */
void button_right_long_click(Button2& btn) {
    logln("Right Hand - Long Click: Entering Deep Sleep..");

    /* Prepare to go to sleep */
    esp_sleep_enable_ext0_wakeup((gpio_num_t)RIGHT_TOUCH_PIN, 1);

    /* Disable the wake-up timer for now */
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);

    /* Go to sleep */
    esp_deep_sleep_start();
}

/* Handler function to execute various power state management tasks */
void power_state_handler() {    
    /* Set a wake-up timer based on the desired frames-per-second */
    esp_sleep_enable_timer_wakeup(1000000/LED_FPS);

    /* Go to sleep */
    esp_light_sleep_start();
}

/* Function to set the default Ghost eye color */
void set_initial_eye_color() {
    LED_EYE_COLOR = rgb2hsv_approximate(LED_DEFAULT_EYE_COLOR);
}

/* Function to calculate the Ghost's eye brightness */
void calculate_eye_fade() {
    /* Calculate the brightness value, based on exponential effect over time */
    float deltaVAL = ((exp(sin(LED_EYE_BREATH_SPEED * millis()/2000.0*PI)) -0.36787944) * LED_EYE_BREATH_DELTA);

    /* Adjust the HSV value to perform dimming */
    LED_EYE_COLOR.v = deltaVAL + LED_EYE_BREATH_MIN;

    //LED_EYE_COLOR.v = beatsin16(30, LED_EYE_BREATH_MIN, LED_EYE_BREATH_MAX);

    /* Update the values in the LED array */
    LED_ARR[LED_RIGHT_EYE_POS] = CHSV(LED_EYE_COLOR.h, LED_EYE_COLOR.s, LED_EYE_COLOR.v);
    LED_ARR[LED_LEFT_EYE_POS] = CHSV(LED_EYE_COLOR.h, LED_EYE_COLOR.s, LED_EYE_COLOR.v);
}

/* Function to pre-pend a message with the current CPU running timestamp */
void time_print(String message) {
    String timed_message = "[" + String((millis()), DEC) + "] " + message;
    Serial.print(timed_message);
}

/* Function to pre-pend a message with the current CPU running timestamp, with CRLF */
void time_println(String message) {
    time_print(message + "\r\n");
}

/* Function to log debug messages during development */
void log(String message) {
    if(LOG_DEBUG) {time_print(message);}
}

/* Function to log debug messages during development, with CRLF */
void logln(String message) {
    log(message + "\r\n");
}

