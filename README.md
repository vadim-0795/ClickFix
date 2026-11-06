# ClickFix

**[Русский](#русский) | [English](#english)**

---

## Русский

Маленькая программа для Windows, которая лечит **ложные двойные клики** мыши — когда левая или правая кнопка иногда срабатывает два раза при одном нажатии.

Обычно это «дребезг» изношенного микропереключателя в мыши. ClickFix отсекает такие ложные срабатывания программно, без замены мыши.

### Возможности

- Блокирует ложные повторные нажатия левой и правой кнопок (вместе с их «отпусканием», чтобы не ломать выделение).
- **Защита перетаскивания** — если кнопка «отскакивает» во время удержания, окно или файл не роняются посередине пути.
- Настраиваемый порог срабатывания: 30–200 мс.
- Счётчик заблокированных ложных кликов — видно, насколько мышь «больна».
- Показ заряда батареи **Bluetooth**-устройств (как в «Параметрах» Windows) и предупреждение при низком заряде.
- Автозапуск вместе с Windows одной галочкой.
- Работает в трее, почти не потребляет ресурсов, без установки.

### Установка

1. Скачайте `ClickFix.exe` из раздела [Releases](../../releases).
2. Запустите — в трее появится значок со щитом.
3. Клик по значку открывает меню с настройками.

> **Предупреждение Windows / антивируса.** Программа не подписана цифровой подписью и перехватывает события мыши, поэтому SmartScreen или антивирус могут на неё ругаться. Если Windows пишет «Windows защитила ваш компьютер» — нажмите «Подробнее» → «Выполнить в любом случае». Исходный код полностью открыт, его можно проверить и собрать самостоятельно.

### Настройка

| Пункт меню | Что делает |
|---|---|
| Порог фильтра | Сколько миллисекунд после отпускания кнопки повторное нажатие считается ложным. По умолчанию 70 мс. Если двойные клики всё ещё проскакивают — увеличьте (90–120). Если перестали работать ваши быстрые двойные клики — уменьшите (40–50). |
| Защита перетаскивания | Держит кнопку «зажатой», если она на миг отскочила при удержании. Отпускание кнопки срабатывает с задержкой, равной порогу. В играх, где важна скорость реакции, можно отключить. |
| Запускать вместе с Windows | Добавляет программу в автозагрузку. |
| Сбросить счётчик | Обнуляет счётчик заблокированных кликов. |

Настройки хранятся в реестре: `HKEY_CURRENT_USER\Software\ClickFix`.

Порог можно задать и параметром запуска: `ClickFix.exe 100`.

### Заряд батареи

Показывается только для мышей (и других устройств), подключённых по **Bluetooth**, если Windows сама видит их заряд. Мыши с USB-приёмником («свистком») используют собственные протоколы производителей, и для них заряд обычно недоступен.

### Сборка из исходников

MinGW-w64:

```
x86_64-w64-mingw32-gcc -O2 -municode -mwindows -static -o ClickFix.exe ClickFix.c -luser32 -lshell32 -lsetupapi -ladvapi32
```

Visual Studio (Developer Command Prompt):

```
cl /O2 /utf-8 ClickFix.c user32.lib shell32.lib setupapi.lib advapi32.lib /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
```

### Совет

Программа маскирует износ кнопки, но не чинит его. Если дребезг со временем усиливается — поможет замена микропереключателей или новая мышь. У беспроводных мышей ложные клики иногда вызывает слабая батарейка или плохой сигнал — попробуйте свежую батарейку и приёмник поближе к мыши.

---

## English

A tiny Windows utility that fixes **mouse double-click chatter** — when the left or right button sometimes registers two clicks for a single press.

This is usually caused by a worn-out microswitch ("switch bounce"). ClickFix filters out these false clicks in software, so you can keep using the mouse.

### Features

- Blocks false repeated presses of the left and right buttons (together with their matching release, so text selection doesn't break).
- **Drag protection** — if the button bounces while held, windows/files are not dropped mid-drag.
- Adjustable threshold: 30–200 ms.
- Counter of blocked false clicks.
- Battery level for **Bluetooth** devices (same value Windows shows in Settings) with a low-battery warning.
- One-click "start with Windows".
- Runs in the system tray, tiny footprint, no installation.

### Installation

1. Download `ClickFix.exe` from [Releases](../../releases).
2. Run it — a shield icon appears in the system tray.
3. Click the icon to open the settings menu.

> **SmartScreen / antivirus warning.** The program is not code-signed and uses a low-level mouse hook, so SmartScreen or antivirus software may flag it. Click "More info" → "Run anyway". The full source code is available for review and you can build it yourself.

### Settings

| Menu item | Description |
|---|---|
| Filter threshold | A press that comes within this many milliseconds after a release is treated as false. Default: 70 ms. Increase (90–120) if chatter still gets through; decrease (40–50) if your real fast double-clicks stop working. |
| Drag protection | Keeps the button "held" if it briefly bounces while pressed. Button releases are delayed by the threshold value. You may want to disable it for fast-paced games. |
| Start with Windows | Adds the program to autostart. |
| Reset counter | Resets the blocked-click counter. |

Settings are stored in `HKEY_CURRENT_USER\Software\ClickFix`. The threshold can also be passed on the command line: `ClickFix.exe 100`.

### Battery level

Only available for **Bluetooth** devices whose battery level Windows itself can read. Mice with a USB dongle use vendor-specific protocols, so battery level is usually unavailable for them.

### Building

MinGW-w64:

```
x86_64-w64-mingw32-gcc -O2 -municode -mwindows -static -o ClickFix.exe ClickFix.c -luser32 -lshell32 -lsetupapi -ladvapi32
```

Visual Studio (Developer Command Prompt):

```
cl /O2 /utf-8 ClickFix.c user32.lib shell32.lib setupapi.lib advapi32.lib /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
```

## License

[MIT](LICENSE)
