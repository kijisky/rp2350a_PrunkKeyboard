# Версия на C (без Python/CircuitPython)

Та же идея, что и в [основном README](../README.md) и в
[`firmware/`](../firmware/) (CircuitPython), но переписанная на чистый
C поверх [Pico SDK](https://github.com/raspberrypi/pico-sdk) и
[TinyUSB](https://github.com/hathach/tinyusb) — компилируется прямо в
`.uf2`, никакого интерпретатора на плате не нужно.

Архитектура и подход к сопоставлению триггеров/раскладки ЙЦУКЕН — те
же, что в `firmware/`; где это важно, в коде оставлены ссылки вида
«мирорит code.py» / «мирорит config.py» для сверки.

**Прежде чем прошивать на устройство, которое не принадлежит лично
вам** — см. [раздел «Этика и границы использования»](../README.md#этика-и-границы-использования)
в основном README. Он касается этой версии точно так же, как и
CircuitPython-версии.

## Архитектура (dual-role USB через TinyUSB)

RP2350A имеет только один «родной» USB-контроллер, поэтому
одновременная работа и host (к клавиатуре), и device (к ПК)
реализована так же, как в референсных проектах (daifukusensei, Seth
Robinson's PrankKeyboard):

- **RHPORT0** (родной USB-контроллер) — режим **device**: плата
  представляется компьютеру как composite-устройство из двух
  интерфейсов — boot-protocol HID-клавиатура (сам MITM-вывод) и
  CDC-ACM "серийный порт" для диагностики (`debug_printf`, см.
  [`src/debug.h`](src/debug.h); ПК он не требуется, нужен только для
  отладки — подключите любой терминал, если нужно увидеть
  `Keyboard connected`/ошибки).
- **RHPORT1** (через [Pico-PIO-USB](https://github.com/sekigon-gonnoc/Pico-PIO-USB),
  bit-banged USB на двух GPIO) — режим **host**: туда подключается
  физическая клавиатура.

Логика перехвата/подмены (TinyUSB host callback
`tuh_hid_report_received_cb` → сопоставление с таблицей макросов →
`tud_hid_keyboard_report` на сторону ПК) — в [`src/main.c`](src/main.c);
таблица макросов и движок сопоставления — в [`src/macros.c`](src/macros.c)/[`.h`](src/macros.h);
таблицы кодов клавиш и раскладки — в [`src/keymap.c`](src/keymap.c)/[`.h`](src/keymap.h).

## Какие GPIO использовать для host-порта

Pico-PIO-USB требует две **соседние** GPIO-линии (D+ и D+1 = D-) для
host-порта. Какие именно GPIO разведены на USB-A host-разъём —
зависит от конкретной платы:

- Если это Waveshare RP2350-USB-A (как в проекте daifukusensei) —
  это **GP12/GP13**.
- Общепринятое значение по умолчанию в похожих самодельных схемах
  (как в PrankKeyboard) — **GP0/GP1**.
- Сверьтесь со схемой/распиновкой именно вашей платы.

Значение передаётся при конфигурации CMake, по умолчанию GP0/GP1:

```bash
cmake -S firmware_c -B build -DPRUNK_PIO_USB_DP_PIN=12   # для GP12/GP13
```

## Сборка

### Вариант 1 — GitHub Actions (ничего ставить локально не нужно)

Любой push, меняющий `firmware_c/**`, собирает `.uf2` автоматически
(см. [`.github/workflows/build-firmware-c.yml`](../.github/workflows/build-firmware-c.yml)).
Готовый файл — во вкладке **Actions** нужного workflow-run, в
Artifacts (`prunk_keyboard_c-<commit>`). При пуше git-тега (`git tag
v1.0.0 && git push --tags`) `.uf2` дополнительно прикладывается к
GitHub Release на этот тег.

### Вариант 2 — локально

Нужны: `cmake` (3.17+), компилятор `arm-none-eabi-gcc` (пакет
`gcc-arm-none-eabi` + `libnewlib-arm-none-eabi` в Debian/Ubuntu, или
[ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
на других системах), `git`, `python3`.

```bash
# 1. pico-sdk — обязательно --recursive: внутри lib/tinyusb лежит ещё
#    один сабмодуль, Pico-PIO-USB, без которого dual host/device сборка
#    не соберётся.
git clone --recursive https://github.com/raspberrypi/pico-sdk.git
export PICO_SDK_PATH="$PWD/pico-sdk"

# 2. конфигурация + сборка
cmake -S firmware_c -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# результат:
ls build/*.uf2
```

`FAMILY`/`BOARD` по умолчанию — `rp2040`/`raspberry_pi_pico2` (RP2350
использует тот же TinyUSB-порт, что и RP2040 — тот же блок USB в
кремнии). Переопределить можно так же через `-D...` при конфигурации,
если у вашей платы другое соответствие.

## Прошивка

Как и с CircuitPython-версией: зажать `BOOTSEL`, подключить плату по
USB (её родной, device-порт — то есть тот, что в обычной работе идёт
к компьютеру), она примонтируется как диск. Скопировать на неё
`build/prunk_keyboard_c.uf2` — плата перезагрузится уже с новой
прошивкой.

## Настройка макросов

Всё редактируемое — в одном месте, [`src/macros.c`](src/macros.c), в
блоке, помеченном `EDIT HERE`:

```c
static const char *const TOK_privet[] = {"и", "к", "и"};   // привет -> приветики
...
static const macro_source_t MACROS[] = {
    {"привет", TOK_privet, 3},
    ...
};
```

Формат — как в `config.py`: триггер (кириллица или латиница — не
важно, резолвится по физической клавише), и массив токенов для
замены: один символ — печатается как есть, имя клавиши
(`"backspace"`, `"enter"`, `"f5"`, ...) — см. полный список в
[`src/keymap.c`](src/keymap.c) (`keymap_name_to_code`). После правки —
пересобрать и перепрошить (`src/main.c`/`src/keymap.c` трогать не
нужно).

Ошибка в конфиге (неизвестный токен, слишком длинный триггер) не
"тихо ломает" устройство — `macros_init()` печатает диагностику в
debug-консоль (CDC) и зависает в цикле, пока прошивка не будет
исправлена и перезалита, — то же поведение, что и падение `config.py`
при импорте в CircuitPython-версии.
