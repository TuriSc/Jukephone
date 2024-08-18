#ifndef CONFIG_H_
#define CONFIG_H_

/* Device identifiers */
#define PROGRAM_NAME            "Jukephone"
#define PROGRAM_VERSION         "1.0.1"
#define PROGRAM_DESCRIPTION     "Repurposing a landline telephone into a jukebox"
#define PROGRAM_URL             "https://turiscandurra.com/circuits"

/* Macros */
#define NUM_TRACKS 999

/* GPIO definitions */
#define KEYPAD_COLS             {8, 4, 2, 1, 0} // Keypad matrix column GPIOs
#define KEYPAD_ROWS             {7, 6, 5, 3}    // Keypad matrix row GPIOs

#define BUTTON_1_PIN             9
#define BUTTON_1_PIN_DESCRIPTION "Extra keypad button"

#define LED_PIN                 10
#define LED_PIN_DESCRIPTION     "Feedback LED"

#define BUZZER_PIN              14
#define BUZZER_PIN_DESCRIPTION  "Buzzer"

#define GPIO_TX                 20
#define GPIO_RX                 21
#define DFPLAYER_UART           uart1

#define BUSY_PIN                22  // Implemented but not used
#define BUSY_PIN_DESCRIPTION    "DFPlayer BUSY pin"

#define POWER_ON_LED_PIN        PICO_DEFAULT_LED_PIN
#define POWER_ON_LED_PIN_DESCRIPTION        "Power-on LED"

/* Timers and delays */
#define BLINK_DURATION_MS       100
#define BEEP_DURATION_MS        50
#define PLAYER_POLL_MS          350     // Should be > 200ms
#define KEYPAD_DEBOUNCE_US      250000
#define INPUT_TIMEOUT_MS        1000    // After this interval, any unsubmitted input
                                        // will be discarded

/* Definitions */
#define PAUSED_OR_IDLE          0
#define PLAYING                 1

#define STATUS                  0
#define PLAY                    1
#define VOLUMEDOWN              2
#define VOLUMEUP                3
#define EQ                      4
#define PAUSE                   5
#define RESUME                  6

/* Debugging */
#define DEBUG                   1

/* Melodies for the buzzer */
struct note_t POSITIVE[] = {
    {NOTE_C4, 16},
    {NOTE_AS4, 16},
    {NOTE_C5, 16},        
    {REST, 8},
    {MELODY_END, 0},
};

struct note_t NEGATIVE[] = {
    {NOTE_C5, 16},
    {NOTE_AS4, 16},
    {NOTE_C4, 16},        
    {REST, 8},
    {MELODY_END, 0},
};

struct note_t VICTORY[] = {
    {NOTE_G4, 8},
    {NOTE_G4, 16},
    {NOTE_G4, 16},
    {NOTE_D5, 4},
    {REST, 8},
    {MELODY_END, 0},
};

#endif /* CONFIG_H_ */
