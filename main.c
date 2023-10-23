/* Jukephone
 * Repurposing a landline telephone into a jukebox with Raspberry Pi Pico and an MP3 player.
 * By Turi Scandurra – https://turiscandurra.com/circuits
 * 2023.10.23 - v1.0.1
 */

#include <stdio.h>
#include <stdlib.h>
#include <pico/stdlib.h>
#include "hardware/adc.h"       // Needed for battery level monitoring
#include "dfplayer.h"           // https://github.com/TuriSc/RP2040-DFPlayer
#include "keypad.h"             // https://github.com/TuriSc/RP2040-Keypad-Matrix
#include "battery-check.h"      // https://github.com/TuriSc/RP2040-Battery-Check
#include "button.h"             // https://github.com/TuriSc/RP2040-Button
#include "pwm-tone.h"           // https://github.com/TuriSc/RP2040-PWM-Tone

#include "config.h"

#if DEBUG
#include "pico/stdio.h"
#endif

#if PICO_ON_DEVICE
#include "pico/binary_info.h"
#endif

static alarm_id_t power_on_alarm;
static alarm_id_t blink_alarm;
static alarm_id_t type_timeout_alarm;
static alarm_id_t scheduled_play_alarm;
static alarm_id_t loading_track_alarm;
static repeating_timer_t status_timer;
static repeating_timer_t low_batt_pulse_timer;

uint8_t status = PAUSED_OR_IDLE;
bool is_paused = false;
bool repeat = false;
uint16_t current_track = 1;
uint8_t next_player_command = STATUS;
uint16_t track_id_prompt;
uint16_t track_id_to_play;
uint8_t eq;
uint16_t shuffled_playlist[NUM_TRACKS+1];
uint16_t playlist_index = 1;

KeypadMatrix keypad;
const uint8_t cols[] = KEYPAD_COLS;
const uint8_t rows[] = KEYPAD_ROWS;

dfplayer_t dfplayer;

struct tonegenerator_t generator; // Used to drive the built-in piezo element

int64_t power_on_complete(){
    gpio_put(POWER_ON_LED_PIN, 0);
    return 0;
}

int64_t blink_complete(){
    gpio_put(LED_PIN, 0);
    return 0;
}

void blink(uint16_t ms){
    gpio_put(LED_PIN, 1);
    if (blink_alarm) cancel_alarm(blink_alarm);
    blink_alarm = add_alarm_in_ms(ms, blink_complete, NULL, true);
}

void randomize_playlist(){
    uint16_t i = NUM_TRACKS;
    uint16_t r1, r2, temp;

    for (i = 0; i <= NUM_TRACKS; i++){
        shuffled_playlist[i] = i;
    }

    for (i = 0; i < NUM_TRACKS*2; i++){
        r1 = rand() % NUM_TRACKS + 1;
        r2 = rand() % NUM_TRACKS + 1;

        temp = shuffled_playlist[r1];
        shuffled_playlist[r1] = shuffled_playlist[r2];
        shuffled_playlist[r2] = temp;
    }
}

void toggle_repeat(){
    repeat = !repeat;
    #if DEBUG
    printf("toggle_repeat: %d\n", repeat);
    #endif
    if(repeat){
        melody(&generator, POSITIVE, 0);
    } else {
        melody(&generator, NEGATIVE, 0);
    }
}

void player_request(uint8_t command){
    next_player_command = command;
}

void next_eq_preset(){
    eq++;
    if(eq > 5){ eq = 0; }
    player_request(EQ);
    #if DEBUG
    printf("next_eq_preset: %d\n", eq);
    #endif
}

void toggle_pause(){
    if(is_paused){
        player_request(RESUME);
        is_paused = false;
    } else { 
        player_request(PAUSE);
        is_paused = true;
    }
    #if DEBUG
    printf("Toggle pause: %d\tstatus:%d\n", is_paused, status);
    #endif
}

void random_track(){
    static bool random_seeded;
    if(!random_seeded){
        srand(time_us_64());
        randomize_playlist();
        random_seeded = true;
    }
    current_track = shuffled_playlist[playlist_index];
    #if DEBUG
    printf("random_track: %d\tplaylist_index:%d\n", current_track, playlist_index);
    #endif
    player_request(PLAY);

    playlist_index++;
    if(playlist_index > NUM_TRACKS){
        randomize_playlist();
        playlist_index = 1;
    }
}

// We could call dfplayer_previous() and dfplayer_next(), but some chips
// in DFPlayer clones have trouble picking the right track when files have
// not been transferred to the microSD card sequentially.
void prev_track(){
    if(current_track > 1){
        current_track--;
        #if DEBUG
        printf("prev_track: %d\n", current_track);
        #endif
        player_request(PLAY);
        // Cancel repeat
        repeat = false;
    }
}

void next_track(){
    if(current_track < NUM_TRACKS){
        current_track++;
        #if DEBUG
        printf("next_track: %d\n", current_track);
        #endif
        player_request(PLAY);
        // Cancel repeat
        repeat = false;
    }
}

void check_player_status(){
    static uint8_t last_player_status = 0;
    // dfplayer_get_status() is unreliable with some of the different chips found on
    // DFPlayer clones. If calling dfplayer_set_checksum_tx(false) does not help
    // (see library README) then you have to rely on reading the digital value of
    // the BUSY pin on the player:
    // uint8_t player_status = !gpio_get(BUSY_PIN);
    uint8_t player_status = dfplayer_get_status(&dfplayer);
    #if DEBUG
    printf("status: %d\tcur_track: %d\trepeat: %d\n", player_status, current_track, repeat);
    #endif
    if(player_status != last_player_status){
        #if DEBUG
        printf("Status changed\n");
        #endif
        status = player_status;
        if(player_status == 0 && !is_paused){
            #if DEBUG
            printf("Track completed\n");
            #endif
            if(repeat){
                player_request(PLAY);
            } else {
                next_track();
            }
        }
        last_player_status = player_status;
    }
}

bool poll_player(){
    switch(next_player_command){
        case PLAY:
            dfplayer_play(&dfplayer, current_track);
        break;
        case VOLUMEDOWN:
            dfplayer_decrease_volume(&dfplayer);
        break;
        case VOLUMEUP:
            dfplayer_increase_volume(&dfplayer);
        break;
        case EQ:
            dfplayer_write(&dfplayer, CMD_EQ, eq);
        break;
        case PAUSE:
            dfplayer_pause(&dfplayer);
        break;
        case RESUME:
            dfplayer_resume(&dfplayer);
        break;
        case STATUS:
        default:
            check_player_status();
        break;
    }

    next_player_command = STATUS;

    return true;
}

int64_t scheduled_play(){
    player_request(PLAY);
    return 0;
}

int64_t input_timeout(){
    track_id_prompt = 0;
    return 0;
}

void type_track_id(uint8_t n){
    if(type_timeout_alarm){ cancel_alarm (type_timeout_alarm); }
    type_timeout_alarm = add_alarm_in_ms(INPUT_TIMEOUT_MS, input_timeout, NULL, true);
    track_id_prompt *= 10;
    track_id_prompt += n;
    #if DEBUG
    printf("track_id_prompt: %d\n", track_id_prompt);
    #endif

    // Here is where I would hide another easter egg. For example:
    if(track_id_prompt == 7777){ melody(&generator, VICTORY, 0); }

    if(track_id_prompt > 0 && track_id_prompt <= NUM_TRACKS){
        current_track = track_id_prompt;
        if(scheduled_play_alarm){ cancel_alarm (scheduled_play_alarm); }
        scheduled_play_alarm = add_alarm_in_ms(INPUT_TIMEOUT_MS, scheduled_play, NULL, true);
    }
}

void key_long_pressed(uint8_t key){
    switch(key){
        case 13: // key 0
            next_eq_preset();
            break;
    }
    blink(BLINK_DURATION_MS); // Feedback blink
    tone(&generator, NOTE_C3, BEEP_DURATION_MS); // Feedback beep
}

bool keypress_available(){
    static uint64_t last_press;
    uint64_t now = time_us_64();
    if(now - last_press < KEYPAD_DEBOUNCE_US){
        #if DEBUG
        printf("Debounced");
        #endif
        return false;
    }
    last_press = now;
    return true;
}

void key_pressed(uint8_t key){
    #if DEBUG
    printf("key: %d\n", key);
    #endif

    // Debounce
    if(!keypress_available()){ return; }

    blink(BLINK_DURATION_MS); // Feedback blink

    switch(key){
        // Numbers
        case 0:
            type_track_id(1);
            tone(&generator, NOTE_C4, BEEP_DURATION_MS);
            break;
        case 1:
            type_track_id(2);
            tone(&generator, NOTE_CS4, BEEP_DURATION_MS);
            break;
        case 2:
            type_track_id(3);
            tone(&generator, NOTE_D4, BEEP_DURATION_MS);
            break;
        case 4:
            type_track_id(4);
            tone(&generator, NOTE_DS4, BEEP_DURATION_MS);
            break;
        case 5:
            type_track_id(5);
            tone(&generator, NOTE_E4, BEEP_DURATION_MS);
            break;
        case 6:
            type_track_id(6);
            tone(&generator, NOTE_F4, BEEP_DURATION_MS);
            break;
        case 8:
            type_track_id(7);
            tone(&generator, NOTE_FS4, BEEP_DURATION_MS);
            break;
        case 9:
            type_track_id(8);
            tone(&generator, NOTE_G4, BEEP_DURATION_MS);
            break;
        case 10:
            type_track_id(9);
            tone(&generator, NOTE_GS4, BEEP_DURATION_MS);
            break;
        case 13:
            type_track_id(0);
            tone(&generator, NOTE_AS4, BEEP_DURATION_MS);
            break;
        // Prev / Next (asterisk and little gate sign keys)
        case 12:
            prev_track();
            tone(&generator, NOTE_A4, BEEP_DURATION_MS);
            break;
        case 14:
            next_track();
            tone(&generator, NOTE_B4, BEEP_DURATION_MS);
            break;
        // Additional keys
        case 3:
            random_track();
            break;
        case 16:
            #if DEBUG
            printf("vol-\n");
            #endif
            player_request(VOLUMEDOWN);
            break;
        case 11:
            #if DEBUG
            printf("vol+\n");
            #endif
            player_request(VOLUMEUP);
            break;
        // Note: on the specific telephone I used, the bottom left key (mute)
        // is not part of the keypad matrix and is connected to its own pin.
        // See button_onchange() below.
        case 7:
            toggle_repeat();
            break;
        case 15:
            toggle_pause();
            break;
    }
}

void button_onchange(button_t *button_p){
    button_t *button = (button_t*)button_p;
    if(button->state) return;   // Ignore button release. Invert the logic if using
                                // a pullup (internal or external).
    switch(button->pin){
        case BUTTON_1_PIN:
            player_request(PLAY);
        break;
    }
    blink(BLINK_DURATION_MS); // Feedback blink
}

void bi_decl_all(){
    #if PICO_ON_DEVICE
    bi_decl(bi_program_name(PROGRAM_NAME));
    bi_decl(bi_program_description(PROGRAM_DESCRIPTION));
    bi_decl(bi_program_version_string(PROGRAM_VERSION));
    bi_decl(bi_program_url(PROGRAM_URL));
    bi_decl(bi_1pin_with_name(LED_PIN, LED_PIN_DESCRIPTION));
    bi_decl(bi_1pin_with_name(BUZZER_PIN, BUZZER_PIN_DESCRIPTION));
    bi_decl(bi_1pin_with_name(POWER_ON_LED_PIN, POWER_ON_LED_PIN_DESCRIPTION));
    bi_decl(bi_1pin_with_name(BUSY_PIN, BUSY_PIN_DESCRIPTION));
    bi_decl(bi_2pins_with_func(GPIO_TX, GPIO_RX, GPIO_FUNC_UART));
    bi_decl(bi_1pin_with_name(cols[0], "Keypad matrix column pin 1"));
    bi_decl(bi_1pin_with_name(cols[1], "Keypad matrix column pin 2"));
    bi_decl(bi_1pin_with_name(cols[2], "Keypad matrix column pin 3"));
    bi_decl(bi_1pin_with_name(cols[3], "Keypad matrix column pin 4"));
    bi_decl(bi_1pin_with_name(cols[4], "Keypad matrix column pin 5"));
    bi_decl(bi_1pin_with_name(rows[0], "Keypad matrix row pin 1"));
    bi_decl(bi_1pin_with_name(rows[1], "Keypad matrix row pin 2"));
    bi_decl(bi_1pin_with_name(rows[2], "Keypad matrix row pin 3"));
    bi_decl(bi_1pin_with_name(rows[3], "Keypad matrix row pin 4"));
    bi_decl(bi_1pin_with_name(BUTTON_1_PIN, BUTTON_1_PIN_DESCRIPTION));
    #endif
}

bool low_batt_pulse(){
    static bool flag;
    gpio_put(LED_PIN, flag = !flag);
}

void battery_low_callback(uint16_t battery_mv){
    battery_check_stop();
    add_repeating_timer_ms(200, low_batt_pulse, NULL, &low_batt_pulse_timer);
}

int main(){
    stdio_init_all();
    #if DEBUG
    stdio_usb_init();
    #endif
    bi_decl_all();

    // Use the onboard LED as a power-on indicator
    gpio_init(POWER_ON_LED_PIN);
    gpio_set_dir(POWER_ON_LED_PIN, GPIO_OUT);
    gpio_put(POWER_ON_LED_PIN, 1);
    power_on_alarm = add_alarm_in_ms(500, power_on_complete, NULL, true);

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    button_t *play_button = create_button(BUTTON_1_PIN, button_onchange);

    adc_init(); // Initialize the ADC for battery level monitoring
    battery_check_init(5000, NULL, battery_low_callback);

    gpio_init(BUSY_PIN);
    gpio_set_dir(BUSY_PIN, GPIO_IN);

    // Initialize the keypad with column and row configuration
    // And declare the number of columns and rows of the keypad
    keypad_init(&keypad, cols, rows, 5, 4);

    // Assign the callbacks for each keypad event
    keypad_on_press(&keypad, key_pressed);
    keypad_on_long_press(&keypad, key_long_pressed);

    dfplayer_init(&dfplayer, DFPLAYER_UART, GPIO_TX, GPIO_RX);
    sleep_ms(200); // Wait 200ms between commands to the player
    
    // Accepted volume values are 1 to 30.
    // Be careful, as it can get dangerously loud for a headset.
    dfplayer_set_volume(&dfplayer, 1);

    blink(BLINK_DURATION_MS); // Feedback blink
    
    tone_init(&generator, BUZZER_PIN);

    add_repeating_timer_ms(PLAYER_POLL_MS, poll_player, NULL, &status_timer);

    while (true){
        keypad_read(&keypad);
        sleep_ms(10);
    }

    return 0;
}