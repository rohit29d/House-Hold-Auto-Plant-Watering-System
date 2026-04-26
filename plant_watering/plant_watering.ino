// ============================================================
//  Automatic Plant Watering System  — v3
//  Hardware : Arduino Uno R3
//  Valve    : 24V solenoid via 5V relay on pin 2
//  Sensor   : Solar panel voltage divider on A0 (daylight only)
//  Button   : Manual override on pin 5 (INPUT_PULLUP, active LOW)
//
//  Day detection — HYBRID mode:
//    PRIMARY  : Solar sensor sunrise/sunset (normal sunny days)
//    FALLBACK : If stuck in NIGHT for 24h AND solar is clearly
//               dark → force a virtual DAY so watering still runs
//               on fully cloudy days. Repeats every 24h until
//               real sunrise is detected.
//
//  v3 fixes:
//    • Fallback now guarded by (dayState == NIGHT) — never
//      interrupts an active solar or virtual day mid-schedule
//    • Fallback also guarded by (solarVal < THRESH_DAY) — won't
//      force a virtual day if light is borderline (sensor in
//      hysteresis band); just re-arms the clock and waits for
//      solar detection to fire naturally
// ============================================================

// ── Pin definitions ─────────────────────────────────────────
#define PIN_VALVE   2
#define PIN_BUTTON  5
#define PIN_SOLAR   A0

// ── Relay polarity ──────────────────────────────────────────
// 1 = active-HIGH (most common blue relay boards)
// 0 = active-LOW  (opto-isolated boards)
#define RELAY_ACTIVE_HIGH 0

#if RELAY_ACTIVE_HIGH
  #define VALVE_ON  HIGH
  #define VALVE_OFF LOW
#else
  #define VALVE_ON  LOW
  #define VALVE_OFF HIGH
#endif

// ── TEST_MODE ────────────────────────────────────────────────
// 1 = bench test  (full cycle visible in ~75 seconds)
// 0 = production  (real-world durations)
#define TEST_MODE 0

#if TEST_MODE
  const unsigned long TON_MS          =   5000UL;  //  5 s   (prod: 20 min)
  const unsigned long TOFF_MS         =  10000UL;  // 10 s   (prod: 1 hr)
  const unsigned long MIN_DAY_MS      =  10000UL;  // 10 s   (prod: 10 min)
  const unsigned long MAX_VALVE_ON_MS =   8000UL;  //  8 s   (prod: 15 min)
  const unsigned long FALLBACK_MS     =  60000UL;  //  1 min (prod: 24 hr)
  const unsigned long CONFIRM_MS      =   2000UL;  //  2 s   (prod: 5 s)
#else
  const unsigned long TON_MS          = 300000UL; // 5 minutes
  const unsigned long TOFF_MS         = 25200000UL; //  7 hours
  const unsigned long MIN_DAY_MS      =  600000UL; // 10 minutes
  const unsigned long MAX_VALVE_ON_MS =  900000UL; // 15 minutes
  const unsigned long FALLBACK_MS     = 86400000UL;// 24 hours
  const unsigned long CONFIRM_MS      =   5000UL;  //  5 seconds
#endif

// ── Solar thresholds ─────────────────────────────────────────
// Hysteresis band:
//   solarVal >= THRESH_DAY   → candidate sunrise (must hold CONFIRM_MS)
//   solarVal <= THRESH_NIGHT → candidate sunset  (must hold CONFIRM_MS)
//   THRESH_NIGHT < val < THRESH_DAY → ambiguous band, no transition
const int THRESH_DAY   = 400;
const int THRESH_NIGHT = 200;

// ── Schedule boundaries (compile-time) ──────────────────────
// Timeline origin = end of MIN_DAY_MS warmup window:
//   Session 1 : [0,          S1_END)
//   Gap       : [S1_END,     GAP_END)
//   Session 2 : [GAP_END,    S2_END)
//   Idle      : [S2_END,     next sunrise)
const unsigned long SCHED_S1_END  = TON_MS;
const unsigned long SCHED_GAP_END = TON_MS + TOFF_MS;
const unsigned long SCHED_S2_END  = TON_MS + TOFF_MS + TON_MS;

// ── State machines ───────────────────────────────────────────
enum DayState   { NIGHT, DAY };
enum WaterPhase {
  PHASE_WARMUP,    // DAY active, MIN_DAY_MS not yet elapsed
  PHASE_SESSION1,  // First watering window  — valve ON
  PHASE_GAP,       // Pause between sessions — valve OFF
  PHASE_SESSION2,  // Second watering window — valve ON
  PHASE_IDLE,      // Both sessions done for today
  PHASE_NIGHT      // dayState == NIGHT
};

DayState   dayState   = NIGHT;
WaterPhase waterPhase = PHASE_NIGHT;

// virtualDay = true  → running on fallback timer (no real sun seen)
// virtualDay = false → running on confirmed solar detection
bool virtualDay = false;

// ── Timing bookmarks ─────────────────────────────────────────
unsigned long lastDayStart      = 0;
unsigned long lastFallbackReset = 0;
unsigned long valveOnSince      = 0;

// Hysteresis confirmation tracking
unsigned long confirmStart      = 0;
bool          pendingTransition = false;
DayState      pendingState      = NIGHT;

bool valveIsOn = false;

// ── Forward declarations ─────────────────────────────────────
void       setValve(bool turnOn, unsigned long now);
void       updatePhase(WaterPhase newPhase);
WaterPhase computePhase(unsigned long now);
void       printSolarDebug(int solarVal, unsigned long now);

// ============================================================
void setup() {
  Serial.begin(9600);
  pinMode(PIN_VALVE,  OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  setValve(true, millis());
  delay(100);
  setValve(false, millis());  
  lastFallbackReset = millis();

  Serial.println(F("=== Plant Watering System v3 — Hybrid ==="));
  Serial.print(F("Mode       : ")); Serial.println(TEST_MODE ? F("TEST") : F("PRODUCTION"));
  Serial.print(F("Relay      : ")); Serial.println(RELAY_ACTIVE_HIGH ? F("Active HIGH") : F("Active LOW"));
  Serial.print(F("TON  (ms)  : ")); Serial.println(TON_MS);
  Serial.print(F("TOFF (ms)  : ")); Serial.println(TOFF_MS);
  Serial.print(F("MIN_DAY(ms): ")); Serial.println(MIN_DAY_MS);
  Serial.print(F("MAX_ON (ms): ")); Serial.println(MAX_VALVE_ON_MS);
  Serial.print(F("FALLBACK ms: ")); Serial.println(FALLBACK_MS);
  Serial.print(F("CONFIRM ms : ")); Serial.println(CONFIRM_MS);
  Serial.println(F("========================================="));
}

// ============================================================
void loop() {
  unsigned long now      = millis();
  int           solarVal = analogRead(PIN_SOLAR);

  printSolarDebug(solarVal, now);

  // ── 1. Solar-based day/night detection (PRIMARY) ─────────
  // Always runs — even during a virtual day, so real sunlight
  // can reclaim control at any time without waiting for fallback.
  DayState solarSuggest = dayState;
  if (dayState == NIGHT && solarVal >= THRESH_DAY)   solarSuggest = DAY;
  if (dayState == DAY   && solarVal <= THRESH_NIGHT) solarSuggest = NIGHT;

  if (solarSuggest != dayState) {
    if (!pendingTransition || pendingState != solarSuggest) {
      pendingTransition = true;
      pendingState      = solarSuggest;
      confirmStart      = now;
    } else if (now - confirmStart >= CONFIRM_MS) {
      dayState          = pendingState;
      pendingTransition = false;

      if (dayState == DAY) {
        lastDayStart      = now;
        lastFallbackReset = now;  // Real sunrise → reset fallback clock
        virtualDay        = false;
        Serial.print(F("[DAY]   Sunrise confirmed (solar) | solar="));
        Serial.println(solarVal);
      } else {
        if (virtualDay) {
          // Virtual day ended via solar (sun dipped below THRESH_NIGHT
          // even on a cloudy day — valid transition, keep virtualDay
          // true so next fallback fires correctly).
          Serial.println(F("[NIGHT] Virtual day ended — awaiting next fallback or sunrise"));
        } else {
          Serial.print(F("[NIGHT] Sunset confirmed (solar) | solar="));
          Serial.println(solarVal);
        }
      }
    }
  } else {
    pendingTransition = false;
  }

  // ── 2. Hybrid 24-hour fallback (SECONDARY) ───────────────
  //
  // [FIX 1] Guard: dayState == NIGHT
  //   Fallback only fires when the system is genuinely stuck in
  //   NIGHT. It can never interrupt an active solar or virtual
  //   day, so no mid-schedule restart is possible.
  //
  // [FIX 2] Guard: solarVal < THRESH_DAY
  //   If light is in the ambiguous hysteresis band (THRESH_NIGHT
  //   < val < THRESH_DAY), the sensor is seeing partial sun.
  //   Don't force a virtual day — just re-arm the fallback clock
  //   and give solar detection more time to confirm naturally.
  //   A genuinely dark cloudy day will always read well below
  //   THRESH_NIGHT, so this guard never blocks real fallback need.
  //
  if (dayState == NIGHT && (now - lastFallbackReset >= FALLBACK_MS)) {
    lastFallbackReset = now;  // Always re-arm the clock

    if (solarVal < THRESH_DAY) {
      // Clearly dark → force virtual day
      lastDayStart      = now;
      dayState          = DAY;
      virtualDay        = true;
      pendingTransition = false;  // Cancel any in-progress confirmation
      Serial.println(F("[FALLBACK] No sunrise detected — VIRTUAL DAY started"));
    } else {
      // Borderline light → solar detection is close; just wait
      Serial.print(F("[FALLBACK] Light borderline, re-arming clock | solar="));
      Serial.println(solarVal);
    }
  }

  // ── 3. Valve safety timeout ───────────────────────────────
  // Unconditional hard ceiling — no path bypasses this.
  if (valveIsOn && (now - valveOnSince >= MAX_VALVE_ON_MS)) {
    setValve(false, now);
    Serial.println(F("[SAFETY] Max ON time exceeded — valve forced OFF"));
  }

  // ── 4. Manual override ────────────────────────────────────
  // Held button forces valve ON. Returns early — schedule and
  // phase state are untouched while button is pressed.
  if (digitalRead(PIN_BUTTON) == LOW) {
    setValve(true, now);
    return;
  }

  // ── 5. Compute phase and drive valve ─────────────────────
  updatePhase(computePhase(now));

  switch (waterPhase) {
    case PHASE_SESSION1:
    case PHASE_SESSION2:
      setValve(true, now);
      break;
    default:
      setValve(false, now);
      break;
  }
}

// ============================================================
//  computePhase() — pure, no side effects
// ============================================================
WaterPhase computePhase(unsigned long now) {
  if (dayState != DAY) return PHASE_NIGHT;

  unsigned long elapsed = now - lastDayStart;
  if (elapsed < MIN_DAY_MS) return PHASE_WARMUP;

  unsigned long schedElapsed = elapsed - MIN_DAY_MS;
  if (schedElapsed < SCHED_S1_END)  return PHASE_SESSION1;
  if (schedElapsed < SCHED_GAP_END) return PHASE_GAP;
  if (schedElapsed < SCHED_S2_END)  return PHASE_SESSION2;
  return PHASE_IDLE;
}

// ============================================================
//  updatePhase() — one Serial line per transition only
// ============================================================
void updatePhase(WaterPhase newPhase) {
  if (newPhase == waterPhase) return;
  waterPhase = newPhase;

  Serial.print(virtualDay ? F("[PHASE/VIRTUAL] ") : F("[PHASE/SOLAR]   "));
  switch (waterPhase) {
    case PHASE_WARMUP:   Serial.println(F("Warmup — waiting for stable daylight")); break;
    case PHASE_SESSION1: Serial.println(F("Session 1 ON"));                         break;
    case PHASE_GAP:      Serial.println(F("Gap — valve OFF between sessions"));     break;
    case PHASE_SESSION2: Serial.println(F("Session 2 ON"));                         break;
    case PHASE_IDLE:     Serial.println(F("Idle — schedule complete for today"));   break;
    case PHASE_NIGHT:    Serial.println(F("Night — valve OFF"));                    break;
  }
}

// ============================================================
//  setValve() — tracks ON duration, prints on state change only
// ============================================================
void setValve(bool turnOn, unsigned long now) {
  if (turnOn == valveIsOn) return;

  valveIsOn = turnOn;
  digitalWrite(PIN_VALVE, turnOn ? VALVE_ON : VALVE_OFF);

  if (turnOn) {
    valveOnSince = now;
    Serial.println(F("[VALVE] ON"));
  } else {
    Serial.print(F("[VALVE] OFF — was ON for "));
    Serial.print(now - valveOnSince);
    Serial.println(F(" ms"));
    valveOnSince = 0;
  }
}

// ============================================================
//  printSolarDebug() — throttled 1/sec, shows full state
// ============================================================
void printSolarDebug(int solarVal, unsigned long now) {
  static unsigned long lastPrint = 0;
  if (now - lastPrint < 1000UL) return;
  lastPrint = now;

  Serial.print(F("[SOLAR] val="));
  Serial.print(solarVal);
  Serial.print(F("  day="));
  Serial.print(dayState == DAY ? F("Y") : F("N"));
  Serial.print(F("  src="));
  Serial.print(virtualDay ? F("VIRTUAL") : F("SOLAR  "));
  Serial.print(F("  phase="));
  switch (waterPhase) {
    case PHASE_WARMUP:   Serial.println(F("WARMUP"));   break;
    case PHASE_SESSION1: Serial.println(F("SESSION1")); break;
    case PHASE_GAP:      Serial.println(F("GAP"));      break;
    case PHASE_SESSION2: Serial.println(F("SESSION2")); break;
    case PHASE_IDLE:     Serial.println(F("IDLE"));     break;
    case PHASE_NIGHT:    Serial.println(F("NIGHT"));    break;
  }
}