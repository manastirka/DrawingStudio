#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

struct PhysicsConstant {
    QString id;
    QString name;
    QString symbolLatex;
    QString unit;
    double value = 0.0;
    QString category;
};

struct PhysicsVariable {
    QString id;
    QString name;
    QString symbolLatex;
    QString unit;
};

struct PhysicsEquation {
    QString id;
    QString field;
    QString name;
    QString latex;
    QString description;
    QVector<PhysicsVariable> variables;
    /** variable id → MathExpression formula using other variable ids (+ constants). */
    QHash<QString, QString> solve;
    QString defaultUnknown;
};

class PhysicsCatalog
{
public:
    static QVector<PhysicsConstant> constants();
    static QHash<QString, double> constantValues();
    static QVector<PhysicsEquation> equations();
    static QStringList fields();
    static QVector<PhysicsEquation> equationsInField(const QString &field);
    static const PhysicsEquation *equationById(const QString &id);
};
