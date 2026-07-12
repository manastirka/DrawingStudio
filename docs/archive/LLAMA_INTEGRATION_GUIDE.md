# Llama Integration Guide for DrawingStudio

## Overview

This guide shows how to integrate Llama 3.2 1B as an intelligent assistant in DrawingStudio using llama.cpp.

---

## Step 1: Add llama.cpp to Project

### 1.1 Add as Submodule
```bash
cd /Users/Lukovic/Apps/DrawingStudio
git submodule add https://github.com/ggerganov/llama.cpp external/llama.cpp
cd external/llama.cpp
git checkout master
```

### 1.2 Update CMakeLists.txt
```cmake
# Add llama.cpp
set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(external/llama.cpp)

# Link to your project
target_link_libraries(DrawingStudio PRIVATE 
    llama
    ${Qt6_LIBRARIES}
)
```

---

## Step 2: Download Model

### 2.1 Create Models Directory
```bash
mkdir -p /Users/Lukovic/Apps/DrawingStudio/models
cd models
```

### 2.2 Download Llama 3.2 1B (Quantized)
```bash
# Download Q4_K_M quantized model (700MB - good balance)
wget https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q4_K_M.gguf

# Or download Q8_0 for better quality (1.3GB)
# wget https://huggingface.co/bartowski/Llama-3.2-1B-Instruct-GGUF/resolve/main/Llama-3.2-1B-Instruct-Q8_0.gguf
```

---

## Step 3: Create LLM Helper Class

### 3.1 Create Header File

**File: `include/LLMHelper.h`**
```cpp
#pragma once

#include <QObject>
#include <QString>
#include <QThread>
#include <memory>

// Forward declarations
struct llama_model;
struct llama_context;

class LLMHelper : public QObject {
    Q_OBJECT
    
public:
    explicit LLMHelper(QObject* parent = nullptr);
    ~LLMHelper();
    
    // Initialize with model path
    bool initialize(const QString& modelPath);
    
    // Check if initialized
    bool isInitialized() const { return m_initialized; }
    
    // Generate text (blocking)
    QString generate(const QString& prompt, int maxTokens = 200);
    
    // Generate text (async)
    void generateAsync(const QString& prompt, int maxTokens = 200);
    
    // Get help for user query
    QString getHelp(const QString& userQuery);
    
    // Suggest next action
    QString suggestAction(const QString& context);
    
    // Generate description
    QString describeImage(const QString& context);
    
signals:
    void generationComplete(const QString& text);
    void generationError(const QString& error);
    void progressUpdate(int percentage);
    
private:
    llama_model* m_model = nullptr;
    llama_context* m_context = nullptr;
    bool m_initialized = false;
    
    // Load documentation context
    QString loadDocumentation();
    
    // Format prompt with context
    QString formatPrompt(const QString& userQuery, const QString& systemContext);
};
```

### 3.2 Create Implementation File

**File: `src/LLMHelper.cpp`**
```cpp
#include "LLMHelper.h"
#include "llama.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <vector>

LLMHelper::LLMHelper(QObject* parent)
    : QObject(parent)
{
}

LLMHelper::~LLMHelper() {
    if (m_context) {
        llama_free(m_context);
    }
    if (m_model) {
        llama_free_model(m_model);
    }
}

bool LLMHelper::initialize(const QString& modelPath) {
    qDebug() << "Initializing LLM with model:" << modelPath;
    
    // Initialize llama backend
    llama_backend_init();
    
    // Model parameters
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99; // Use Metal on macOS
    
    // Load model
    m_model = llama_load_model_from_file(
        modelPath.toStdString().c_str(),
        model_params
    );
    
    if (!m_model) {
        qDebug() << "Failed to load model";
        return false;
    }
    
    // Context parameters
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = 2048; // Context size
    ctx_params.n_batch = 512;
    ctx_params.n_threads = 4;
    
    // Create context
    m_context = llama_new_context_with_model(m_model, ctx_params);
    
    if (!m_context) {
        qDebug() << "Failed to create context";
        llama_free_model(m_model);
        m_model = nullptr;
        return false;
    }
    
    m_initialized = true;
    qDebug() << "LLM initialized successfully";
    return true;
}

QString LLMHelper::generate(const QString& prompt, int maxTokens) {
    if (!m_initialized) {
        return "Error: LLM not initialized";
    }
    
    // Tokenize prompt
    std::vector<llama_token> tokens;
    tokens.resize(prompt.length() + 1);
    
    int n_tokens = llama_tokenize(
        m_model,
        prompt.toStdString().c_str(),
        prompt.length(),
        tokens.data(),
        tokens.size(),
        true,  // add_bos
        false  // special
    );
    
    tokens.resize(n_tokens);
    
    // Generate
    QString result;
    std::vector<llama_token> output_tokens;
    
    for (int i = 0; i < maxTokens; ++i) {
        // Evaluate
        if (llama_decode(m_context, llama_batch_get_one(tokens.data(), tokens.size(), 0, 0))) {
            qDebug() << "Failed to decode";
            break;
        }
        
        // Sample next token
        llama_token new_token = llama_sample_token_greedy(m_context, nullptr);
        
        // Check for end of generation
        if (new_token == llama_token_eos(m_model)) {
            break;
        }
        
        // Convert token to text
        char buf[128];
        int n = llama_token_to_piece(m_model, new_token, buf, sizeof(buf));
        if (n < 0) {
            break;
        }
        
        result += QString::fromUtf8(buf, n);
        
        // Add token for next iteration
        tokens.clear();
        tokens.push_back(new_token);
    }
    
    return result;
}

QString LLMHelper::loadDocumentation() {
    QString docs;
    
    // Load main documentation
    QFile mainDoc(":/docs/LLAMA_DOCUMENTATION.md");
    if (mainDoc.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&mainDoc);
        docs += in.readAll();
        mainDoc.close();
    }
    
    return docs;
}

QString LLMHelper::formatPrompt(const QString& userQuery, const QString& systemContext) {
    return QString(
        "<|begin_of_text|><|start_header_id|>system<|end_header_id|>\n\n"
        "You are a helpful assistant for DrawingStudio, a professional drawing application.\n"
        "Use the following documentation to answer user questions:\n\n"
        "%1\n\n"
        "Provide clear, concise, step-by-step instructions. "
        "Reference specific tools, shortcuts, and menu items.\n"
        "<|eot_id|><|start_header_id|>user<|end_header_id|>\n\n"
        "%2<|eot_id|><|start_header_id|>assistant<|end_header_id|>\n\n"
    ).arg(systemContext, userQuery);
}

QString LLMHelper::getHelp(const QString& userQuery) {
    QString docs = loadDocumentation();
    QString prompt = formatPrompt(userQuery, docs);
    return generate(prompt, 300);
}

QString LLMHelper::suggestAction(const QString& context) {
    QString prompt = formatPrompt(
        QString("Based on this context: %1\nWhat should the user do next?").arg(context),
        loadDocumentation()
    );
    return generate(prompt, 150);
}

QString LLMHelper::describeImage(const QString& context) {
    QString prompt = QString(
        "Describe this drawing briefly: %1"
    ).arg(context);
    return generate(prompt, 100);
}
```

---

## Step 4: Integrate into MainWindow

### 4.1 Add to MainWindow.h
```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT
    
private:
    // ... existing members
    LLMHelper* m_llmHelper = nullptr;
    
    // UI for assistant
    QDockWidget* m_assistantDock = nullptr;
    QTextEdit* m_assistantOutput = nullptr;
    QLineEdit* m_assistantInput = nullptr;
    QPushButton* m_askButton = nullptr;
};
```

### 4.2 Initialize in MainWindow.cpp
```cpp
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // ... existing initialization
    
    // Initialize LLM Helper
    m_llmHelper = new LLMHelper(this);
    QString modelPath = QCoreApplication::applicationDirPath() + 
                       "/../models/Llama-3.2-1B-Instruct-Q4_K_M.gguf";
    
    if (!m_llmHelper->initialize(modelPath)) {
        qDebug() << "Warning: Failed to initialize LLM assistant";
        QMessageBox::warning(this, "AI Assistant", 
            "Failed to load AI assistant. Some features may be unavailable.");
    } else {
        qDebug() << "AI Assistant loaded successfully";
    }
    
    // Create assistant UI
    createAssistantPanel();
}
```

### 4.3 Create Assistant Panel
```cpp
void MainWindow::createAssistantPanel() {
    m_assistantDock = new QDockWidget("AI Assistant", this);
    m_assistantDock->setObjectName("AssistantDock");
    
    QWidget* assistantWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(assistantWidget);
    
    // Output area
    m_assistantOutput = new QTextEdit();
    m_assistantOutput->setReadOnly(true);
    m_assistantOutput->setPlaceholderText("Ask me anything about DrawingStudio...");
    layout->addWidget(m_assistantOutput);
    
    // Input area
    QHBoxLayout* inputLayout = new QHBoxLayout();
    m_assistantInput = new QLineEdit();
    m_assistantInput->setPlaceholderText("Type your question here...");
    inputLayout->addWidget(m_assistantInput);
    
    m_askButton = new QPushButton("Ask");
    inputLayout->addWidget(m_askButton);
    
    layout->addLayout(inputLayout);
    
    m_assistantDock->setWidget(assistantWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);
    
    // Connect signals
    connect(m_askButton, &QPushButton::clicked, this, &MainWindow::onAskAssistant);
    connect(m_assistantInput, &QLineEdit::returnPressed, this, &MainWindow::onAskAssistant);
}

void MainWindow::onAskAssistant() {
    QString question = m_assistantInput->text().trimmed();
    if (question.isEmpty()) return;
    
    // Show question
    m_assistantOutput->append("<b>You:</b> " + question);
    m_assistantInput->clear();
    
    // Show loading
    m_assistantOutput->append("<i>Thinking...</i>");
    m_askButton->setEnabled(false);
    
    // Get answer (in real app, do this async)
    QString answer = m_llmHelper->getHelp(question);
    
    // Show answer
    m_assistantOutput->append("<b>Assistant:</b> " + answer);
    m_assistantOutput->append(""); // Empty line
    m_askButton->setEnabled(true);
}
```

---

## Step 5: Add Assistant Menu

### 5.1 Add Menu Items
```cpp
void MainWindow::createMenus() {
    // ... existing menus
    
    // AI Assistant Menu
    QMenu* assistantMenu = menuBar()->addMenu("&Assistant");
    
    QAction* showAssistantAction = assistantMenu->addAction("Show Assistant", [this]() {
        m_assistantDock->show();
        m_assistantDock->raise();
    });
    showAssistantAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_H));
    
    QAction* quickHelpAction = assistantMenu->addAction("Quick Help", [this]() {
        QString context = getCurrentContext();
        QString help = m_llmHelper->suggestAction(context);
        QMessageBox::information(this, "Quick Help", help);
    });
    quickHelpAction->setShortcut(QKeySequence(Qt::Key_F1));
    
    assistantMenu->addSeparator();
    
    QAction* describeAction = assistantMenu->addAction("Describe Selection", [this]() {
        QString description = describeSelectedObjects();
        m_assistantOutput->append("<b>Description:</b> " + description);
    });
}
```

---

## Step 6: Context-Aware Features

### 6.1 Get Current Context
```cpp
QString MainWindow::getCurrentContext() const {
    QString context;
    
    // Current tool
    context += "Current tool: " + getToolName(m_canvas->currentTool()) + "\n";
    
    // Selected objects
    auto selected = m_canvas->selectedObjects();
    if (!selected.empty()) {
        context += QString("Selected: %1 objects\n").arg(selected.size());
        for (auto* obj : selected) {
            context += "- " + getPrimitiveName(obj) + "\n";
        }
    }
    
    // Active layer
    if (m_layerManager) {
        context += "Active layer: " + m_layerManager->activeLayer()->name() + "\n";
    }
    
    return context;
}
```

### 6.2 Smart Suggestions
```cpp
void MainWindow::showSmartSuggestion() {
    QString context = getCurrentContext();
    QString suggestion = m_llmHelper->suggestAction(context);
    
    // Show as tooltip or notification
    m_statusLabel->setText("💡 " + suggestion);
}
```

---

## Step 7: Advanced Features

### 7.1 Natural Language Commands
```cpp
void MainWindow::executeNaturalLanguageCommand(const QString& command) {
    // Ask LLM to parse command
    QString prompt = QString(
        "Parse this drawing command and return JSON:\n"
        "Command: %1\n"
        "Format: {\"action\": \"draw\", \"shape\": \"circle\", \"x\": 100, \"y\": 100, \"radius\": 50}"
    ).arg(command);
    
    QString response = m_llmHelper->generate(prompt, 100);
    
    // Parse JSON and execute
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    if (!doc.isNull()) {
        executeDrawingCommand(doc.object());
    }
}
```

### 7.2 Auto Layer Naming
```cpp
QString MainWindow::generateLayerName() {
    QString context = "Layer contains: ";
    auto primitives = m_layerManager->activeLayer()->primitives();
    for (const auto& prim : primitives) {
        context += getPrimitiveName(prim.get()) + ", ";
    }
    
    QString prompt = QString(
        "Generate a short descriptive name for a layer containing: %1"
    ).arg(context);
    
    return m_llmHelper->generate(prompt, 20).trimmed();
}
```

### 7.3 Color Palette Suggestions
```cpp
QVector<QColor> MainWindow::suggestColorPalette(const QString& theme) {
    QString prompt = QString(
        "Suggest 5 hex colors for a %1 color palette. "
        "Return only hex codes separated by commas."
    ).arg(theme);
    
    QString response = m_llmHelper->generate(prompt, 50);
    
    // Parse colors
    QVector<QColor> colors;
    QStringList hexCodes = response.split(",");
    for (const QString& hex : hexCodes) {
        colors.append(QColor(hex.trimmed()));
    }
    
    return colors;
}
```

---

## Step 8: Embed Documentation

### 8.1 Add to Resources
**File: `resources/resources.qrc`**
```xml
<RCC>
    <qresource prefix="/docs">
        <file>LLAMA_DOCUMENTATION.md</file>
        <file>LLAMA_TECHNICAL_REFERENCE.md</file>
    </qresource>
</RCC>
```

### 8.2 Update CMakeLists.txt
```cmake
qt_add_resources(DrawingStudio "docs"
    PREFIX "/docs"
    FILES
        LLAMA_DOCUMENTATION.md
        LLAMA_TECHNICAL_REFERENCE.md
)
```

---

## Step 9: Build and Test

### 9.1 Build
```bash
cd /Users/Lukovic/Apps/DrawingStudio/build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### 9.2 Test
```bash
./DrawingStudio
```

### 9.3 Test Assistant
1. Press `Ctrl+H` to show assistant
2. Type: "How do I draw a circle?"
3. Press Enter
4. Assistant should respond with instructions

---

## Performance Tips

### Optimize Model Loading
```cpp
// Load model once at startup
// Keep context alive during app lifetime
// Use smaller quantized models (Q4_K_M)
```

### Async Generation
```cpp
void LLMHelper::generateAsync(const QString& prompt, int maxTokens) {
    QThread* thread = QThread::create([this, prompt, maxTokens]() {
        QString result = generate(prompt, maxTokens);
        emit generationComplete(result);
    });
    thread->start();
}
```

### Cache Common Queries
```cpp
QMap<QString, QString> m_responseCache;

QString LLMHelper::getHelp(const QString& query) {
    if (m_responseCache.contains(query)) {
        return m_responseCache[query];
    }
    
    QString response = generate(formatPrompt(query, loadDocumentation()));
    m_responseCache[query] = response;
    return response;
}
```

---

## Example Use Cases

### 1. Tool Help
**User**: "How do I use the curve tool?"
**Assistant**: "To use the Curve Tool: 1. Press 'U' or select Curve Tool from toolbar..."

### 2. Troubleshooting
**User**: "My text is invisible"
**Assistant**: "If text is invisible, check: 1. Text color (might match background)..."

### 3. Feature Discovery
**User**: "Can I detect subjects in images?"
**Assistant**: "Yes! To detect subjects: 1. Import image (I key)..."

### 4. Workflow Suggestions
**User**: "What should I do next?"
**Assistant**: "Based on your selected circle, you could: 1. Change color..."

---

## Troubleshooting

### Model Won't Load
```
Error: Failed to load model
Solution: Check model path, verify file exists, ensure enough RAM
```

### Slow Generation
```
Issue: Generation takes too long
Solution: Use smaller model (Q4_K_M), reduce maxTokens, enable GPU
```

### Crashes on Generation
```
Issue: App crashes during generation
Solution: Increase context size, check memory, update llama.cpp
```

---

## Next Steps

1. ✅ Integrate llama.cpp
2. ✅ Create LLMHelper class
3. ✅ Add assistant UI
4. ✅ Embed documentation
5. 🔄 Test with users
6. 🔄 Add more features
7. 🔄 Optimize performance

---

*This guide provides everything needed to integrate Llama 3.2 1B as an intelligent assistant in DrawingStudio!*
