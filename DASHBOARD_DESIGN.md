# RX-8 Dashboard UI Design

## Inspiration

All three reference images share a retro-futuristic aesthetic:

- **Dark/black background** with bright neon accents (green, cyan, amber)
- **Blocky digital fonts** reminiscent of 80s CRT displays
- **High contrast** — everything readable at a glance
- **Bar graphs** for secondary gauges
- Large central element as the hero gauge
- Minimal chrome/borders — let the data speak

## Gauges

| Gauge | Priority | Display Style | Range | Warning |
|-------|----------|---------------|-------|---------|
| RPM | Hero | Large analog-style arc gauge (center) | 0–9000 | Redline at 7500+ |
| Speed | High | Large digital readout | 0–260 km/h | — |
| Water Temp | High | Bar graph | 60–130 °C | Warning at 105 °C |
| Oil Temp | Medium | Bar graph | 60–150 °C | Warning at 120 °C |
| Fuel Level | Medium | Bar graph | 0–100 % | Low at 15% |
| Battery Voltage | Low | Bar graph | 10–16 V | Low at <11.5V, high at >15V |

## Layout — Tiling (Grid)

800×480 resolution. 2-column grid layout:

```
┌──────────────────┬──────────────────┐
│                  │                  │
│                  │     SPEED        │
│       RPM        │     128 km/h     │
│   (arc gauge)    │                  │
│                  ├──────────────────┤
│                  │  Water Temp  82°C│
│                  │  ████████░░░░░░░ │
├──────────────────┼──────────────────┤
│  Oil Temp   95°C │  Fuel        72% │
│  █████████░░░░░░ │  ████████████░░░ │
├──────────────────┼──────────────────┤
│  Voltage  13.8V  │                  │
│  ██████████░░░░░ │     (status)     │
│                  │                  │
└──────────────────┴──────────────────┘
```

### Tile breakdown

- **Top-left (large):** RPM arc gauge — the hero element, ~400×280 px
- **Top-right upper:** Speed digital readout
- **Top-right lower:** Water temp bar graph
- **Bottom-left:** Oil temp bar graph
- **Bottom-center:** Fuel level bar graph
- **Bottom-right:** Voltage bar graph + status info (OBD connection, etc.)

## Color Palette

| Role | Color | Hex |
|------|-------|-----|
| Background | Near black | `#0A0A0A` |
| Primary text/gauges | Neon green | `#00FF41` |
| Secondary text | Dim green | `#00AA30` |
| Warning | Amber | `#FFAA00` |
| Critical | Red | `#FF2222` |
| Accent | Cyan | `#00DDFF` |
| Bar graph fill | Neon green | `#00FF41` |
| Bar graph track | Dark gray | `#1A1A1A` |

## Fonts

- Primary: Monospaced, blocky digital font (LVGL built-in or custom bitmap font)
- Fallback: Montserrat 14 (already enabled in lv_conf.h)
- Ideal: A 7-segment or dot-matrix style font for the retro feel

## Warnings

- Values approaching thresholds: text/gauge turns **amber**
- Values at/above thresholds: text/gauge turns **red**, optional blinking
- Rotary-specific: coolant temp warning at 105 °C (rotaries run hot)
- RPM redline: >7500 amber, >8500 red
