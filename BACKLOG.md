# Backlog

## UI/UX Improvements
- [ ] Optimize client ComboBox population:
  - Currently, `provideFillClientComboBox` is emitted on every `loadEvent()`, causing the entire client list to be reloaded from the database each time.
  - **Goal:** Avoid repeated database calls on each event click.
  - **Possible solutions:**
    1. Use `QClientModel` as a `QAbstractListModel` for the ComboBox, so updates propagate automatically.
    2. Keep using a plain `QComboBox`, but:
       - Fill it once at initialization,
       - Update it only when `clientsUpdated()` signal is emitted from `Application`,
       - Maintain a `QHash<obx_id,int>` for fast index lookup.
  - If the number of clients becomes very large, consider lazy loading or a search-driven UI (e.g., `QCompleter`).
  - **Priority:** Medium (important for performance, but current functionality works).
