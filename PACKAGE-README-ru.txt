Полный исходный архив SM62
Commit: 1e2857fe9ebb3e2dcd3468a1803ac5b4a330de49
Все gitlinks рекурсивно раскрыты из точных Git-объектов; в том числе LVGL и ChibiOS.
Никаких файлов из изменяемых рабочих деревьев не копировалось. Отдельных непроверенных overlays нет: принятые изменения содержатся в соответствующих commit/gitlink.
Историческая framebuffer-база, НЕ точный ранее доставленный test13. Используется старый LVGL e19410f8f8a256609da72cff549598e0df6fa4cf, без новых cooperative overlays.
Это исходники, не готовая прошивка и не проверка компиляции. Сборка/железо/прошивка не запускались.
Требуются внешние инструменты: ARM GNU toolchain, make, Python/QMK requirements (списки включены); компилятор и установленный Python environment не являются исходными submodules и не упакованы.
Историческая 2 MiB prototype и новая 4 MiB storage geometry различаются; совместимость downgrade не квалифицирована.
Git metadata и рабочие/build outputs не включены. Трекаемые upstream binary test fixtures/fonts/resources сохранены как часть точных исходников, не как результаты нашей сборки.
Проверка: PACKAGE-SOURCE-MANIFEST.json содержит каждый исходный путь, Git blob, mode, размер и SHA-256. PACKAGE-PROVENANCE.json перечисляет все раскрытые зависимости.
