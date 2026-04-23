#pragma once
#include <QObject>
#include <QColor>
#include <QColorDialog>

class ColorDialogHelper : public QObject
{
    Q_OBJECT
public:
    explicit ColorDialogHelper(QObject* parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QColor openColorDialog(const QColor& current) {
        QColor chosen = QColorDialog::getColor(current, nullptr, "Select a Color");
        if (chosen.isValid())
            return chosen;

        return current; // fallback, dialog cancelled
    }
};
