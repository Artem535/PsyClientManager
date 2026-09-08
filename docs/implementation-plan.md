# PsyClientManager — implementation plan

> Рабочий инженерный план для исполнения roadmap. Стратегические цели и границы
> продукта находятся в [roadmap.md](roadmap.md).

## Правила исполнения

- Каждая задача должна давать проверяемый результат и отдельный набор тестов.
- Изменения схемы выполняются только после успешного backup-теста.
- UI не обращается к SQL напрямую: запись проходит через существующий `Database`
  API, а новые подсистемы постепенно выносятся в сервисы.
- Рекуррентные серии, virtual occurrences, overrides и exceptions сохраняют
  текущую семантику.
- До `1.0` приложение остаётся local-first: интернет не является обязательным.
- Не логировать PII, тексты заметок, токены, ключи и полные meeting URL.
- Каждая версия заканчивается сборкой Linux/Windows/macOS, проверкой миграций,
  `git diff --check` и обновлением README, CHANGELOG и AsciiDoc.

## Зависимости этапов

```text
0.1.30 current baseline
        ↓
0.2.0 core workflow + identity + schema metadata
        ↓
0.3.0 client history
        ↓
0.4.0 export + encrypted backup
        ↓
0.5.0 user-owned cloud backup
        ↓
0.6.0 local encryption
        ↓
0.7.0 calendar integration
        ↓
0.8.0 snapshot sync beta
        ↓
0.9.0 release candidate
        ↓
1.0.0 stable release
        ↓
1.1.0 embedded Jitsi sessions
        ↓
1.2.0 consent-aware recording and local transcription
```

## Этап 0.1.6 — стабилизация

### Task 0.1: Inventory текущих контрактов

Файлы для проверки: `src/event_view/`, `src/database/`, `src/client_model/`,
`src/pages/`, `test/`, `docs/asciidoc/06-database-and-data-model.adoc`.

Составить таблицу текущих полей события, статусов, ролей моделей, операций серии,
уведомлений и аналитики. Зафиксировать, какие значения являются source data,
relation data и derived data.

Проверка: все UI-потоки create/edit/delete события и серии имеют описанный
источник данных и ожидаемое уведомление обновления.

### Task 0.2: Нормализовать статусы и конфликт расписания

Создать доменные перечисления для event status и confirmation status, сохранив
отдельный payment status. Вынести проверку пересечений в
`ScheduleConflictService`, который учитывает обычные события, virtual occurrences,
overrides, exceptions и будущие буферы.

Ожидаемые тесты:

- пересечение обычных событий;
- пересечение с virtual occurrence;
- исключение occurrence из серии;
- изменение одной встречи без ложного конфликта с собой;
- `this and future` не конфликтует с исходной серией после split.

### Task 0.3: Устранить остаточные UI и logging defects

Проверить `src/event_view/`, `src/widgets/`, `resources/`, переводческие файлы и
Qlementine patch. Исправить popup sizing, stylesheet warnings, подписи RU/EN,
иконки, приватные уведомления и напоминания серий.

Проверка: ручной smoke-test календаря, формы события, Timeline, аналитики и
settings на Linux; `git diff --check`; production-логи не содержат PII.

### Task 0.4: Release gate 0.1.6

Обновить `CMakeLists.txt`, `src/app/application.cpp`, `CHANGELOG.md`, README и
сайт. Собрать `build-release` и проверить CI workflow.

## Этап 0.2.0 — core workflow, identity and schema metadata

### Task 0.2.1: Ввести идентификаторы workspace и device

Файлы: `src/database/`, `src/config/`, миграции схемы и
`docs/asciidoc/06-database-and-data-model.adoc`.

Добавить стабильные UUID для workspace, device, client, event, event series, note,
attachment и backup. Числовые ключи не удалять. Значения создавать только один
раз и сохранять в локальной базе или конфигурации.

Проверка: перезапуск приложения и backup/restore не меняют UUID; старые записи
получают UUID детерминированно в одной миграции.

### Task 0.2.2: Добавить ApplicationMetadata и migration runner

Добавить `schema_version`, `backup_format_version`, `workspace_uuid`, даты создания
и последней миграции. Каждая миграция проверяет исходную версию, выполняется в
транзакции и безопасно повторяется после прерывания.

Проверка: тесты миграции с предыдущей схемы, повторный запуск, повреждённая
исходная версия и backup перед миграцией.

### Task 0.2.3: Усилить контракт BackupService

Файлы: новый сервис в `src/database/` или `src/data_protection/`, тесты в `test/`.

Проверить и закрепить формат `.psybackup` с `manifest.json`, согласованным
snapshot DuckDB, `attachments/` и `checksums.json` на границе миграций. Использовать
временный файл и атомарное переименование. Открытый файл DuckDB не копировать
напрямую.

Проверка: backup базы без вложений, backup с вложениями, checksum mismatch,
прерванная запись и запрет удаления последней рабочей копии.

### Task 0.2.4: Усилить RestoreService и validator

Восстанавливать сначала во временный каталог, проверять manifest, schema version,
checksums и вложения, затем менять рабочий каталог. Перед restore создавать
защитный backup текущего состояния.

Проверка: полный round-trip backup → restore → чтение клиента, события, серии,
заметки и вложения; ошибка не изменяет текущую базу.

### Task 0.2.5: Release gate 0.2.0

Обновить документацию `docs/backup-restore.md`, раздел data model, CHANGELOG и
README. Выполнить end-to-end restore test и migration test matrix.

## Этап 0.3.0 — история клиента

### Task 0.3.1: Расширить модель заметок

Добавить `updated_at`, `deleted_at`, `revision`, `content_format`, `pinned` и
стабильный UUID. Существующие заметки мигрировать без потери вложений.

Проверка: создание, редактирование, черновик, отмена несохранённых изменений,
миграция и восстановление из backup.

### Task 0.3.2: Добавить NoteRevision

Хранить предыдущие версии текста, дату, revision и device UUID. Ограничить
операцию восстановления транзакцией.

Проверка: две редакции, восстановление первой версии, конфликт revision и
повторный запуск после сбоя.

### Task 0.3.3: Добавить ClientTask и теги

Реализовать задачи клиента, due date, status, связь с событием и фильтрацию по
тегам. Не смешивать задачи клиента с event status.

Проверка: задача отображается в карточке клиента, общем списке и дневном списке;
удаление клиента не оставляет активную задачу без владельца.

### Task 0.3.4: Собрать ClientTimeline

Создать сервис или projection, объединяющий события, заметки, вложения, оплаты,
отмены и задачи. Сохранить отдельные специализированные модели до завершения
миграции на общую модель.

Проверка: последняя и следующая встреча, незавершённая заметка, неоплаченная
встреча и потерянное вложение отображаются корректно.

## Этап 0.4.0 — экспорт и encrypted backup

### Task 0.4.1: Спроектировать versioned export formats

Создать `docs/export-format.md` с версиями JSON, правилами CSV и mapping ICS.
Определить предупреждение о чувствительных данных и выбор состава экспорта.

### Task 0.4.2: Реализовать export/import services

Добавить `ClientExportService`, `CalendarExportService`,
`AnalyticsExportService` и `ImportService`. Импорт делать через preview и отчёт
ошибок по отдельным строкам.

Проверка: клиент в Markdown/HTML/JSON, календарь в ICS с recurrence, аналитика
в CSV, CSV import без тихого пропуска ошибочных записей.

### Task 0.4.3: Зафиксировать crypto design

Создать ADR с выбором libsodium, Argon2id, authenticated encryption, форматом
зашифрованного backup и правилами удаления старой незашифрованной копии.

До принятия ADR не реализовывать собственную криптографию.

### Task 0.4.4: Реализовать encrypted backup

Зашифровать backup целиком, добавить проверку пароля, recovery workflow и
обнаружение изменения ciphertext. Ключи и пароли не писать в логи.

Проверка: неверный пароль, повреждённый ciphertext, восстановление на другом
каталоге и сохранение предыдущей рабочей копии.

## Этапы 0.5.0–0.8.0 — cloud, local encryption, calendar, sync

Для этих этапов сначала создавать отдельный design/ADR документ и spike. Реализация
начинается только после проверки внешнего API и отказоустойчивого поведения.

### Task 0.5: User-owned cloud backup

Начать с `LocalDirectoryProvider`, immutable remote files, manifest generation,
retry и проверки загрузки. Ошибка cloud storage не должна блокировать локальное
приложение.

### Task 0.6: Local encryption

Провести отдельный spike по поддерживаемому DuckDB encryption. Затем реализовать
keychain, миграцию существующей базы и encrypted attachment storage с временным
открытием файлов.

### Task 0.7: Calendar integration

Начать с ICS и подготовленных сообщений. CalDAV/Google/Outlook подключать только
после описания recurrence mapping, external IDs и conflict policy.

### Task 0.8: Snapshot sync beta

Реализовать workspace/device UUID, generation, remote manifest, content hash,
загрузку более нового snapshot и conflict resolver. Никогда не синхронизировать
рабочий файл DuckDB напрямую.

## Этап 0.9.0 — release candidate

Проверить:

- миграции от всех поддерживаемых предыдущих версий;
- полный backup/restore round-trip;
- encrypted backup и recovery;
- экспорт/import;
- recurrence, override и exception workflows;
- Windows, Linux и macOS packaging;
- отсутствие PII в production-логах;
- русский и английский UI;
- клавиатурную навигацию и приватные уведомления.

Критические дефекты блокируют `1.0.0`.

## Этап 1.0.0 — release gate

Версия `1.0.0` выпускается только при наличии стабильных миграций, проверенного
restore, encrypted backup, экспорта, удаления/архивирования данных, документации
восстановления и поддерживаемых desktop-пакетов.

PsyNote, OCR, AI, realtime sync, командные аккаунты и собственный backend не
являются условиями выпуска.

## После 1.0 — встроенные онлайн-сессии

### Task 1.1.1: Провести Jitsi integration spike

Зафиксировать в ADR поддерживаемую модель развёртывания: настраиваемый
self-hosted Jitsi или managed deployment. Проверить Qt WebEngine и Jitsi IFrame
API на Linux, Windows и macOS: аудио, видео, выбор устройств, потерю сети,
закрытие конференции и освобождение WebEngine-профиля.

Проверка: spike не передаёт имя клиента, заметки или финансовые данные в Jitsi;
комната не остаётся доступной из памяти приложения после закрытия окна;
пользователь может продолжить работу с событием при ошибке подключения.

### Task 1.1.2: Реализовать OnlineSessionService

Добавить сервис для создания непредсказуемого room ID, настройки домена,
безопасного запуска и завершения звонка. Сохранить существующий `meeting_url`
как fallback для внешних провайдеров. JWT и другие краткоживущие секреты не
сохранять в базе, backup или логах.

Проверка: событие создаёт и повторно открывает свою Jitsi-комнату, внешний URL
остаётся работоспособен, а WebEngine-сессия удаляется после звонка.

### Task 1.2.1: Добавить согласие, запись и транскрибацию

Сначала ввести подтверждаемое специалистом согласие на запись и политику
удаления. Затем добавить метаданные записи и локальную очередь распознавания
на базе `whisper.cpp`. Транскрипт является редактируемым черновиком, связанным
с событием; он не становится автоматически клинической заметкой.

Проверка: запись нельзя начать без согласия; отмена транскрибации не повреждает
исходный файл; удаление соблюдает retention policy; аудио и текст не появляются
в production-логах.

## Документы по этапам

- `docs/roadmap.md` — продуктовые цели и последовательность релизов;
- `docs/implementation-plan.md` — этот инженерный план;
- `docs/asciidoc/08-recursive-model-migration.adoc` — миграция к recursive model;
- `docs/asciidoc/09-recurring-events.adoc` — семантика recurring events;
- `docs/backup-restore.md` — формат и операции backup/restore для `0.2.0`;
- `docs/export-format.md` — стабильные export/import форматы для `0.4.0`;
- ADR по crypto, DuckDB encryption, cloud provider, snapshot sync и Jitsi;
- `docs/online-sessions.md` — границы данных, согласие, запись и транскрибация;
- `CHANGELOG.md` и README — пользовательские изменения и release instructions.

## Definition of Done для задачи

Задача считается завершённой, если:

1. изменены только относящиеся к задаче файлы;
2. добавлен regression test или зафиксирован обоснованный manual test;
3. миграция имеет номер и rollback/recovery сценарий, если меняется схема;
4. UI имеет RU/EN перевод, если добавляется пользовательский текст;
5. `git diff --check` и сборка проходят;
6. документация и CHANGELOG обновлены на границе релиза;
7. изменение не записывает PII в production-логи.
