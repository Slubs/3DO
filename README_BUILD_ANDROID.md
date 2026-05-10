# Инструкция по компиляции opera-libretro для Android

Эта инструкция поможет вам скомпилировать ядро Opera для RetroArch на Android.

## Требования

1. **Android NDK** (версия 21 или выше)
2. **Командная строка** (cmd.exe, PowerShell или терминал)

## Установка NDK (если не установлен)

### Способ 1: Через Android Studio
1. Откройте Android Studio
2. Перейдите в **Tools → SDK Manager**
3. Выберите вкладку **SDK Tools**
4. Отметьте **NDK (Side by side)**
5. Нажмите **Apply**

### Способ 2: Через командную строку
```cmd
cd C:\Users\Slubs\AppData\Local\Android\Sdk\cmdline-tools
sdkmanager "ndk;27.2.12479018"
```

### Способ 3: Прямая загрузка
Скачайте NDK с https://developer.android.com/ndk/downloads и распакуйте в удобное место.

## Компиляция

### Шаг 1: Откройте командную строку
```cmd
cd C:\Users\Slubs\Desktop\opera-libretro-master
```

### Шаг 2: Установите переменную окружения для NDK
```cmd
set NDK_HOME=C:\Users\Slubs\AppData\Local\Android\Sdk\ndk\27.2.12479018
```
*Замените путь на актуальный, если у вас другая версия NDK*

### Шаг 3: Запустите компиляцию
```cmd
%NDK_HOME%\ndk-build -C jni clean
%NDK_HOME%\ndk-build -C jni -j4
```

### Шаг 4: Найдите скомпилированное ядро
После успешной компиляции ядро будет находиться в:
```
libs/arm64-v8a/opera_libretro.so    (для 64-битных устройств)
libs/armeabi-v7a/opera_libretro.so  (для 32-битных устройств)
```

## Установка в RetroArch

1. Скопируйте `opera_libretro.so` в папку cores RetroArch на Android:
   ```
   /Android/data/com.retroarch/files/cores/
   ```

2. Или используйте встроенный загрузчик ядер в RetroArch:
   - Откройте RetroArch
   - Перейдите в **Online Updater → Core Downloader**
   - Найдите и установите Opera (если доступно)
   - Затем замените файл на скомпилированный

## Устранение проблем

### Ошибка: "ndk-build: command not found"
Убедитесь, что NDK установлен и переменная `NDK_HOME` установлена правильно.

### Ошибка компиляции в libretro-common
Проверьте, что все исходные файлы существуют в указанных путях.

### Ошибка: "No rule to make target"
Убедитесь, что вы находитесь в директории `opera-libretro-master` и запускаете `ndk-build` из неё.

## Что было улучшено в этой версии

1. **Поддержка смены дисков** - теперь можно переключаться между дисками в multi-disc играх
2. **Улучшенный звук** - увеличен буфер с 1024 до 4096 сэмплов, добавлена кольцевая буферизация
3. **Проверка BIOS** - эмулятор проверяет наличие BIOS файла перед запуском
4. **Улучшенная обработка ошибок** - добавлена валидация параметров и логирование

## Поддерживаемые форматы образов

- `.iso`
- `.bin`
- `.cue` (с поддержкой multi-disc)
- `.chd`