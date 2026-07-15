# Refactor smoke checklist

Run after each god-object extraction PR or feature polish drop.

## Build / tests

```bash
cd build && cmake --build . -j && QT_QPA_PLATFORM=offscreen ctest --output-on-failure
```

Optional API smoke (starts the app if needed):

```bash
./scripts/smoke_api.sh
```

## Manual UI

1. New project → draw line + rectangle → undo / redo  
2. Double-click text → classic text tool + SimpleTextPanel sync  
3. Format → Advanced text editor (if available)  
4. Save `.drawing` → reopen  
5. Export PNG (progress dialog should appear on large canvases)  
6. Dirty project → wait ~2 min → status “Autosaved recovery”  
7. Kill app → restart → restore recovery prompt  
8. AI generate/edit → **AI → Cancel AI Job** (`Ctrl+Shift+.`) → status cancelled, no crash  

## Command server (port 19100)

1. `GET /api/status` → `"status":"ok"`  
2. `GET /api/commands` (or `/api/help`) → catalog with `commandCount` + `draw_line`  
3. `POST /api/command` `draw_line` → geometry + objectCount +1  
4. `POST /api/undo` → objectCount back  
5. Optional: `POST /api/batch` with `stopOnError` (max 100 commands)  
6. Optional: `import_image` → `render_mosaic` / `auto_trace` / `render_photo_copy`  

## Mask (if touched)

1. Import image → detect masks  
2. Floating panel open / next / prev mask  

## Last automated run

| Check | Result | When |
|-------|--------|------|
| `cmake --build` + `ctest` (30 tests) | PASS | 2026-07-15 |
| Command API catalog + batch limits | covered by `tst_CommandServer` | 2026-07-15 |
| Object align/distribute | covered by `tst_ObjectLayoutOps` | 2026-07-15 |
| AI connection missing-key paths | covered by `tst_AIImageClient` | 2026-07-15 |
| Command server smoke script | PASS (catalog + draw + undo) | 2026-07-15 |
| Primitive JSON round-trip | covered by `tst_PrimitiveRoundTrip` | 2026-07-14 |
| Silent autosave session callbacks | covered by `tst_ProjectFileService` | 2026-07-14 |
| Manual UI (items 1–8) | _operator_ | — |
| AI Settings → Test Connection | _operator_ | — |

Dead code: `docs/archive/ImageAdjustments*.cpp` (not built).
