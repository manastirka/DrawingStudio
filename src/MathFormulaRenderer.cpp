#include "MathFormulaRenderer.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
#include <memory>
#include <vector>

namespace {

thread_local int g_romanDepth = 0;

struct Box {
    virtual ~Box() = default;
    virtual QSizeF size(const QFont &font) const = 0;
    virtual void paint(QPainter &p, const QPointF &origin, const QFont &font,
                       const QColor &color) const = 0;
    virtual float baseline(const QFont &font) const
    {
        return size(font).height() * 0.72f;
    }
};

using BoxPtr = std::shared_ptr<Box>;

struct TextBox : Box {
    QString text;
    bool italic = false;
    explicit TextBox(QString t, bool it = false) : text(std::move(t)), italic(it) {}
    QFont f(const QFont &base) const
    {
        QFont x = base;
        x.setItalic(italic && g_romanDepth == 0);
        return x;
    }
    QSizeF size(const QFont &font) const override
    {
        QFontMetricsF fm(f(font));
        return QSizeF(fm.horizontalAdvance(text), fm.height());
    }
    float baseline(const QFont &font) const override
    {
        QFontMetricsF fm(f(font));
        return static_cast<float>(fm.ascent());
    }
    void paint(QPainter &p, const QPointF &origin, const QFont &font,
               const QColor &color) const override
    {
        p.setFont(f(font));
        p.setPen(color);
        QFontMetricsF fm(f(font));
        p.drawText(QPointF(origin.x(), origin.y() + fm.ascent()), text);
    }
};

struct SpaceBox : Box {
    float em = 0.3f;
    explicit SpaceBox(float e) : em(e) {}
    QSizeF size(const QFont &font) const override
    {
        return QSizeF(font.pointSizeF() * em, font.pointSizeF() * 0.2);
    }
    float baseline(const QFont &font) const override
    {
        return font.pointSizeF() * 0.15f;
    }
    void paint(QPainter &, const QPointF &, const QFont &,
               const QColor &) const override
    {
    }
};

struct RowBox : Box {
    std::vector<BoxPtr> kids;
    QSizeF size(const QFont &font) const override
    {
        float w = 0, above = 0, below = 0;
        for (const auto &k : kids) {
            const QSizeF s = k->size(font);
            const float bl = k->baseline(font);
            w += s.width();
            above = std::max<qreal>(above, bl);
            below = std::max<qreal>(below, s.height() - bl);
        }
        return QSizeF(w, above + below);
    }
    float baseline(const QFont &font) const override
    {
        float above = 0;
        for (const auto &k : kids)
            above = std::max<qreal>(above, k->baseline(font));
        return above;
    }
    void paint(QPainter &p, const QPointF &origin, const QFont &font,
               const QColor &color) const override
    {
        const float rowBl = baseline(font);
        float x = origin.x();
        for (const auto &k : kids) {
            const float bl = k->baseline(font);
            k->paint(p, QPointF(x, origin.y() + (rowBl - bl)), font, color);
            x += k->size(font).width();
        }
    }
};

struct ColBox : Box {
    std::vector<BoxPtr> kids;
    float gapEm = 0.45f;
    QSizeF size(const QFont &font) const override
    {
        float w = 0, h = 0;
        const float gap = font.pointSizeF() * gapEm;
        for (size_t i = 0; i < kids.size(); ++i) {
            const QSizeF s = kids[i]->size(font);
            w = std::max<qreal>(w, s.width());
            h += s.height();
            if (i + 1 < kids.size())
                h += gap;
        }
        return QSizeF(w, h);
    }
    float baseline(const QFont &font) const override
    {
        return kids.empty() ? 0.f : kids.front()->baseline(font);
    }
    void paint(QPainter &p, const QPointF &origin, const QFont &font,
               const QColor &color) const override
    {
        const float gap = font.pointSizeF() * gapEm;
        float y = origin.y();
        for (size_t i = 0; i < kids.size(); ++i) {
            const QSizeF s = kids[i]->size(font);
            const float x = origin.x() + (size(font).width() - s.width()) * 0.5;
            kids[i]->paint(p, QPointF(x, y), font, color);
            y += s.height();
            if (i + 1 < kids.size())
                y += gap;
        }
    }
};

struct FracBox : Box {
    BoxPtr num, den;
    QSizeF size(const QFont &font) const override
    {
        QFont small = font;
        small.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() * 0.85));
        const QSizeF ns = num->size(small);
        const QSizeF ds = den->size(small);
        const float gap = font.pointSizeF() * 0.45f;
        return QSizeF(std::max<qreal>(ns.width(), ds.width()) + 8,
                      ns.height() + ds.height() + gap);
    }
    float baseline(const QFont &font) const override
    {
        QFont small = font;
        small.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() * 0.85));
        return num->size(small).height() + font.pointSizeF() * 0.22f;
    }
    void paint(QPainter &p, const QPointF &origin, const QFont &font,
               const QColor &color) const override
    {
        QFont small = font;
        small.setPointSizeF(std::max<qreal>(8.0, font.pointSizeF() * 0.85));
        const QSizeF ns = num->size(small);
        const QSizeF ds = den->size(small);
        const float w = std::max<qreal>(ns.width(), ds.width()) + 8;
        const float gap = font.pointSizeF() * 0.45f;
        num->paint(p, QPointF(origin.x() + (w - ns.width()) * 0.5, origin.y()),
                   small, color);
        const float barY = origin.y() + ns.height() + gap * 0.35f;
        p.setPen(QPen(color, std::max<qreal>(1.0, font.pointSizeF() * 0.06)));
        p.drawLine(QPointF(origin.x() + 2, barY),
                   QPointF(origin.x() + w - 2, barY));
        den->paint(p,
                   QPointF(origin.x() + (w - ds.width()) * 0.5, barY + gap * 0.35f),
                   small, color);
    }
};

struct SupSubBox : Box {
    BoxPtr base, sup, sub;
    QSizeF size(const QFont &font) const override
    {
        QFont small = font;
        small.setPointSizeF(std::max<qreal>(7.0, font.pointSizeF() * 0.65));
        const QSizeF b = base->size(font);
        float extraW = 0, extraH = 0;
        if (sup) {
            const QSizeF s = sup->size(small);
            extraW = std::max<qreal>(extraW, s.width());
            extraH += s.height() * 0.55f;
        }
        if (sub) {
            const QSizeF s = sub->size(small);
            extraW = std::max<qreal>(extraW, s.width());
            extraH += s.height() * 0.35f;
        }
        return QSizeF(b.width() + extraW + 2, b.height() + extraH);
    }
    float baseline(const QFont &font) const override
    {
        QFont small = font;
        small.setPointSizeF(std::max<qreal>(7.0, font.pointSizeF() * 0.65));
        float bl = base->baseline(font);
        if (sup)
            bl += sup->size(small).height() * 0.45f;
        return bl;
    }
    void paint(QPainter &p, const QPointF &origin, const QFont &font,
               const QColor &color) const override
    {
        QFont small = font;
        small.setPointSizeF(std::max<qreal>(7.0, font.pointSizeF() * 0.65));
        float yOff = 0;
        if (sup)
            yOff = sup->size(small).height() * 0.45f;
        base->paint(p, QPointF(origin.x(), origin.y() + yOff), font, color);
        const float bx = origin.x() + base->size(font).width() + 1;
        if (sup)
            sup->paint(p, QPointF(bx, origin.y()), small, color);
        if (sub) {
            const float sy =
                origin.y() + yOff + base->size(font).height() -
                sub->size(small).height() * 0.35f;
            sub->paint(p, QPointF(bx, sy), small, color);
        }
    }
};

struct SqrtBox : Box {
    BoxPtr inner;
    QSizeF size(const QFont &font) const override
    {
        const QSizeF s = inner->size(font);
        return QSizeF(s.width() + font.pointSizeF() * 0.9f, s.height() + 4);
    }
    float baseline(const QFont &font) const override
    {
        return inner->baseline(font) + 2;
    }
    void paint(QPainter &p, const QPointF &origin, const QFont &font,
               const QColor &color) const override
    {
        const QSizeF s = inner->size(font);
        const float radical = font.pointSizeF() * 0.7f;
        inner->paint(p, QPointF(origin.x() + radical + 2, origin.y() + 2), font,
                     color);
        p.setPen(QPen(color, std::max<qreal>(1.2, font.pointSizeF() * 0.07)));
        QPainterPath path;
        path.moveTo(origin.x(), origin.y() + s.height() * 0.55f);
        path.lineTo(origin.x() + radical * 0.35f, origin.y() + s.height() * 0.35f);
        path.lineTo(origin.x() + radical * 0.7f, origin.y() + s.height() + 1);
        path.lineTo(origin.x() + radical + 2 + s.width(), origin.y() + 1);
        p.drawPath(path);
    }
};

static QString greek(const QString &cmd)
{
    static const QHash<QString, QString> map = {
        {QStringLiteral("alpha"), QStringLiteral("α")},
        {QStringLiteral("beta"), QStringLiteral("β")},
        {QStringLiteral("gamma"), QStringLiteral("γ")},
        {QStringLiteral("delta"), QStringLiteral("δ")},
        {QStringLiteral("epsilon"), QStringLiteral("ε")},
        {QStringLiteral("varepsilon"), QStringLiteral("ε")},
        {QStringLiteral("theta"), QStringLiteral("θ")},
        {QStringLiteral("lambda"), QStringLiteral("λ")},
        {QStringLiteral("mu"), QStringLiteral("μ")},
        {QStringLiteral("pi"), QStringLiteral("π")},
        {QStringLiteral("sigma"), QStringLiteral("σ")},
        {QStringLiteral("phi"), QStringLiteral("φ")},
        {QStringLiteral("omega"), QStringLiteral("ω")},
        {QStringLiteral("hbar"), QStringLiteral("ℏ")},
        {QStringLiteral("Alpha"), QStringLiteral("Α")},
        {QStringLiteral("Beta"), QStringLiteral("Β")},
        {QStringLiteral("Gamma"), QStringLiteral("Γ")},
        {QStringLiteral("Delta"), QStringLiteral("Δ")},
        {QStringLiteral("Pi"), QStringLiteral("Π")},
        {QStringLiteral("Sigma"), QStringLiteral("Σ")},
        {QStringLiteral("Omega"), QStringLiteral("Ω")},
        {QStringLiteral("infty"), QStringLiteral("∞")},
        {QStringLiteral("pm"), QStringLiteral("±")},
        {QStringLiteral("mp"), QStringLiteral("∓")},
        {QStringLiteral("leq"), QStringLiteral("≤")},
        {QStringLiteral("geq"), QStringLiteral("≥")},
        {QStringLiteral("neq"), QStringLiteral("≠")},
        {QStringLiteral("approx"), QStringLiteral("≈")},
        {QStringLiteral("times"), QStringLiteral("×")},
        {QStringLiteral("cdot"), QStringLiteral("·")},
        {QStringLiteral("div"), QStringLiteral("÷")},
        {QStringLiteral("rightarrow"), QStringLiteral("→")},
        {QStringLiteral("leftarrow"), QStringLiteral("←")},
        {QStringLiteral("Rightarrow"), QStringLiteral("⇒")},
        {QStringLiteral("Leftarrow"), QStringLiteral("⇐")},
        {QStringLiteral("sum"), QStringLiteral("∑")},
        {QStringLiteral("prod"), QStringLiteral("∏")},
        {QStringLiteral("int"), QStringLiteral("∫")},
        {QStringLiteral("partial"), QStringLiteral("∂")},
        {QStringLiteral("nabla"), QStringLiteral("∇")},
        {QStringLiteral("circ"), QStringLiteral("°")},
        {QStringLiteral("degree"), QStringLiteral("°")},
    };
    return map.value(cmd);
}

static BoxPtr parseExpr(const QString &s, int &i);

static BoxPtr parseGroup(const QString &s, int &i)
{
    if (i < s.size() && s[i] == QLatin1Char('{')) {
        ++i;
        auto row = std::make_shared<RowBox>();
        while (i < s.size() && s[i] != QLatin1Char('}')) {
            auto b = parseExpr(s, i);
            if (b)
                row->kids.push_back(b);
            else
                break;
        }
        if (i < s.size() && s[i] == QLatin1Char('}'))
            ++i;
        return row;
    }
    return parseExpr(s, i);
}

static BoxPtr parseAtom(const QString &s, int &i)
{
    while (i < s.size() && s[i].isSpace())
        ++i;
    if (i >= s.size())
        return nullptr;

    if (s[i] == QLatin1Char('{'))
        return parseGroup(s, i);

    if (s[i] == QLatin1Char('\\')) {
        ++i;
        int start = i;
        while (i < s.size() && s[i].isLetter())
            ++i;
        const QString cmd = s.mid(start, i - start);

        if (cmd == QLatin1String("frac")) {
            auto num = parseGroup(s, i);
            auto den = parseGroup(s, i);
            auto f = std::make_shared<FracBox>();
            f->num = num ? num : std::make_shared<TextBox>(QStringLiteral("?"));
            f->den = den ? den : std::make_shared<TextBox>(QStringLiteral("?"));
            return f;
        }
        if (cmd == QLatin1String("sqrt")) {
            auto inner = parseGroup(s, i);
            auto sq = std::make_shared<SqrtBox>();
            sq->inner =
                inner ? inner : std::make_shared<TextBox>(QStringLiteral("?"));
            return sq;
        }
        if (cmd == QLatin1String("mathrm") || cmd == QLatin1String("textrm") ||
            cmd == QLatin1String("text") || cmd == QLatin1String("rm")) {
            ++g_romanDepth;
            auto inner = parseGroup(s, i);
            --g_romanDepth;
            return inner ? inner
                         : std::make_shared<TextBox>(QString());
        }
        // Spacing
        if (cmd == QLatin1String("quad"))
            return std::make_shared<SpaceBox>(1.0f);
        if (cmd == QLatin1String("qquad"))
            return std::make_shared<SpaceBox>(2.0f);
        if (cmd == QLatin1String(",") || cmd == QLatin1String("thinspace"))
            return std::make_shared<SpaceBox>(0.17f);
        if (cmd == QLatin1String(";") || cmd == QLatin1String(":"))
            return std::make_shared<SpaceBox>(0.28f);
        if (cmd == QLatin1String("!") )
            return std::make_shared<SpaceBox>(-0.17f);
        if (cmd == QLatin1String(" ") || cmd == QLatin1String("space"))
            return std::make_shared<SpaceBox>(0.35f);
        if (cmd == QLatin1String("left") || cmd == QLatin1String("right") ||
            cmd.isEmpty()) {
            if (cmd.isEmpty() && i < s.size())
                return std::make_shared<TextBox>(QString(s[i++]));
            // skip delimiter after \left/\right
            if (i < s.size() &&
                (s[i] == QLatin1Char('(') || s[i] == QLatin1Char(')') ||
                 s[i] == QLatin1Char('[') || s[i] == QLatin1Char(']') ||
                 s[i] == QLatin1Char('|') || s[i] == QLatin1Char('.'))) {
                return std::make_shared<TextBox>(QString(s[i++]));
            }
            return parseAtom(s, i);
        }
        const QString g = greek(cmd);
        if (!g.isEmpty())
            return std::make_shared<TextBox>(g, false);
        // unknown command — skip silently (avoids "mathrm"/"quad" junk)
        return parseAtom(s, i);
    }

    if (s[i] == QLatin1Char('^') || s[i] == QLatin1Char('_'))
        return nullptr;

    int start = i;
    if (s[i].isLetter()) {
        while (i < s.size() && s[i].isLetter())
            ++i;
        const QString word = s.mid(start, i - start);
        const bool fn = word.size() > 1;
        return std::make_shared<TextBox>(word, !fn && g_romanDepth == 0);
    }
    if (s[i].isDigit() || s[i] == QLatin1Char('.')) {
        while (i < s.size() && (s[i].isDigit() || s[i] == QLatin1Char('.')))
            ++i;
        return std::make_shared<TextBox>(s.mid(start, i - start), false);
    }
    QString sym(s[i++]);
    if (sym == QLatin1String("*"))
        sym = QStringLiteral("·");
    return std::make_shared<TextBox>(sym, false);
}

static BoxPtr parseExpr(const QString &s, int &i)
{
    while (i < s.size() && s[i].isSpace())
        ++i;
    if (i >= s.size() || s[i] == QLatin1Char('}'))
        return nullptr;

    BoxPtr base = parseAtom(s, i);
    if (!base)
        return nullptr;

    for (;;) {
        while (i < s.size() && s[i].isSpace())
            ++i;
        if (i >= s.size())
            break;
        if (s[i] != QLatin1Char('^') && s[i] != QLatin1Char('_'))
            break;
        auto ss = std::make_shared<SupSubBox>();
        ss->base = base;
        while (i < s.size() &&
               (s[i] == QLatin1Char('^') || s[i] == QLatin1Char('_'))) {
            const bool isSup = s[i] == QLatin1Char('^');
            ++i;
            auto g = parseGroup(s, i);
            if (isSup)
                ss->sup = g;
            else
                ss->sub = g;
        }
        base = ss;
    }
    return base;
}

static BoxPtr parseAll(const QString &input)
{
    QString s = input.trimmed();
    if (s.startsWith(QLatin1Char('$')) && s.endsWith(QLatin1Char('$')) &&
        s.size() >= 2)
        s = s.mid(1, s.size() - 2);
    s.replace(QStringLiteral("\\("), QString());
    s.replace(QStringLiteral("\\)"), QString());

    auto row = std::make_shared<RowBox>();
    int i = 0;
    while (i < s.size()) {
        auto b = parseExpr(s, i);
        if (!b) {
            if (i < s.size()) {
                if (s[i] == QLatin1Char('}')) {
                    ++i;
                    continue;
                }
                row->kids.push_back(std::make_shared<TextBox>(QString(s[i++])));
            }
            continue;
        }
        row->kids.push_back(b);
    }
    return row;
}

static QImage paintBox(const BoxPtr &root, const QFont &font, const QColor &color,
                       const QColor &bg, bool drawBg, int padding)
{
    if (!root)
        return {};
    const QSizeF logical = root->size(font);
    const int w =
        qMax(8, static_cast<int>(std::ceil(logical.width())) + padding * 2);
    const int h =
        qMax(8, static_cast<int>(std::ceil(logical.height())) + padding * 2);

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(drawBg ? bg : Qt::transparent);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    root->paint(p, QPointF(padding, padding), font, color);
    p.end();
    return img;
}

} // namespace

QImage MathFormulaRenderer::render(const QString &latexOrPlain, const Options &opt)
{
    if (latexOrPlain.trimmed().isEmpty())
        return {};

    BoxPtr root = parseAll(latexOrPlain);
    QFont font = opt.font;
    if (font.family().isEmpty())
        font = QFont(QStringLiteral("Times New Roman"), 28);
    return paintBox(root, font, opt.textColor, opt.background, opt.drawBackground,
                    opt.padding);
}

QImage MathFormulaRenderer::renderPaperSolution(const QStringList &equationLines,
                                                const QString &answerLatex,
                                                const Options &opt)
{
    QFont eqFont = opt.font;
    if (eqFont.family().isEmpty())
        eqFont = QFont(QStringLiteral("Times New Roman"), 26);
    QFont ansFont = eqFont;
    ansFont.setPointSizeF(eqFont.pointSizeF() * 1.18);
    ansFont.setBold(true);

    const int pad = std::max(20, opt.padding);
    const QColor ink = opt.textColor;
    const QColor paper =
        opt.drawBackground ? opt.background : QColor(255, 255, 255);
    const QColor rule(180, 180, 185);

    // Measure lines
    QVector<BoxPtr> eqBoxes;
    QVector<QSizeF> eqSizes;
    float contentW = 0;
    float contentH = 0;
    const float lineGap = eqFont.pointSizeF() * 0.55f;

    for (const QString &line : equationLines) {
        if (line.trimmed().isEmpty())
            continue;
        auto b = parseAll(line);
        const QSizeF s = b->size(eqFont);
        eqBoxes.push_back(b);
        eqSizes.push_back(s);
        contentW = std::max<qreal>(contentW, s.width());
        contentH += s.height() + lineGap;
    }
    if (contentH > 0)
        contentH -= lineGap;

    BoxPtr ansBox = parseAll(answerLatex);
    const QSizeF ansSize = ansBox ? ansBox->size(ansFont) : QSizeF();
    contentW = std::max<qreal>(contentW, ansSize.width());

    const float ruleGap = eqFont.pointSizeF() * 0.55f;
    const float afterRule = eqFont.pointSizeF() * 0.65f;
    const float ruleH = 1.0f;

    float totalH = contentH;
    if (!eqBoxes.isEmpty() && ansBox)
        totalH += ruleGap + ruleH + afterRule;
    totalH += ansSize.height();

    const int w = qMax(120, static_cast<int>(std::ceil(contentW)) + pad * 2);
    const int h = qMax(80, static_cast<int>(std::ceil(totalH)) + pad * 2);

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(paper);

    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    float y = pad;
    for (int i = 0; i < eqBoxes.size(); ++i) {
        const float x = pad + (contentW - eqSizes[i].width()) * 0.5f;
        eqBoxes[i]->paint(p, QPointF(x, y), eqFont, ink);
        y += eqSizes[i].height();
        if (i + 1 < eqBoxes.size())
            y += lineGap;
    }

    if (!eqBoxes.isEmpty() && ansBox) {
        y += ruleGap;
        p.setPen(QPen(rule, 1.0));
        const float ruleInset = pad * 0.35f;
        p.drawLine(QPointF(ruleInset, y), QPointF(w - ruleInset, y));
        y += ruleH + afterRule;
    }

    if (ansBox) {
        const float x = pad + (contentW - ansSize.width()) * 0.5f;
        ansBox->paint(p, QPointF(x, y), ansFont, ink);
    }

    // Soft paper edge
    p.setPen(QPen(QColor(210, 210, 214), 1.0));
    p.setBrush(Qt::NoBrush);
    p.drawRect(0, 0, w - 1, h - 1);

    p.end();
    return img;
}
