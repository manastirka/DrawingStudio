# OCR Integration Guide - Offline Tesseract

## ✅ Offline OCR with Tesseract!

I've integrated **Tesseract OCR** - a powerful, open-source OCR engine that works **completely offline**!

---

## 🚀 Quick Start

### Step 1: Install Tesseract

```bash
cd /Users/Lukovic/Apps/DrawingStudio
./install_ocr.sh
```

Or manually:
```bash
brew install tesseract tesseract-lang
```

### Step 2: Rebuild DrawingStudio

```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
cmake ..
make
```

### Step 3: Run and Test!

```bash
./DrawingStudio
```

---

## 📚 What is Tesseract?

- **Open-source** OCR engine developed by Google
- **Offline** - no internet or API keys required
- **Multi-language** - supports 100+ languages
- **Accurate** - industry-standard OCR quality
- **Fast** - processes images in milliseconds
- **Free** - completely free to use

---

## 🎨 Features

### Text Extraction
- Extract text from images
- Extract text from selected regions
- Multi-language support
- Confidence scoring

### Image Analysis
- Automatic page segmentation
- Text line detection
- Word-level recognition
- Character-level recognition

### Preprocessing
- Automatic grayscale conversion
- Contrast enhancement
- Noise reduction
- Image optimization

---

## 💡 Usage Examples

### Basic Text Extraction

```cpp
#include "OCRHelper.h"

// Create OCR helper
OCRHelper* ocr = new OCRHelper(this);

// Initialize with English
if (ocr->initialize("", "eng")) {
    // Extract text from image
    QString text = ocr->extractText(image);
    qDebug() << "Extracted:" << text;
    qDebug() << "Confidence:" << ocr->getConfidence() << "%";
}
```

### Extract from Region

```cpp
// Extract text from specific region
QRect region(100, 100, 400, 200);
QString text = ocr->extractTextFromRegion(image, region);
```

### Async Extraction (Non-blocking)

```cpp
// Connect signal
connect(ocr, &OCRHelper::textExtracted, [](const QString& text, int confidence) {
    qDebug() << "Text:" << text;
    qDebug() << "Confidence:" << confidence << "%";
});

// Extract asynchronously
ocr->extractTextAsync(image);
```

### Multi-language

```cpp
// Spanish
ocr->setLanguage("spa");
QString spanishText = ocr->extractText(image);

// French
ocr->setLanguage("fra");
QString frenchText = ocr->extractText(image);

// Chinese Simplified
ocr->setLanguage("chi_sim");
QString chineseText = ocr->extractText(image);
```

### Page Segmentation Modes

```cpp
// Single text line
ocr->setPageSegmentationMode(7);
QString line = ocr->extractText(image);

// Sparse text (find all text)
ocr->setPageSegmentationMode(11);
QString allText = ocr->extractText(image);

// Fully automatic (default)
ocr->setPageSegmentationMode(3);
QString autoText = ocr->extractText(image);
```

---

## 🌍 Supported Languages

### Installed by Default:
- **eng** - English
- **spa** - Spanish
- **fra** - French
- **deu** - German
- **ita** - Italian
- **por** - Portuguese

### Additional Languages Available:
- **rus** - Russian
- **chi_sim** - Chinese Simplified
- **chi_tra** - Chinese Traditional
- **jpn** - Japanese
- **kor** - Korean
- **ara** - Arabic
- **hin** - Hindi
- **tha** - Thai
- **vie** - Vietnamese
- **pol** - Polish
- **ukr** - Ukrainian
- **nld** - Dutch
- **swe** - Swedish
- **nor** - Norwegian
- **dan** - Danish
- **fin** - Finnish
- **tur** - Turkish
- **heb** - Hebrew
- **ell** - Greek
- ... and 70+ more!

### Install Additional Languages:
```bash
# All languages
brew install tesseract-lang

# Or specific language
# (Download .traineddata file to tessdata directory)
```

---

## 🎯 Integration Ideas

### 1. OCR Tool in Toolbar
```cpp
void MainWindow::ocrTool() {
    if (!m_ocrHelper) {
        m_ocrHelper = new OCRHelper(this);
        m_ocrHelper->initialize();
    }
    
    // Get selected image
    if (ImagePrimitive* img = getSelectedImage()) {
        QString text = m_ocrHelper->extractText(img->image());
        
        // Show in dialog or create text primitive
        showOCRResult(text);
    }
}
```

### 2. Region Selection OCR
```cpp
void MainWindow::ocrRegion() {
    // Let user select region
    m_canvas->setCurrentTool(DrawingTool::Select);
    
    // After selection
    QRect region = m_canvas->selectedRegion();
    QImage screenshot = m_canvas->grabRegion(region);
    
    QString text = m_ocrHelper->extractText(screenshot);
    createTextPrimitive(text, region.topLeft());
}
```

### 3. AI Command Integration
```cpp
// In LLMHelper::parseCommand()
else if (lower.contains("ocr") || lower.contains("extract text")) {
    return "ocr_extract";
}

// In MainWindow::handleAssistantCommand()
else if (commandType == "ocr_extract") {
    if (m_ocrHelper && hasSelectedImage()) {
        m_ocrHelper->extractTextAsync(selectedImage());
    }
}
```

### 4. Batch OCR
```cpp
void MainWindow::batchOCR() {
    QStringList results;
    
    for (ImagePrimitive* img : getAllImages()) {
        QString text = m_ocrHelper->extractText(img->image());
        results.append(text);
    }
    
    // Save to file or show in dialog
    saveOCRResults(results);
}
```

### 5. Screenshot OCR
```cpp
void MainWindow::screenshotOCR() {
    // Capture screen region
    QImage screenshot = captureScreenRegion();
    
    // Extract text
    QString text = m_ocrHelper->extractText(screenshot);
    
    // Create text primitive on canvas
    auto textPrim = std::make_unique<TextPrimitive>(text, QVector2D(100, 100));
    m_canvas->addPrimitive(std::move(textPrim));
}
```

---

## 📊 Performance

### Speed:
- **Small images** (< 1MP): < 100ms
- **Medium images** (1-4MP): 100-500ms
- **Large images** (> 4MP): 500ms-2s

### Accuracy:
- **Clean text**: 95-99%
- **Printed documents**: 90-95%
- **Handwriting**: 60-80% (depends on quality)
- **Low quality**: 50-70%

### Tips for Better Results:
1. **High contrast** - Black text on white background
2. **Good resolution** - At least 300 DPI
3. **Straight text** - Avoid skewed images
4. **Clean images** - Remove noise and artifacts
5. **Proper segmentation** - Choose correct PSM mode

---

## 🔧 Page Segmentation Modes (PSM)

| Mode | Description | Use Case |
|------|-------------|----------|
| 0 | OSD only | Detect orientation |
| 1 | Auto with OSD | Full page, any orientation |
| 3 | Fully automatic | **Default** - most cases |
| 4 | Single column | Newspaper column |
| 5 | Vertical text block | Vertical Asian text |
| 6 | Uniform text block | Single paragraph |
| 7 | Single text line | One line of text |
| 8 | Single word | One word |
| 9 | Single word in circle | Circular text |
| 10 | Single character | One character |
| 11 | Sparse text | Find all text |
| 12 | Sparse text with OSD | Find all, any orientation |
| 13 | Raw line | Bypass segmentation |

---

## 🎨 UI Integration Example

### Add OCR Button to Toolbar:
```cpp
// In MainWindow::createToolbar()
QAction *ocrAction = toolbar->addAction(createOCRIcon(), "OCR", this, &MainWindow::ocrTool);
ocrAction->setShortcut(QKeySequence("O"));
ocrAction->setToolTip("Extract text from image (O)");
```

### Add OCR Menu:
```cpp
// In MainWindow::createMenus()
QMenu *toolsMenu = menuBar()->addMenu("Tools");
QAction *ocrAction = toolsMenu->addAction("Extract Text (OCR)");
ocrAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
connect(ocrAction, &QAction::triggered, this, &MainWindow::ocrTool);
```

### Add OCR Dialog:
```cpp
void MainWindow::showOCRResult(const QString& text, int confidence) {
    QDialog dialog(this);
    dialog.setWindowTitle("OCR Result");
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    // Confidence indicator
    QLabel *confLabel = new QLabel(QString("Confidence: %1%").arg(confidence));
    layout->addWidget(confLabel);
    
    // Text display
    QTextEdit *textEdit = new QTextEdit();
    textEdit->setPlainText(text);
    layout->addWidget(textEdit);
    
    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog.exec() == QDialog::Accepted) {
        // User accepted - create text primitive
        QString editedText = textEdit->toPlainText();
        createTextPrimitive(editedText);
    }
}
```

---

## 🚀 Next Steps

### 1. Install Tesseract:
```bash
./install_ocr.sh
```

### 2. Rebuild Project:
```bash
cd build
cmake ..
make
```

### 3. Add OCR Features:
- Add OCR button to toolbar
- Add "Extract Text" menu item
- Integrate with AI commands
- Add region selection OCR
- Add batch OCR for multiple images

### 4. Test:
```bash
./DrawingStudio
# Load an image with text
# Click OCR button
# See extracted text!
```

---

## 📝 Example: Complete OCR Feature

```cpp
// In MainWindow.h
private:
    OCRHelper* m_ocrHelper;
    
private slots:
    void ocrSelectedImage();
    void ocrRegion();
    void showOCRDialog(const QString& text, int confidence);

// In MainWindow.cpp
MainWindow::MainWindow() {
    // Initialize OCR
    m_ocrHelper = new OCRHelper(this);
    if (m_ocrHelper->initialize()) {
        qDebug() << "OCR initialized successfully";
    }
    
    // Connect signals
    connect(m_ocrHelper, &OCRHelper::textExtracted, 
            this, &MainWindow::showOCRDialog);
}

void MainWindow::ocrSelectedImage() {
    // Get selected image primitive
    auto selected = m_canvas->selectedObjects();
    for (auto* obj : selected) {
        if (auto* imgPrim = dynamic_cast<ImagePrimitive*>(obj)) {
            // Extract text asynchronously
            m_ocrHelper->extractTextAsync(imgPrim->image());
            m_statusLabel->setText("Extracting text...");
            return;
        }
    }
    
    QMessageBox::information(this, "OCR", "Please select an image first");
}

void MainWindow::showOCRDialog(const QString& text, int confidence) {
    m_statusLabel->setText(QString("OCR complete - %1% confidence").arg(confidence));
    
    // Show result dialog
    // ... (dialog code from above)
}
```

---

## ✅ Benefits

- ✅ **Offline** - No internet required
- ✅ **Free** - No API costs
- ✅ **Private** - Data stays on your machine
- ✅ **Fast** - Processes images quickly
- ✅ **Accurate** - Industry-standard quality
- ✅ **Multi-language** - 100+ languages
- ✅ **Open-source** - Fully transparent
- ✅ **Maintained** - Actively developed by Google

---

**Tesseract OCR is ready to integrate into DrawingStudio!** 🎉📝
