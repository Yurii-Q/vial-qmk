Полный исходный архив SM62
Commit: 5b64017342e7de53049c525879d822a18b3e67f2
Все gitlinks рекурсивно раскрыты из точных Git-объектов; в том числе LVGL и ChibiOS.
Никаких файлов из изменяемых рабочих деревьев не копировалось. Отдельных непроверенных overlays нет: принятые изменения содержатся в соответствующих commit/gitlink.
Принятый resident100 checkpoint; не экспериментальный кандидат. LVGL 0e00e1a8461e8486ca68648642c27409c633cc35 содержит принятые version-specific изменения.
Это исходники, не готовая прошивка и не проверка компиляции. Сборка/железо/прошивка не запускались.
Требуются внешние инструменты: ARM GNU toolchain, make, Python/QMK requirements (списки включены); компилятор и установленный Python environment не являются исходными submodules и не упакованы.
Историческая 2 MiB prototype и новая 4 MiB storage geometry различаются; совместимость downgrade не квалифицирована.
Git metadata и рабочие/build outputs не включены. Трекаемые upstream binary test fixtures/fonts/resources сохранены как часть точных исходников, не как результаты нашей сборки.
Проверка: PACKAGE-SOURCE-MANIFEST.json содержит каждый исходный путь, Git blob, mode, размер и SHA-256. PACKAGE-PROVENANCE.json перечисляет все раскрытые зависимости.
