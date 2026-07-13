#include "MathExpression.h"

#include <QHash>
#include <QPointF>
#include <QVector>
#include <QtMath>
#include <cctype>

MathExpression::MathExpression(const QString &expression)
{
    setExpression(expression);
}

void MathExpression::setExpression(const QString &expression)
{
    m_source = expression;
    m_error.clear();
    m_normalized = normalizeInput(expression);
    if (m_normalized.isEmpty())
        m_error = QStringLiteral("Expression is empty.");
    // Full parse validation happens in evaluate() with real bindings.
}

void MathExpression::setNamedValues(const QHash<QString, double> &values)
{
    m_named.clear();
    for (auto it = values.begin(); it != values.end(); ++it)
        m_named.insert(it.key().toLower(), it.value());
}

void MathExpression::addNamedValue(const QString &name, double value)
{
    m_named.insert(name.toLower(), value);
}

QString MathExpression::normalizeInput(const QString &raw)
{
    QString s = raw.trimmed();
    // Strip $...$ / \( \) wrappers common in LaTeX tutorials
    if (s.startsWith(QLatin1Char('$')) && s.endsWith(QLatin1Char('$')) && s.size() >= 2)
        s = s.mid(1, s.size() - 2).trimmed();
    s.replace(QStringLiteral("\\("), QString());
    s.replace(QStringLiteral("\\)"), QString());

    // LaTeX command aliases → plain functions
    static const QList<QPair<QString, QString>> reps = {
        {QStringLiteral("\\cdot"), QStringLiteral("*")},
        {QStringLiteral("\\times"), QStringLiteral("*")},
        {QStringLiteral("\\div"), QStringLiteral("/")},
        {QStringLiteral("\\pi"), QStringLiteral("pi")},
        {QStringLiteral("\\sin"), QStringLiteral("sin")},
        {QStringLiteral("\\cos"), QStringLiteral("cos")},
        {QStringLiteral("\\tan"), QStringLiteral("tan")},
        {QStringLiteral("\\arcsin"), QStringLiteral("asin")},
        {QStringLiteral("\\arccos"), QStringLiteral("acos")},
        {QStringLiteral("\\arctan"), QStringLiteral("atan")},
        {QStringLiteral("\\sinh"), QStringLiteral("sinh")},
        {QStringLiteral("\\cosh"), QStringLiteral("cosh")},
        {QStringLiteral("\\tanh"), QStringLiteral("tanh")},
        {QStringLiteral("\\exp"), QStringLiteral("exp")},
        {QStringLiteral("\\ln"), QStringLiteral("ln")},
        {QStringLiteral("\\log"), QStringLiteral("log")},
        {QStringLiteral("\\sqrt"), QStringLiteral("sqrt")},
        {QStringLiteral("\\abs"), QStringLiteral("abs")},
        {QStringLiteral("\\left"), QString()},
        {QStringLiteral("\\right"), QString()},
        {QStringLiteral("\\,"), QString()},
        {QStringLiteral("\\;"), QString()},
        {QStringLiteral("\\:"), QString()},
        {QStringLiteral("\\!"), QString()},
        {QStringLiteral("{"), QStringLiteral("(")},
        {QStringLiteral("}"), QStringLiteral(")")},
    };
    for (const auto &p : reps)
        s.replace(p.first, p.second);

    // Implicit multiplication: 2x → 2*x, )( → )*(, 2sin → 2*sin, x( → x*(
    // but keep sin( as a function call (no star before '(' after a known fn).
    QString clean;
    clean.reserve(s.size() * 2);
    auto isIdentChar = [](QChar ch) {
        return ch.isLetter() || ch == QLatin1Char('_');
    };
    for (int i = 0; i < s.size(); ++i) {
        if (i > 0) {
            const QChar a = s[i - 1];
            const QChar b = s[i];
            const bool needStar =
                ((a.isDigit() || a == QLatin1Char('.')) &&
                 (isIdentChar(b) || b == QLatin1Char('('))) ||
                (a == QLatin1Char(')') &&
                 (isIdentChar(b) || b.isDigit() || b == QLatin1Char('(') ||
                  b == QLatin1Char('.'))) ||
                (isIdentChar(a) && b == QLatin1Char('('));
            if (needStar) {
                int j = i - 1;
                while (j >= 0 && isIdentChar(s[j]))
                    --j;
                const QString name = s.mid(j + 1, i - 1 - j).toLower();
                const bool fn = functionNames().contains(name) ||
                                name == QLatin1String("log10");
                if (!(fn && b == QLatin1Char('(')))
                    clean.append(QLatin1Char('*'));
            }
        }
        clean.append(s[i]);
    }
    clean.remove(QLatin1Char(' '));
    clean.remove(QLatin1Char('\t'));
    clean.remove(QLatin1Char('\n'));
    return clean;
}


std::optional<double> MathExpression::evaluate(const QHash<QString, double> &vars) const
{
    if (!isValid())
        return std::nullopt;
    QHash<QString, double> merged = m_named;
    for (auto it = vars.begin(); it != vars.end(); ++it)
        merged.insert(it.key().toLower(), it.value());
    int i = 0;
    QString err;
    const double v = parseExpression(m_normalized, i, merged, &err);
    if (!err.isEmpty() || !std::isfinite(v))
        return std::nullopt;
    return v;
}

std::optional<double> MathExpression::eval(double x) const
{
    QHash<QString, double> vars;
    vars.insert(QStringLiteral("x"), x);
    return evaluate(vars);
}

QVector<QPointF> MathExpression::sample(double xMin, double xMax, int samples) const
{
    QVector<QPointF> pts;
    if (!isValid() || samples < 2 || !(xMax > xMin))
        return pts;
    pts.reserve(samples);
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / (samples - 1);
        const double x = xMin + t * (xMax - xMin);
        if (auto y = eval(x))
            pts.append(QPointF(x, *y));
    }
    return pts;
}

QStringList MathExpression::functionNames()
{
    return {QStringLiteral("sin"),   QStringLiteral("cos"),
            QStringLiteral("tan"),   QStringLiteral("asin"),
            QStringLiteral("acos"),  QStringLiteral("atan"),
            QStringLiteral("sinh"),  QStringLiteral("cosh"),
            QStringLiteral("tanh"),  QStringLiteral("exp"),
            QStringLiteral("ln"),    QStringLiteral("log"),
            QStringLiteral("log10"), QStringLiteral("sqrt"),
            QStringLiteral("abs"),   QStringLiteral("floor"),
            QStringLiteral("ceil"),  QStringLiteral("round"),
            QStringLiteral("sign"),  QStringLiteral("min"),
            QStringLiteral("max")};
}

QStringList MathExpression::constantNames()
{
    return {QStringLiteral("pi"), QStringLiteral("e"), QStringLiteral("x")};
}

QString MathExpression::tutorialMarkdown()
{
    return QStringLiteral(
        "<h2>How to enter math</h2>"
        "<p>DrawingStudio accepts <b>plain formulas</b> and light "
        "<b>LaTeX-style</b> input. Graphs use the same language for "
        "<code>y = f(x)</code>.</p>"

        "<h3>1. Basics</h3>"
        "<ul>"
        "<li><code>2 + 3 * x</code> — addition, multiplication</li>"
        "<li><code>(x + 1) / (x - 1)</code> — parentheses</li>"
        "<li><code>x^2</code> or <code>x**2</code> — powers (use <code>^</code>)</li>"
        "<li><code>2x</code> — same as <code>2*x</code> (implicit multiply)</li>"
        "<li><code>pi</code>, <code>e</code> — constants (&pi;, Euler&rsquo;s number)</li>"
        "</ul>"

        "<h3>2. Functions</h3>"
        "<p>Always write parentheses: <code>sin(x)</code>, not <code>sin x</code>.</p>"
        "<ul>"
        "<li>Trig: <code>sin</code> <code>cos</code> <code>tan</code> "
        "<code>asin</code> <code>acos</code> <code>atan</code></li>"
        "<li>Hyperbolic: <code>sinh</code> <code>cosh</code> <code>tanh</code></li>"
        "<li>Exp / log: <code>exp(x)</code> <code>ln(x)</code> <code>log(x)</code> "
        "(=ln) <code>log10(x)</code></li>"
        "<li>Other: <code>sqrt(x)</code> <code>abs(x)</code> <code>floor</code> "
        "<code>ceil</code> <code>round</code> <code>sign</code></li>"
        "<li>Two-arg: <code>min(a,b)</code> <code>max(a,b)</code></li>"
        "</ul>"

        "<h3>3. LaTeX shortcuts (optional)</h3>"
        "<ul>"
        "<li><code>\\sin(x)</code>, <code>\\cos(x)</code>, <code>\\tan(x)</code></li>"
        "<li><code>\\pi</code>, <code>\\sqrt(x)</code>, <code>\\ln(x)</code></li>"
        "<li><code>\\cdot</code> / <code>\\times</code> &rarr; multiply</li>"
        "<li>Braces <code>{ }</code> are treated like parentheses for graphs</li>"
        "<li>You may wrap formula display in <code>$ ... $</code></li>"
        "</ul>"
        "<p><b>Note:</b> Display formulas (Insert Formula) support richer LaTeX "
        "like <code>\\frac{a}{b}</code>, <code>x^{2}</code>, <code>\\sqrt{x}</code>. "
        "Graphs need an evaluable expression such as <code>a/b</code> or "
        "<code>x^2</code>.</p>"

        "<h3>4. Graph examples</h3>"
        "<ul>"
        "<li><code>sin(x)</code></li>"
        "<li><code>x^2 - 2*x + 1</code></li>"
        "<li><code>exp(-x^2)</code> — Gaussian bump</li>"
        "<li><code>1/(1+x^2)</code> — witch of Agnesi</li>"
        "<li><code>sin(x)/x</code> — sinc (undefined at 0; gap appears)</li>"
        "<li><code>abs(x)</code>, <code>floor(x)</code></li>"
        "</ul>"

        "<h3>5. Formula display examples</h3>"
        "<ul>"
        "<li><code>E = mc^2</code></li>"
        "<li><code>\\frac{-b \\pm \\sqrt{b^2-4ac}}{2a}</code></li>"
        "<li><code>\\int_0^1 x^2 \\, dx</code></li>"
        "<li><code>\\sum_{n=1}^{N} \\frac{1}{n^2}</code></li>"
        "<li><code>a_{ij} = \\bar{x}_i</code></li>"
        "</ul>"

        "<h3>6. Tips</h3>"
        "<ul>"
        "<li>Angles for trig are in <b>radians</b> (use <code>sin(pi/2)</code>).</li>"
        "<li>If the graph is empty, check domain (e.g. <code>ln(x)</code> needs "
        "x &gt; 0) or syntax errors shown in red.</li>"
        "<li>Use the example chips in the dialog to paste starters.</li>"
        "</ul>");
}


void MathExpression::skipWs(const QString &s, int &i)
{
    while (i < s.size() && s[i].isSpace())
        ++i;
}

double MathExpression::parseExpression(const QString &s, int &i,
                                       const QHash<QString, double> &vars,
                                       QString *err) const
{
    double v = parseTerm(s, i, vars, err);
    if (err && !err->isEmpty())
        return v;
    for (;;) {
        skipWs(s, i);
        if (i >= s.size())
            break;
        const QChar op = s[i];
        if (op != QLatin1Char('+') && op != QLatin1Char('-'))
            break;
        ++i;
        const double r = parseTerm(s, i, vars, err);
        if (err && !err->isEmpty())
            return v;
        v = (op == QLatin1Char('+')) ? v + r : v - r;
    }
    return v;
}

double MathExpression::parseTerm(const QString &s, int &i,
                                 const QHash<QString, double> &vars,
                                 QString *err) const
{
    double v = parsePower(s, i, vars, err);
    if (err && !err->isEmpty())
        return v;
    for (;;) {
        skipWs(s, i);
        if (i >= s.size())
            break;
        const QChar op = s[i];
        if (op != QLatin1Char('*') && op != QLatin1Char('/'))
            break;
        ++i;
        const double r = parsePower(s, i, vars, err);
        if (err && !err->isEmpty())
            return v;
        if (op == QLatin1Char('*'))
            v *= r;
        else {
            if (r == 0.0) {
                if (err)
                    *err = QStringLiteral("Division by zero.");
                return 0;
            }
            v /= r;
        }
    }
    return v;
}

double MathExpression::parsePower(const QString &s, int &i,
                                  const QHash<QString, double> &vars,
                                  QString *err) const
{
    double v = parseUnary(s, i, vars, err);
    if (err && !err->isEmpty())
        return v;
    skipWs(s, i);
    if (i < s.size() &&
        (s[i] == QLatin1Char('^') ||
         (s[i] == QLatin1Char('*') && i + 1 < s.size() &&
          s[i + 1] == QLatin1Char('*')))) {
        if (s[i] == QLatin1Char('*'))
            i += 2;
        else
            ++i;
        const double e = parsePower(s, i, vars, err);
        if (err && !err->isEmpty())
            return v;
        v = std::pow(v, e);
    }
    return v;
}

double MathExpression::parseUnary(const QString &s, int &i,
                                  const QHash<QString, double> &vars,
                                  QString *err) const
{
    skipWs(s, i);
    if (i < s.size() && s[i] == QLatin1Char('+')) {
        ++i;
        return parseUnary(s, i, vars, err);
    }
    if (i < s.size() && s[i] == QLatin1Char('-')) {
        ++i;
        return -parseUnary(s, i, vars, err);
    }
    return parsePrimary(s, i, vars, err);
}

double MathExpression::parsePrimary(const QString &s, int &i,
                                    const QHash<QString, double> &vars,
                                    QString *err) const
{
    skipWs(s, i);
    if (i >= s.size()) {
        if (err)
            *err = QStringLiteral("Unexpected end of expression.");
        return 0;
    }
    if (s[i] == QLatin1Char('(')) {
        ++i;
        const double v = parseExpression(s, i, vars, err);
        skipWs(s, i);
        if (i >= s.size() || s[i] != QLatin1Char(')')) {
            if (err)
                *err = QStringLiteral("Missing ')'.");
            return v;
        }
        ++i;
        return v;
    }
    if (s[i].isDigit() || s[i] == QLatin1Char('.')) {
        int start = i;
        while (i < s.size() && (s[i].isDigit() || s[i] == QLatin1Char('.')))
            ++i;
        bool ok = false;
        const double v = s.mid(start, i - start).toDouble(&ok);
        if (!ok) {
            if (err)
                *err = QStringLiteral("Invalid number.");
            return 0;
        }
        return v;
    }
    if (s[i].isLetter())
        return parseIdentOrCall(s, i, vars, err);
    if (err)
        *err = QStringLiteral("Unexpected character: %1").arg(s[i]);
    return 0;
}

double MathExpression::parseIdentOrCall(const QString &s, int &i,
                                        const QHash<QString, double> &vars,
                                        QString *err) const
{
    int start = i;
    while (i < s.size() && (s[i].isLetterOrNumber() || s[i] == QLatin1Char('_')))
        ++i;
    const QString name = s.mid(start, i - start);
    const QString key = name.toLower();

    if (vars.contains(key))
        return vars.value(key);
    if (m_named.contains(key))
        return m_named.value(key);

    if (key == QLatin1String("pi"))
        return M_PI;
    // Euler's number — only when not overridden; avoid bare physics charge conflict
    if (key == QLatin1String("e"))
        return M_E;

    skipWs(s, i);
    if (i >= s.size() || s[i] != QLatin1Char('(')) {
        if (err)
            *err = QStringLiteral("Unknown name or missing '(': %1").arg(name);
        return 0;
    }
    ++i;
    const double a = parseExpression(s, i, vars, err);
    if (err && !err->isEmpty())
        return 0;
    double b = 0;
    bool two = false;
    skipWs(s, i);
    if (i < s.size() && s[i] == QLatin1Char(',')) {
        ++i;
        b = parseExpression(s, i, vars, err);
        two = true;
        if (err && !err->isEmpty())
            return 0;
    }
    skipWs(s, i);
    if (i >= s.size() || s[i] != QLatin1Char(')')) {
        if (err)
            *err = QStringLiteral("Missing ')' after function call.");
        return 0;
    }
    ++i;

    if (key == QLatin1String("sin")) return std::sin(a);
    if (key == QLatin1String("cos")) return std::cos(a);
    if (key == QLatin1String("tan")) return std::tan(a);
    if (key == QLatin1String("asin")) return std::asin(a);
    if (key == QLatin1String("acos")) return std::acos(a);
    if (key == QLatin1String("atan")) return std::atan(a);
    if (key == QLatin1String("sinh")) return std::sinh(a);
    if (key == QLatin1String("cosh")) return std::cosh(a);
    if (key == QLatin1String("tanh")) return std::tanh(a);
    if (key == QLatin1String("exp")) return std::exp(a);
    if (key == QLatin1String("ln") || key == QLatin1String("log")) return std::log(a);
    if (key == QLatin1String("log10")) return std::log10(a);
    if (key == QLatin1String("sqrt")) return std::sqrt(a);
    if (key == QLatin1String("abs")) return std::fabs(a);
    if (key == QLatin1String("floor")) return std::floor(a);
    if (key == QLatin1String("ceil")) return std::ceil(a);
    if (key == QLatin1String("round")) return std::round(a);
    if (key == QLatin1String("sign")) return (a > 0) - (a < 0);
    if (key == QLatin1String("min") && two) return std::min(a, b);
    if (key == QLatin1String("max") && two) return std::max(a, b);

    if (err)
        *err = QStringLiteral("Unknown function: %1").arg(name);
    return 0;
}
