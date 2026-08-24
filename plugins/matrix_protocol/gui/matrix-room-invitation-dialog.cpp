/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-room-invitation-dialog.h"
#include "matrix-room-invitation-dialog.moc"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

MatrixRoomInvitationDialog::MatrixRoomInvitationDialog(const QString &roomName, QWidget *parent)
        : QDialog{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Matrix Room Invitation"));

    auto *layout = new QVBoxLayout{this};
    auto *label = new QLabel{tr("You have been invited to the Matrix room <b>%1</b>. Do you want to join?")
                                  .arg(roomName.toHtmlEscaped()),
                              this};
    label->setWordWrap(true);
    layout->addWidget(label);

    auto *buttons = new QDialogButtonBox{Qt::Horizontal, this};
    auto *joinButton = buttons->addButton(tr("Join"), QDialogButtonBox::AcceptRole);
    auto *rejectButton = buttons->addButton(tr("Reject invitation"), QDialogButtonBox::DestructiveRole);
    buttons->addButton(tr("Later"), QDialogButtonBox::RejectRole);
    layout->addWidget(buttons);

    connect(joinButton, &QPushButton::clicked, this, &MatrixRoomInvitationDialog::joinRequested);
    connect(rejectButton, &QPushButton::clicked, this, &MatrixRoomInvitationDialog::rejectRequested);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}
