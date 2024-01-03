#include <Adafruit_NeoPixel.h>
#include <math.h>

#define NUM_PIXELS 200
#define LED_PIN D6

// Knob to Pin mapping
#define R1 A8
#define R2 A5
#define R3 A2
#define R4 A9
#define R5 A4
#define R6 A1
#define R7 A10
#define R8 A3
#define R9 A0

// knob min/max bit values
#define KNOB_MIN_VAL 0
#define KNOB_MAX_VAL 4095
#define KNOB_MID_VAL ((KNOB_MAX_VAL - KNOB_MIN_VAL) / 2)

#define CHANGE_THRESHOLD 0.02 // only update param if changes by this fraction of max
#define THRESHOLD_WAVELN 1

#define SPEED_ZERO_RANGE 100 // range around mid knob value that is considered 0

// speed limits in units led/s
#define SPEED_MIN 0
#define SPEED_MAX 0.5

// wavelength limits in units leds
#define WAVELN_MIN 0
#define WAVELN_MAX 200

#define BRIGHTNESS_MAX 1

struct color {
  double speed;
  double waveln;
  double brightness;
  double phase_us;
  uint32_t speed_changed_us; // saves micros() when speed is changed, 0 if no new change
} red, grn, blu;

Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_PIXELS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Map the linear value to an exponential scale
float mapToExponential(float x, float in_min, float in_max, float out_min, float out_max) {
  // Map the linear value to a normalized range [0, 1]
  float normalizedValue = (x - in_min) / (in_max - in_min);
  // Map the normalized value to the exponential scale
  float exponentialValue = out_min + (out_max - out_min) * pow(normalizedValue, 2);
  return exponentialValue;
}

// invert knob value so value increases when turned clockwise
double read_knob(int knob_pin) {
  return KNOB_MAX_VAL - analogRead(knob_pin);
}

// calculates speed based on a knob position.
// the sign of the speed indicates direction.
// returns a value that is between -1.0 and 1.0
double calc_speed(int knob_pin, double last_speed) {
  // shift knob value so range is +/- the middle value
  double val = read_knob(knob_pin) - KNOB_MID_VAL;

  // if val is within SPEED_ZERO_RANGE of 0, consider the value 0
  if (abs(val) < SPEED_ZERO_RANGE) val = 0;

  // convert to sepad
  val = (SPEED_MAX *(val / KNOB_MID_VAL));

  // if value hasn't changed by enough, keep speed the same
  if (abs(val - last_speed) < (SPEED_MAX * CHANGE_THRESHOLD)) return last_speed;

  return val;
}

// calculates wave length based on a knob position.
// returns a value between 0 and WAVELEN_MAX
double calc_waveln(int knob_pin, double last_waveln) {
  double val = WAVELN_MAX * (read_knob(knob_pin) / KNOB_MAX_VAL);
  if (val == 0) return val; // return 0, no further math needed

  // Map exponentially to get smaller wavelenghts easier
  val = mapToExponential(val, 0, WAVELN_MAX, 0, WAVELN_MAX);

  // make it easier to have a wavelength of 1
  if (val < 2) val = 1;

  // if value hasn't changed by enough, keep wavelength the same
  // if (abs(val - last_waveln) < (WAVELN_MAX * CHANGE_THRESHOLD)) return last_waveln;
  if (abs(val - last_waveln) < THRESHOLD_WAVELN) return last_waveln;

  return val;
}

// calculates the brightness based on a knob position.
// returns a value between 0.0 and 1.0
double calc_brightness(int knob_pin, double last_brightness) {
  double val = BRIGHTNESS_MAX * read_knob(knob_pin) / KNOB_MAX_VAL;
  if (val == 0) return val; // return 0, no further math needed

  // Map exponentially to match how our eyes perceive brightness
  val = mapToExponential(val, 0, BRIGHTNESS_MAX, 0, BRIGHTNESS_MAX);

  // if value hasn't changed by enough, keep brightness the same
  if (abs(val - last_brightness) < (BRIGHTNESS_MAX * CHANGE_THRESHOLD)) return last_brightness;

  return val;
}

// Updates color params based on knobs
void update_params() {
  double new_speed;

  new_speed = calc_speed(R1, red.speed);
  if (new_speed != red.speed) {
    red.speed_changed_us = micros();
  }
  red.speed = new_speed;

  new_speed = calc_speed(R2, grn.speed);
  if (new_speed != grn.speed) {
    grn.speed_changed_us = micros();
  }
  grn.speed = new_speed;

  new_speed = calc_speed(R3, blu.speed);
  if (new_speed != blu.speed) {
    blu.speed_changed_us = micros();
  }
  blu.speed = new_speed;

  red.waveln = calc_waveln(R4, red.waveln);
  grn.waveln = calc_waveln(R5, grn.waveln);
  blu.waveln = calc_waveln(R6, blu.waveln);

  red.brightness = calc_brightness(R7, red.brightness);
  grn.brightness = calc_brightness(R8, grn.brightness);
  blu.brightness = calc_brightness(R9, blu.brightness);
}

void debug_print_params() {
  Serial.println("color speed wave bright phase_us");
  Serial.printf("red %f %f %f %f\n", red.speed, red.waveln, red.brightness, red.phase_us);
  Serial.printf("grn %f %f %f %f\n", grn.speed, grn.waveln, grn.brightness, grn.phase_us);
  Serial.printf("blu %f %f %f %f\n", blu.speed, blu.waveln, blu.brightness, blu.phase_us);
}

void setup() {
  Serial.begin(115200);

  pinMode(R1, INPUT);
  pinMode(R2, INPUT);
  pinMode(R3, INPUT);
  pinMode(R4, INPUT);
  pinMode(R5, INPUT);
  pinMode(R6, INPUT);
  pinMode(R7, INPUT);
  pinMode(R8, INPUT);
  pinMode(R9, INPUT);

  pinMode(LED_PIN, OUTPUT);
  pixels.begin();
}

uint8_t calc_color(color* rgb, int i, uint32_t t) {
  if (rgb->waveln == 0) return 0;

  if (rgb->speed_changed_us) {
    rgb->phase_us += t - rgb->speed_changed_us;
    rgb->speed_changed_us = 0;
  }

  // time/phase debug
  //Serial.printf("t: %d\np: %f\n",t,rgb->phase_us);


  double pos = ((double) i / rgb->waveln) + (rgb->speed * (t - rgb->phase_us) / 1000000.0);
  pos = 0.5 * (1.0 + sin(2 * PI * pos));
  pos = pos * 255 * rgb->brightness;
  return pos;
}

void loop() {
  update_params();

  uint32_t t = micros();
  for (int i = 0; i < NUM_PIXELS; i++) {
    uint8_t r = calc_color(&red, i, t);
    uint8_t g = calc_color(&grn, i, t);
    uint8_t b = calc_color(&blu, i, t);
    pixels.setPixelColor(i, pixels.Color(r,g,b));
  }
  pixels.show();

  // DEBUG
  static uint32_t debug_timer = millis();
  if (millis() - debug_timer > 500) {
    debug_print_params();
    debug_timer = millis();
  }
}
