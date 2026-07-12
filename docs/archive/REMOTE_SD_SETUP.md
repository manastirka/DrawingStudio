# Remote Stable Diffusion Server Integration

## 🌐 Overview

Your DrawingStudio app now supports using a remote Stable Diffusion server (SD 3.5 Large) running on your network at `192.168.1.58:7860`. This allows you to generate high-quality images using a powerful remote GPU while keeping your local machine free.

## ⚙️ Setup

### Server Configuration
- **IP Address**: `192.168.1.58`
- **Port**: `7860`
- **Model**: SD 3.5 Large (sd3.5_large.safetensors)
- **API**: Automatic1111 WebUI API

### Requirements
1. Stable Diffusion WebUI running on 192.168.1.58
2. API enabled in WebUI settings
3. Network connectivity between your Mac and the server

## 🎯 How to Use

### Switching Between Backends

1. **Open the Assistant Menu**
   - Go to `Assistant` → `Stable Diffusion Backend`

2. **Choose Your Backend**
   - **💻 Local (CPU - ggml)**: Uses local CPU-based SD (slower, but works offline)
   - **🌐 Remote Server (SD 3.5 Large)**: Uses your remote server (faster, better quality)

3. **Test Connection**
   - Use `Assistant` → `Stable Diffusion Backend` → `🔌 Test Remote Connection`
   - This will verify the server is accessible

### Using Remote SD

Once you've selected the Remote backend:

1. **Generate Images in Assistant**
   ```
   Open Assistant panel (Ctrl+H)
   Type: "generate a beautiful sunset landscape"
   Press Enter or click "Ask"
   ```

2. **Context-Aware Generation**
   ```
   Select an image with AI Mask tool
   Use Assistant to generate contextual content
   Remote SD will be used automatically
   ```

3. **Image Parameters**
   - **Size**: 512x512 (remote) vs 384x384 (local)
   - **Steps**: 25 (remote) vs 15 (local)
   - **Quality**: Higher with remote SD 3.5 Large

## 🔧 Technical Details

### API Endpoints Used

**Check Models**:
```
GET http://192.168.1.58:7860/sdapi/v1/sd-models
```

**Generate Image**:
```
POST http://192.168.1.58:7860/sdapi/v1/txt2img
Body: {
  "prompt": "your prompt",
  "negative_prompt": "blurry, low quality",
  "width": 512,
  "height": 512,
  "steps": 25,
  "cfg_scale": 7.0,
  "sampler_name": "Euler a",
  "override_settings": {
    "sd_model_checkpoint": "sd3.5_large.safetensors"
  }
}
```

**Check Progress**:
```
GET http://192.168.1.58:7860/sdapi/v1/progress
```

### Code Integration

The remote SD is integrated at multiple points:

1. **Assistant Image Generation** (`MainWindow.cpp:6786-6794`)
   - Detects backend selection
   - Routes to remote or local SD
   - Adjusts parameters based on backend

2. **Context-Aware Generation** (`MainWindow.cpp:3247-3254`)
   - Uses selected backend for contextual fills
   - Maintains same workflow

3. **Connection Management**
   - Automatic connection on backend switch
   - Connection testing available
   - Error handling with user feedback

## 🚀 Benefits of Remote SD

### Performance
- **Faster Generation**: GPU acceleration on server
- **Higher Quality**: SD 3.5 Large model
- **Larger Images**: 512x512 vs 384x384
- **More Steps**: 25 vs 15 for better detail

### Workflow
- **Free Local Resources**: Your Mac stays responsive
- **Background Generation**: Server handles heavy lifting
- **Seamless Switching**: Toggle between local/remote anytime

## 🔍 Troubleshooting

### Connection Failed

**Error**: "Failed to connect to Remote SD server"

**Solutions**:
1. Check server is running:
   ```bash
   # On server machine
   ps aux | grep "stable-diffusion"
   ```

2. Verify network connectivity:
   ```bash
   # On your Mac
   ping 192.168.1.58
   curl http://192.168.1.58:7860/sdapi/v1/sd-models
   ```

3. Check firewall settings on server

4. Ensure WebUI API is enabled:
   - Launch WebUI with `--api` flag
   - Or enable in settings

### Slow Generation

**Issue**: Remote generation taking too long

**Solutions**:
- Check server GPU utilization
- Reduce steps (modify code if needed)
- Ensure no other processes using GPU
- Check network bandwidth

### Wrong Model Used

**Issue**: Not using SD 3.5 Large

**Solutions**:
1. Verify model is loaded on server
2. Check model name matches: `sd3.5_large.safetensors`
3. Update model name in code if different:
   ```cpp
   // In RemoteSDHelper.cpp constructor
   m_currentModel("your_model_name.safetensors")
   ```

## 📝 Configuration

### Change Server IP/Port

Edit `RemoteSDHelper.cpp`:
```cpp
// Line 11
m_serverUrl("http://YOUR_IP:YOUR_PORT")
```

Or pass to initialize:
```cpp
m_remoteSDHelper->initialize("http://YOUR_IP:YOUR_PORT");
```

### Change Default Model

Edit `RemoteSDHelper.cpp`:
```cpp
// Line 12
m_currentModel("your_model.safetensors")
```

### Adjust Generation Parameters

Edit `MainWindow.cpp`:
```cpp
// For assistant generation (line 6789)
m_remoteSDHelper->generateImageAsync(
    prompt, 
    "blurry, low quality", 
    512,    // width
    512,    // height
    25,     // steps
    7.0f,   // cfg_scale
    -1      // seed (-1 = random)
);
```

## 🎨 Advanced Usage

### Custom Prompts

The remote SD supports all standard SD prompts:
- Positive prompts
- Negative prompts
- Emphasis with `(word:1.5)`
- Multiple concepts

### Model Selection

You can add UI to select different models:
1. Fetch available models from server
2. Display in dropdown menu
3. Call `m_remoteSDHelper->setModel("model_name")`

### Progress Monitoring

The helper emits progress signals:
```cpp
connect(m_remoteSDHelper, &RemoteSDHelper::generationProgress, 
    this, [](int percentage) {
        qDebug() << "Progress:" << percentage << "%";
    });
```

## 📊 Status Messages

Watch the status bar for feedback:
- `🔄 Connecting to remote SD server...` - Connecting
- `✓ Connected to Remote SD server (SD 3.5 Large)` - Success
- `❌ Failed to connect to Remote SD server` - Error
- `✓ Using Remote SD server (SD 3.5 Large)` - Active
- `✓ Using Local SD backend` - Local mode

## 🎯 Best Practices

1. **Test connection first** before generating
2. **Use remote for final renders** (better quality)
3. **Use local for quick tests** (faster iteration)
4. **Monitor server resources** to avoid overload
5. **Keep server model loaded** for faster generation

## 🔐 Security Notes

- Server is accessed over HTTP (not HTTPS)
- Only use on trusted local networks
- Consider VPN for remote access
- No authentication currently implemented

## 📚 Further Reading

- [Automatic1111 WebUI API Docs](https://github.com/AUTOMATIC1111/stable-diffusion-webui/wiki/API)
- [SD 3.5 Large Model Info](https://stability.ai/news/introducing-stable-diffusion-3-5)
- [Qt Network Module](https://doc.qt.io/qt-6/qtnetwork-index.html)

---

**Your remote SD integration is ready!** Switch to the Remote backend in the Assistant menu and start generating high-quality images with SD 3.5 Large! 🎨✨
