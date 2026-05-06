#define valvecontrol   2
#define overide_water  5
#define solar          A0   // NOTE: use A0–A5 for analog reads for arduino

const unsigned long ton  = 10000;   //valve on time
const unsigned long toff = 300000;   //valve off time


bool daystate        = false;
unsigned long lastdaystart = 0;
const unsigned int sunthresh = 600;


void setup() {
  pinMode(valvecontrol,  OUTPUT);
  pinMode(overide_water, INPUT_PULLUP);
  digitalWrite(valvecontrol, LOW); // ensure valve starts closed
  Serial.begin(9600);              // optional, helpful for debugging
}


void loop()
{
  int solarvalue    = analogRead(solar);
  unsigned long now = millis();

  // ── Sunrise detection ───────────────────────────────────────────────
  if (!daystate && solarvalue > sunthresh) {
    daystate     = true;
    lastdaystart = now;
    Serial.println("Sunrise detected - day started");
  }

  // ── Sunset detection ────────────────────────────────────────────────
  if (solarvalue < 200) {
    daystate = false;
  }

  // ── Fallback 24hr clock (cloudy day) ────────────────────────────────
  if (now - lastdaystart >= 86400000UL) {
    lastdaystart = now;
    daystate     = false;   // allow re-trigger on next sunrise
    Serial.println("24hr fallback reset");
  }

  // ── Manual override (hold button to water) ──────────────────────────
  if (digitalRead(overide_water) == LOW) {
    digitalWrite(valvecontrol, HIGH);
    return;
  }

  // ── Auto watering (only during day) ─────────────────────────────────
  if (daystate) {
    unsigned long elapsed = now - lastdaystart;

    if (elapsed < ton) {
      digitalWrite(valvecontrol, HIGH);   // Session 1 ON
    }
    else if (elapsed < ton + toff) {
      digitalWrite(valvecontrol, LOW);    // Gap between sessions
    }
    else if (elapsed < 2 * ton + toff) {
      digitalWrite(valvecontrol, HIGH);   // Session 2 ON
    }
    else {
      digitalWrite(valvecontrol, LOW);    // Done for the day
    }
  }
  else {
    digitalWrite(valvecontrol, LOW);      // Night – valve closed
  }
}