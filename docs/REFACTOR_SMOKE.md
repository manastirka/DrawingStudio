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

## Authenticated command server

The server is off by default. Start the app with a strong token:

```bash
DRAWINGSTUDIO_AUTOMATION_TOKEN='<at-least-16-random-bytes>' \
  ./build/DrawingStudio --enable-automation
```

Every request below requires
`Authorization: Bearer $DRAWINGSTUDIO_AUTOMATION_TOKEN`. Filesystem commands
also require the launch flag `--automation-allow-filesystem`.

1. `GET /api/status` → `"status":"ok"`  
2. `GET /api/commands` (or `/api/help`) → version 2 catalog with unique actions,
   `requiredParams`, `optionalParams`, and `filesystemAccess`
3. `POST /api/command` `draw_line` → geometry + objectCount +1; omitting a
   required coordinate returns an error without dispatching
4. `POST /api/undo` → objectCount back  
5. Optional: `POST /api/batch` with `stopOnError` (max 100 commands)  
6. Optional: `import_image` → `render_mosaic` / `auto_trace` / `render_photo_copy`  

## Mask (if touched)

1. Import image → detect masks  
2. Floating panel open / next / prev mask  

## Last automated run

| Check | Result | When |
|-------|--------|------|
| `cmake --build` + `ctest` (33 tests) | PASS | 2026-07-23 |
| Command API auth, CORS, body/catalog/batch limits | covered by `tst_CommandServer` | 2026-07-22 |
| Command catalog/dispatcher parity and required-parameter validation | covered by `tst_CommandServer` + `tst_DrawingCommandDispatcher` | 2026-07-22 |
| Object align/distribute | covered by `tst_ObjectLayoutOps` | 2026-07-15 |
| AI connection missing-key paths | covered by `tst_AIImageClient` | 2026-07-15 |
| AI connection-test drafts remain unsaved; Remote SD URL validation/default | covered by `tst_AIImageClient` | 2026-07-22 |
| AI generation redirect refusal, deadlines, and 128 MiB response cap | covered by `tst_AIImageClient` | 2026-07-22 |
| Remote SD same-thread async networking, deadlines, response caps, and strict image decoding | covered by `tst_RemoteSDHelper` | 2026-07-23 |
| Authenticated command server smoke script | _run after app rebuild_ | — |
| SAM2 loopback binding, bearer auth, protocol marker, and request limit | covered by SAM2 security tests | 2026-07-22 |
| SAM2 deadlines, response caps, strict mask decoding, and result budgets | covered by `tst_YOLOIntegration` | 2026-07-23 |
| Atomic, validated SAM mask cache and corrupt-entry removal | covered by `tst_MaskCache` | 2026-07-22 |
| Image resize bounds, aspect lock, mask-state restoration, and bounded refinements | covered by `tst_ImagePrimitiveResize` | 2026-07-22 |
| Strict/bounded embedded PNG decoding and corrupt-image load preservation | covered by `tst_ImagePrimitiveResize` + `tst_ProjectFileService` | 2026-07-23 |
| Bounded vector geometry, coordinate schema validation, and curve parameter clamps | covered by `tst_PrimitiveRoundTrip` + `tst_ProjectFileService` | 2026-07-23 |
| Common style/transform bounds and malformed style rejection | covered by `tst_PrimitiveRoundTrip` | 2026-07-23 |
| Finite/range-checked scalar geometry for shapes, dimensions, text, and images | covered by `tst_PrimitiveRoundTrip` | 2026-07-23 |
| Strict layer/canvas metadata schema, bounded names, and finite opacity | covered by `tst_LayerManager` + `tst_ProjectFileService` | 2026-07-23 |
| Bounded text content/formatting schema and strict nested effects | covered by `tst_PrimitiveRoundTrip` | 2026-07-23 |
| Strict image-mask candidate/selection schema and total contour budgets | covered by `tst_ImagePrimitiveResize` + `tst_ProjectFileService` | 2026-07-23 |
| Strict UUID fields and duplicate layer/primitive identity rejection | covered by `tst_PrimitiveRoundTrip` + `tst_ProjectFileService` | 2026-07-23 |
| Finite direct setters and recursively bounded automation parameters | covered by `tst_PrimitiveRoundTrip` + `tst_DrawingCommandDispatcher` | 2026-07-23 |
| Atomic/bounded project persistence, rejected-load preservation, and recovery isolation | covered by `tst_ProjectFileService` | 2026-07-22 |
| Primitive JSON round-trip | covered by `tst_PrimitiveRoundTrip` | 2026-07-14 |
| Silent autosave session callbacks | covered by `tst_ProjectFileService` | 2026-07-14 |
| Manual UI (items 1–8) | _operator_ | — |
| AI Settings → Test Connection | _operator_ | — |

Dead code: `docs/archive/ImageAdjustments*.cpp` (not built).
