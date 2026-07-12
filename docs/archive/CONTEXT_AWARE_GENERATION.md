# Context-Aware Image Generation Pipeline

## Overview

The app now supports intelligent, context-aware image generation that analyzes masked areas and generates images that match the surrounding context.

## How It Works

### Pipeline Steps

1. **Mask Selection** (SAM2)
   - Use AI Mask tool to select an area in an image
   - The mask defines where the new image will be generated

2. **Context Analysis** (LLaMA)
   - Extracts visual context from the masked area:
     - Image resolution
     - Masked area dimensions
     - Dominant colors (>5% of pixels)
     - Surrounding image style
   - LLaMA analyzes this data and creates an enhanced prompt

3. **Image Generation** (Stable Diffusion)
   - Generates image with context-aware dimensions
   - Matches colors, style, and perspective
   - Resolution optimized for the masked area size
   - **Automatically composites into masked area**

## Usage

### Basic Workflow

1. **Load an image** into the canvas
2. **Select the image** (click on it)
3. **Use AI Mask tool** to detect/select an area
   - Click "Detect Subject" or "Detect Human"
   - **Optional:** Click "Invert" to select background instead
4. **Generate with context:**
   ```
   create image of a cat
   ```

### What Happens

**Without mask:**
- Standard generation (384x384, generic prompt)
- Image added to canvas at default position

**With mask:**
- 🎭 Step 1: Extracts visual context
- 🧠 Step 2: LLaMA analyzes and enhances prompt
- 🎨 Step 3: Generates contextual image
- 🖼️ Step 4: **Composites into masked area automatically**
- Image dimensions match masked area (rounded to 64px multiples)
- Original image is updated with generated content

## Example Prompts

### Simple
```
create a sunset
generate flowers
make a mountain
```

### Detailed
```
create a realistic cat sitting
generate abstract geometric pattern
make a watercolor landscape
```

### Inverted Mask (Background Fill)
```
# Detect subject → Click "Invert" → Generate
create a beach sunset background
generate bokeh blur effect
make a forest background
```

## Technical Details

### Context Analysis

The system extracts:
- **Resolution:** Original image size
- **Mask bounds:** Width/height of selected area
- **Dominant colors:** Colors that appear in >5% of pixels
- **User intent:** What you want to generate

### LLaMA Enhancement

LLaMA receives:
```
Analyze this image context for AI generation:
- Resolution: 1920x1080
- Masked area size: 512x384
- Dominant colors: #3a5f8c, #e8d4b8, #2c3e50
- User wants to generate: sunset

Create a detailed Stable Diffusion prompt that matches 
the image style, colors, and perspective.
```

LLaMA outputs enhanced prompt:
```
Vibrant sunset with warm orange and pink tones, 
matching blue-toned atmospheric perspective, 
photorealistic style, golden hour lighting, 
cinematic composition, 4k quality
```

### Generation Parameters

- **Resolution:** Matches mask size (min 256, max 512)
- **Rounded to 64px:** For optimal SD performance
- **Steps:** 15 (optimized for CPU)
- **CFG Scale:** 7.0
- **Negative prompt:** "blurry, low quality, distorted"

## Performance

### Without Context (Standard)
- **Time:** ~15-20 seconds
- **Resolution:** 384x384 fixed
- **Quality:** Good

### With Context (Masked)
- **Time:** ~25-35 seconds (includes LLaMA analysis)
- **Resolution:** Adaptive (256-512px)
- **Quality:** Excellent (context-matched)

## Tips

### For Best Results

1. **Clear mask selection**
   - Use high-contrast areas
   - Avoid partial selections

2. **Specific prompts**
   - "realistic cat" > "cat"
   - "watercolor flowers" > "flowers"

3. **Match existing style**
   - System auto-detects colors
   - LLaMA matches art style

4. **Inverted masks for backgrounds**
   - Detect subject first
   - Click "Invert" button
   - Generate background that fits
   - Perfect for replacing backgrounds!

### Troubleshooting

**No context detected:**
- Ensure image is selected
- Check mask is active (green outline)
- Try re-detecting subject

**Generation too slow:**
- Reduce mask area size
- Use simpler prompts
- Standard mode is faster

**Colors don't match:**
- LLaMA analyzes dominant colors
- May need more specific color prompt

## Future Enhancements

- [ ] Inpainting support (fill masked area directly)
- [ ] Style transfer from surrounding area
- [ ] Multi-mask support
- [ ] Real-time preview
- [ ] GPU acceleration (when stable)

## Current Limitations

- CPU-only generation (15-35 seconds)
- Single mask at a time
- No real-time preview
- Max resolution: 512x512

---

**Status:** ✅ Fully implemented and working
**Last Updated:** 2025-10-25
