/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#pragma once

#include <QtCore/QString>
#include <QtWidgets/QWidget>

#include <optional>

class QComboBox;
class QLineEdit;

class MatrixPowerLevelEditor final : public QWidget
{
    Q_OBJECT

public:
    explicit MatrixPowerLevelEditor(QWidget *parent = nullptr);
    virtual ~MatrixPowerLevelEditor() = default;

    void setValue(std::optional<qint64> explicitValue, qint64 inheritedValue);
    void setInheritedValue(qint64 inheritedValue);
    std::optional<qint64> explicitValue() const;
    bool isValueValid() const;
    void setEditorEnabled(bool enabled);

signals:
    void valueChanged();

private:
    enum class Mode
    {
        Default,
        Moderator,
        Administrator,
        Custom
    };

    QComboBox *m_modeCombo;
    QLineEdit *m_customValueEdit;
    qint64 m_inheritedValue = 0;

    Mode mode() const;
    void setMode(Mode mode);
    void refreshDefaultLabel();
    void refreshCustomEditor();
};

