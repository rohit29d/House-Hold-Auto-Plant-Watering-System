#define valvecontrol 2
#define overide_water 5

const int ton = 10000;
const int toff = 30000;

void setup() {
  pinMode(valvecontrol, OUTPUT);
  pinMode(overide_water, INPUT_PULLUP);
}

void loop() {

  // 🔴 Override (always priority)
  if (digitalRead(overide_water) == LOW) {
    digitalWrite(valvecontrol, HIGH);
    return;
  }

  // 🟢 OFF phase (non-blocking style)
  digitalWrite(valvecontrol, LOW);
  for (int i = 0; i < toff / 100; i++) {
    if (digitalRead(overide_water) == LOW) return;
    delay(100);
  }

  // 🟢 ON phase
  digitalWrite(valvecontrol, HIGH);
  for (int i = 0; i < ton / 100; i++) {
    if (digitalRead(overide_water) == LOW) return;
    delay(100);
  }
}