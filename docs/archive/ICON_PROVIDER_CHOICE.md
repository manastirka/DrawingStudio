# Icon Generation Provider Choice

## Overview
Added the ability to choose between **Imagen 3** (Google) and **DALL-E 3** (OpenAI) for icon regeneration in the DrawingStudio application.

## Changes Made

### 1. AISettingsDialog Updates
**Files Modified:**
- `include/AISettingsDialog.h`
- `src/AISettingsDialog.cpp`

**New Features:**
- Added `m_iconProviderCombo` dropdown to select icon generation provider
- Added `m_iconHelpLabel` to display provider-specific information
- New methods:
  - `getIconProvider()` - Get selected icon provider
  - `setIconProvider()` - Set icon provider
  - `onIconProviderChanged()` - Handle provider selection changes

**UI Changes:**
- Icon Generator dropdown with two options:
  - 🎨 Google Imagen 3
  - 🖼️ OpenAI DALL-E 3
- Context-sensitive help text that updates based on selected provider
- Settings are saved to `AI/iconProvider` in QSettings

### 2. IconRegenerator Updates
**Files Modified:**
- `include/IconRegenerator.h`
- `src/IconRegenerator.cpp`

**New Features:**
- Added `m_iconProvider` member variable to store selected provider
- New method: `generateIconWithImagen()` - Generate icons using Google's Imagen 3 API
- New slot: `onImagenGenerationComplete()` - Handle Imagen 3 API responses
- Updated `regenerateIcon()` to use the selected provider

**Implementation Details:**

#### Imagen 3 Integration
- **Endpoint:** `https://generativelanguage.googleapis.com/v1beta/models/imagen-3.0-generate-001:predict`
- **Authentication:** Uses Gemini API key
- **Request Format:**
  ```json
  {
    "instances": [{"prompt": "..."}],
    "parameters": {
      "sampleCount": 1,
      "aspectRatio": "1:1",
      "negativePrompt": "text, words, letters, watermark..."
    }
  }
  ```
- **Response Format:** Base64-encoded image in `predictions[0].bytesBase64Encoded`

#### DALL-E 3 Integration (Existing)
- **Endpoint:** `https://api.openai.com/v1/images/generations`
- **Authentication:** Uses OpenAI API key
- **Response Format:** URL to generated image

## Usage

### For Users
1. Open **AI Settings** dialog
2. Select your preferred **Icon Generator**:
   - **Imagen 3**: Requires Gemini API key (same as main provider if using Gemini)
   - **DALL-E 3**: Requires OpenAI API key
3. Save settings
4. Use icon regeneration feature - it will automatically use your selected provider

### API Key Requirements
- **Imagen 3**: Requires Gemini API key from [Google AI Studio](https://makersuite.google.com/app/apikey)
- **DALL-E 3**: Requires OpenAI API key from [OpenAI Platform](https://platform.openai.com/api-keys)

## Technical Notes

### Provider Selection Logic
The provider is determined at runtime from QSettings:
```cpp
m_iconProvider = settings.value("AI/iconProvider", "imagen").toString();
```

Default provider is **Imagen 3** if not set.

### Icon Generation Flow
1. User triggers icon regeneration
2. System loads `AI/iconProvider` setting
3. Creates optimized prompt for icon generation
4. Calls appropriate API (Imagen 3 or DALL-E 3)
5. Receives generated image
6. Scales to 32x32 pixels
7. Emits `iconRegenerated` signal with new icon

### Error Handling
Both providers include comprehensive error handling:
- API key validation
- Network error handling
- Response parsing validation
- User-friendly error messages with setup instructions

## Benefits
- **Flexibility**: Choose the AI provider that works best for your needs
- **Cost Optimization**: Different providers have different pricing models
- **Quality Comparison**: Test which provider generates better icons for your use case
- **Redundancy**: If one provider is unavailable, switch to the other

## Future Enhancements
Potential improvements:
- Add more providers (Midjourney, Stable Diffusion, etc.)
- Provider-specific prompt optimization
- Batch generation comparison
- Quality/speed preference settings
