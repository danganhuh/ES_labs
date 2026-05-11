# Lab 5.2 — relay-heater backup (frozen)

This folder is a **read-only snapshot** of the previous Lab 5.2 implementation
that drove **3 parallel resistors as a heater** through a 5 V relay on D8 and
read a **DHT11/DHT22** on D2.

It is preserved here for reference only. The PlatformIO `build_src_filter` in
`platformio.ini` matches `+<Lab5/*.cpp>` non-recursively, so files inside this
subfolder are **not compiled** and do not collide with the live Lab 5.2 in
`src/Lab5_2/`.

## Why it was scrapped

- The 3×100 Ω parallel resistors at 5 V dissipate only ~0.75 W, which the
  ATmega2560's regulator could supply only marginally — leading to coil
  chatter and almost no measurable heating effect at the DHT.
- The relay's mechanical contacts wore visibly during PID time-proportional
  cycling.
- DHT11 / DHT22 type confusion produced checksum-valid garbage readings
  (≈ -0.4 °C, 14 % RH) until `dhtprobe` was added.
- The plant was asymmetric (heating fast, cooling passive) which made PID
  tuning frustrating compared to the cooling-fan setup used in the
  reference labs.

## What replaced it

The new `src/Lab5_2/` rewrites the lab around a **DS18B20 1-Wire sensor** and
an **L9110H PWM-driven cooling fan**, mirroring the reference solution from
`SI lab 6.1` + `SI lab 6.2` (cooling, ON-OFF + PID combined into one app).

If you ever need to resurrect this old build:

1. Move these files back to `src/Lab5_2/` (overwriting whatever is there).
2. Restore the `lab5_2_setup()` / `lab5_2_loop()` dispatcher entry in
   `src/main.cpp` if it has changed.
3. Re-add the relay-on-D8 wiring in the Wokwi `diagram.json`.
