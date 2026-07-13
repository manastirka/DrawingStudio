#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QString>
#include <QStringList>

/** Render LaTeX-ish / plain math to a transparent (or paper) QImage. */
class MathFormulaRenderer
{
public:
    struct Options {
        QFont font;
        QColor textColor = QColor(20, 20, 24);
        QColor background = Qt::transparent;
        int padding = 16;
        bool drawBackground = false;

        Options() : font(QStringLiteral("Times New Roman"), 28) {}
    };

    /** Single expression / line. */
    static QImage render(const QString &latexOrPlain, const Options &opt);

    /**
     * Paper-ready solution card:
     *   equation line(s)
     *   thin rule
     *   answer line (slightly larger)
     */
    static QImage renderPaperSolution(const QStringList &equationLines,
                                      const QString &answerLatex,
                                      const Options &opt);
};
