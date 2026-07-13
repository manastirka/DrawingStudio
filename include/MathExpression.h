#pragma once

#include <QHash>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cmath>
#include <optional>

/**
 * Lightweight expression evaluator for math graphs and physics solving.
 * Supports + - * / ^, parentheses, common functions, pi, e, and named values.
 */
class MathExpression
{
public:
    explicit MathExpression(const QString &expression = {});

    void setExpression(const QString &expression);
    QString expression() const { return m_source; }
    QString normalized() const { return m_normalized; }

    /** Persistent named constants (physics constants, etc.). */
    void setNamedValues(const QHash<QString, double> &values);
    void addNamedValue(const QString &name, double value);
    QHash<QString, double> namedValues() const { return m_named; }

    QString error() const { return m_error; }
    bool isValid() const { return m_error.isEmpty() && !m_normalized.isEmpty(); }

    /** Evaluate with variable bindings (e.g. {{"v0",1},{"a",2},{"t",3}}). */
    std::optional<double> evaluate(const QHash<QString, double> &vars = {}) const;

    /** Convenience: bind x and evaluate (graphs). */
    std::optional<double> eval(double x) const;

    QVector<QPointF> sample(double xMin, double xMax, int samples) const;

    static QStringList functionNames();
    static QStringList constantNames();
    static QString tutorialMarkdown();

private:
    QString m_source;
    QString m_normalized;
    QString m_error;
    QHash<QString, double> m_named;

    static QString normalizeInput(const QString &raw);
    double parseExpression(const QString &s, int &i,
                           const QHash<QString, double> &vars,
                           QString *err) const;
    double parseTerm(const QString &s, int &i, const QHash<QString, double> &vars,
                     QString *err) const;
    double parsePower(const QString &s, int &i, const QHash<QString, double> &vars,
                      QString *err) const;
    double parseUnary(const QString &s, int &i, const QHash<QString, double> &vars,
                      QString *err) const;
    double parsePrimary(const QString &s, int &i, const QHash<QString, double> &vars,
                        QString *err) const;
    double parseIdentOrCall(const QString &s, int &i,
                            const QHash<QString, double> &vars,
                            QString *err) const;
    static void skipWs(const QString &s, int &i);
};
