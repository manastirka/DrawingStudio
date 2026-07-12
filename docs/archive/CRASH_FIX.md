# Memory Crash Fix

## Issue
App was crashing with memory error after AI generation:
```
malloc: *** error for object 0xa06db5800: pointer being freed was not allocated
```

## Root Cause
We were calling `llama_batch_free()` on a batch created with `llama_batch_get_one()`.

The `llama_batch_get_one()` function returns a **view** into existing data, not an allocated batch. It should NOT be freed.

## Fix Applied
Removed the `llama_batch_free(batch)` calls after using `llama_batch_get_one()`.

### Before:
```cpp
llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
if (llama_decode(m_context, batch) != 0) {
    llama_batch_free(batch);  // ❌ WRONG - causes crash
    return "Error";
}
llama_batch_free(batch);  // ❌ WRONG - causes crash
```

### After:
```cpp
llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
if (llama_decode(m_context, batch) != 0) {
    return "Error";  // ✅ CORRECT - no free needed
}
// ✅ CORRECT - no free needed
```

## Status
✅ **FIXED** - Rebuilt successfully

## Test Again
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
./DrawingStudio
# Press Ctrl+H
# Ask: "How do I draw a circle?"
# Should work without crashing now!
```

The AI Assistant should now work properly without memory crashes! 🎉
