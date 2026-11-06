# AZ MUS: Digital Audio Workstation
**Azimxodjayev Music** · Windows · macOS · Linux · *Powered by Tracktion Engine*

![AZ MUS](docs/screenshot.png)

## 0.1.1: xavfsiz plagin skaneri
Har bir VST3 plagin AZ MUS'ning alohida nusxasida tekshiriladi (Cubase'dagi kabi). Skanerlash vaqtida qulaydigan yoki 45 soniyadan ko'p javob bermaydigan plagin qora ro'yxatga tushadi, AZ MUS esa ochilishda davom etadi. Splash ekranni bosish skanerlashni o'tkazib yuboradi. Qora ro'yxatni tozalash: Plaginlar → VST3 plaginlarni skanerlash → Options → Clear blacklist.

## 0.4.0: Control Room · Monitor Match · Pro Tools uslubidagi yakuniy natija
![Control Room](docs/control_room.png)

**Control Room kanali** (Mikser F3, MASTER yonida) faqat monitorlarga boradi; eksport, bounce va mixdown'ga hech qachon tushmaydi.

| Bo'lim | Nima qiladi |
|---|---|
| **Monitor Match** | Sizning monitoringizni (manba) boshqa monitorga (maqsad) aylantiradi. Kechikishsiz minimum-phase FIR |
| Manba (12 ta) | KRK Rokit 5/6/8 G4, ADAM A8X, T5V, T7V, Yamaha HS5, JBL 305P MkII, PreSonus Eris E5, M-Audio BX5 D3, Mackie CR4-X, Behringer B2030A, **yoki o'z o'lchovingiz (REW)** |
| Maqsad (9 ta) | Genelec 8340A, Focal Shape 65, Neumann KH 310, Yamaha HS8, NS-10M, Flat, Auratone 5C, Smartfon, Avtomobil |
| % emulyatsiya | Korreksiya kuchi; egri chiziq kanalda ko'rinadi |
| 2 × CR insert | Istalgan VST3 (masalan, tashqi Monitor Match yoki Sonarworks) faqat monitor yo'lida |
| Level · DIM · MONO · MUTE | Monitor boshqaruvi |

Ichki profillar ishlab chiqaruvchi grafiklari asosidagi **taxminiy** egri chiziqlar: xonani bilmaydi. Aniq natija uchun REW + o'lchov mikrofoni bilan tinglash nuqtasida o'lchang va Plaginlar → "Monitor Match: monitor o'lchov faylini yuklash (REW)..." orqali yuklang.

**Pro Tools uslubi:** master fader yangi loyihada 0 dB (unity, avval −3 dB edi), pan law −2.5 dB markaz, mixdown 24-bit + TPDF dither yoki 32-bit float (Fayl menyusi), bounce 32-bit float (headroom yo'qolmaydi).

## 0.3.1: Master insert (mastering zanjiri)
| Qayerda | Qanday |
|---|---|
| **Mikser (F3) → MASTER kanali** | Yuqoridagi 4 ta "+ Master insert" slot: bosing → plagin qidiruvi (masalan "limit", "eq") |
| **Plaginlar menyusi** | "Master insert qo'shish (mastering)..." |
| Slotda o'ng tugma | Ochish / Bypass / O'chirish |

Master zanjiri Control Room'dan oldin turadi, shuning uchun eksport (B, WAV) ga kiradi; Control Room esa faqat monitorga ta'sir qiladi.

## 0.3.0: Audio ulanishlar, FX kanallar, plagin qidiruvi
| Yangilik | Tafsilot |
|---|---|
| **F4: Audio ulanishlar** | Inputs (yoqish, bus nomi), Outputs (Main Mix tanlash), Group/FX (FX kanallar), External FX va External Instruments (apparat qurilmalar send/return), Control Room (monitor chiqishi, balandlik, Dim −20 dB, Mono, Mute; eksportga ta'sir qilmaydi) |
| **Sendlar** | Inspektorda SENDLAR: FX kanalga post-fader send, gorizontal sudrash bilan balandlik |
| **Plagin qidiruvi** | Nom, bosh harflar yoki ishlab chiqaruvchi bo'yicha; ↑ ↓ Enter; Tab: kategoriya |
| **Sichqoncha** | Bo'sh joyda sudrash: to'rtburchak bilan belgilash; Alt + sudrash: nusxa; bir nechta klip birga ko'chadi |
| **Masshtab** | Butun DAW oynasi: − / + (menyu qatori o'ngida) yoki Ctrl + / Ctrl − / Ctrl 0 |
| **Tuzatish** | Delete klipni o'chirmasdi (Windows'da Num Del bilan bir xil kod), endi o'chiradi |

## 0.2.0: Daftar DAW klavishlari, 3 til, yopiladigan panellar
| Klavish | Vazifa |
|---|---|
| Space / Enter (Num) | Ijro (to'xtatish) |
| R / Num * | Yozish (Cubase kabi) |
| Num 0 / Num . / Num + − / Num / | Stop / boshiga / takt oldinga-orqaga / loop |
| B | Bounce Selection: audio kliplar bitta audioga; MIDI esa synth bilan audioga (yangi trek) |
| P | Locatorlar (loop) = tanlangan kliplar |
| S / Ctrl+E | Kursor joyida kesish |
| 1 2 3 4 | Asbob: tanlash / qaychi / o'chirg'ich / mute |
| H / G, Shift+H / G | Gorizontal zoom, trek balandligi |
| Ctrl+Shift+A, Ctrl+A, Esc | Hammasini ekranga sig'dirish, hammasini belgilash, bekor qilish |
| F3 / F4 / F5 / Alt+I | Mikser / Audio sozlamalari / Media brauzer / Inspektor |
| J | Snap |

Panellar (Inspektor, Media brauzer, Mikser) yopiladi va ochiladi; holati saqlanadi. Til: logotip yonidagi **UZ · RU · EN** yoki **Til** menyusi. Tuzatildi: har bir trek indikatori faqat o'z signalini ko'rsatadi.

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
