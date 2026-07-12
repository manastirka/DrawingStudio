#include "SpinnerDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

SpinnerDialog::SpinnerDialog(const QString& title, const QString& message, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setModal(true);
    setMinimumWidth(350);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    
    // Spinner
    m_spinner = new ColorfulSpinner(this);
    m_spinner->setFixedSize(64, 64);
    m_spinner->start();
    
    QHBoxLayout* spinnerLayout = new QHBoxLayout();
    spinnerLayout->addStretch();
    spinnerLayout->addWidget(m_spinner);
    spinnerLayout->addStretch();
    mainLayout->addLayout(spinnerLayout);
    
    // Message label
    m_messageLabel = new QLabel(message, this);
    m_messageLabel->setAlignment(Qt::AlignCenter);
    m_messageLabel->setWordWrap(true);
    QFont font = m_messageLabel->font();
    font.setPointSize(12);
    m_messageLabel->setFont(font);
    mainLayout->addWidget(m_messageLabel);
    
    mainLayout->addSpacing(10);
    
    // Cancel button
    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setFixedWidth(100);
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        emit canceled();
        reject();
    });
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    // Prevent resizing
    setFixedHeight(sizeHint().height());
}

void SpinnerDialog::setProgress(int percentage) {
    m_spinner->setProgress(percentage);
}

void SpinnerDialog::setMessage(const QString& message) {
    m_messageLabel->setText(message);
}

void SpinnerDialog::setCancelEnabled(bool enabled) {
    m_cancelButton->setEnabled(enabled);
}
