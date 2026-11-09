# ClickFix

**[Русский](#русский) | [English](#english)**

---

## Русский

Маленькая программа для Windows, которая лечит **ложные двойные клики** мыши — когда левая или правая кнопка иногда срабатывает два раза при одном нажатии.

Обычно это «дребезг» изношенного микропереключателя в мыши. ClickFix отсекает такие ложные срабатывания программно, без замены мыши.

### Возможности

- Блокирует ложные повторные нажатия левой и правой кнопок (вместе с их «отпусканием», чтобы не ломать выделение).
- **Защита перетаскивания** — если кнопка «отскакивает» во время удержания, окно или файл не роняются посередине пути.
- Отдельные настройки для левой, правой и средней кнопки (порог 30–200 мс).
- Защита от ложного «назад»: если у мыши при нажатии правой кнопки иногда срабатывает боковая кнопка «назад», такое срабатывание блокируется. Боковые кнопки можно и полностью отключить.
- Пауза одним кликом — например, на время игры.
- **Автообновление**: программа сама проверяет новые версии на GitHub (при запуске и раз в сутки), сообщает о них и обновляется в один клик.
- Интерфейс на русском и английском (выбирается автоматически или вручную).
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
| Левая / правая / средняя кнопка → порог | Сколько миллисекунд после отпускания кнопки повторное нажатие считается ложным. По умолчанию 70 мс. Если двойные клики всё ещё проскакивают — увеличьте (90–120). Если перестали работать ваши быстрые двойные клики — уменьшите (40–50). |
| … → Защита перетаскивания | Держит кнопку «зажатой», если она на миг отскочила при удержании. Отпускание кнопки срабатывает с задержкой, равной порогу. По умолчанию включена только для левой кнопки: для правой она мешает жестам мыши в браузерах (например, «назад»). |
| Средняя кнопка → Фильтровать | Включает фильтр для колеса (по умолчанию выключен). |
| Боковые кнопки | «Блокировать «назад», если он срабатывает от правого клика» — боковая кнопка, нажатая вместе с правой (или в пределах 150 мс), игнорируется. Обычное нажатие «назад» срабатывает с задержкой ~60 мс. Можно также отключить боковые кнопки полностью. |
| Обновления | «Проверить обновления сейчас» и «Проверять обновления автоматически». Если найдена новая версия, появится уведомление: нажмите на него, программа скачает новую версию и перезапустится. |
| Пауза | Временно выключает фильтр, значок в трее меняется на жёлтый. |
| Язык / Language | Автоматически, русский или английский. |
| Запускать вместе с Windows | Добавляет программу в автозагрузку. |
| Сбросить счётчик | Обнуляет счётчик заблокированных кликов. |

Настройки хранятся в реестре: `HKEY_CURRENT_USER\Software\ClickFix`.

Порог можно задать и параметром запуска: `ClickFix.exe 100`.

### Заряд батареи

Показывается только для мышей (и других устройств), подключённых по **Bluetooth**, если Windows сама видит их заряд. Мыши с USB-приёмником («свистком») используют собственные протоколы производителей, и для них заряд обычно недоступен.

### Сборка из исходников

MinGW-w64:

```
x86_64-w64-mingw32-gcc -O2 -municode -mwindows -static -o ClickFix.exe ClickFix.c -luser32 -lshell32 -lsetupapi -ladvapi32 -lwinhttp
```

Visual Studio (Developer Command Prompt):

```
cl /O2 /utf-8 ClickFix.c user32.lib shell32.lib setupapi.lib advapi32.lib winhttp.lib /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
```

### Для разработчика: выпуск новой версии

Автообновление берёт последний релиз из GitHub Releases. Чтобы оно сработало:
1. Поднимите `APP_VERSION` в `ClickFix.c`.
2. Создайте релиз с тегом вида `v1.4` (номер больше текущего).
3. Прикрепите к релизу файл с расширением `.exe` (например, `ClickFix.exe`).

### Совет

Программа маскирует износ кнопки, но не чинит его. Если дребезг со временем усиливается — поможет замена микропереключателей или новая мышь. У беспроводных мышей ложные клики иногда вызывает слабая батарейка или плохой сигнал — попробуйте свежую батарейку и приёмник поближе к мыши.

---

## English

A tiny Windows utility that fixes **mouse double-click chatter** — when the left or right button sometimes registers two clicks for a single press.

This is usually caused by a worn-out microswitch ("switch bounce"). ClickFix filters out these false clicks in software, so you can keep using the mouse.

### Features

- Blocks false repeated presses of the left and right buttons (together with their matching release, so text selection doesn't break).
- **Drag protection** — if the button bounces while held, windows/files are not dropped mid-drag.
- Separate settings for the left, right and middle buttons (threshold 30–200 ms).
- False "back" protection: if the side "back" button sometimes fires when you press the right button, that event is blocked. Side buttons can also be disabled completely.
- One-click pause, e.g. while gaming.
- **Auto-update**: checks GitHub for new versions (at startup and once a day), notifies you and updates itself in one click.
- Russian and English interface (automatic or manual).
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
| Left / right / middle button → threshold | A press that comes within this many milliseconds after a release is treated as false. Default: 70 ms. Increase (90–120) if chatter still gets through; decrease (40–50) if your real fast double-clicks stop working. |
| … → Drag protection | Keeps the button "held" if it briefly bounces while pressed. Button releases are delayed by the threshold value. On by default for the left button only: on the right button it interferes with browser mouse gestures (e.g. "back"). |
| Middle button → Filter | Enables the filter for the wheel button (off by default). |
| Side buttons | "Block back when it fires from a right click" — a side-button press that comes together with a right click (or within 150 ms) is ignored. A normal "back" press fires with a ~60 ms delay. Side buttons can also be disabled completely. |
| Updates | "Check for updates now" and "Check for updates automatically". When a new version is found, a notification appears: click it and the program downloads the new version and restarts. |
| Pause | Temporarily disables the filter; the tray icon turns yellow. |
| Язык / Language | Automatic, Russian or English. |
| Start with Windows | Adds the program to autostart. |
| Reset counter | Resets the blocked-click counter. |

Settings are stored in `HKEY_CURRENT_USER\Software\ClickFix`. The threshold can also be passed on the command line: `ClickFix.exe 100`.

### Battery level

Only available for **Bluetooth** devices whose battery level Windows itself can read. Mice with a USB dongle use vendor-specific protocols, so battery level is usually unavailable for them.

### Building

MinGW-w64:

```
x86_64-w64-mingw32-gcc -O2 -municode -mwindows -static -o ClickFix.exe ClickFix.c -luser32 -lshell32 -lsetupapi -ladvapi32 -lwinhttp
```

Visual Studio (Developer Command Prompt):

```
cl /O2 /utf-8 ClickFix.c user32.lib shell32.lib setupapi.lib advapi32.lib winhttp.lib /link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup
```

## License

[MIT](LICENSE)
