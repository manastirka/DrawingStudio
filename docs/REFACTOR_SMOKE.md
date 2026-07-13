# Refactor smoke checklist

Run after each god-object extraction PR.

## Build / tests

```bash
cd build && cmake --build . -j && ctest --output-on-failure
```

## Manual UI

1. New project → draw line + rectangle → undo / redo  
2. Double-click text (if any) → classic text tool activates; toolbar syncs  
3. Format → Advanced text editor (if available) opens via canvas signal  
4. Save `.drawing` → reopen  
5. Export PNG  

## Command server (port 19100)

If bots / image-to-drawing used:

1. `status` endpoint OK  
2. `draw_line` creates geometry  
3. `import_image` → `render_mosaic` / `auto_trace` / `render_photo_copy` (no crash)  
4. Undo after render  

## Mask (if touched)

1. Import image → detect masks  
2. Floating panel open / next / prev mask  


## Last automated run

| Check | Result | When |
|-------|--------|------|
| `cmake --build` + `ctest` (22 tests) | PASS | 2026-07-14 |
| Command server `/api/status` + `draw_line` | PASS (objectCount 0→1) | 2026-07-14 |
| Manual UI (draw/undo/text/save/export) | _operator_ | — |
| Mask flow | _operator_ | — |

Dead code: `ImageAdjustments*.cpp` archived under `docs/archive/` (not built).
