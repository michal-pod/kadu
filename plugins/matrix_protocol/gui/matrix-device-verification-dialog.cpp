/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "matrix-device-verification-dialog.h"
#include "matrix-device-verification-dialog.moc"

#include "matrix-device-verification-widget.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

MatrixDeviceVerificationDialog::MatrixDeviceVerificationDialog(
    Quotient::KeyVerificationSession *session, QWidget *parent)
    : QDialog{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Verify Matrix Device"));

    auto *layout = new QVBoxLayout{this};
    m_verificationWidget = new MatrixDeviceVerificationWidget{session, this};
    layout->addWidget(m_verificationWidget);

    auto *buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_closeButton = buttons->addButton(QDialogButtonBox::Close);
    m_closeButton->setText(tr("Cancel"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_verificationWidget, &MatrixDeviceVerificationWidget::verificationSucceeded,
            this, [this] { m_closeButton->setText(tr("Close")); });
    connect(m_verificationWidget, &MatrixDeviceVerificationWidget::verificationFailed,
            this, [this] { m_closeButton->setText(tr("Close")); });
}

void MatrixDeviceVerificationDialog::reject()
{
    m_verificationWidget->cancelVerification();
    QDialog::reject();
}
