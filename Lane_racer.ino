/*
  LANE RACER - one-button racing game for a 128x64 SSD1306 OLED

  Board : ESP32
  OLED  : SSD1306 128x64 I2C  (SDA = GPIO 22, SCL = GPIO 21, address 0x3C)
  Button: GPIO 2 -> GND  (uses the internal pull-up, no resistor needed)

  Libraries (Library Manager):
    - Adafruit SSD1306
    - Adafruit GFX Library

  How to play:
    Each click moves your car to the next lane (1 -> 2 -> 3 -> 1 ...).
    Dodge the other cars. Every car you pass = 1 point. Speed rises with score.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- Hardware ----------
#define SCREEN_W   128
#define SCREEN_H   64
#define OLED_ADDR  0x3C
#define PIN_SDA    22
#define PIN_SCL    21
#define PIN_BTN    23

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);

// ---------- Road / car layout ----------
const int LANES    = 3;
const int LANE_W   = 20;
const int ROAD_X   = 8;                    // left edge of the road
const int ROAD_W   = LANES * LANE_W;       // 60 px wide -> road spans x = 8..68
const int CAR_W    = 12;
const int CAR_H    = 16;
const int PLAYER_Y = SCREEN_H - CAR_H - 2; // player sits near the bottom

// ---------- Game tuning (play with these!) ----------
const int           MAX_ENEMIES = 3;
const int           START_SPEED = 3;       // pixels per frame
const int           MAX_SPEED   = 6;
const int           SPAWN_CHANCE = 10;     // % chance per frame when there is room
const unsigned long FRAME_MS    = 30;      // frame time (~33 fps max)

// ---------- Game state ----------
enum GameState { STATE_TITLE, STATE_PLAY, STATE_OVER };
GameState state = STATE_TITLE;

struct Enemy {
  bool active;
  int  lane;
  int  y;
};
Enemy enemies[MAX_ENEMIES];

int  playerLane = 1;
int  score      = 0;
int  bestScore  = 0;
int  roadScroll = 0;

unsigned long lastFrame = 0;
unsigned long overAt    = 0;
bool pendingPress = false;

// =====================================================
//  Button (debounced, returns true once per press)
// =====================================================
bool readButtonPress() {
  static bool lastReading  = HIGH;
  static bool stableState  = HIGH;
  static unsigned long lastChange = 0;

  bool reading = digitalRead(PIN_BTN);
  if (reading != lastReading) {
    lastChange  = millis();
    lastReading = reading;
  }

  bool pressed = false;
  if ((millis() - lastChange) > 25 && reading != stableState) {
    stableState = reading;
    if (stableState == LOW) pressed = true;   // pressed = pulled to GND
  }
  return pressed;
}

// =====================================================
//  Helpers
// =====================================================
int laneX(int lane) {
  return ROAD_X + lane * LANE_W + (LANE_W - CAR_W) / 2;
}

int currentSpeed() {
  int s = START_SPEED + score / 8;
  return (s > MAX_SPEED) ? MAX_SPEED : s;
}

void drawCentered(const char* txt, int y, int size) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(txt, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_W - w) / 2, y);
  display.print(txt);
}

// =====================================================
//  Drawing
// =====================================================
void drawCar(int x, int y, bool player) {
  // wheels
  display.fillRect(x,             y + 2,  3, 4, WHITE);
  display.fillRect(x + CAR_W - 3, y + 2,  3, 4, WHITE);
  display.fillRect(x,             y + 10, 3, 4, WHITE);
  display.fillRect(x + CAR_W - 3, y + 10, 3, 4, WHITE);

  if (player) {
    // solid body with a "windshield" cut-out
    display.fillRoundRect(x + 2, y, CAR_W - 4, CAR_H, 2, WHITE);
    display.fillRect(x + 4, y + 4, CAR_W - 8, 4, BLACK);
  } else {
    // outlined body so enemies look different from you
    display.drawRoundRect(x + 2, y, CAR_W - 4, CAR_H, 2, WHITE);
    display.fillRect(x + 4, y + 6, CAR_W - 8, 4, WHITE);
  }
}

void drawRoad() {
  // road edges
  display.drawFastVLine(ROAD_X - 1,     0, SCREEN_H, WHITE);
  display.drawFastVLine(ROAD_X + ROAD_W, 0, SCREEN_H, WHITE);

  // scrolling dashed lane dividers
  int offset = roadScroll % 16;
  for (int i = 1; i < LANES; i++) {
    int x = ROAD_X + i * LANE_W;
    for (int y = -16 + offset; y < SCREEN_H; y += 16) {
      display.drawFastVLine(x, y, 8, WHITE);
    }
  }
}

void drawPanel() {
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(80, 4);
  display.print("SCORE");
  display.setTextSize(2);
  display.setCursor(80, 15);
  display.print(score);

  display.setTextSize(1);
  display.setCursor(80, 40);
  display.print("BEST");
  display.setCursor(80, 50);
  display.print(bestScore);
}

void drawGame() {
  display.clearDisplay();
  drawRoad();
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) drawCar(laneX(enemies[i].lane), enemies[i].y, false);
  }
  drawCar(laneX(playerLane), PLAYER_Y, true);
  drawPanel();
  display.display();
}

void drawTitle() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawCentered("LANE RACER", 8, 2);
  drawCar(laneX(1) + 40, 34, true);       // little decoration
  drawCentered("Press button", 54, 1);
  display.display();
}

void drawGameOver() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  drawCentered("GAME OVER", 4, 2);

  char buf[24];
  snprintf(buf, sizeof(buf), "Score: %d", score);
  drawCentered(buf, 28, 1);
  snprintf(buf, sizeof(buf), "Best:  %d", bestScore);
  drawCentered(buf, 40, 1);
  drawCentered("Press to retry", 54, 1);
  display.display();
}

// =====================================================
//  Game logic
// =====================================================
void startGame() {
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
  playerLane = 1;
  score      = 0;
  roadScroll = 0;
  state      = STATE_PLAY;
}

void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].active = true;
      enemies[i].lane   = random(LANES);
      enemies[i].y      = -CAR_H;
      return;
    }
  }
}

bool checkCrash() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    if (enemies[i].lane != playerLane) continue;
    // a little forgiveness (3 px) so it doesn't feel unfair
    if (enemies[i].y + CAR_H > PLAYER_Y + 3 &&
        enemies[i].y < PLAYER_Y + CAR_H - 3) {
      return true;
    }
  }
  return false;
}

void updateGame(bool press) {
  if (press) playerLane = (playerLane + 1) % LANES;

  int spd = currentSpeed();
  roadScroll += spd;

  // move enemies, score when they leave the bottom
  int topMostY = 1000;
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;
    enemies[i].y += spd;
    if (enemies[i].y > SCREEN_H) {
      enemies[i].active = false;
      score++;
    } else if (enemies[i].y < topMostY) {
      topMostY = enemies[i].y;
    }
  }

  // spawn a new car only when there is enough room after the last one,
  // so there is always time to dodge (gap grows with speed)
  if (topMostY > 28 + spd * 4 && random(100) < SPAWN_CHANCE) {
    spawnEnemy();
  }

  if (checkCrash()) {
    if (score > bestScore) bestScore = score;
    // quick flash on crash
    display.invertDisplay(true);
    delay(120);
    display.invertDisplay(false);
    overAt = millis();
    state  = STATE_OVER;
  }
}

// =====================================================
//  Arduino entry points
// =====================================================
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN, INPUT_PULLUP);

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("SSD1306 not found - check wiring / I2C address");
    for (;;) delay(1000);
  }

  randomSeed(esp_random());
  display.clearDisplay();
  display.display();
}

void loop() {
  // read the button every loop so quick taps are never missed
  if (readButtonPress()) pendingPress = true;

  // run the game at a fixed frame rate
  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  bool press = pendingPress;
  pendingPress = false;

  switch (state) {
    case STATE_TITLE:
      drawTitle();
      if (press) startGame();
      break;

    case STATE_PLAY:
      updateGame(press);
      if (state == STATE_PLAY) drawGame();
      break;

    case STATE_OVER:
      drawGameOver();
      // ignore presses for a moment so you don't restart by accident
      if (press && millis() - overAt > 700) startGame();
      break;
  }
}
