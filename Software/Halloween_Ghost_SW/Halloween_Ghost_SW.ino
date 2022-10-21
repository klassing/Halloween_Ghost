/* 
    Author: Ryan Klassing 
    Date: 10/18/22
    Version: */ #define SW_Version "v0.0.1" /*
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
    #include <WiFiAP.h>         // Included in ESP32 Arduino Core - Tested with v2.0.5 - https://github.com/espressif/arduino-esp32/releases/tag/2.0.5
    #include <WebServer.h>      // Included in ESP32 ARduino Core - Tested with v2.0.5 - https://github.com/espressif/arduino-esp32/releases/tag/2.0.5
    #include <EEPROM.h>         // Included in ESP32 ARduino Core - Tested with v2.0.5 - https://github.com/espressif/arduino-esp32/releases/tag/2.0.5
    #include <ESP2SOTA.h>       // Tested with v1.0.2 - https://github.com/pangodream/ESP2SOTA/releases/tag/1.0.2
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
    uint8_t led_fill_peripheral = false;
    uint8_t led_peripheral_hue = 0;

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

/* -------- [START] WiFi AP Configuration -------------- */
    const char* ota_ap_ssid = "HalloWiFi";
    const char* ota_ap_password = "password";
    WebServer server(80);
    uint8_t check_for_ota_update = false;       //Boolean flag to allow handler to determine when to check for OTA updates
    uint32_t ota_handler_timer_start = 0;       //Time when the timer was started
    uint32_t ota_handler_timer_duration = 0;    //Duration (in ms) for the timer
    uint8_t ota_handler_index = 0;              //Index for which function the ota handler should call
    #define QTY_OF_OTA_HANDLER_FUNC 3           //Max # of functions called by the ota handler
    #define OTA_HANDLER_TIMEOUT 300000          //Timeout (in ms) for how long to keep the WiFi AP alive while checking for OTA update
    uint32_t ota_start_time = 0;                //Keep track of when OTA was started
/* ---------- [END] WiFi AP Configuration -------------- */

/* ------ [START] EEPROM Configuration -------------- */
    #define EEPROM_SIZE 3                       //# bytes to use in flash for EEPROM emulation
    #define EEPROM_BLANK 0xff                   // used to check for unwritted/blank bytes (initial boot)
    #define EEPROM_ADDR_EYE_R 0                 //Address to store the Red content of the Eye Color
    #define EEPROM_ADDR_EYE_G 1                 //Address to store the Green content of the Eye Color
    #define EEPROM_ADDR_EYE_B 2                 //Address to store the Blue content of the Eye Color
/* -------- [END] EEPROM Configuration -------------- */

/* ---- [START] Power Configuration -------------- */
    #define POWER_RUNTIME 300000                //Time to run after waking up (in ms) before going back to Deep Sleep
/* ------ [END] Power Configuration -------------- */

/* ------------ [START] Define Function Prototypes -------------- */
    /* General Protoypes */
    void pin_config();                          //Function to initialize HW config
    void print_welcome_message();               //Function to print a welcome message with the SW version
    void check_wakeup_reason();                 //Fucntion to check the wake-up reason for this boot cycle

    /* Power Management Prototypes */
    void power_state_handler();                 //Handler function to execute various power state management tasks
    void disableWiFi();                         //Function to disable WiFi for power savings
    void disableBT();                           //Function to disable BT for power savings
    void enter_deep_sleep_IO_wake();            //Function to enter Deep Sleep but wake-up on button press only

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

    /* OTA Prototypes */
    void ota_handler();                                                 //Handler function to execute various OTA management tasks
    void ota_handler_next_function(uint32_t delay_before_next = 0);     //Function to cycle to the next handler function index, with a "delay" (non-blocking) before executing the next function
    void ota_handler_start_timer();                                     //Function to start a timer for the ota handler
    uint8_t ota_handler_check_timer();                                  //Function to check if the "delay" (non-blocking) timer has expired
    void ota_handler_stop_timer();                                      //Function to clear the existing timer (if any)
    void ota_enable_ap();                                               //Function to enable WiFi AP for supporting OTA updates
    void ota_start_server();                                            //Function to start the web server to be used for OTA updates
    void ota_check_for_update();                                        //Function to check for OTA updates in the background

    /* EEPROM Prototypes */
    CRGB EEPROM_check_last_eye_color();         //Function to read the eye color (RGB) stored in EEPROM
    void EEPROM_save_eye_color(CRGB eye_color); //Function to write the current eye color (RGB) to be stored in EEPROM
    void EEPROM_save_eye_color(CHSV eye_color); //Function to write the current eye color (HSV converted to RGB) to be stored in EEPROM

    /* Debug / Printing Prototypes */
    void time_print(String message);            //Function to pre-pend a message with the current CPU running timestamp
    void time_println(String message);          //Function to pre-pend a message with the current CPU running timestamp, with CRLF
    void log(String message);                   //Function to log debug messages during development
    void logln(String message);                 //Function to log debug messages during development, with CRLF
    
/* -------------- [END] Define Function Prototypes -------------- */

void setup() {
    /* Initialize the Serial Terminal */
    Serial.begin(SERIAL_BAUD);

    /* Initialize the EEPROM for later access */
    EEPROM.begin(EEPROM_SIZE);

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

    /* Handle any OTA tasks */
    ota_handler();

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

/* Function to enter Deep Sleep but wake-up on button press only */
void enter_deep_sleep_IO_wake() {
    /* Prepare to go to sleep */
    esp_sleep_enable_ext0_wakeup((gpio_num_t)RIGHT_TOUCH_PIN, 1);

    /* Disable the wake-up timer for now */
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);

    /* Store the current eye color in EEPROM */
    EEPROM_save_eye_color(LED_EYE_COLOR);

    /* Go to sleep */
    esp_deep_sleep_start();
}

/* Handler function to execute various LED management tasks */
void led_handler() {
    /* Calculate the eye fade */
    calculate_eye_fade();

    /* Fill the surrounding lights periodically */
    EVERY_N_SECONDS(10) { led_fill_peripheral = !led_fill_peripheral;}

    if (led_fill_peripheral) {
        EVERY_N_MILLISECONDS(20) {led_peripheral_hue++;}
        fill_rainbow(LED_ARR + LED_PER_START_POS, LED_QTY - LED_PER_START_POS, led_peripheral_hue,  7);
    } else {
        fadeToBlackBy(LED_ARR + LED_PER_START_POS, LED_QTY - LED_PER_START_POS, 1);
    }

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
    left_hand_btn.setLongClickTime(5000);
    left_hand_btn.setLongClickHandler(button_left_long_click);

    /* Configure Right Hand */
    right_hand_btn.begin(RIGHT_TOUCH_PIN, INPUT, false, false);       //(pin, mode, isCapacitive, activeLow)
    right_hand_btn.setDebounceTime(500);
    right_hand_btn.setClickHandler(button_right_click);
    right_hand_btn.setLongClickTime(5000);
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
    logln("Left Hand - Long Click: No Action");

    
}

/* Callback function to be executed when right is clicked */
void button_right_click(Button2& btn) {
    logln("Right Hand - Click: Entering Deep Sleep..");

    enter_deep_sleep_IO_wake();
}

/* Callback function to be executed when right is long clicked */
void button_right_long_click(Button2& btn) {
    logln("Right Hand - Long Click: Starting OTA Scan..");

    check_for_ota_update = true;
}

/* Handler function to execute various power state management tasks */
void power_state_handler() {    
    /* Set a wake-up timer based on the desired frames-per-second */
    //esp_sleep_enable_timer_wakeup(1000000/LED_FPS);

    /* Go to sleep */
    //esp_light_sleep_start();

    if (millis() > POWER_RUNTIME) {
        Serial.println("Runtime exceeded (" + String (POWER_RUNTIME/1000.0/60.0, DEC) + " min)");
        Serial.println("Enterring Deep Sleep for reduced power");
        enter_deep_sleep_IO_wake();
     }
}

/* Function to set the default Ghost eye color */
void set_initial_eye_color() {
    LED_EYE_COLOR = rgb2hsv_approximate(EEPROM_check_last_eye_color());
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

/* Handler function to execute various OTA management tasks */
void ota_handler() {
    if(check_for_ota_update) {
        /* If the ota_handler_timer has expired, call the next function in the index list */
            /* Note: ota_handler_index will be incremented in each function as needed */
        if (ota_handler_check_timer()) {
            switch(ota_handler_index) {
                case 0:
                    ota_enable_ap();
                    break;
                case 1:
                    ota_start_server();
                    break;
                case 2:
                    ota_check_for_update();
                    break;
                default:
                    /* If made it here, ota_handler_max_index was defined too large for how many functions are being used - increment again to loop through eventually */
                    ota_handler_next_function();
                    break;
            }
        }
    } else {
        /* Reset the ota handler index back to 0, to start from the begining when starting an OTA check */
        ota_handler_index = 0;

        /* Reset the OTA timer for the next use */
        ota_start_time = millis();
    }
}

/* Function to cycle to the next handler function index, with a "delay" (non-blocking) before executing the next function */
void ota_handler_next_function(uint32_t delay_before_next/*=0*/) {
    /* Increment the index, rolling over to the start if needed */
    ota_handler_index = (ota_handler_index + 1) % QTY_OF_OTA_HANDLER_FUNC;

    /* Set the delay timer if needed */
    if (delay_before_next) {
        /* Set the expiration time */
        ota_handler_timer_duration = delay_before_next;

        /* Start the timer */
        ota_handler_start_timer();
    } else {
        /* Ensure no timer/delay is active */
        ota_handler_stop_timer();
    }
}

/* Function to start a timer for the ota handler */
void ota_handler_start_timer() {
    /* Keep track of the start time */
    ota_handler_timer_start = millis();
}

/* Function to check if the "delay" (non-blocking) timer has expired */
uint8_t ota_handler_check_timer() {
    if ((millis() - ota_handler_timer_start) > ota_handler_timer_duration) {
        /* stop the timer, it has expired */
        ota_handler_stop_timer();

        return true;
    }

    /* Timer didn't yet expire */
    return false;
}

/* Function to clear the existing timer (if any) */
void ota_handler_stop_timer() {
    /* clear the timer start and duration */
    ota_handler_timer_start = 0;
    ota_handler_timer_duration = 0;
}

/* Function to enable WiFi AP for supporting OTA updates */
void ota_enable_ap() {
    /* Start the ota timer */
    ota_start_time = millis();

    time_println("Enabling WiFi AP to check for OTA updates..");
    time_print("..Connect to SSID: "); Serial.println(ota_ap_ssid);
    time_print("..       Password: "); Serial.println(ota_ap_password);
    time_print("OTA mode will be held for " + String(OTA_HANDLER_TIMEOUT/1000, DEC) + " seconds before going back to sleep.");

    /* Enable Wifi radio and start the access point */
    WiFi.disconnect(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ota_ap_ssid, ota_ap_password);

    /* Increment to the next function index after 1000ms */
    ota_handler_next_function(1000);
}

/* Function to start the web server to be used for OTA updates */
void ota_start_server() {
    /* Set the IP Address */
    IPAddress IP = IPAddress(10, 10, 10, 1);
    IPAddress NMask = IPAddress(255, 255, 255, 0);
    WiFi.softAPConfig(IP, IP, NMask);

    /* Confirm and print the IP Address to the terminal */
    IPAddress myIP = WiFi.softAPIP();
    time_print(".. AP IP Address: "); Serial.println(myIP);

    /* Configure the server */
    server.on("/myurl", HTTP_GET, []() {
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "Hello there!");
    });

    /* Initialize the server & OTA library */
    ESP2SOTA.begin(&server);
    server.begin();

    /* Increment to the next function index without any delay */
    ota_handler_next_function();
}

/* Function to check for OTA updates in the background */
void ota_check_for_update() {
    /* Go to Deep Sleep if the OTA timeout has been reached, to prevent wasting power */
    if (millis() - ota_start_time > OTA_HANDLER_TIMEOUT) {
        time_println("OTA Timeout occured, going to deep sleep for power savings..");

        enter_deep_sleep_IO_wake();
    }

    /* Check for any updates */
    server.handleClient();
}

/* Function to read the eye color (RGB) stored in EEPROM */
CRGB EEPROM_check_last_eye_color() {
    CRGB eye_reader;
    eye_reader.r = EEPROM.read(EEPROM_ADDR_EYE_R);
    eye_reader.g = EEPROM.read(EEPROM_ADDR_EYE_G);
    eye_reader.b = EEPROM.read(EEPROM_ADDR_EYE_B);

    /* If EEPROM was blank, eyes will be all white and fully saturated - set a default color in this case */
    if (eye_reader.r == EEPROM_BLANK && eye_reader.g == EEPROM_BLANK && eye_reader.b == EEPROM_BLANK) {return LED_DEFAULT_EYE_COLOR;}

    return eye_reader;
}

/* Function to write the current eye color (RGB) to be stored in EEPROM */
void EEPROM_save_eye_color(CRGB eye_color) {
    EEPROM.write(EEPROM_ADDR_EYE_R, eye_color.r);
    EEPROM.write(EEPROM_ADDR_EYE_G, eye_color.g);
    EEPROM.write(EEPROM_ADDR_EYE_B, eye_color.b);

    EEPROM.commit();
}

/* Function to write the current eye color (HSV converted to RGB) to be stored in EEPROM */
void EEPROM_save_eye_color(CHSV eye_color) {
    CRGB rgb_eye_color;

    hsv2rgb_rainbow(eye_color, rgb_eye_color);

    EEPROM_save_eye_color(rgb_eye_color);
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
