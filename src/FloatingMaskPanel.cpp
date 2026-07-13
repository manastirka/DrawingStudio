#include "FloatingMaskPanel.h"

#include "CommandManager.h"
#include "Commands.h"
#include "DrawingCanvas.h"
#include "DrawingPrimitive.h"
#include "ImagePrimitive.h"
#include "LayerManager.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <memory>
#include <vector>

FloatingMaskPanel::FloatingMaskPanel(QWidget *canvasParent)
    : QWidget(canvasParent)
{
    buildUi();
}

void FloatingMaskPanel::setHost(Host host)
{
    m_host = std::move(host);
}

void FloatingMaskPanel::setStatus(const QString &text)
{
    if (m_host.setStatusText)
        m_host.setStatusText(text);
}

void FloatingMaskPanel::buildUi()
{

this->setWindowFlags(Qt::Widget);
  this->setAutoFillBackground(true);

  this->setStyleSheet(R"(
    QWidget#floatingMaskPanel {
        background-color: rgba(18, 20, 26, 210);
        border: 1px solid rgba(74, 144, 226, 0.25);
        border-radius: 14px;
        color: white;
    }
    QLabel { color: #c8c8d0; background: transparent; border: none; }
  )");
  this->setObjectName("floatingMaskPanel");
  this->setMouseTracking(true);

  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 14, 16, 14);
  mainLayout->setSpacing(10);

  // ── Title bar ──
  QWidget *titleBar = new QWidget();
  titleBar->setObjectName("maskPanelTitleBar");
  titleBar->setStyleSheet("background: transparent; border: none;");
  titleBar->setCursor(Qt::SizeAllCursor);
  titleBar->setMouseTracking(true);
  QHBoxLayout *titleLayout = new QHBoxLayout(titleBar);
  titleLayout->setContentsMargins(0, 0, 0, 0);

  QLabel *dragDots = new QLabel("⠿");
  dragDots->setStyleSheet("color: #505060; font-size: 14px;");
  titleLayout->addWidget(dragDots);

  QLabel *titleLabel = new QLabel("MASK SELECTION");
  titleLabel->setStyleSheet("font-size: 11px; font-weight: 700; color: #707080; letter-spacing: 2px;");
  titleLayout->addWidget(titleLabel);
  titleLayout->addStretch();

  QPushButton *miniClose = new QPushButton(QString::fromUtf8("\xc3\x97"));
  miniClose->setFixedSize(22, 22);
  miniClose->setStyleSheet(R"(
    QPushButton { border: none; color: #606070; background: rgba(255,255,255,0.04); border-radius: 11px; font-size: 14px; }
    QPushButton:hover { color: #ff6b6b; background: rgba(255,100,100,0.12); }
  )");
  connect(miniClose, &QPushButton::clicked, this, &FloatingMaskPanel::hide);
  titleLayout->addWidget(miniClose);
  mainLayout->addWidget(titleBar);
  titleBar->installEventFilter(this);

  // ── Mask counter ──
  m_counterLabel = new QLabel("- / -");
  m_counterLabel->setAlignment(Qt::AlignCenter);
  m_counterLabel->setStyleSheet("font-size: 28px; font-weight: 200; color: #4a90e2; letter-spacing: 4px; margin: 2px 0;");
  mainLayout->addWidget(m_counterLabel);

  // ── Score bar ──
  m_scoreBar = new QWidget();
  m_scoreBar->setFixedHeight(4);
  m_scoreBar->setStyleSheet("background: rgba(74, 144, 226, 0.5); border-radius: 2px;");
  mainLayout->addWidget(m_scoreBar);

  // ── Stats row ──
  QHBoxLayout *statsLayout = new QHBoxLayout();
  statsLayout->setSpacing(6);

  auto makeStatLabel = [](const QString &text) {
    QLabel *lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("font-size: 10px; color: #808090; background: rgba(255,255,255,0.03); border-radius: 4px; padding: 3px 4px;");
    return lbl;
  };

  m_scoreLabel = makeStatLabel("Score --");
  m_areaLabel = makeStatLabel("Area --%");
  m_stabilityLabel = makeStatLabel("Stab --");
  m_iouLabel = makeStatLabel("IoU --");

  statsLayout->addWidget(m_scoreLabel);
  statsLayout->addWidget(m_areaLabel);
  statsLayout->addWidget(m_stabilityLabel);
  statsLayout->addWidget(m_iouLabel);
  mainLayout->addLayout(statsLayout);

  // ── Prev / Next nav ──
  QString navBtnStyle = R"(
    QPushButton {
        background: rgba(255, 255, 255, 0.05);
        color: #c0c0cc;
        border: 1px solid rgba(255, 255, 255, 0.08);
        border-radius: 16px;
        font-size: 13px;
        min-width: 36px; min-height: 32px;
    }
    QPushButton:hover { background: rgba(74, 144, 226, 0.2); border-color: rgba(74, 144, 226, 0.4); color: white; }
    QPushButton:pressed { background: rgba(74, 144, 226, 0.1); }
    QPushButton:disabled { color: #404050; border-color: rgba(255,255,255,0.03); }
  )";

  QHBoxLayout *navLayout = new QHBoxLayout();
  navLayout->setSpacing(8);

  m_prevBtn = new QPushButton(QString::fromUtf8("\xe2\x97\x80"));
  m_prevBtn->setStyleSheet(navBtnStyle);
  connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
    if (m_host.selectPreviousMask) m_host.selectPreviousMask();
  });

  m_nextBtn = new QPushButton(QString::fromUtf8("\xe2\x96\xb6"));
  m_nextBtn->setStyleSheet(navBtnStyle);
  connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
    if (m_host.selectNextMask) m_host.selectNextMask();
  });

  navLayout->addStretch();
  navLayout->addWidget(m_prevBtn);
  navLayout->addWidget(m_nextBtn);
  navLayout->addStretch();
  mainLayout->addLayout(navLayout);

  // ── Slider for scrubbing ──
  m_slider = new QSlider(Qt::Horizontal);
  m_slider->setStyleSheet(R"(
    QSlider::groove:horizontal { background: rgba(255,255,255,0.06); height: 4px; border-radius: 2px; }
    QSlider::handle:horizontal { background: #4a90e2; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }
    QSlider::handle:horizontal:hover { background: #5ba0f2; }
  )");
  connect(m_slider, &QSlider::valueChanged, this, [this](int val) {
    if (m_host.onMaskSelectionChanged) m_host.onMaskSelectionChanged(val);
    refresh();
  });
  mainLayout->addWidget(m_slider);

  // ── Action buttons (2x2 grid) ──
  QGridLayout *grid = new QGridLayout();
  grid->setSpacing(6);

  QString actionBtnStyle = R"(
    QPushButton {
        background: rgba(255, 255, 255, 0.04);
        color: #c0c0cc;
        border: 1px solid rgba(255, 255, 255, 0.06);
        border-radius: 8px;
        font-size: 11px;
        padding: 10px 6px;
    }
    QPushButton:hover { background: rgba(74, 144, 226, 0.25); border-color: rgba(74, 144, 226, 0.4); color: white; }
    QPushButton:pressed { background: rgba(74, 144, 226, 0.15); }
  )";

  QPushButton *editBtn = new QPushButton("Edit Mask");
  editBtn->setStyleSheet(actionBtnStyle);
  connect(editBtn, &QPushButton::clicked, [this]() {
    if (!m_host.canvas)
      return;
    for (auto *obj : m_host.canvas->selectedObjects()) {
      if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
        imgPrim->setEditMode(true);
        setStatus("Editing mask contour");
        break;
      }
    }
  });

  QPushButton *cutBtn = new QPushButton("Cut Out");
  cutBtn->setStyleSheet(actionBtnStyle);
  connect(cutBtn, &QPushButton::clicked, [this]() {
    if (!m_host.canvas || !m_host.commandManager)
      return;
    for (auto *obj : m_host.canvas->selectedObjects()) {
      if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
        auto command = std::make_unique<ExtractSubjectCommand>(m_host.canvas, imgPrim);
        m_host.commandManager->executeCommand(std::move(command));
        setStatus("Subject extracted");
        break;
      }
    }
    hide();
  });

  QPushButton *invertBtn = new QPushButton("Invert");
  invertBtn->setStyleSheet(actionBtnStyle);
  connect(invertBtn, &QPushButton::clicked, [this]() {
    if (m_host.invertSelectedMask) m_host.invertSelectedMask();
  });

  QPushButton *removeBgBtn = new QPushButton("Remove BG");
  removeBgBtn->setStyleSheet(actionBtnStyle);
  connect(removeBgBtn, &QPushButton::clicked, [this]() {
    if (!m_host.canvas)
      return;
    for (auto *obj : m_host.canvas->selectedObjects()) {
      if (auto *imgPrim = dynamic_cast<ImagePrimitive *>(obj)) {
        // Only proceed when there's actually a mask/contour to extract.
        if (imgPrim->getEditableContour().empty()) {
          setStatus("No mask to remove. Run detection first.");
          break;
        }
        // Remove BG = extract the subject and delete the original image,
        // wrapped in a single undoable compound command (like Cut Out but the
        // background-carrying original is removed too).
        auto compound = std::make_unique<CompoundCommand>("Remove Background");
        compound->addCommand(
            std::make_unique<ExtractSubjectCommand>(m_host.canvas, imgPrim));
        std::vector<DrawingPrimitive *> toDelete = {imgPrim};
        compound->addCommand(
            std::make_unique<DeletePrimitivesCommand>(m_host.canvas, toDelete));
        if (m_host.commandManager) {
          m_host.commandManager->executeCommand(std::move(compound));
        }
        setStatus("Background removed");
        break;
      }
    }
    hide();
  });

  grid->addWidget(editBtn, 0, 0);
  grid->addWidget(cutBtn, 0, 1);
  grid->addWidget(invertBtn, 1, 0);
  grid->addWidget(removeBgBtn, 1, 1);
  mainLayout->addLayout(grid);

  this->setFixedWidth(280);
  this->adjustSize();
  this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  this->hide();

}

bool FloatingMaskPanel::eventFilter(QObject *obj, QEvent *event)
{

    static QWidget *cachedTitleBar = nullptr;
    if (!cachedTitleBar) {
        cachedTitleBar = findChild<QWidget *>("maskPanelTitleBar");
    }
    if (cachedTitleBar && obj == cachedTitleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                m_isDragging = true;
                m_dragStartPosition = mouseEvent->pos();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_isDragging) {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                QPoint newPos = mapToParent(mouseEvent->pos() - m_dragStartPosition);
                move(newPos);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_isDragging = false;
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);

}

void FloatingMaskPanel::showAtDefaultPosition()
{

    if (!m_host.canvas) return;

    // Position at right edge of canvas, vertically centered, 20px margin
    int panelW = this->width();
    int panelH = this->height();
    int x = m_host.canvas->width() - panelW - 20;
    int y = (m_host.canvas->height() - panelH) / 2;
    if (x < 0) x = 10;
    if (y < 0) y = 10;
    this->move(x, y);

    this->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    this->show();
    this->raise();

}

void FloatingMaskPanel::refresh()
{

    if (!m_host.canvas) return;

    ImagePrimitive *imgPrim = nullptr;
    for (auto *obj : m_host.canvas->selectedObjects()) {
        if (auto *ip = dynamic_cast<ImagePrimitive *>(obj)) {
            imgPrim = ip;
            break;
        }
    }

    if ((!imgPrim || imgPrim->getMaskCandidateCount() == 0) && m_host.canvas->layerManager()) {
        for (auto *p : m_host.canvas->layerManager()->getAllPrimitives()) {
            if (auto *ip = dynamic_cast<ImagePrimitive *>(p)) {
                if (ip->getMaskCandidateCount() > 0) {
                    imgPrim = ip;
                    break;
                }
            }
        }
    }

    if (!imgPrim || imgPrim->getMaskCandidateCount() == 0) {
        if (m_counterLabel) m_counterLabel->setText("- / -");
        return;
    }

    int count = imgPrim->getMaskCandidateCount();
    int current = imgPrim->getSelectedMaskIndex();

    // Counter
    if (m_counterLabel)
        m_counterLabel->setText(QString("%1 / %2").arg(current + 1).arg(count));

    // Stats from selected candidate
    const auto *c = imgPrim->getSelectedCandidate();
    if (c) {
        if (m_scoreLabel) m_scoreLabel->setText(QString("Score %1").arg(c->score, 0, 'f', 2));
        if (m_areaLabel) m_areaLabel->setText(QString("Area %1%").arg(c->area_percent, 0, 'f', 1));
        if (m_stabilityLabel) m_stabilityLabel->setText(QString("Stab %1").arg(c->stability, 0, 'f', 2));
        if (m_iouLabel) m_iouLabel->setText(QString("IoU %1").arg(c->predicted_iou, 0, 'f', 2));

        // Score bar width as percentage of panel width
        if (m_scoreBar) {
            int barW = static_cast<int>(c->score * 248); // 280 - 32 margins
            if (barW < 4) barW = 4;
            m_scoreBar->setFixedWidth(barW);
        }
    }

    // Nav buttons
    if (m_prevBtn) m_prevBtn->setEnabled(count > 1);
    if (m_nextBtn) m_nextBtn->setEnabled(count > 1);

    // Slider
    if (m_slider) {
        m_slider->blockSignals(true);
        m_slider->setRange(0, count - 1);
        m_slider->setValue(current);
        m_slider->blockSignals(false);
        m_slider->setVisible(count > 1);
    }

}
