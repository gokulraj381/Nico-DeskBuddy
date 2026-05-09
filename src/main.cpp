// ==================================================
// NICO
// ==================================================

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "time.h"
#include <math.h>
#include <driver/i2s.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SDA_PIN 8
#define SCL_PIN 9
Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

const char* wifiSsid = "YourSSID";
const char* wifiPass = "YourPassword";
const char* ntpServer = "pool.ntp.org";
const char* tzString = "IST-5:30";

// --- EMOTION PARTICLES (16x16) ---
const unsigned char bmp_heart[] PROGMEM = { 0x00, 0x00, 0x0c, 0x60, 0x1e, 0xf0, 0x3f, 0xf8, 0x7f, 0xfc, 0x7f, 0xfc, 0x7f, 0xfc, 0x3f, 0xf8, 0x1f, 0xf0, 0x0f, 0xe0, 0x07, 0xc0, 0x03, 0x80, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_zzz[] PROGMEM = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x00, 0x0c, 0x00, 0x18, 0x00, 0x30, 0x00, 0x7e, 0x00, 0x00, 0x3c, 0x00, 0x0c, 0x00, 0x18, 0x00, 0x30, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x00, 0x00 };
const unsigned char bmp_anger[] PROGMEM = { 0x00, 0x00, 0x11, 0x10, 0x2a, 0x90, 0x44, 0x40, 0x80, 0x20, 0x80, 0x20, 0x44, 0x40, 0x2a, 0x90, 0x11, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// --- Touch Sensor ---
#define TOUCH_PIN 0  // TTP223 on GPIO0

// --- I2S Speaker Pins ---
#define I2S_BCLK 4
#define I2S_WS   3
#define I2S_DOUT 5

// --- WEB SERVER ---
WebServer server(80);

// --- ALARM ---
int alarmHour = 22;   // 24h format
int alarmMin  = 48;
bool alarmEnabled = true;
bool alarmTriggered = false;
bool alarmActive = false;
unsigned long alarmStart = 0;
const unsigned long ALARM_DURATION = 15000; // 15 sec buzzer

// --- REMINDERS ---
#define MAX_REMINDERS 5
struct Reminder {
  int hour, min;
  char text[32];
  bool active;
  bool triggered;
};
Reminder reminders[MAX_REMINDERS];
int reminderCount = 3;
bool showingReminder = false;
unsigned long reminderShowStart = 0;
int activeReminderIdx = 0;
const unsigned long REMINDER_SHOW_DURATION = 5000; // 5s per reminder
unsigned long lastReminderSwitch = 0;

// --- DINO GAME ---
bool dinoActive = false;
bool dinoGameOver = false;
unsigned long dinoGameOverTime = 0;
int dinoY = 0;           // dino vertical offset (0 = ground)
float dinoVel = 0;       // vertical velocity
bool dinoJumping = false;
int dinoScore = 0;
unsigned long dinoLastFrame = 0;
unsigned long dinoLastObstacle = 0;
bool lastTouchState = false;

#define MAX_OBSTACLES 3
struct Obstacle {
  float x;
  int w, h;
  bool active;
};
Obstacle obstacles[MAX_OBSTACLES];
float dinoSpeed = 2.0;

// Dino animation extras
float groundScrollX = 0;
#define NUM_CLOUDS 3
float cloudX[NUM_CLOUDS] = {30, 70, 110};
float cloudY[NUM_CLOUDS] = {5, 12, 8};
float cloudW[NUM_CLOUDS] = {16, 12, 14};
float cloudSpeed[NUM_CLOUDS] = {0.3, 0.5, 0.4};
bool dinoWasJumping = false;
int dustX = 0, dustFrame = 0;
bool dustActive = false;
#define NUM_STARS 8
int starX[NUM_STARS], starY[NUM_STARS];
bool nightMode = false;

// --- WORK MODE ---
bool workModeActive = false;
bool workBreakActive = false;
unsigned long workStartTime = 0;
unsigned long workDuration = 15UL * 60UL * 1000UL; // 15 minutes
unsigned long workBreakStart = 0;
const unsigned long WORK_BREAK_DURATION = 5000; // 5 sec break animation

// --- STATE ---
int currentPage = 0;  // 0=eyes, 1=clock, 2=alarm info, 3=reminders list
unsigned long lastPageSwitch = 0;
const unsigned long EYES_DURATION = 5000;    // 5s on eyes
const unsigned long CLOCK_DURATION = 5000;   // 5s on clock
const unsigned long ALARM_PAGE_DURATION = 5000;  // 5s alarm info
const unsigned long REM_PAGE_DURATION = 5000;    // 5s reminders list

// MOODS
#define MOOD_NORMAL 0
#define MOOD_HAPPY 1
#define MOOD_SURPRISED 2
#define MOOD_SLEEPY 3
#define MOOD_ANGRY 4
#define MOOD_SAD 5
#define MOOD_EXCITED 6
#define MOOD_LOVE 7
#define MOOD_SUSPICIOUS 8
int currentMood = MOOD_NORMAL;
unsigned long lastMoodChange = 0;
const unsigned long MOOD_INTERVAL = 2000;

// ==================================================
// PHYSICS ENGINE
// ==================================================

struct Eye {
  float x, y, w, h;
  float targetX, targetY, targetW, targetH;
  float pupilX, pupilY;
  float targetPupilX, targetPupilY;
  float velX, velY, velW, velH;
  float pVelX, pVelY;
  float k = 0.04;
  float d = 0.75;
  float pk = 0.03;
  float pd = 0.70;
  bool blinking;
  unsigned long lastBlink;
  unsigned long nextBlinkTime;

  void init(float _x, float _y, float _w, float _h) {
    x = targetX = _x;
    y = targetY = _y;
    w = targetW = _w;
    h = targetH = _h;
    pupilX = targetPupilX = 0;
    pupilY = targetPupilY = 0;
    nextBlinkTime = millis() + random(1000, 4000);
  }

  void update() {
    float ax = (targetX - x) * k;
    float ay = (targetY - y) * k;
    float aw = (targetW - w) * k;
    float ah = (targetH - h) * k;
    velX = (velX + ax) * d;
    velY = (velY + ay) * d;
    velW = (velW + aw) * d;
    velH = (velH + ah) * d;
    x += velX; y += velY; w += velW; h += velH;

    float pax = (targetPupilX - pupilX) * pk;
    float pay = (targetPupilY - pupilY) * pk;
    pVelX = (pVelX + pax) * pd;
    pVelY = (pVelY + pay) * pd;
    pupilX += pVelX; pupilY += pVelY;
  }
};

Eye leftEye, rightEye;
unsigned long lastSaccade = 0;
unsigned long saccadeInterval = 3000;
float breathVal = 0;



// ==================================================
// DRAWING
// ==================================================

void drawEyelidMask(float x, float y, float w, float h, int mood, bool isLeft) {
  int ix = (int)x; int iy = (int)y; int iw = (int)w; int ih = (int)h;

  if (mood == MOOD_ANGRY) {
    if (isLeft)
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SH110X_BLACK);
    else
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SH110X_BLACK);
  } else if (mood == MOOD_SAD) {
    if (isLeft)
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy - 6 + i, ix + iw, iy + i, SH110X_BLACK);
    else
      for (int i = 0; i < 16; i++) display.drawLine(ix, iy + i, ix + iw, iy - 6 + i, SH110X_BLACK);
  } else if (mood == MOOD_HAPPY || mood == MOOD_LOVE || mood == MOOD_EXCITED) {
    display.fillRect(ix, iy + ih - 12, iw, 14, SH110X_BLACK);
    display.fillCircle(ix + iw / 2, iy + ih + 6, iw / 1.3, SH110X_BLACK);
  } else if (mood == MOOD_SLEEPY) {
    display.fillRect(ix, iy, iw, ih / 2 + 2, SH110X_BLACK);
  } else if (mood == MOOD_SUSPICIOUS) {
    if (isLeft) display.fillRect(ix, iy, iw, ih / 2 - 2, SH110X_BLACK);
    else display.fillRect(ix, iy + ih - 8, iw, 8, SH110X_BLACK);
  }
}

void drawEye(Eye& e, bool isLeft) {
  int ix = (int)e.x; int iy = (int)e.y; int iw = (int)e.w; int ih = (int)e.h;
  int r = 8;
  if (iw < 20) r = 3;
  display.fillRoundRect(ix, iy, iw, ih, r, SH110X_WHITE);

  int cx = ix + iw / 2;
  int cy = iy + ih / 2;
  int pw = iw / 2.2;
  int ph = ih / 2.2;
  int px = cx + (int)e.pupilX - (pw / 2);
  int py = cy + (int)e.pupilY - (ph / 2);
  if (px < ix) px = ix;
  if (px + pw > ix + iw) px = ix + iw - pw;
  if (py < iy) py = iy;
  if (py + ph > iy + ih) py = iy + ih - ph;
  display.fillRoundRect(px, py, pw, ph, r / 2, SH110X_BLACK);

  if (iw > 15 && ih > 15) {
    display.fillCircle(px + pw - 4, py + 4, 2, SH110X_WHITE);
  }

  drawEyelidMask(e.x, e.y, e.w, e.h, currentMood, isLeft);
}

void updatePhysics() {
  unsigned long now = millis();
  breathVal = sin(now / 800.0) * 1.5;

  if (now > leftEye.nextBlinkTime) {
    leftEye.blinking = true;
    leftEye.lastBlink = now;
    rightEye.blinking = true;
    leftEye.nextBlinkTime = now + random(2000, 6000);
  }
  if (leftEye.blinking) {
    leftEye.targetH = 2;
    rightEye.targetH = 2;
    if (now - leftEye.lastBlink > 120) {
      leftEye.blinking = false;
      rightEye.blinking = false;
    }
  }

  // Left-top ↔ Right-bottom, 5s each (slow)
  if (!leftEye.blinking && now - lastSaccade > 5000) {
    lastSaccade = now;
    static bool atLeft = true;
    float lx = atLeft ? 8 : -8;
    float ly = atLeft ? 6 : -6;
    atLeft = !atLeft;

    leftEye.targetPupilX = lx; leftEye.targetPupilY = ly;
    rightEye.targetPupilX = lx; rightEye.targetPupilY = ly;
    leftEye.targetX = 18 + (lx * 0.3); leftEye.targetY = 14 + (ly * 0.3);
    rightEye.targetX = 74 + (lx * 0.3); rightEye.targetY = 14 + (ly * 0.3);
  }

  if (!leftEye.blinking) {
    float baseW = 36, baseH = 36;
    baseH += breathVal;
    switch (currentMood) {
      case MOOD_NORMAL:
        leftEye.targetW = baseW; leftEye.targetH = baseH;
        rightEye.targetW = baseW; rightEye.targetH = baseH;
        break;
      case MOOD_HAPPY: case MOOD_LOVE:
        leftEye.targetW = 40; leftEye.targetH = 32;
        rightEye.targetW = 40; rightEye.targetH = 32;
        break;
      case MOOD_SURPRISED:
        leftEye.targetW = 30; leftEye.targetH = 45;
        rightEye.targetW = 30; rightEye.targetH = 45;
        leftEye.targetPupilX += random(-1, 2);
        break;
      case MOOD_SLEEPY:
        leftEye.targetW = 38; leftEye.targetH = 30;
        rightEye.targetW = 38; rightEye.targetH = 30;
        break;
      case MOOD_ANGRY:
        leftEye.targetW = 34; leftEye.targetH = 32;
        rightEye.targetW = 34; rightEye.targetH = 32;
        break;
      case MOOD_SAD:
        leftEye.targetW = 34; leftEye.targetH = 40;
        rightEye.targetW = 34; rightEye.targetH = 40;
        break;
      case MOOD_EXCITED:
        leftEye.targetW = 40; leftEye.targetH = 32;
        rightEye.targetW = 40; rightEye.targetH = 32;
        break;
      case MOOD_SUSPICIOUS:
        leftEye.targetW = 36; leftEye.targetH = 20;
        rightEye.targetW = 36; rightEye.targetH = 42;
        break;
    }
  }

  leftEye.update();
  rightEye.update();
}

void drawEyes() {
  updatePhysics();
  if (currentMood == MOOD_LOVE) display.drawBitmap(56, 0, bmp_heart, 16, 16, SH110X_WHITE);
  else if (currentMood == MOOD_SLEEPY) display.drawBitmap(110, 0, bmp_zzz, 16, 16, SH110X_WHITE);
  else if (currentMood == MOOD_ANGRY) display.drawBitmap(56, 0, bmp_anger, 16, 16, SH110X_WHITE);
  drawEye(leftEye, true);
  drawEye(rightEye, false);
}

void drawClock() {
  struct tm t;
  if (!getLocalTime(&t)) {
    display.setFont(NULL);
    display.setCursor(30, 30);
    display.print("Syncing...");
    return;
  }
  int h12 = t.tm_hour % 12;
  if (h12 == 0) h12 = 12;
  String ampm = (t.tm_hour >= 12) ? "PM" : "AM";
  display.setTextColor(SH110X_WHITE);

  display.setFont(&FreeSansBold18pt7b);
  char hrStr[3], mnStr[3];
  sprintf(hrStr, "%02d", h12);
  sprintf(mnStr, "%02d", t.tm_min);

  display.setCursor(10, 38);
  display.print(hrStr);

  // Blinking colon
  if (t.tm_sec % 2 == 0) {
    display.fillCircle(60, 22, 3, SH110X_WHITE);
    display.fillCircle(60, 34, 3, SH110X_WHITE);
  }

  display.setCursor(70, 38);
  display.print(mnStr);

  // AM/PM
  display.setFont(NULL);
  display.setCursor(114, 2);
  display.print(ampm);

  // Seconds bar
  int barY = 44;
  display.drawRoundRect(10, barY, 108, 5, 2, SH110X_WHITE);
  int barW = (int)(t.tm_sec * 104.0 / 59.0);
  if (barW > 0) display.fillRoundRect(12, barY + 1, barW, 3, 1, SH110X_WHITE);

  // Date strip
  display.fillRect(0, 53, 128, 11, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  display.setFont(NULL);
  char dateStr[22];
  strftime(dateStr, 22, "%A, %b %d", &t);
  int dateW = strlen(dateStr) * 6;
  display.setCursor((128 - dateW) / 2, 55);
  display.print(dateStr);
  display.setTextColor(SH110X_WHITE);
}

// ==================================================
// DINO GAME
// ==================================================

#define GROUND_Y 54
#define DINO_W 10
#define DINO_H 12
#define DINO_X 15

void dinoReset() {
  dinoY = 0;
  dinoVel = 0;
  dinoJumping = false;
  dinoScore = 0;
  dinoSpeed = 2.0;
  dinoGameOver = false;
  dinoLastFrame = millis();
  dinoLastObstacle = millis();
  for (int i = 0; i < MAX_OBSTACLES; i++) obstacles[i].active = false;
  groundScrollX = 0;
  cloudX[0] = 30; cloudX[1] = 70; cloudX[2] = 110;
  dustActive = false;
  dustFrame = 0;
  nightMode = false;
  dinoWasJumping = false;
  for (int i = 0; i < NUM_STARS; i++) {
    starX[i] = random(0, 128);
    starY[i] = random(2, 30);
  }
}

void dinoStart() {
  dinoActive = true;
  dinoReset();
}

void dinoJump() {
  if (!dinoJumping && dinoY == 0) {
    dinoVel = -6.5;
    dinoJumping = true;
  }
}

void dinoUpdate() {
  unsigned long now = millis();
  if (now - dinoLastFrame < 30) return;
  dinoLastFrame = now;

  // Gravity
  dinoVel += 0.45;
  dinoY += (int)dinoVel;
  if (dinoY >= 0) {
    dinoY = 0;
    dinoVel = 0;
    dinoJumping = false;
  }

  // Spawn obstacles
  if (now - dinoLastObstacle > (unsigned long)(800 / (dinoSpeed / 2.0) + random(200, 600))) {
    for (int i = 0; i < MAX_OBSTACLES; i++) {
      if (!obstacles[i].active) {
        obstacles[i].x = 128;
        obstacles[i].w = random(4, 8);
        obstacles[i].h = random(8, 16);
        obstacles[i].active = true;
        dinoLastObstacle = now;
        break;
      }
    }
  }

  // Move obstacles
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      obstacles[i].x -= dinoSpeed;
      if (obstacles[i].x + obstacles[i].w < 0) {
        obstacles[i].active = false;
        dinoScore++;
      }
      // Collision check
      int dy = GROUND_Y - DINO_H + dinoY;
      int ox = (int)obstacles[i].x;
      int oy = GROUND_Y - obstacles[i].h;
      if (DINO_X + DINO_W > ox + 2 && DINO_X < ox + obstacles[i].w - 2 &&
          dy + DINO_H > oy + 2) {
        dinoGameOver = true;
        dinoGameOverTime = now;
      }
    }
  }

  // Speed up
  if (dinoScore > 0 && dinoScore % 5 == 0) {
    dinoSpeed = 2.0 + dinoScore * 0.15;
    if (dinoSpeed > 6.0) dinoSpeed = 6.0;
  }
}

void drawDino() {
  display.setFont(NULL);
  display.setTextSize(1);

  // Night mode after score 15
  nightMode = (dinoScore >= 15);

  // Background: stars in night mode
  if (nightMode) {
    for (int i = 0; i < NUM_STARS; i++) {
      // Twinkle: some blink on/off
      if ((millis() / 400 + i * 37) % 3 != 0)
        display.drawPixel(starX[i], starY[i], SH110X_WHITE);
    }
  }

  // Scrolling clouds (parallax)
  for (int i = 0; i < NUM_CLOUDS; i++) {
    cloudX[i] -= cloudSpeed[i] * (dinoSpeed / 2.0);
    if (cloudX[i] + cloudW[i] < 0) {
      cloudX[i] = 128 + random(0, 20);
      cloudY[i] = random(4, 18);
      cloudW[i] = random(10, 18);
    }
    int cx = (int)cloudX[i];
    int cy = (int)cloudY[i];
    int cw = (int)cloudW[i];
    // Cloud shape: rounded puffs
    display.fillRoundRect(cx, cy, cw, 4, 2, SH110X_WHITE);
    display.fillRoundRect(cx + 2, cy - 2, cw - 4, 3, 1, SH110X_WHITE);
  }

  // Ground line with scrolling texture
  display.drawLine(0, GROUND_Y + 1, 127, GROUND_Y + 1, SH110X_WHITE);
  groundScrollX -= dinoSpeed;
  if (groundScrollX < -8) groundScrollX += 8;
  // Ground bumps & pebbles
  for (int gx = (int)groundScrollX; gx < 128; gx += 8) {
    display.drawPixel(gx, GROUND_Y + 3, SH110X_WHITE);
    if ((gx / 8) % 3 == 0) {
      display.drawLine(gx + 1, GROUND_Y + 4, gx + 3, GROUND_Y + 4, SH110X_WHITE);
    }
    if ((gx / 8) % 5 == 0) {
      display.drawPixel(gx + 5, GROUND_Y + 5, SH110X_WHITE);
    }
  }

  // Score with high-score flash at milestones
  display.setCursor(85, 2);
  if (dinoScore > 0 && dinoScore % 10 == 0 && (millis() / 200) % 2 == 0) {
    // Flash score at milestones
  } else {
    display.print("S:");
    display.print(dinoScore);
  }

  // Dino body
  int dy = GROUND_Y - DINO_H + dinoY;
  display.fillRect(DINO_X, dy, DINO_W - 2, DINO_H, SH110X_WHITE);
  // Head
  display.fillRect(DINO_X + 2, dy - 4, DINO_W - 2, 5, SH110X_WHITE);
  // Mouth line
  display.drawLine(DINO_X + DINO_W - 1, dy - 1, DINO_X + DINO_W + 1, dy - 1, SH110X_WHITE);
  // Eye (blinks occasionally)
  if ((millis() / 80) % 30 != 0) {
    display.drawPixel(DINO_X + DINO_W - 2, dy - 2, SH110X_BLACK);
  }
  // Tail (wagging)
  int tailWag = ((millis() / 120) % 2 == 0) ? -1 : 0;
  display.drawLine(DINO_X - 1, dy + 3, DINO_X - 4, dy + 1 + tailWag, SH110X_WHITE);
  display.drawLine(DINO_X - 1, dy + 4, DINO_X - 4, dy + 2 + tailWag, SH110X_WHITE);

  // Arms (tiny, animated)
  int armAnim = ((millis() / 200) % 2 == 0) ? 0 : 1;
  display.drawPixel(DINO_X + DINO_W - 2, dy + 4 + armAnim, SH110X_WHITE);
  display.drawPixel(DINO_X + DINO_W - 1, dy + 5 + armAnim, SH110X_WHITE);

  // Legs (animate running)
  if (dinoY == 0) {
    int legPhase = (millis() / 100) % 4;
    switch (legPhase) {
      case 0:
        display.drawLine(DINO_X + 1, dy + DINO_H, DINO_X, dy + DINO_H + 3, SH110X_WHITE);
        display.drawLine(DINO_X + 5, dy + DINO_H, DINO_X + 6, dy + DINO_H + 3, SH110X_WHITE);
        break;
      case 1:
        display.drawLine(DINO_X + 2, dy + DINO_H, DINO_X + 2, dy + DINO_H + 3, SH110X_WHITE);
        display.drawLine(DINO_X + 6, dy + DINO_H, DINO_X + 5, dy + DINO_H + 3, SH110X_WHITE);
        break;
      case 2:
        display.drawLine(DINO_X + 3, dy + DINO_H, DINO_X + 4, dy + DINO_H + 3, SH110X_WHITE);
        display.drawLine(DINO_X + 6, dy + DINO_H, DINO_X + 6, dy + DINO_H + 3, SH110X_WHITE);
        break;
      case 3:
        display.drawLine(DINO_X + 2, dy + DINO_H, DINO_X + 1, dy + DINO_H + 3, SH110X_WHITE);
        display.drawLine(DINO_X + 5, dy + DINO_H, DINO_X + 5, dy + DINO_H + 3, SH110X_WHITE);
        break;
    }
  } else {
    // In air: legs tucked
    display.drawLine(DINO_X + 2, dy + DINO_H, DINO_X + 3, dy + DINO_H + 1, SH110X_WHITE);
    display.drawLine(DINO_X + 5, dy + DINO_H, DINO_X + 4, dy + DINO_H + 1, SH110X_WHITE);
  }

  // Landing dust particles
  if (dinoWasJumping && dinoY == 0 && !dinoJumping) {
    dustActive = true;
    dustFrame = 0;
    dustX = DINO_X;
  }
  dinoWasJumping = dinoJumping;
  if (dustActive) {
    dustFrame++;
    int dfy = GROUND_Y + 1;
    if (dustFrame < 3) {
      display.drawPixel(dustX - 2, dfy - 1, SH110X_WHITE);
      display.drawPixel(dustX + DINO_W + 1, dfy - 1, SH110X_WHITE);
      display.drawPixel(dustX - 1, dfy - 2, SH110X_WHITE);
      display.drawPixel(dustX + DINO_W + 2, dfy - 2, SH110X_WHITE);
    } else if (dustFrame < 6) {
      display.drawPixel(dustX - 3, dfy - 2, SH110X_WHITE);
      display.drawPixel(dustX + DINO_W + 3, dfy - 2, SH110X_WHITE);
      display.drawPixel(dustX - 4, dfy - 3, SH110X_WHITE);
      display.drawPixel(dustX + DINO_W + 4, dfy - 3, SH110X_WHITE);
    } else if (dustFrame < 9) {
      display.drawPixel(dustX - 5, dfy - 3, SH110X_WHITE);
      display.drawPixel(dustX + DINO_W + 5, dfy - 3, SH110X_WHITE);
    } else {
      dustActive = false;
    }
  }

  // Obstacles (cacti with arms)
  for (int i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      int ox = (int)obstacles[i].x;
      int oh = obstacles[i].h;
      int ow = obstacles[i].w;
      // Main trunk
      display.fillRect(ox, GROUND_Y - oh, ow, oh, SH110X_WHITE);
      // Cactus arms (on taller cacti)
      if (oh >= 10) {
        // Left arm
        display.fillRect(ox - 2, GROUND_Y - oh + 3, 2, 4, SH110X_WHITE);
        display.fillRect(ox - 2, GROUND_Y - oh + 3, 3, 1, SH110X_WHITE);
        // Right arm  
        display.fillRect(ox + ow, GROUND_Y - oh + 5, 2, 3, SH110X_WHITE);
        display.fillRect(ox + ow - 1, GROUND_Y - oh + 5, 3, 1, SH110X_WHITE);
      }
      // Top detail
      display.drawPixel(ox + ow / 2, GROUND_Y - oh - 1, SH110X_WHITE);
    }
  }

  // Game over overlay
  if (dinoGameOver) {
    display.fillRect(10, 14, 108, 46, SH110X_BLACK);
    display.drawRect(10, 14, 108, 46, SH110X_WHITE);
    // Flashing "GAME OVER" text
    if ((millis() / 500) % 2 == 0) {
      display.setCursor(30, 20);
      display.setTextSize(1);
      display.print("GAME OVER!");
    }
    display.setCursor(35, 35);
    display.print("Score: ");
    display.print(dinoScore);
    display.setCursor(22, 48);
    display.print("Touch to exit");
  }
}

// ==================================================
// WORK MODE
// ==================================================

void playBreakChime() {
  // Pleasant ascending chime: C5, E5, G5, C6
  static int chimeStep = 0;
  static unsigned long lastChime = 0;
  int freqs[] = {523, 659, 784, 1047, 784, 1047};
  int numNotes = 6;
  unsigned long now = millis();
  
  int noteIdx = ((now - workBreakStart) / 400) % numNotes;
  int freq = freqs[noteIdx];
  
  const int bufSize = 128;
  int16_t buf[bufSize];
  static unsigned long sampleCount = 0;
  for (int i = 0; i < bufSize; i++) {
    float s = (float)(sampleCount++) / 16000.0;
    // Soft sine with gentle envelope
    float env = 0.7 + 0.3 * sin(2.0 * M_PI * 3.0 * s);
    buf[i] = (int16_t)(8000 * env * sin(2.0 * M_PI * freq * s));
  }
  size_t written = 0;
  i2s_write(I2S_NUM_0, buf, bufSize * sizeof(int16_t), &written, 0);
}

void drawWorkCountdown() {
  unsigned long elapsed = millis() - workStartTime;
  unsigned long remaining = 0;
  if (elapsed < workDuration) remaining = workDuration - elapsed;
  
  int totalSec = remaining / 1000;
  int mins = totalSec / 60;
  int secs = totalSec % 60;
  float progress = (float)elapsed / (float)workDuration;
  if (progress > 1.0) progress = 1.0;
  
  unsigned long now = millis();

  // Animated dots after "WORK MODE" (cycling 1-3 dots)
  display.setFont(NULL);
  display.setTextSize(1);
  display.setCursor(38, 4);
  display.print("WORK MODE");
  int dots = (now / 500) % 4;
  for (int d = 0; d < dots; d++) display.print(".");

  // Big centered timer
  display.setTextSize(3);
  char timeBuf[6];
  sprintf(timeBuf, "%02d:%02d", mins, secs);
  display.setCursor(16, 20);
  display.print(timeBuf);

  // Blinking colon effect - redraw colon area black on alternate seconds
  if ((now / 500) % 2 == 0) {
    display.fillRect(58, 20, 12, 24, SH110X_BLACK);
  }

  // Progress bar at bottom
  display.drawRoundRect(10, 52, 108, 8, 3, SH110X_WHITE);
  int fillW = (int)(104 * progress);
  if (fillW > 0) display.fillRoundRect(12, 54, fillW, 4, 2, SH110X_WHITE);

  // Small ticking dot animation at end of progress bar
  if ((now / 1000) % 2 == 0) {
    int dotX = 12 + fillW + 2;
    if (dotX < 114) display.fillCircle(dotX, 56, 1, SH110X_WHITE);
  }
}

void drawBreakAnimation() {
  unsigned long now = millis();

  // Flash: alternate between normal and inverted every 800ms
  bool inverted = ((now / 800) % 2 == 0);
  if (inverted) {
    display.fillRect(0, 0, 128, 64, SH110X_WHITE);
  }
  int fg = inverted ? SH110X_BLACK : SH110X_WHITE;

  // Bouncing text offset
  int bounce = (int)(3.0 * sin(now / 250.0));

  // "TAKE A" centered big
  display.setFont(NULL);
  display.setTextSize(2);
  display.setTextColor(fg);
  display.setCursor(16, 16 + bounce);
  display.print("TAKE A");
  display.setCursor(16, 36 - bounce);
  display.print("BREAK!");

  // Blinking "Touch to dismiss" at bottom
  if ((now / 500) % 2 == 0) {
    display.setTextSize(1);
    display.setCursor(22, 56);
    display.print("Touch to dismiss");
  }

  // Reset text color to white for rest of code
  display.setTextColor(SH110X_WHITE);
}

// ==================================================
// WEB SERVER
// ==================================================

// Forward declarations
void stopAlarm();

const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Nico</title>
<style>
body{font-family:Arial;background:#1a1a2e;color:#eee;margin:0;padding:20px;}
h1{color:#e94560;text-align:center;}
.card{background:#16213e;border-radius:12px;padding:20px;margin:15px 0;}
label{display:block;margin:8px 0 4px;color:#aaa;font-size:14px;}
input[type=time],input[type=text]{width:100%;padding:10px;border:1px solid #444;
  border-radius:8px;background:#0f3460;color:#fff;font-size:16px;box-sizing:border-box;}
button{width:100%;padding:12px;border:none;border-radius:8px;font-size:16px;
  cursor:pointer;margin:8px 0;}
.btn-set{background:#e94560;color:#fff;}
.btn-off{background:#555;color:#fff;}
.btn-del{background:#c0392b;color:#fff;padding:8px;font-size:13px;width:auto;}
.status{text-align:center;color:#0f0;font-size:14px;margin:5px 0;}
.rem-item{display:flex;justify-content:space-between;align-items:center;
  background:#0f3460;padding:10px;border-radius:8px;margin:5px 0;}
.rem-time{color:#e94560;font-weight:bold;}
</style></head><body>
<h1>&#128059; Nico</h1>
<div class='card'>
<h2>&#9200; Alarm</h2>
<label>Time</label>
<input type='time' id='at'>
<button class='btn-set' onclick='setAlarm()'>Set Alarm</button>
<button class='btn-off' onclick='fetch("/alarm/off").then(()=>s.textContent="Alarm OFF")'>Turn Off</button>
<div class='status' id='s'></div>
</div>
<div class='card'>
<h2>&#128187; Work Mode</h2>
<button class='btn-set' onclick='startWork()'>Start 15 min Focus</button>
<button class='btn-off' onclick='fetch("/work/stop").then(()=>s.textContent="Work stopped")'>Stop Work Mode</button>
<div class='status' id='ws'></div>
</div>
<div class='card'>
<h2>&#128221; Reminders</h2>
<label>Time</label>
<input type='time' id='rt'>
<label>Message (max 30 chars)</label>
<input type='text' id='rm' maxlength='30' placeholder='Take a break!'>
<button class='btn-set' onclick='addRem()'>Add Reminder</button>
<div id='rlist'></div>
</div>
<script>
var s=document.getElementById('s');
function setAlarm(){
  var t=document.getElementById('at').value;
  if(!t)return;
  fetch('/alarm/set?t='+t).then(r=>r.text()).then(d=>s.textContent=d);
}
function startWork(){
  fetch('/work/start').then(r=>r.text()).then(d=>document.getElementById('ws').textContent=d);
}
function addRem(){
  var t=document.getElementById('rt').value;
  var m=document.getElementById('rm').value;
  if(!t||!m)return;
  fetch('/rem/add?t='+t+'&m='+encodeURIComponent(m)).then(r=>r.text()).then(()=>loadRem());
}
function delRem(i){fetch('/rem/del?i='+i).then(()=>loadRem());}
function loadRem(){
  fetch('/rem/list').then(r=>r.json()).then(d=>{
    var h='';
    d.forEach((r,i)=>{
      h+="<div class='rem-item'><span><span class='rem-time'>"+r.time+"</span> "+r.text+"</span>";
      h+="<button class='btn-del' onclick='delRem("+i+")'>X</button></div>";
    });
    document.getElementById('rlist').innerHTML=h;
  });
}
loadRem();
</script>
</body></html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", webpage);
}

void handleAlarmSet() {
  String t = server.arg("t");  // "HH:MM"
  if (t.length() >= 5) {
    alarmHour = t.substring(0, 2).toInt();
    alarmMin = t.substring(3, 5).toInt();
    alarmEnabled = true;
    alarmTriggered = false;
    int h12 = alarmHour % 12;
    if (h12 == 0) h12 = 12;
    String ampm = (alarmHour >= 12) ? "PM" : "AM";
    char buf[20];
    sprintf(buf, "Alarm set %d:%02d %s", h12, alarmMin, ampm.c_str());
    server.send(200, "text/plain", buf);
  } else {
    server.send(400, "text/plain", "Invalid time");
  }
}

void handleAlarmOff() {
  alarmEnabled = false;
  if (alarmActive) stopAlarm();
  server.send(200, "text/plain", "OK");
}

void handleRemAdd() {
  if (reminderCount >= MAX_REMINDERS) {
    server.send(400, "text/plain", "Max 5 reminders");
    return;
  }
  String t = server.arg("t");
  String m = server.arg("m");
  if (t.length() >= 5 && m.length() > 0) {
    Reminder &r = reminders[reminderCount];
    r.hour = t.substring(0, 2).toInt();
    r.min = t.substring(3, 5).toInt();
    m.toCharArray(r.text, 32);
    r.active = true;
    r.triggered = false;
    reminderCount++;
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Invalid");
  }
}

void handleRemDel() {
  int idx = server.arg("i").toInt();
  if (idx >= 0 && idx < reminderCount) {
    for (int i = idx; i < reminderCount - 1; i++) {
      reminders[i] = reminders[i + 1];
    }
    reminderCount--;
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Invalid index");
  }
}

void handleRemList() {
  String json = "[";
  for (int i = 0; i < reminderCount; i++) {
    if (i > 0) json += ",";
    int h12 = reminders[i].hour % 12;
    if (h12 == 0) h12 = 12;
    String ampm = (reminders[i].hour >= 12) ? "PM" : "AM";
    char timeBuf[10];
    sprintf(timeBuf, "%d:%02d %s", h12, reminders[i].min, ampm.c_str());
    json += "{\"time\":\"" + String(timeBuf) + "\",\"text\":\"" + String(reminders[i].text) + "\"}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleWorkStart() {
  workModeActive = true;
  workBreakActive = false;
  workStartTime = millis();
  server.send(200, "text/plain", "Work mode started! 15 min focus");
}

void handleWorkStop() {
  workModeActive = false;
  workBreakActive = false;
  i2s_zero_dma_buffer(I2S_NUM_0);
  server.send(200, "text/plain", "Work mode stopped");
}

void setupWebServer() {
  server.on("/", handleRoot);
  server.on("/alarm/set", handleAlarmSet);
  server.on("/alarm/off", handleAlarmOff);
  server.on("/rem/add", handleRemAdd);
  server.on("/rem/del", handleRemDel);
  server.on("/rem/list", handleRemList);
  server.on("/work/start", handleWorkStart);
  server.on("/work/stop", handleWorkStop);
  server.begin();
}

// ==================================================
// REMINDER DISPLAY
// ==================================================

void drawReminder(int idx) {
  unsigned long now = millis();
  int frame = (now / 150) % 6;  // animation frame

  // Flashing border
  if ((now / 400) % 2 == 0) {
    display.drawRect(0, 0, 128, 64, SH110X_WHITE);
    display.drawRect(1, 1, 126, 62, SH110X_WHITE);
  }

  // Bell icon animation (rocking bell)
  int bellX = 56;
  int bellY = 2;
  int rock = (frame < 3) ? (frame - 1) * 3 : (4 - frame) * 3;  // -3 to +3 swing

  // Bell body
  display.fillTriangle(bellX + 8 + rock, bellY, bellX + 2 + rock, bellY + 10, bellX + 14 + rock, bellY + 10, SH110X_WHITE);
  display.fillRect(bellX + 1 + rock, bellY + 8, 14, 4, SH110X_WHITE);
  // Bell clapper
  display.fillCircle(bellX + 8 + rock, bellY + 14, 2, SH110X_WHITE);
  // Bell top
  display.fillCircle(bellX + 8 + rock, bellY, 2, SH110X_WHITE);

  // Sound waves (alternating sides)
  if (frame % 2 == 0) {
    display.drawCircle(bellX - 4 + rock, bellY + 6, 3, SH110X_WHITE);
    display.drawCircle(bellX + 20 + rock, bellY + 6, 3, SH110X_WHITE);
  } else {
    display.drawCircle(bellX - 6 + rock, bellY + 6, 5, SH110X_WHITE);
    display.drawCircle(bellX + 22 + rock, bellY + 6, 5, SH110X_WHITE);
  }

  // "REMINDER" header with bounce
  display.setFont(NULL);
  display.setTextSize(1);
  int bounceY = 20 + ((frame % 3 == 0) ? -1 : (frame % 3 == 1) ? 1 : 0);
  display.setCursor(28, bounceY);
  display.print("* REMINDER *");

  // Time display
  int h12 = reminders[idx].hour % 12;
  if (h12 == 0) h12 = 12;
  String ampm = (reminders[idx].hour >= 12) ? "PM" : "AM";
  char timeBuf[12];
  sprintf(timeBuf, "%d:%02d %s", h12, reminders[idx].min, ampm.c_str());
  display.setCursor(40, 32);
  display.print(timeBuf);

  // Reminder text - larger
  display.setTextSize(2);
  int textW = strlen(reminders[idx].text) * 12;
  int textX = (128 - textW) / 2;
  if (textX < 0) textX = 0;
  display.setCursor(textX, 42);
  display.print(reminders[idx].text);
  display.setTextSize(1);
}

// ==================================================
// ALARM / REMINDER INFO PAGES
// ==================================================

void drawAlarmPage() {
  display.setFont(NULL);
  display.setTextSize(1);
  // Header
  display.fillRect(0, 0, 128, 12, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(35, 2);
  display.print("ALARM");
  display.setTextColor(SH110X_WHITE);

  if (!alarmEnabled) {
    display.setCursor(35, 30);
    display.print("-- OFF --");
  } else {
    int h12 = alarmHour % 12;
    if (h12 == 0) h12 = 12;
    String ampm = (alarmHour >= 12) ? "PM" : "AM";
    display.setFont(&FreeSansBold18pt7b);
    char buf[6];
    sprintf(buf, "%d:%02d", h12, alarmMin);
    display.setCursor(15, 42);
    display.print(buf);
    display.setFont(NULL);
    display.setCursor(105, 28);
    display.print(ampm);
    display.setCursor(40, 54);
    display.print("Enabled");
  }
}

void drawRemindersPage() {
  display.setFont(NULL);
  display.setTextSize(1);
  // Header
  display.fillRect(0, 0, 128, 12, SH110X_WHITE);
  display.setTextColor(SH110X_BLACK);
  display.setCursor(25, 2);
  display.print("REMINDERS");
  display.setTextColor(SH110X_WHITE);

  if (reminderCount == 0) {
    display.setCursor(30, 30);
    display.print("No reminders");
  } else {
    int y = 16;
    int maxShow = (reminderCount > 4) ? 4 : reminderCount;
    for (int i = 0; i < maxShow; i++) {
      int h12 = reminders[i].hour % 12;
      if (h12 == 0) h12 = 12;
      String ampm = (reminders[i].hour >= 12) ? "P" : "A";
      char line[40];
      sprintf(line, "%d:%02d%s %s", h12, reminders[i].min, ampm.c_str(), reminders[i].text);
      // Truncate to fit screen (21 chars)
      line[21] = '\0';
      display.setCursor(2, y);
      display.print(line);
      y += 12;
    }
    if (reminderCount > 4) {
      display.setCursor(2, y);
      display.print("+");
      display.print(reminderCount - 4);
      display.print(" more");
    }
  }
}

// ==================================================
// ALARM TONE
// ==================================================

void i2s_speaker_init() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pins = {
    .mck_io_num = I2S_PIN_NO_CHANGE,
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void playAlarmTone() {
  // Alternating 800Hz / 1200Hz beep pattern
  unsigned long t = millis() - alarmStart;
  int freq = ((t / 300) % 2 == 0) ? 800 : 1200;
  const int bufSize = 128;
  int16_t buf[bufSize];
  static unsigned long sampleCount = 0;
  for (int i = 0; i < bufSize; i++) {
    float s = (float)(sampleCount++) / 16000.0;
    buf[i] = (int16_t)(12000 * sin(2.0 * M_PI * freq * s));
  }
  size_t written = 0;
  i2s_write(I2S_NUM_0, buf, bufSize * sizeof(int16_t), &written, 0);
}

void stopAlarm() {
  alarmActive = false;
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void drawAlarm() {
  unsigned long now = millis();
  unsigned long elapsed = now - alarmStart;

  // Screen shake
  int shakeX = ((elapsed / 50) % 2 == 0) ? 2 : -2;
  int shakeY = ((elapsed / 70) % 2 == 0) ? 1 : -1;

  // Flashing inverted background
  bool flash = ((now / 300) % 2 == 0);
  if (flash) {
    display.fillRect(0, 0, 128, 64, SH110X_WHITE);
    display.setTextColor(SH110X_BLACK);
  }

  // Rocking bell icon (center top)
  int bellCX = 64 + shakeX;
  int bellCY = 6 + shakeY;
  int rock = (int)(4.0 * sin(now / 80.0)); // fast rocking

  // Bell dome
  display.fillTriangle(bellCX + rock, bellCY - 2,
                       bellCX - 8 + rock, bellCY + 10,
                       bellCX + 8 + rock, bellCY + 10, flash ? SH110X_BLACK : SH110X_WHITE);
  // Bell base
  display.fillRect(bellCX - 9 + rock, bellCY + 9, 18, 3, flash ? SH110X_BLACK : SH110X_WHITE);
  // Clapper
  display.fillCircle(bellCX + rock, bellCY + 14, 2, flash ? SH110X_BLACK : SH110X_WHITE);
  // Bell top knob
  display.fillCircle(bellCX + rock, bellCY - 2, 2, flash ? SH110X_BLACK : SH110X_WHITE);

  // Sound waves emanating from bell
  int wavePhase = (now / 150) % 3;
  int wc = flash ? SH110X_BLACK : SH110X_WHITE;
  if (wavePhase >= 1) {
    display.drawPixel(bellCX - 15 + rock, bellCY + 3, wc);
    display.drawLine(bellCX - 16 + rock, bellCY + 5, bellCX - 16 + rock, bellCY + 8, wc);
    display.drawPixel(bellCX - 15 + rock, bellCY + 10, wc);
    display.drawPixel(bellCX + 15 + rock, bellCY + 3, wc);
    display.drawLine(bellCX + 16 + rock, bellCY + 5, bellCX + 16 + rock, bellCY + 8, wc);
    display.drawPixel(bellCX + 15 + rock, bellCY + 10, wc);
  }
  if (wavePhase >= 2) {
    display.drawPixel(bellCX - 19 + rock, bellCY + 2, wc);
    display.drawLine(bellCX - 20 + rock, bellCY + 4, bellCX - 20 + rock, bellCY + 9, wc);
    display.drawPixel(bellCX - 19 + rock, bellCY + 11, wc);
    display.drawPixel(bellCX + 19 + rock, bellCY + 2, wc);
    display.drawLine(bellCX + 20 + rock, bellCY + 4, bellCX + 20 + rock, bellCY + 9, wc);
    display.drawPixel(bellCX + 19 + rock, bellCY + 11, wc);
  }

  // "ALARM!" big bouncing text
  display.setFont(NULL);
  display.setTextSize(2);
  int bounce = (int)(2.0 * sin(now / 150.0));
  int textX = 20 + shakeX;
  display.setCursor(textX, 22 + bounce + shakeY);
  display.print("ALARM!");

  // Time display
  display.setTextSize(1);
  int h12 = alarmHour % 12;
  if (h12 == 0) h12 = 12;
  String ampm = (alarmHour >= 12) ? "PM" : "AM";
  char buf[12];
  sprintf(buf, "%d:%02d %s", h12, alarmMin, ampm.c_str());
  int tw = strlen(buf) * 6;
  display.setCursor((128 - tw) / 2 + shakeX, 42 + shakeY);
  display.print(buf);

  // Countdown bar at bottom
  unsigned long remaining = 0;
  if (elapsed < ALARM_DURATION) remaining = ALARM_DURATION - elapsed;
  int barW = (int)(108.0 * remaining / ALARM_DURATION);
  display.drawRect(10 + shakeX, 54 + shakeY, 108, 6, flash ? SH110X_BLACK : SH110X_WHITE);
  if (barW > 0) display.fillRect(11 + shakeX, 55 + shakeY, barW, 4, flash ? SH110X_BLACK : SH110X_WHITE);

  // Reset text color
  display.setTextColor(SH110X_WHITE);
}

// ==================================================
// SETUP & LOOP
// ==

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  display.begin(0x3C, true);
  display.clearDisplay();
  display.display();
  delay(100);
  display.setTextColor(SH110X_WHITE);
  leftEye.init(18, 14, 36, 36);
  rightEye.init(74, 14, 36, 36);

  // Show connecting status on OLED
  display.clearDisplay();
  display.setFont(NULL);
  display.setCursor(10, 10);
  display.print("Connecting WiFi...");
  display.setCursor(10, 30);
  display.print("SSID: ");
  display.print(wifiSsid);
  display.display();

  // Connect WiFi for clock (try twice)
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);  // Lower power for stability
  delay(100);
  WiFi.begin(wifiSsid, wifiPass);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart < 15000)) {
    delay(300);
  }
  // Retry once if failed
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.disconnect();
    delay(500);
    WiFi.begin(wifiSsid, wifiPass);
    wifiStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiStart < 10000)) {
      delay(300);
    }
  }
  if (WiFi.status() == WL_CONNECTED) {
    configTime(0, 0, ntpServer);
    setenv("TZ", tzString, 1);
    tzset();

    // Wait for NTP sync
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print("Syncing time...");
    display.display();
    struct tm t;
    int retry = 0;
    while (!getLocalTime(&t) && retry < 10) {
      delay(500);
      retry++;
    }
    // Start web server
    setupWebServer();

    // Show IP on OLED
    display.clearDisplay();
    display.setCursor(10, 10);
    display.print("WiFi OK!");
    display.setCursor(10, 30);
    display.print(WiFi.localIP());
    display.display();
    delay(3000);
  } else {
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print("WiFi failed!");
    display.display();
    delay(2000);
  }
  // Init speaker for alarm
  i2s_speaker_init();

  // Init touch sensor
  pinMode(TOUCH_PIN, INPUT);

  lastMoodChange = millis();

  // Init 3 reminders
  strcpy(reminders[0].text, "Laundry");
  reminders[0].hour = 8; reminders[0].min = 0; reminders[0].active = true;

  strcpy(reminders[1].text, "Gym");
  reminders[1].hour = 16; reminders[1].min = 0; reminders[1].active = true;

  strcpy(reminders[2].text, "Haircut");
  reminders[2].hour = 17; reminders[2].min = 0; reminders[2].active = true;

  lastReminderSwitch = millis();
}

void loop() {
  unsigned long now = millis();

  // Handle web server requests
  server.handleClient();

  // Check alarm trigger
  if (alarmEnabled && !alarmTriggered) {
    struct tm t;
    if (getLocalTime(&t)) {
      if (t.tm_hour == alarmHour && t.tm_min == alarmMin) {
        alarmTriggered = true;
        alarmActive = true;
        alarmStart = now;
      }
    }
  }
  // Reset trigger when minute passes
  if (alarmTriggered && !alarmActive) {
    struct tm t;
    if (getLocalTime(&t) && t.tm_min != alarmMin) {
      alarmTriggered = false;
    }
  }

  // Alarm active - play sound and show alarm screen
  if (alarmActive) {
    if (now - alarmStart < ALARM_DURATION) {
      playAlarmTone();
      display.clearDisplay();
      drawAlarm();
      display.display();
      return;
    } else {
      stopAlarm();
    }
  }

  // Check reminders
  if (!showingReminder) {
    struct tm t;
    if (getLocalTime(&t)) {
      for (int i = 0; i < reminderCount; i++) {
        if (reminders[i].active && !reminders[i].triggered &&
            t.tm_hour == reminders[i].hour && t.tm_min == reminders[i].min) {
          reminders[i].triggered = true;
          showingReminder = true;
          reminderShowStart = now;
          activeReminderIdx = i;
          break;
        }
        // Reset trigger when minute passes
        if (reminders[i].triggered && t.tm_min != reminders[i].min) {
          reminders[i].triggered = false;
        }
      }
    }
  }

  // Show reminder on screen
  if (showingReminder) {
    if (now - reminderShowStart < REMINDER_SHOW_DURATION) {
      display.clearDisplay();
      drawReminder(activeReminderIdx);
      display.display();
      return;
    } else {
      showingReminder = false;
      activeReminderIdx = -1;
    }
  }

  // Read touch sensor
  bool touchState = digitalRead(TOUCH_PIN);
  bool touchPressed = (touchState && !lastTouchState);  // Rising edge
  lastTouchState = touchState;

  // Work mode active
  if (workModeActive) {
    unsigned long elapsed = millis() - workStartTime;
    if (elapsed >= workDuration && !workBreakActive) {
      // Timer done — start break animation
      workBreakActive = true;
      workBreakStart = now;
    }
    if (workBreakActive) {
      if (now - workBreakStart < WORK_BREAK_DURATION) {
        playBreakChime();
        display.clearDisplay();
        drawBreakAnimation();
        display.display();
        // Touch to dismiss early
        if (touchPressed) {
          workModeActive = false;
          workBreakActive = false;
          i2s_zero_dma_buffer(I2S_NUM_0);
        }
        return;
      } else {
        workModeActive = false;
        workBreakActive = false;
        i2s_zero_dma_buffer(I2S_NUM_0);
      }
    } else {
      // Show countdown
      display.clearDisplay();
      drawWorkCountdown();
      display.display();
      return;
    }
  }

  // Dino game active
  if (dinoActive) {
    if (dinoGameOver) {
      // Touch to exit game over screen
      if (touchPressed && (now - dinoGameOverTime > 500)) {
        dinoActive = false;
        lastPageSwitch = now;
      }
    } else {
      if (touchPressed) dinoJump();
      dinoUpdate();
    }
    display.clearDisplay();
    drawDino();
    display.display();
    return;
  }

  // Touch to start dino game
  if (touchPressed && !dinoActive) {
    dinoStart();
    return;
  }

  // Default: cycle pages (eyes → clock → alarm → reminders)
  unsigned long pageDurations[] = {EYES_DURATION, CLOCK_DURATION, ALARM_PAGE_DURATION, REM_PAGE_DURATION};
  if (now - lastPageSwitch >= pageDurations[currentPage]) {
    currentPage = (currentPage + 1) % 4;
    lastPageSwitch = now;
  }

  display.clearDisplay();
  switch (currentPage) {
    case 0: // Eyes
      currentMood = MOOD_NORMAL;
      updatePhysics();
      drawEye(leftEye, true);
      drawEye(rightEye, false);
      break;
    case 1: // Clock
      drawClock();
      break;
    case 2: // Alarm info
      drawAlarmPage();
      break;
    case 3: // Reminders list
      drawRemindersPage();
      break;
  }
  display.display();
}
