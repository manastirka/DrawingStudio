# Google Gemini API Setup for AI Image Generation

## Overview
This application uses Google Gemini API for AI-powered image generation. When you select an object in the drawing canvas, an AI prompt appears allowing you to generate images that will replace the selected object.

## Setup Instructions

### 1. Get Your API Key
1. Go to [Google AI Studio](https://makersuite.google.com/app/apikey)
2. Sign in with your Google account
3. Click "Create API Key"
4. Copy your API key

### 2. Set Environment Variable

#### macOS/Linux:
```bash
export GEMINI_API_KEY="your-api-key-here"
```

To make it permanent, add to your `~/.zshrc` or `~/.bashrc`:
```bash
echo 'export GEMINI_API_KEY="your-api-key-here"' >> ~/.zshrc
source ~/.zshrc
```

#### Windows:
```cmd
set GEMINI_API_KEY=your-api-key-here
```

Or set it permanently in System Environment Variables.

### 3. Run the Application
```bash
cd /Users/Lukovic/Apps/DrawingStudio
./build/DrawingStudio
```

## How to Use

1. **Draw or select an object** on the canvas
2. **AI prompt widget appears** automatically in the bottom-right corner
3. **Type your image description**, for example:
   - "a red sports car"
   - "a mountain landscape at sunset"
   - "a cute cartoon cat"
4. **Press Enter** or click "Generate Image"
5. The AI will generate an image and replace the selected object

## API Details

- **Endpoint**: `https://generativelanguage.googleapis.com/v1beta/models/gemini-pro-vision:generateContent`
- **Method**: POST
- **Authentication**: API Key in URL parameter
- **Rate Limits**: Check [Google AI Studio](https://ai.google.dev/pricing) for current limits

## Troubleshooting

### "API Key Missing" Error
- Make sure you've set the `GEMINI_API_KEY` environment variable
- Restart the application after setting the variable
- Verify the key is correct: `echo $GEMINI_API_KEY`

### "Generation Failed" Error
- Check your internet connection
- Verify your API key is valid
- Check if you've exceeded rate limits
- Review the error message in the dialog

### No Response
- The API call may take 5-30 seconds depending on complexity
- Check the application console for debug messages
- Ensure you have sufficient API quota

## Future Enhancements

The current implementation is a foundation. Future updates will include:
- [ ] Parse and display generated images
- [ ] Replace selected objects with ImagePrimitive
- [ ] Support for different image sizes
- [ ] Style transfer options
- [ ] Image editing capabilities
- [ ] Batch generation
- [ ] Local caching of generated images

## Alternative AI Services

You can modify the code to use other AI image generation services:
- **OpenAI DALL-E 3**: `https://api.openai.com/v1/images/generations`
- **Stability AI**: `https://api.stability.ai/v1/generation`
- **Midjourney**: Via their API (when available)

## Support

For issues or questions:
- Check the console output for debug messages
- Review the Google Gemini API documentation
- Verify your API key and quota

---

**Note**: This feature requires an active internet connection and a valid Google Gemini API key.
