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

#include "matrix-power-level-editor.h"
#include "matrix-power-level-editor.moc"

#include <QtCore/QSignalBlocker>
#include <QtCore/QRegularExpression>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLineEdit>

constexpr qint64 MATRIX_MINIMUM_POWER_LEVEL = -9007199254740991LL;
constexpr qint64 MATRIX_MAXIMUM_POWER_LEVEL = 9007199254740991LL;

MatrixPowerLevelEditor::MatrixPowerLevelEditor(QWidget *parent)
        : QWidget{parent}, m_modeCombo{new QComboBox{this}}, m_customValueEdit{new QLineEdit{this}}
{
    auto layout = new QHBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_modeCombo, 1);
    layout->addWidget(m_customValueEdit);

    m_modeCombo->addItem({}, static_cast<int>(Mode::Default));
    m_modeCombo->addItem(tr("Moderator"), static_cast<int>(Mode::Moderator));
    m_modeCombo->addItem(tr("Administrator"), static_cast<int>(Mode::Administrator));
    m_modeCombo->insertSeparator(m_modeCombo->count());
    m_modeCombo->addItem(tr("Custom..."), static_cast<int>(Mode::Custom));

    m_customValueEdit->setMaximumWidth(150);
    m_customValueEdit->setPlaceholderText(tr("Power level"));
    m_customValueEdit->setValidator(
        new QRegularExpressionValidator{QRegularExpression{QStringLiteral("-?[0-9]{0,16}")}, m_customValueEdit});
    m_customValueEdit->setToolTip(
        tr("Recommended values are between 0 and 100. Matrix also permits custom values outside this range."));

    connect(m_modeCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (mode() == Mode::Custom && m_customValueEdit->text().isEmpty())
            m_customValueEdit->setText(QString::number(m_inheritedValue));
        refreshCustomEditor();
        emit valueChanged();
    });
    connect(m_customValueEdit, &QLineEdit::textChanged, this, [this] { emit valueChanged(); });

    refreshDefaultLabel();
    refreshCustomEditor();
}

void MatrixPowerLevelEditor::setValue(std::optional<qint64> explicitValue, qint64 inheritedValue)
{
    const QSignalBlocker comboBlocker{m_modeCombo};
    const QSignalBlocker valueBlocker{m_customValueEdit};
    m_inheritedValue = inheritedValue;
    refreshDefaultLabel();

    if (!explicitValue)
        setMode(Mode::Default);
    else if (*explicitValue == 50)
        setMode(Mode::Moderator);
    else if (*explicitValue == 100)
        setMode(Mode::Administrator);
    else
    {
        setMode(Mode::Custom);
        m_customValueEdit->setText(QString::number(*explicitValue));
    }
    refreshCustomEditor();
}

void MatrixPowerLevelEditor::setInheritedValue(qint64 inheritedValue)
{
    m_inheritedValue = inheritedValue;
    refreshDefaultLabel();
}

std::optional<qint64> MatrixPowerLevelEditor::explicitValue() const
{
    switch (mode())
    {
    case Mode::Default:
        return std::nullopt;
    case Mode::Moderator:
        return 50;
    case Mode::Administrator:
        return 100;
    case Mode::Custom:
    {
        bool valid = false;
        const auto value = m_customValueEdit->text().toLongLong(&valid);
        return valid && value >= MATRIX_MINIMUM_POWER_LEVEL && value <= MATRIX_MAXIMUM_POWER_LEVEL
                   ? std::optional<qint64>{value}
                   : std::nullopt;
    }
    }
    return std::nullopt;
}

bool MatrixPowerLevelEditor::isValueValid() const
{
    if (mode() != Mode::Custom)
        return true;

    bool valid = false;
    const auto value = m_customValueEdit->text().toLongLong(&valid);
    return valid && value >= MATRIX_MINIMUM_POWER_LEVEL && value <= MATRIX_MAXIMUM_POWER_LEVEL;
}

void MatrixPowerLevelEditor::setEditorEnabled(bool enabled)
{
    m_modeCombo->setEnabled(enabled);
    m_customValueEdit->setEnabled(enabled);
}

MatrixPowerLevelEditor::Mode MatrixPowerLevelEditor::mode() const
{
    return static_cast<Mode>(m_modeCombo->currentData().toInt());
}

void MatrixPowerLevelEditor::setMode(Mode mode)
{
    const auto index = m_modeCombo->findData(static_cast<int>(mode));
    if (index >= 0)
        m_modeCombo->setCurrentIndex(index);
}

void MatrixPowerLevelEditor::refreshDefaultLabel()
{
    m_modeCombo->setItemText(0, tr("Default"));
}

void MatrixPowerLevelEditor::refreshCustomEditor()
{
    m_customValueEdit->setVisible(mode() == Mode::Custom);
}
