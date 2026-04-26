# Automatic Plant Watering System (v1)

The plants at my place were slowly dying… and life was getting busy.
So instead of trying to “be more consistent,” I built something that doesn’t forget.

This is a simple, practical system that waters plants automatically — but with just enough intelligence to behave differently based on real-world conditions like sunlight and cloudy days.

---

## Idea

At its core, this is not just a timer.

Most basic systems blindly water at fixed times.
This one tries to understand the day.

Instead of relying on an RTC module or fixed clock:

* It detects sunrise using light
* Tracks the day lifecycle
* Adapts when the weather isn’t ideal

---

## Working Principle

The system runs on a day–night state machine:

1. Sunrise Detection

   * A small solar panel acts as a light sensor
   * When light crosses a threshold → system confirms “DAY”

2. Warmup Period

   * Waits ~10 minutes to avoid false triggers (cloud flickers etc.)

3. Watering Schedule

   * Session 1 → waters plants
   * Waits ~7 hours
   * Session 2 → waters again

4. Sunset Detection

   * Light drops below threshold → system switches to “NIGHT”
   * No watering happens at night

If sunset happens before session 2, the system skips it.
(This turned out to be better for plant health.)

---

## Cloudy Day Fallback

This was one of the more interesting problems.

What if:

* It’s cloudy all day?
* No clear sunrise is detected?

### Solution:

A fallback timer-based virtual day

* If no sunrise is detected for 24 hours:

  * The system simulates a new day
  * Runs the watering cycle anyway

* The moment real sunlight returns:

  * It switches back to normal solar-based operation

This avoids needing an RTC module while still being reliable.

---

## Hardware Setup

* Microcontroller: Arduino Uno
* Sensor: Small 70×70 solar panel (used as light sensor)
* Actuator: 24V solenoid valve (½ inch)
* Control: Relay module
* Water Distribution:

  * Main ½ inch pipe
  * Drip emitters for each pot
  * End capped to maintain pressure and ensure even distribution

---

## Setup
<img width="1280" height="575" alt="pwr_setup" src="https://github.com/user-attachments/assets/a9ef49db-35ae-48f4-97c5-ec80ffa271f9" />
  - 
<img width="1280" height="575" alt="module_setup" src="https://github.com/user-attachments/assets/4f36dacd-1399-4763-9a35-26c1cd094180" />
  -
<img width="1280" height="575" alt="internal_setup" src="https://github.com/user-attachments/assets/9d25e7ea-f8f1-48de-a632-c818eb5876f8" />
  -
---

## Demo: Attached in the repo.

---

## Watering Strategy

Current configuration:

* 2 watering sessions per day
* ~5 minutes per session (tuned to avoid overflow)
* Drip-based distribution

This setup:

* Prevents overwatering
* Allows soil to absorb moisture gradually
* Works well in hot conditions with minor tuning

---

## Design Decisions

* Solar panel instead of LDR

  * Already generates voltage → cleaner analog signal
  * More robust outdoors

* State machine instead of delays

  * Fully non-blocking
  * Predictable behavior

* Light-based scheduling

  * Adapts to season and day length automatically

* Hybrid fallback

  * Keeps system working even without perfect sunlight

---

## Observations

* Direct sunlight matters — placement of the panel is critical
* Threshold tuning depends on environment (balcony vs open area)
* Drip emitters make a big difference in consistency
* Evening watering is intentionally avoided
* The pot soil should be well gardened with adegate air gaps to let water seep well down to the botton of the pot. 

---

## Future Improvements

* Soil moisture sensing (closed-loop control)
* RTC integration (optional precision control)
* Solar + battery for full off-grid operation
* Multi-zone irrigation

---

## Why I built this

Not everything needs to be over-engineered.

This started as:
“My plants are dying”

And ended up being:
A small system that reacts to the real world instead of blindly following time.

---

## Version

v1 — Functional, reliable, and deployed

---

If you’re building something similar or improving this, feel free to take ideas from here.
This project is intentionally simple — but surprisingly capable.
