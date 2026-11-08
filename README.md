# Redzone

Мастеринг-плагин (VST3, Windows) для FL Studio и других DAW. Собственная реализация на JUCE 7 / C++17.

Поддерживаемые системы: **Windows 8, 8.1, 10, 11 (64-bit)**. JUCE 8 требует Windows 10, поэтому проект намеренно собран на JUCE 7.0.12, а CI проверяет, что бинарники не импортируют функции, появившиеся только в Windows 10.

## Цепочка обработки

`Input → EQ → Multiband Compressor → Imager → Maximizer → Meters`

| Модуль | Что делает |
|---|---|
| **EQ** | 6 полос: low shelf, 4 peak, high shelf; плавное изменение параметров; кривая АЧХ поверх спектра |
| **Multiband** | 3 полосы (кроссоверы Linkwitz-Riley 4-го порядка), компрессор с мягким коленом, linked-stereo |
| **Imager** | 3 полосы Mid/Side, ширина 0–200 % на полосу, моно-режим для нижней полосы |
| **Maximizer** | Look-ahead лимитер (1.5 мс) с 2× оверсэмплингом, Drive / Ceiling / Release, жёсткий потолок |
| **Meters** | LUFS (momentary / short-term / integrated, BS.1770-4 + гейтинг), Peak L/R, корреляция, GR лимитера и мультибэнда, анализатор спектра |

Каждый модуль имеет Bypass с плавным кроссфейдом. Задержка плагина (latency) репортится хосту автоматически и не меняется при bypass.

## Установка (Windows)

Каждый push собирает установщик через GitHub Actions:

1. Откройте вкладку **Releases** (или **Actions → последний запуск → Artifacts**).
2. Скачайте `Redzone-Setup-<версия>.exe` и запустите (нужны права администратора).
   VST3 ставится в `C:\Program Files\Common Files\VST3\Redzone.vst3`.
   Опционально — Standalone-версия.
3. В FL Studio: **Options → Manage plugins → Find installed plugins** (Verify plugins), затем ищите *Redzone* в списке эффектов.

Без установщика: распакуйте `Redzone-VST3.zip` в `C:\Program Files\Common Files\VST3\`.

## Локальная сборка

Нужны CMake ≥ 3.22 и Visual Studio 2022 (Desktop C++).

```bash
cmake -S . -B build -A x64
cmake --build build --config Release
# результат: build/Redzone_artefacts/Release/VST3/Redzone.vst3
```

Установщик: `ISCC.exe /DAppVersion=0.1.0 /DBuildDir=..\build installer\redzone.iss` (Inno Setup 6.3+).

## Структура

```
Source/Parameters.*         параметры (APVTS)
Source/PluginProcessor.*    цепочка обработки
Source/PluginEditor.*       интерфейс
Source/dsp/                 EQ, кроссоверы, мультибэнд, имиджер, лимитер, LUFS
Source/ui/                  стиль, панели параметров, спектр, метры
installer/redzone.iss       скрипт установщика Inno Setup
.github/workflows/build.yml CI: сборка + установщик + релиз
```

## Лицензия

Используется JUCE (AGPLv3 / коммерческая лицензия). Проект личный; при распространении бинарников соблюдайте условия AGPLv3 (открытый исходный код) либо приобретите лицензию JUCE.
