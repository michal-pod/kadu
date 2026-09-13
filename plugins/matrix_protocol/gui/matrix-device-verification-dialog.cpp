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

#include "icons/icons-manager.h"
#include "icons/kadu-icon.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

MatrixDeviceVerificationDialog::MatrixDeviceVerificationDialog(
    Quotient::KeyVerificationSession *session, IconsManager *iconsManager, QWidget *parent)
    : QDialog{parent}, m_iconsManager{iconsManager}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Verify Matrix Device"));

    if (m_iconsManager)
        setWindowIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("security-high")}));

    auto *layout = new QVBoxLayout{this};
    m_verificationWidget = new MatrixDeviceVerificationWidget{session, m_iconsManager, this};
    layout->addWidget(m_verificationWidget);

    auto *buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_closeButton = buttons->addButton(QDialogButtonBox::Close);
    m_closeButton->setText(tr("Cancel"));
    if (m_iconsManager)
        m_closeButton->setIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-cancel")}));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_verificationWidget, &MatrixDeviceVerificationWidget::verificationSucceeded,
            this, [this] {
                m_closeButton->setText(tr("Close"));
                if (m_iconsManager)
                    m_closeButton->setIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-ok")}));
            });
    connect(m_verificationWidget, &MatrixDeviceVerificationWidget::verificationFailed,
            this, [this] {
                m_closeButton->setText(tr("Close"));
                if (m_iconsManager)
                    m_closeButton->setIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-ok")}));
            });
}

void MatrixDeviceVerificationDialog::reject()
{
    m_verificationWidget->cancelVerification();
    QDialog::reject();
}
