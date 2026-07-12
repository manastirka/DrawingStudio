# Stable Diffusion Status

## Current Solution: CPU-Only (Stable & Reliable)

After extensive testing, the app now uses **CPU-only ggml backend** for Stable Diffusion.

### Why CPU-Only?

| Backend | Speed | Stability | Image Quality | Status |
|---------|-------|-----------|---------------|--------|
| **ggml CPU** | 🐌 30-60s | ✅ **Stable** | ✅ **Perfect** | ✅ **Active** |
| ggml Metal | ⚡ Fast | ❌ **Crashes** | N/A | ❌ Disabled |
| Core ML (MPS) | ⚡ 3-8s | ⚠️ Unstable | ❌ **Black images** | ❌ Disabled |

### Issues Encountered:

1. **ggml Metal Backend**
   - Crashes with `ggml_abort` from `ggml_metal_op_encode`
   - Unsupported Metal operations
   - Happens with F32 GGUF and .ckpt models
   - Bug in stable-diffusion.cpp library

2. **Core ML / MPS Backend**
   - Generates black/corrupted images
   - Known PyTorch MPS backend bug
   - Affects many users on macOS
   - No reliable workaround found

3. **CPU Backend (Current)**
   - ✅ Stable and reliable
   - ✅ Generates perfect images
   - ⚠️ Slower (30-60 seconds per image)
   - ✅ No crashes or black images

## Current Setup

**Model:** SD 1.5 (.ckpt or .gguf format)
**Backend:** ggml CPU-only
**Performance:** 30-60 seconds per 512x512 image
**Quality:** Excellent
**Stability:** 100% reliable

## How to Use

1. Place SD 1.5 model in `models/` folder:
   - `v1-5-pruned-emaonly.ckpt` (recommended)
   - `sd-v1-5.ckpt`
   - `sd-v1-5-q4_0.gguf`

2. Run DrawingStudio

3. Wait for: `✓ All AI models loaded`

4. Generate images via AI Assistant:
   ```
   create image of sunset
   ```

5. Wait 30-60 seconds

6. Image appears on canvas!

## Performance Tips

While CPU mode is slower, you can:
- ✅ Use lower step counts (15 instead of 20) = faster
- ✅ Generate smaller images (256x256) = much faster
- ✅ Run in background while working on other things
- ✅ Be patient - quality is worth the wait!

## Future Improvements

Potential solutions being monitored:
- [ ] Wait for stable-diffusion.cpp Metal fixes
- [ ] Wait for PyTorch MPS black image fixes
- [ ] Use external API (Replicate, Fal.ai)
- [ ] Implement ComfyUI backend
- [ ] Add batch generation queue

## Conclusion

**CPU mode works perfectly** - it's just slower. For a drawing app where you're spending minutes on each piece anyway, 30-60 seconds for AI generation is acceptable.

The alternative (crashes or black images) is not acceptable, so CPU mode is the right choice for now.

---

**Status:** ✅ Working reliably with CPU backend
**Last Updated:** 2025-10-25
