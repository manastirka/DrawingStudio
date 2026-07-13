# ImageAdjustments*.cpp — archived dead code

Moved out of `src/` on 2026-07-14 (post E24–E29 cleanup).

These files implemented `MainWindow::show*Adjustment` / blur helpers and are **not** in CMake.
Live path is `ImageAdjustmentController` (+ `ImageAdjustmentControllerBlur.cpp`),
invoked from `MainWindowImageMask.cpp`.

Safe to delete later if no archaeology needed.
