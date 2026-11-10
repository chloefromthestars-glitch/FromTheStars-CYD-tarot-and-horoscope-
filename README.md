# ✦ FromTheStars ✦
### daily horoscope and tarot for the Cheap Yellow Display

☾ ✧ ⋆ ✦ ⋆ ✧ ☽

FromTheStars is a small tarot and horoscope firmware for the **ESP32-2432S028R**, better known as the Cheap Yellow Display (CYD). It runs entirely on the board: pick your sign once, draw cards whenever you like, and check your daily horoscope over Wi-Fi.

Made by **Chloefromthestars**.

---

## ✧ Features

- **Three-card draw.** Pulls three different cards from the full 78-card deck, major and minor arcana. Each card can come up reversed. Step through past, present and future, then read a short summary of the spread.
- **Daily horoscope.** Uses your saved sign and today's date. Needs Wi-Fi to get the date.
- **Sign picker on first boot.** The board asks for your sign the first time it starts and remembers it, so anyone can set it up for themselves. Change it any time from the sign button on the home screen.
- **Wi-Fi setup on the device.** Tap **WI-FI** to scan for networks (sorted by signal strength), pick one, and type the password on the touch keyboard (lowercase, uppercase and symbols). The network is saved and reconnects on every boot. You can rescan or forget a network from the same screen.
- **Works offline.** Card draws don't need Wi-Fi at all.
- **Cute on purpose.** Crescent moon, sparkles and gold-and-pink text.

## ✧ Hardware

- ESP32-2432S028R ("Cheap Yellow Display"), 2.8" 320×240 resistive touch screen
- USB cable for flashing

Other CYD variants with different display drivers or touch pins may need changes to the display setup in the source.

## ✧ Installing

Two builds are provided in `firmware/`:

| File | Use it for |
|------|------------|
| `FromTheStars_merged.bin` | Flashing the whole board. Write at address **0x0**. |
| `FromTheStars.bin` | App-only image, for launchers such as **M5 Launcher**. |

### With esptool

```bash
esptool.py --chip esp32 --port <your-port> write_flash 0x0 FromTheStars_merged.bin
```

Replace `<your-port>` with your board's port (for example `COM5` on Windows or `/dev/ttyUSB0` on Linux).

### With a web flasher

Any ESP32 web flasher that accepts a single `.bin` works. Choose `FromTheStars_merged.bin` and set the address to `0x0`.

### With M5 Launcher

Copy `FromTheStars.bin` to the SD card and open it from the launcher.

## ✧ First boot

1. Calibrate the touch screen if asked.
2. Pick your zodiac sign.
3. Tap **WI-FI**, choose your network and enter the password, if you want the daily horoscope.
4. Draw some cards ✦

## ✧ Building from source

The project is an Arduino sketch in `FromTheStars/`. The program lives in `FromTheStars.cpp`; `FromTheStars.ino` is a stub, because the Arduino preprocessor mangles some C++ in `.ino` files.

You'll need:

- The ESP32 Arduino core
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) for the display and touch

Build with the Arduino IDE or `arduino-cli`, targeting a generic ESP32 dev module with 4MB flash.

### `config.h`

Everything here is optional, since Wi-Fi can be set up on the board.

- `WIFI_SSID` / `WIFI_PASS`: a fallback network, used only if no network has been saved on the board. Leave empty to skip.
- UTC offset in hours, used for the date. For example, New York is `-5` in winter and `-4` in summer.

## ✧ Known limits

- The ESP32 only joins **2.4 GHz** networks, not 5 GHz.
- Networks that need a username or certificate (enterprise Wi-Fi, like many school and work networks) won't work.
- Horoscope readings are written into the firmware. They aren't pulled from a live astrology service.
- Card meanings are short one-line summaries.
- The built-in font has plain letters and numbers only, so stars and moons are drawn shapes.

## ✧ Contributing

Issues and pull requests are welcome, especially fuller card meanings, new spreads, and support for other CYD variants.

## ✧ License

MIT. See [LICENSE](LICENSE).

Tarot and horoscopes here are for fun and reflection, not advice. ✧

⋆ ✦ ⋆ made with love by Chloefromthestars ⋆ ✦ ⋆
