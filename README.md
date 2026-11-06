# AZ MUS: Digital Audio Workstation
**Azimxodjayev Music** · Windows · macOS · Linux · *Powered by Tracktion Engine*

![AZ MUS](docs/screenshot.png)

## Versiya 0.1.0: imkoniyatlar

| Bo'lim | Nima bor |
|---|---|
| Ochilish | Splash ekran: audio drayver → MIDI → VST3 skan → kutubxona → loyiha |
| Audio | ASIO (Windows), WASAPI, CoreAudio. Trekka yozish, kirishni tanlash, avto-monitoring |
| Treklar | Audio va instrument treklar, M/S/R, rang, nom, o'chirish, undo/redo |
| Aranjirovka | Audio to'lqinlari, MIDI notalar, kliplarni sudrash, uzunlikni o'zgartirish, trekdan trekka ko'chirish, bo'lish (Ctrl+E), nusxa (Ctrl+D), snap, zoom |
| Markerlar | Chizg'ichga 2x bosish: qo'shiq bo'limi (Intro, Kuplet, Naqarot, Bridge, Outro) |
| Loop / metronom | Chizg'ichni sudrab loop zonasi, L va C tugmalari |
| MIDI | USB klaviatura tanlangan instrument trekda darhol chalinadi, MIDI yozish, MIDI import |
| Piano roll | Nota qo'yish, ko'chirish, uzunlik, o'chirish, grid 1/4…1/32, Q: kvantlash |
| Plaginlar | VST3 (AU macOS'da) hosting, plagin oynalari, bypass. 186 ta Airwindows effekti, AZ EQ/Kompressor/Delay/Chorus/Filtr, AZ Synth, AZ Sampler |
| Mikser | Fader, pan, L/R metr, insertlar, MASTER |
| Media brauzer | Sample papkalari, bir bosishda eshitish, 2x bosish yoki sudrash bilan trekka qo'shish |
| Displey | Pozitsiya (takt.hissa.1/16.tik), vaqt, temp (sudrash yoki 2x bosish), o'lchov, tonallik |
| Fayl | Loyiha `.azmus` (Windows'da 2x bosganda ochiladi), har daqiqada avto-saqlash, WAV mixdown eksport |

## Avtomatik test (`--selftest`)
Test yangi loyiha yaratadi, demo kontent qo'shadi (vokal, baraban, 808, pad, markerlar), AZ VERB VST3'ni yuklaydi, undo qiladi, saqlaydi va qayta ochadi, keyin render qilib natijani o'lchaydi. Natija: **17/17 o'tdi**, 62 s mixdown, RMS −16.8 dBFS.

## Yig'ish
GitHub Actions avtomatik yig'adi: `main`ga push qilinganda Windows `AZ-MUS-Setup.exe` va macOS zip **Releases**ga chiqadi.

Qo'lda yig'ish:
```
git clone https://github.com/Tracktion/tracktion_engine.git te
cd te && git checkout a5e44996f5f08dfa6af5f5d02d900fb3cfa37422
git -c url."https://github.com/".insteadOf="git@github.com:" submodule update --init modules/juce
cd .. && cmake -B build && cmake --build build --config Release
```
ASIO uchun Steinberg ASIO SDK'ni `./asiosdk` papkasiga qo'ying (CI buni o'zi qiladi).

## Litsenziyalar
- **Tracktion Engine**: Personal litsenziya (yillik daromad $50k gacha bepul, "Powered by Tracktion Engine" yozuvi shart). Daromad oshsa: Indie $35/oy ($200k gacha).
- **JUCE**: JUCE litsenziyasi (Personal/Indie).
- **Airwindows**: MIT. **Inter shrifti**: OFL.

## Keyingi bosqichlar
Avtomatizatsiya chiziqlari, sendlar va FX/bus treklar, audio muharrir (fade, gain, time-stretch), AZ Drum (Nerve uslubi), MIDI CC, freeze/bounce, ko'p yo'lakli eksport.
