#ifndef SPINNERDIALOG_H
#define SPINNERDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include "ColorfulSpinner.h"

class SpinnerDialog : public QDialog {
    Q_OBJECT
    
public:
    explicit SpinnerDialog(const QString& title, const QString& message, QWidget* parent = nullptr);
    
    void setProgress(int percentage);
    void setMessage(const QString& message);
    void setCancelEnabled(bool enabled);
    
signals:
    void canceled();
    
private:
    ColorfulSpinner* m_spinner;
    QLabel* m_messageLabel;
    QPushButton* m_cancelButton;
};

#endif // SPINNERDIALOG_H
