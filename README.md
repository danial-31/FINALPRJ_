# FINALPRJ — C++ / OpenGL

Стартовый проект университетской 3D-игры с изометрическим видом.
Сейчас программа открывает чёрное окно **1280 × 720**, создаёт контекст
**OpenGL 3.3 Core**, обрабатывает события и очищает буферы цвета и глубины.
Размер окна можно менять. **Esc** или крестик закрывают программу.
Игровой процесс и камера пока не реализованы.

## Что нужно установить

- Компилятор с поддержкой C++17. На Windows: Visual Studio 2022 или новее,
  рабочая нагрузка **«Разработка классических приложений на C++»**, Windows SDK
  и компонент **«Средства C++ CMake для Windows»**.
- CMake 3.20 или новее.
- Видеодрайвер с поддержкой OpenGL 3.3 или новее.

OpenGL предоставляет видеодрайвер. Копировать `opengl32.dll` в проект не нужно.
Исходники вспомогательных библиотек уже находятся в `third_party/`:
после клонирования интернет для их сборки не нужен, подмодулей нет.

## Запуск в Visual Studio

1. Клонировать репозиторий:

   ```powershell
   git clone https://github.com/danial-31/FINALPRJ_.git
   ```

2. Выбрать **Файл → Открыть → Папка** и открыть папку `FINALPRJ_`,
   в которой лежит `CMakeLists.txt`.
3. Дождаться настройки CMake, выбрать цель **finalprj.exe** и запустить её.

## Сборка из терминала

На Windows использовать **Developer PowerShell for Visual Studio**.
На остальных системах нужен установленный системный компилятор.

```powershell
cd FINALPRJ_
cmake -S . -B build
cmake --build build --config Debug --parallel
```

Запуск при использовании генератора Visual Studio:

```powershell
.\build\Debug\finalprj.exe
```

При использовании Ninja или Makefiles исполняемый файл находится непосредственно
в `build/`: `./build/finalprj` (на Windows — `build/finalprj.exe`).

На Linux для GLFW нужны системные пакеты разработки оконной системы.
Например, для Ubuntu/Debian с X11:

```sh
sudo apt install build-essential cmake xorg-dev
cmake -S . -B build -DGLFW_BUILD_WAYLAND=OFF
cmake --build build --parallel
./build/finalprj
```

На macOS нужны Xcode Command Line Tools и CMake. Используются те же команды
сборки, без параметра `GLFW_BUILD_WAYLAND`.

## Проверка запуска

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Проверка создаёт скрытое настоящее окно OpenGL, рисует три кадра, проверяет
чёрный пиксель в центре и отсутствие ошибок OpenGL, затем завершает программу.
Нужны графическая сессия и рабочий OpenGL; это не тест без видеоконтекста.

## Подключённые библиотеки

| Библиотека | Версия | Назначение |
| --- | --- | --- |
| [GLFW](https://www.glfw.org/) | 3.5.1 | Окно, контекст OpenGL, клавиатура и мышь |
| [GLAD](https://github.com/Dav1dde/glad) | генератор 2.0.8 | Загрузка функций OpenGL 3.3 Core |
| [GLM](https://github.com/g-truc/glm) | 1.0.3 | Векторы и матрицы для 3D и камеры |
| [stb_image](https://github.com/nothings/stb) | 2.30, фиксированный коммит | Чтение PNG/JPEG и других текстур |

GLFW и GLAD уже используются при запуске. GLM доступна через `#include <glm/glm.hpp>`.
stb_image скомпилирована в отдельную библиотеку: достаточно `#include <stb_image.h>`;
повторно определять `STB_IMAGE_IMPLEMENTATION` не нужно.
Источники, точные ревизии и лицензии перечислены в `third_party/README.md`.

## Где писать код

- `src/main.cpp` — инициализация, ввод и главный цикл; место отрисовки отмечено комментарием.
- `src/stb_image.cpp` — единственная реализация загрузчика изображений.
- `CMakeLists.txt` — настройки сборки и подключение библиотек.
- `third_party/` — исходники зависимостей и их лицензии.

Для новых `.cpp` добавлять пути в `add_executable(finalprj ...)`.
Ассеты, шейдеры и игровые системы добавим по мере реализации концепта.
