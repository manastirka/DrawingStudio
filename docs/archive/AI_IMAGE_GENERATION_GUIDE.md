# AI Image Generation - User Guide

## Overview
The AI image generation feature has been streamlined to focus on creating images within your drawings, not for generating app icons.

## What Changed

### Removed Features
- ❌ **Regenerate App Icons** - Removed from AI menu
- ❌ **Extract Icons from Image** - Removed from AI menu  
- ❌ **Use Image as Template** - Removed from AI menu
- ❌ **Restore Default Icons** - Removed from AI menu

All icon-related AI features have been removed to simplify the interface and focus on drawing-related AI functionality.

### Enhanced Features
- ✅ **AI Image Generation** - Now supports two modes
- ✅ **Mode Selection** - Choose how to insert generated images

## How to Use AI Image Generation

### Step 1: Open AI Prompt
Click the **AI** button in the toolbar or go to **AI → AI Settings** to configure your API keys.

### Step 2: Enter Your Prompt
In the AI prompt panel on the right side:
1. Type a description of the image you want to generate
2. Choose a generation mode (see below)
3. Click **Generate Image**

### Generation Modes

#### Mode 1: 🖼️ Replace Selected Object
**What it does:**
- Replaces the currently selected object with the AI-generated image
- The new image matches the size and position of the selected object
- The original object is deleted

**How to use:**
1. Select an object on the canvas (rectangle, ellipse, line, etc.)
2. Enter your AI prompt
3. Select "🖼️ Replace Selected Object" mode
4. Click "Generate Image"
5. The selected object will be replaced with the generated image

**Example:**
- Draw a rectangle
- Select it
- Prompt: "a beautiful sunset over mountains"
- Result: Rectangle is replaced with a sunset image

#### Mode 2: 🎨 Fill Closed Shape
**What it does:**
- Generates an image inside a closed shape
- The original shape is preserved
- Image is positioned and sized to fit within the shape bounds

**How to use:**
1. Draw a closed shape (rectangle, ellipse, closed spline, or closed curve)
2. Select the shape
3. Enter your AI prompt
4. Select "🎨 Fill Closed Shape" mode
5. Click "Generate Image"
6. The image appears inside the shape, and the shape remains visible

**Example:**
- Draw an ellipse
- Select it
- Prompt: "a galaxy with stars"
- Result: Galaxy image appears inside the ellipse, ellipse outline remains

### Supported Shapes
Both modes work with:
- ✅ Rectangles
- ✅ Ellipses
- ✅ Closed Splines
- ✅ Closed Curves
- ✅ Any other drawable object

## API Providers

### Supported Providers
1. **OpenAI DALL-E** (Recommended)
   - Model: DALL-E 3
   - Size: 1024x1024
   - Best quality and reliability

2. **Stability AI**
   - Model: Stable Diffusion XL
   - Size: 1024x1024
   - Good alternative to DALL-E

3. **Google Gemini**
   - Note: Gemini doesn't directly generate images
   - Can be used to enhance prompts
   - Switch to OpenAI or Stability for actual generation

### Configuration
Go to **AI → AI Settings** to:
- Set your API key
- Choose your provider
- Select the model

## Tips for Best Results

### Writing Good Prompts
- **Be specific**: "a red sports car on a mountain road at sunset"
- **Include style**: "photorealistic", "oil painting", "cartoon style"
- **Describe details**: colors, lighting, mood, composition
- **Keep it concise**: 1-2 sentences usually work best

### Shape Selection
- **Replace mode**: Works with any object
- **Fill mode**: Best with closed shapes (rectangles, ellipses)
- Make sure the shape is selected before generating

### Image Quality
- Generated images are 1024x1024 pixels
- They scale to fit your selected object
- Larger objects = better visible detail
- Smaller objects = image is scaled down

## Workflow Examples

### Example 1: Create a Framed Picture
1. Draw a rectangle for the frame
2. Select it
3. Mode: "🎨 Fill Closed Shape"
4. Prompt: "a serene lake with mountains"
5. Result: Picture inside the frame

### Example 2: Replace Placeholder
1. Draw a placeholder rectangle
2. Select it
3. Mode: "🖼️ Replace Selected Object"
4. Prompt: "company logo with blue and white colors"
5. Result: Logo replaces the placeholder

### Example 3: Create a Circular Portrait
1. Draw a circle
2. Select it
3. Mode: "🎨 Fill Closed Shape"
4. Prompt: "portrait of a person, professional headshot"
5. Result: Portrait inside the circle

## Troubleshooting

### "No Selection" Error
**Problem**: No object selected to replace/fill
**Solution**: Select an object on the canvas first

### "API Key Missing" Error
**Problem**: No API key configured
**Solution**: Go to AI → AI Settings and enter your API key

### "Failed to Generate Image" Error
**Problem**: API request failed
**Solution**: 
- Check your API key is valid
- Check your internet connection
- Verify you have API credits/quota remaining
- Try a different provider

### Image Doesn't Appear
**Problem**: Generation succeeded but image not visible
**Solution**:
- Check if the object is on a visible layer
- Check if the object is within the canvas bounds
- Try zooming out to see the full canvas

### Poor Image Quality
**Problem**: Generated image looks bad
**Solution**:
- Improve your prompt (be more specific)
- Try a different AI provider
- Generate multiple times with variations
- Use larger shapes for better detail

## Keyboard Shortcuts

- **AI Settings**: None (use menu)
- **Generate**: Click button in AI panel
- **Cancel**: Close AI panel

## Future Enhancements

Potential features for future versions:
- Multiple image generation (variations)
- Image editing/refinement
- Style transfer
- Inpainting (edit parts of generated images)
- Batch generation
- Custom size options
- Image-to-image generation

## API Costs

### OpenAI DALL-E 3
- ~$0.04 per image (1024x1024)
- ~$0.08 per image (1024x1792 or 1792x1024)

### Stability AI
- ~$0.01-0.02 per image
- Varies by model and settings

### Google Gemini
- Text generation: Free tier available
- Image generation: Not directly supported

**Note**: Check current pricing on provider websites as costs may change.

## Privacy & Data

- Your prompts are sent to the AI provider
- Generated images are returned via API
- No data is stored on our servers
- Images are only saved in your project file
- API keys are stored locally in your settings

## Support

For issues or questions:
1. Check this guide first
2. Verify API configuration
3. Check console output for errors
4. Try with a simple prompt first
5. Test with different providers

## Summary

The AI image generation feature is now focused on enhancing your drawings:
- ✅ Two flexible generation modes
- ✅ Works with any drawable shape
- ✅ Multiple AI provider support
- ✅ Simple, intuitive interface
- ✅ Integrated into your drawing workflow

No more icon generation clutter - just pure drawing-focused AI assistance!
