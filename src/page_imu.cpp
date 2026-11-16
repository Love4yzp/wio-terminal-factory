#include "page.h"
#include "ui.h"
#include "tests.h"
#include "test_config.h"
#include <math.h>

const char *PageImu::title() const { return "IMU"; }
const char *PageImu::hint() const { return "A/NavPress reset   tilt board   L/R pages"; }

static const int CX = 86, CY = 124, R = 62;
static float s_sx = 0, s_sy = 0;
static int s_prevBx = CX, s_prevBy = CY;
static uint32_t s_lastFrame = 0, s_lastRetry = 0;

static void drawCircleFrame() {
  tft.drawCircle(CX, CY, R, C::dim);
  tft.drawCircle(CX, CY, R - 1, C::dim);
  tft.drawFastHLine(CX - R, CY, R * 2, C::panel);
  tft.drawFastVLine(CX, CY - R, R * 2, C::panel);
}

static void drawBars(float ax, float ay, float az) {
  const int x0 = 188, w = 80, h = 12;
  const int y0 = 52, gap = 18;
  const char *lbl[3] = {"X", "Y", "Z"};
  float val[3] = {ax, ay, az};
  uint16_t col[3] = {C::err, C::ok, C::accent};

  for (int i = 0; i < 3; i++) {
    int y = y0 + i * gap;
    tft.fillRect(x0, y, w, h, C::panel);
    float v = val[i];
    if (v < -2.0f) v = -2.0f;
    if (v > 2.0f) v = 2.0f;
    int mid = x0 + w / 2;
    int len = (int)(v * (w / 2) / 2.0f);
    if (len >= 0) tft.fillRect(mid, y + 2, len, h - 4, col[i]);
    else tft.fillRect(mid + len, y + 2, -len, h - 4, col[i]);
    tft.drawFastVLine(mid, y, h, C::text);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C::dim, C::bg);
    tft.setTextSize(1);
    tft.drawString(lbl[i], 176, y + h / 2);

    char buf[10];
    snprintf(buf, sizeof(buf), "%+.2f", v);
    tft.fillRect(274, y, 38, h, C::bg);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C::text, C::bg);
    tft.drawString(buf, 310, y + h / 2);
  }
}

static void drawInfoRay(float ax, float ay, float az, bool online) {
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  char buf[24];
  tft.fillRect(172, 140, 142, 26, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  tft.drawString("|a|", 174, 156);
  tft.setTextColor(C::text, C::bg);
  tft.setTextSize(2);
  snprintf(buf, sizeof(buf), "%.2fg", mag);
  tft.drawString(buf, 198, 152);

  tft.fillRect(172, 168, 142, 18, C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(online ? C::ok : C::err, C::bg);
  tft.setTextSize(1);
  snprintf(buf, sizeof(buf), "%s", online ? "LIS3DHTR online" : "offline: A re-init");
  tft.drawString(buf, 174, 176);
}

static void drawBubble(float ax, float ay) {
  s_sx = s_sx * 0.7f + ax * 0.3f;
  s_sy = s_sy * 0.7f + ay * 0.3f;

  int bx = CX - (int)(s_sx * (R - 12));
  int by = CY + (int)(s_sy * (R - 12));
  int dx = bx - CX, dy = by - CY;
  int dist = (int)sqrtf(dx * dx + dy * dy);
  if (dist > R - 12) {
    float k = (float)(R - 12) / dist;
    bx = CX + (int)(dx * k);
    by = CY + (int)(dy * k);
  }

  if (bx != s_prevBx || by != s_prevBy) {
    tft.fillCircle(s_prevBx, s_prevBy, 8, C::bg);
    drawCircleFrame();
    s_prevBx = bx;
    s_prevBy = by;
  }
  tft.fillCircle(bx, by, 7, C::accent2);
  tft.drawCircle(bx, by, 7, C::text);
}

void PageImu::enter() {
  imuBegin(false);
  s_sx = s_sy = 0;
  s_prevBx = CX;
  s_prevBy = CY;
  s_lastFrame = 0;
  s_lastRetry = 0;

  tft.fillRect(0, bodyTop(), 320, bodyBottom() - bodyTop(), C::bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(C::dim, C::bg);
  tft.setTextSize(1);
  tft.drawString("BUBBLE LEVEL", 10, 36);
  tft.drawString("ACCEL  +/-2g", 174, 36);
  tft.drawString("LIS3DHTR @ Wire1", 174, 192);
  drawCircleFrame();
  drawBars(0, 0, 1);
  drawInfoRay(0, 0, 1, imuOnline());
}

void PageImu::update() {
  uint32_t now = millis();
  if (!imuOnline() && now - s_lastRetry > 1000) {
    s_lastRetry = now;
    imuBegin(false);
  }
  if (now - s_lastFrame < IMU_FRAME_MS) return;
  s_lastFrame = now;

  float ax = 0, ay = 0, az = 1;
  imuRead(ax, ay, az);
  drawBubble(ax, ay);
  drawBars(ax, ay, az);
  drawInfoRay(ax, ay, az, imuOnline());
}

void PageImu::onInput(InputEvent e) {
  if (e == InputEvent::KeyA || e == InputEvent::NavPress) {
    imuBegin(true);
    s_sx = s_sy = 0;
    _redraw = true;
  }
}
