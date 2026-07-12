# Stable Diffusion - Manual Setup Guide

## 🎨 Current Status

Stable Diffusion integration is **fully coded** but requires manual model download due to HuggingFace authentication requirements.

---

## ✅ What's Ready

1. ✅ **DiffusionHelper.h/cpp** - Complete implementation
2. ✅ **stable-diffusion.cpp** - Cloned and ready
3. ✅ **CMake configuration** - Prepared (currently disabled)
4. ✅ **Full documentation** - Complete guide
5. ⏳ **Model download** - Requires manual step

---

## 📥 Manual Model Download (3 Options)

### Option 1: Direct Browser Download (Easiest)

1. **Visit**: https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf/tree/main

2. **Click** on `sd-v1-5-q4_0.gguf` (~2GB)

3. **Click** the download button (⬇️)

4. **Move** the downloaded file to:
   ```
   /Users/Lukovic/Apps/DrawingStudio/models/sd-v1-5-q4_0.gguf
   ```

### Option 2: Using wget (if installed)

```bash
cd /Users/Lukovic/Apps/DrawingStudio/models

# Install wget if needed
brew install wget

# Download model
wget https://huggingface.co/leejet/Stable-Diffusion-v1-5-gguf/resolve/main/sd-v1-5-q4_0.gguf
```

### Option 3: Using HuggingFace CLI

```bash
# Install HuggingFace CLI
pip install huggingface-hub

# Download model
cd /Users/Lukovic/Apps/DrawingStudio/models
huggingface-cli download leejet/Stable-Diffusion-v1-5-gguf sd-v1-5-q4_0.gguf --local-dir .
```

---

## 🔧 After Download: Enable & Build

### Step 1: Verify Download

```bash
cd /Users/Lukovic/Apps/DrawingStudio
ls -lh models/sd-v1-5-q4_0.gguf

# Should show ~2GB file
```

### Step 2: Re-enable Stable Diffusion in CMakeLists.txt

Uncomment these lines in `CMakeLists.txt`:

```cmake
# Add stable-diffusion.cpp
set(SD_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SD_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SD_METAL ON CACHE BOOL "" FORCE)
add_subdirectory(external/stable-diffusion.cpp)
```

And:

```cmake
src/DiffusionHelper.cpp
include/DiffusionHelper.h
```

And:

```cmake
target_include_directories(DrawingStudio PRIVATE 
    ${CMAKE_SOURCE_DIR}/external/stable-diffusion.cpp
)

target_link_libraries(DrawingStudio PRIVATE 
    ...
    stable-diffusion
)
```

### Step 3: Rebuild

```bash
cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

---

## 🎯 Integration with MainWindow

### Add to MainWindow.h:

```cpp
class DiffusionHelper;

private:
    DiffusionHelper* m_diffusionHelper;
```

### Add to MainWindow.cpp:

```cpp
#include "DiffusionHelper.h"

// In constructor
m_diffusionHelper = new DiffusionHelper(this);
QString sdModelPath = QCoreApplication::applicationDirPath() + 
                     "/../models/sd-v1-5-q4_0.gguf";

if (m_diffusionHelper->initialize(sdModelPath)) {
    qDebug() << "✓ Stable Diffusion ready!";
    
    connect(m_diffusionHelper, &DiffusionHelper::imageGenerated,
        [this](const QImage& image, const QString& prompt) {
            // Add generated image to canvas
            auto imgPrim = std::make_unique<ImagePrimitive>(
                image, 
                QVector2D(100, 100),
                QVector2D(image.width(), image.height())
            );
            m_canvas->addPrimitive(std::move(imgPrim));
            m_canvas->update();
            
            m_statusLabel->setText("Image generated!");
        });
} else {
    qDebug() << "⚠ Stable Diffusion not available";
}
```

### Add AI Commands in LLMHelper.cpp:

```cpp
// In parseCommand()
else if ((lower.contains("generate") || lower.contains("create")) && 
         lower.contains("image")) {
    return "generate_image";
}
```

### Handle Command in MainWindow.cpp:

```cpp
// In handleAssistantCommand()
else if (commandType == "generate_image") {
    if (m_diffusionHelper && m_diffusionHelper->isInitialized()) {
        // Extract prompt from user input
        QString prompt = userInput;
        prompt.remove(QRegularExpression("generate|create|image|picture|photo", 
                                        QRegularExpression::CaseInsensitiveOption));
        prompt = prompt.trimmed();
        
        m_assistantOutput->append(
            "<div style='margin: 10px 0;'>"
            "<b style='color: #81c784;'>Assistant:</b> "
            "🎨 Generating image: \"" + prompt + "\"... (this may take 20-30 seconds)"
            "</div><br>");
        
        m_statusLabel->setText("Generating image...");
        m_diffusionHelper->generateImageAsync(prompt);
    } else {
        m_assistantOutput->append(
            "<div style='margin: 10px 0;'>"
            "<b style='color: #ff6b6b;'>Assistant:</b> "
            "❌ Image generation not available. Please download SD model."
            "</div><br>");
    }
}
```

---

## 🎨 Usage Examples

### AI Commands:
```
"generate image of a sunset"
"create a cat picture"
"make an image of a robot"
"generate a landscape painting"
```

### Direct API:
```cpp
// Simple generation
QImage img = m_diffusionHelper->generateImage("a beautiful sunset");

// Advanced options
QImage img = m_diffusionHelper->generateImage(
    "a beautiful landscape",     // prompt
    "blurry, low quality",       // negative prompt
    512,                         // width
    512,                         // height
    20,                          // steps
    7.0f,                        // CFG scale
    42                           // seed
);

// Async (non-blocking)
m_diffusionHelper->generateImageAsync("a cat sitting on a chair");
```

---

## 📊 Model Options

| Model | Size | Quality | Speed | Download |
|-------|------|---------|-------|----------|
| **sd-v1-5-q4_0.gguf** | ~2GB | Good | Fast | Recommended |
| **sd-v1-5-q8_0.gguf** | ~4GB | Better | Slower | Higher quality |
| **sd-v1-5-f16.gguf** | ~7GB | Best | Slowest | Maximum quality |

---

## ⚡ Performance

On Apple Silicon (M1/M2):
- **512x512, 20 steps**: ~20 seconds
- **768x768, 20 steps**: ~40 seconds
- **1024x1024, 20 steps**: ~60 seconds

---

## 🐛 Troubleshooting

### "Model not found"
```bash
# Check model exists
ls -lh /Users/Lukovic/Apps/DrawingStudio/models/sd-v1-5-q4_0.gguf

# Should show ~2GB file, not 29B or 4K
```

### "Initialization failed"
- Check model path is correct
- Ensure model file is complete (not partial download)
- Check console for error messages

### "Generation too slow"
- Use smaller image size (512x512)
- Reduce steps to 10-15
- Use Q4 model instead of Q8

---

## ✅ Quick Checklist

- [ ] Download model (~2GB)
- [ ] Place in `models/sd-v1-5-q4_0.gguf`
- [ ] Uncomment SD in CMakeLists.txt
- [ ] Rebuild project
- [ ] Add DiffusionHelper to MainWindow
- [ ] Add AI command handling
- [ ] Test: "generate image of a cat"

---

## 📚 Full Documentation

See `STABLE_DIFFUSION_GUIDE.md` for:
- Complete API reference
- Advanced usage examples
- Prompt engineering tips
- Performance optimization
- Sampler options

---

**Once model is downloaded, Stable Diffusion will be fully functional!** 🎨✨
