/*
 * %kadu copyright begin%
 * Copyright 2009, 2010, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2012 Wojciech Treter (juzefwt@gmail.com)
 * Copyright 2010 Tomasz Rostański (rozteck@interia.pl)
 * Copyright 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2009 Michał Podsiadlik (michal@kadu.net)
 * Copyright 2009 Bartłomiej Zimoń (uzi18@o2.pl)
 * Copyright 2010, 2011, 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2012, 2013, 2014, 2015 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include <QtCore/QFileInfo>
#include <QtCore/QLocale>
#include <QtCore/QUrl>
#include <QtGui/QPixmap>
#include <QtGui/QResizeEvent>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFileIconProvider>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <QtXml/QDomElement>

#include "actions/action.h"
#include "actions/base-action-context.h"
#include "actions/chat-widget/bold-action.h"
#include "actions/chat-widget/italic-action.h"
#include "actions/chat-widget/underline-action.h"
#include "buddies/buddy-set.h"
#include "chat/chat.h"
#include "configuration/configuration-api.h"
#include "configuration/configuration.h"
#include "configuration/deprecated-configuration-api.h"
#include "contacts/contact-set.h"
#include "contacts/contact.h"
#include "core/injected-factory.h"
#include "gui/configuration/chat-configuration-holder.h"
#include "icons/icons-manager.h"
#include "identities/identity.h"
#include "misc/change-notifier-lock.h"
#include "misc/error.h"
#include "protocols/protocol.h"
#include "protocols/services/chat-image-service.h"
#include "status/status-configuration-holder.h"
#include "status/status-container-manager.h"
#include "widgets/chat-edit-box-size-manager.h"
#include "widgets/chat-widget/chat-widget.h"
#include "widgets/custom-input.h"
#include "widgets/location-selector-dialog.h"
#include "widgets/talkable-tree-view.h"
#include "widgets/toolbar.h"
#include "windows/message-dialog.h"

#include "chat-edit-box.h"
#include "chat-edit-box.moc"

QList<ChatEditBox *> chatEditBoxes;

ChatEditBox::ChatEditBox(const Chat &chat, QWidget *parent)
        : MainWindow(new BaseActionContext(this), "chat", parent), CurrentChat(chat)
{
}

ChatEditBox::~ChatEditBox()
{
    // 	disconnect(m_chatWidgetActions->colorSelector(), 0, this, 0);
    disconnect(InputBox, 0, this, 0);

    chatEditBoxes.removeAll(this);
}

void ChatEditBox::setBoldAction(BoldAction *boldAction)
{
    m_boldAction = boldAction;
}

void ChatEditBox::setChatConfigurationHolder(ChatConfigurationHolder *chatConfigurationHolder)
{
    m_chatConfigurationHolder = chatConfigurationHolder;
}

void ChatEditBox::setIconsManager(IconsManager *iconsManager)
{
    m_iconsManager = iconsManager;
}

void ChatEditBox::setItalicAction(ItalicAction *italicAction)
{
    m_italicAction = italicAction;
}

void ChatEditBox::setStatusConfigurationHolder(StatusConfigurationHolder *statusConfigurationHolder)
{
    m_statusConfigurationHolder = statusConfigurationHolder;
}

void ChatEditBox::setStatusContainerManager(StatusContainerManager *statusContainerManager)
{
    m_statusContainerManager = statusContainerManager;
}

void ChatEditBox::setUnderlineAction(UnderlineAction *underlineAction)
{
    m_underlineAction = underlineAction;
}

void ChatEditBox::init()
{
    chatEditBoxes.append(this);

    Context = static_cast<BaseActionContext *>(actionContext());

    ChangeNotifierLock lock(Context->changeNotifier());

    RoleSet roles;
    if (CurrentChat.contacts().size() > 1)
        roles.insert(ChatRole);
    else
        roles.insert(BuddyRole);
    Context->setRoles(roles);

    Context->setChat(CurrentChat);
    Context->setContacts(CurrentChat.contacts());
    Context->setBuddies(CurrentChat.contacts().toBuddySet());
    updateContext();

    connect(m_statusConfigurationHolder, SIGNAL(setStatusModeChanged()), this, SLOT(updateContext()));

    InputBox = injectedFactory()->makeInjected<CustomInput>(CurrentChat, this);
    InputBox->setWordWrapMode(QTextOption::WordWrap);

    auto *editorContainer = new QWidget(this);
    auto *editorLayout = new QHBoxLayout(editorContainer);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(6);
    editorLayout->addWidget(InputBox, 1);

    m_attachmentPreview = new QFrame(editorContainer);
    auto *attachmentLayout = new QVBoxLayout(m_attachmentPreview);
    attachmentLayout->setContentsMargins(6, 6, 6, 6);
    attachmentLayout->setSpacing(4);

    auto *attachmentHeader = new QHBoxLayout;
    m_attachmentIcon = new QLabel(m_attachmentPreview);
    m_attachmentIcon->setFixedSize(64, 64);
    m_attachmentIcon->setAlignment(Qt::AlignCenter);
    attachmentHeader->addWidget(m_attachmentIcon);

    auto *removeButton = new QToolButton(m_attachmentPreview);
    removeButton->setText(QStringLiteral("×"));
    removeButton->setToolTip(tr("Remove attachment"));
    attachmentHeader->addWidget(removeButton, 0, Qt::AlignTop);
    attachmentLayout->addLayout(attachmentHeader);

    m_attachmentName = new QLabel(m_attachmentPreview);
    m_attachmentName->setWordWrap(true);
    m_attachmentName->setMaximumWidth(140);
    attachmentLayout->addWidget(m_attachmentName);
    m_attachmentPreview->setFixedWidth(152);
    m_attachmentPreview->hide();
    editorLayout->addWidget(m_attachmentPreview);

    setCentralWidget(editorContainer);

    bool old_top = loadOldToolBarsFromConfig("chatTopDockArea", Qt::TopToolBarArea);
    bool old_middle = loadOldToolBarsFromConfig("chatMiddleDockArea", Qt::TopToolBarArea);
    bool old_bottom = loadOldToolBarsFromConfig("chatBottomDockArea", Qt::BottomToolBarArea);
    bool old_left = loadOldToolBarsFromConfig("chatLeftDockArea", Qt::LeftToolBarArea);
    bool old_right = loadOldToolBarsFromConfig("chatRightDockArea", Qt::RightToolBarArea);

    if (old_top || old_middle || old_bottom || old_left || old_right)
        writeToolBarsToConfig();   // port old config

    // Toolbars are persisted independently of their defaults. Migrate existing chat toolbars so the attachment
    // action is discoverable without resetting the user's other buttons or layout.
    auto toolbarConfig = findExistingToolbarOnArea(configuration(), QStringLiteral("chat_topDockArea"));
    if (toolbarConfig.isNull())
        toolbarConfig = findExistingToolbar(configuration(), QStringLiteral("chat"));
    addToolButton(configuration(), toolbarConfig, QStringLiteral("attachFileAction"));

    // addToolButton() appends missing entries. The send action is intentionally right-aligned after the spacer,
    // so move the newly introduced attachment button before that spacer and keep it with editor actions.
    auto attachmentButton = configuration()->api()->findElementByProperty(
        toolbarConfig, QStringLiteral("ToolButton"), QStringLiteral("action_name"), QStringLiteral("attachFileAction"));
    auto spacerButton = configuration()->api()->findElementByProperty(
        toolbarConfig, QStringLiteral("ToolButton"), QStringLiteral("action_name"), QStringLiteral("__spacer1"));
    if (!attachmentButton.isNull() && !spacerButton.isNull() && attachmentButton.nextSibling() != spacerButton)
        toolbarConfig.insertBefore(attachmentButton, spacerButton);
    loadToolBarsFromConfig();

    // 	connect(m_chatWidgetActions->colorSelector(), SIGNAL(actionCreated(Action *)),
    // 			this, SLOT(colorSelectorActionCreated(Action *)));
    connect(
        InputBox, SIGNAL(keyPressed(QKeyEvent *, CustomInput *, bool &)), this,
        SIGNAL(keyPressed(QKeyEvent *, CustomInput *, bool &)));
    connect(InputBox, SIGNAL(fontChanged(QFont)), this, SLOT(fontChanged(QFont)));
    connect(InputBox, SIGNAL(cursorPositionChanged()), this, SLOT(cursorPositionChanged()));
    connect(InputBox, SIGNAL(attachmentSelected(QUrl)), this, SLOT(setAttachment(QUrl)));
    connect(removeButton, &QToolButton::clicked, this, &ChatEditBox::clearAttachment);

    connect(m_chatConfigurationHolder, SIGNAL(chatConfigurationUpdated()), this, SLOT(configurationUpdated()));

    configurationUpdated();
}

void ChatEditBox::fontChanged(QFont font)
{
    if (m_boldAction->action(actionContext()))
        m_boldAction->action(actionContext())->setChecked(font.bold());
    if (m_italicAction->action(actionContext()))
        m_italicAction->action(actionContext())->setChecked(font.italic());
    if (m_underlineAction->action(actionContext()))
        m_underlineAction->action(actionContext())->setChecked(font.underline());
}

void ChatEditBox::colorSelectorActionCreated(Action *action)
{
    if (action->parent() == this)
        setColorFromCurrentText(true);
}

void ChatEditBox::cursorPositionChanged()
{
    setColorFromCurrentText(false);
}

void ChatEditBox::configurationUpdated()
{
    setColorFromCurrentText(true);

    InputBox->setAutoSend(m_chatConfigurationHolder->autoSend());
}

void ChatEditBox::setAutoSend(bool autoSend)
{
    InputBox->setAutoSend(autoSend);
}

void ChatEditBox::setAttachmentsEnabled(bool enabled)
{
    if (m_attachmentsEnabled == enabled)
        return;

    m_attachmentsEnabled = enabled;
    InputBox->setAttachmentsEnabled(enabled);
    Context->changeNotifier().notify();
}

CustomInput *ChatEditBox::inputBox()
{
    return InputBox;
}

QString ChatEditBox::attachmentPath() const
{
    return m_attachmentPath;
}

bool ChatEditBox::attachmentsEnabled() const
{
    return m_attachmentsEnabled;
}

void ChatEditBox::clearAttachment()
{
    if (InputBox && InputBox->toPlainText() == m_attachmentDescription)
        InputBox->clear();

    m_attachmentPath.clear();
    m_attachmentDescription.clear();
    if (m_attachmentPreview)
        m_attachmentPreview->hide();
}

void ChatEditBox::setAttachment(const QUrl &fileUrl)
{
    const QFileInfo fileInfo{fileUrl.toLocalFile()};
    if (!fileInfo.isFile() || !fileInfo.isReadable() || !canAttachFile(fileInfo))
        return;

    m_attachmentPath = fileInfo.absoluteFilePath();
    m_attachmentDescription.clear();
    InputBox->setPlainText(m_attachmentDescription);

    const QPixmap preview{m_attachmentPath};
    if (!preview.isNull())
        m_attachmentIcon->setPixmap(preview.scaled(m_attachmentIcon->size(), Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
    else
        m_attachmentIcon->setPixmap(QFileIconProvider{}.icon(fileInfo).pixmap(m_attachmentIcon->size()));

    m_attachmentName->setText(fileInfo.fileName());
    m_attachmentName->setToolTip(m_attachmentPath);
    m_attachmentPreview->show();
}

bool ChatEditBox::canAttachFile(const QFileInfo &fileInfo)
{
    if (!m_attachmentsEnabled)
        return false;

    const auto *protocol = CurrentChat.chatAccount().protocolHandler();
    if (!protocol || !protocol->isAttachmentsSupported())
        return false;

    const auto maximumSize = protocol->maximumAttachmentSize();
    if (maximumSize > 0 && fileInfo.size() > maximumSize)
    {
        MessageDialog::show(
            m_iconsManager->iconByPath(KaduIcon("dialog-error")), tr("Kadu"),
            tr("The selected file is larger than the protocol attachment limit of %1.")
                .arg(QLocale{}.formattedDataSize(maximumSize)),
            QMessageBox::Ok, this);
        return false;
    }

    if (fileInfo.size() <= AttachmentWarningSize)
        return true;

    auto *dialog = MessageDialog::create(
        m_iconsManager->iconByPath(KaduIcon("dialog-warning")), tr("Kadu"),
        tr("The selected file is %1. Do you want to attach it?").arg(QLocale{}.formattedDataSize(fileInfo.size())), this);
    dialog->addButton(QMessageBox::Yes, tr("Attach file"));
    dialog->addButton(QMessageBox::No, tr("Cancel"));
    return dialog->ask();
}

bool ChatEditBox::supportsActionType(ActionDescription::ActionType type)
{
    return (
        type == ActionDescription::TypeGlobal || type == ActionDescription::TypeChat ||
        type == ActionDescription::TypeUser);
}

TalkableProxyModel *ChatEditBox::talkableProxyModel()
{
    ChatWidget *cw = chatWidget();
    if (cw && cw->chat().contacts().count() > 1)
        return cw->talkableProxyModel();

    return 0;
}

void ChatEditBox::updateContext()
{
    if (m_statusConfigurationHolder->isSetStatusPerIdentity())
        Context->setStatusContainer(CurrentChat.chatAccount().accountIdentity().statusContainer());
    else if (m_statusConfigurationHolder->isSetStatusPerAccount())
        Context->setStatusContainer(CurrentChat.chatAccount().statusContainer());
    else
        Context->setStatusContainer(m_statusContainerManager);
}

ChatWidget *ChatEditBox::chatWidget()
{
    ChatWidget *result = qobject_cast<ChatWidget *>(parentWidget());
    if (result)
        return result;

    result = qobject_cast<ChatWidget *>(parent()->parent());
    if (result)
        return result;

    return 0;
}

void ChatEditBox::createDefaultToolbars(Configuration *configuration, QDomElement toolbarsConfig)
{
    QDomElement dockAreaConfig = getDockAreaConfigElement(configuration, toolbarsConfig, "chat_topDockArea");
    QDomElement toolbarConfig = configuration->api()->createElement(dockAreaConfig, "ToolBar");

    addToolButton(configuration, toolbarConfig, "autoSendAction");
    addToolButton(configuration, toolbarConfig, "clearChatAction");
    addToolButton(configuration, toolbarConfig, "insertEmoticonAction", Qt::ToolButtonTextBesideIcon);
    addToolButton(configuration, toolbarConfig, "insertImageAction");
    addToolButton(configuration, toolbarConfig, "attachFileAction");
    addToolButton(configuration, toolbarConfig, "showHistoryAction");
    addToolButton(configuration, toolbarConfig, "remoteHistorySearchAction");
    addToolButton(configuration, toolbarConfig, "encryptionAction");
    addToolButton(configuration, toolbarConfig, "editUserAction");
    addToolButton(configuration, toolbarConfig, "__spacer1", Qt::ToolButtonTextBesideIcon);
    addToolButton(configuration, toolbarConfig, "sendAction", Qt::ToolButtonTextBesideIcon);
}

void ChatEditBox::openInsertImageDialog()
{
    ChatImageService *chatImageService = CurrentChat.chatAccount().protocolHandler()->chatImageService();
    if (!chatImageService)
        return;

    // QTBUG-849
    QString selectedFile = QFileDialog::getOpenFileName(
        this, tr("Insert image"), configuration()->deprecatedApi()->readEntry("Chat", "LastImagePath"),
        tr("Images (*.png *.PNG *.jpg *.JPG *.jpeg *.JPEG *.gif *.GIF *.bmp *.BMP);;All Files (*)"));
    if (!selectedFile.isEmpty())
    {
        QFileInfo f(selectedFile);

        configuration()->deprecatedApi()->writeEntry("Chat", "LastImagePath", f.absolutePath());

        if (!f.isReadable())
        {
            MessageDialog::show(
                m_iconsManager->iconByPath(KaduIcon("dialog-warning")), tr("Kadu"), tr("This file is not readable"),
                QMessageBox::Ok, this);
            return;
        }

        Error imageSizeError = chatImageService->checkImageSize(f.size());
        if (!imageSizeError.message().isEmpty())
        {
            MessageDialog *dialog = MessageDialog::create(
                m_iconsManager->iconByPath(KaduIcon("dialog-warning")), tr("Kadu"), imageSizeError.message(), this);
            dialog->addButton(QMessageBox::Yes, tr("Send anyway"));
            dialog->addButton(QMessageBox::No, tr("Cancel"));

            switch (imageSizeError.severity())
            {
            case NoError:
                break;
            case ErrorLow:
                if (dialog->ask())
                    return;
                break;
            case ErrorHigh:
                MessageDialog::show(
                    m_iconsManager->iconByPath(KaduIcon("dialog-error")), tr("Kadu"), imageSizeError.message(),
                    QMessageBox::Ok, this);
                return;
            default:
                break;
            }
        }

        int tooBigCounter = 0;
        int disconnectedCounter = 0;

        for (auto const &contact : CurrentChat.contacts())
        {
            if (contact.currentStatus().isDisconnected())
                disconnectedCounter++;
            else if (contact.maximumImageSize() == 0 || contact.maximumImageSize() * 1024 < f.size())
                tooBigCounter++;
        }

        QString message;
        if (1 == CurrentChat.contacts().count())
        {
            Contact contact = *CurrentChat.contacts().constBegin();
            if (tooBigCounter > 0)
                message = tr("This image has %1 KiB and may be too big for %2.")
                              .arg((f.size() + 1023) / 1024)
                              .arg(contact.display(true)) +
                          '\n';
            else if (disconnectedCounter > 0)
                message = tr("%1 appears to be offline and may not receive images.").arg(contact.display(true)) + '\n';
        }
        else
        {
            if (tooBigCounter > 0)
                message = tr("This image has %1 KiB and may be too big for %2 of %3 contacts in this conference.")
                              .arg((f.size() + 1023) / 1024)
                              .arg(tooBigCounter)
                              .arg(CurrentChat.contacts().count()) +
                          '\n';
            if (disconnectedCounter > 0)
                message += tr("%1 of %2 contacts appear to be offline and may not receive images.")
                               .arg(disconnectedCounter)
                               .arg(CurrentChat.contacts().count()) +
                           '\n';
        }
        if (tooBigCounter > 0 || disconnectedCounter > 0)
            message += tr("Do you really want to send this image?");

        MessageDialog *dialog =
            MessageDialog::create(m_iconsManager->iconByPath(KaduIcon("dialog-question")), tr("Kadu"), message, this);
        dialog->addButton(QMessageBox::Yes, tr("Send anyway"));
        dialog->addButton(QMessageBox::No, tr("Cancel"));

        if (!message.isEmpty() && !dialog->ask())
            return;

        InputBox->insertHtml(QString("<img src='%1' />").arg(selectedFile));
    }
}

void ChatEditBox::openAttachFileDialog()
{
    if (!m_attachmentsEnabled)
        return;

    const auto *protocol = CurrentChat.chatAccount().protocolHandler();
    if (!protocol || !protocol->isAttachmentsSupported())
        return;

    const auto selectedFile = QFileDialog::getOpenFileName(
        this, tr("Attach file"), configuration()->deprecatedApi()->readEntry("Chat", "LastAttachmentPath"),
        tr("All files (*)"));
    if (selectedFile.isEmpty())
        return;

    const QFileInfo fileInfo{selectedFile};
    if (!fileInfo.isReadable())
    {
        MessageDialog::show(
            m_iconsManager->iconByPath(KaduIcon("dialog-warning")), tr("Kadu"), tr("This file is not readable"),
            QMessageBox::Ok, this);
        return;
    }

    configuration()->deprecatedApi()->writeEntry("Chat", "LastAttachmentPath", fileInfo.absolutePath());
    setAttachment(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
}

void ChatEditBox::openLocationDialog()
{
    const auto *protocol = CurrentChat.chatAccount().protocolHandler();
    if (!protocol || !protocol->isLocationSendingSupported())
        return;

    auto *dialog = new LocationSelectorDialog{this};
    connect(dialog, &LocationSelectorDialog::locationSelected, this, &ChatEditBox::locationSelected);
    dialog->open();
}

void ChatEditBox::changeColor(const QColor &newColor)
{
    CurrentColor = newColor;

    QPixmap p(12, 12);
    p.fill(CurrentColor);

    // 	Action *action = m_chatWidgetActions->colorSelector()->action(this);
    // 	if (action)
    // 		action->setIcon(p);

    InputBox->setTextColor(CurrentColor);
}

void ChatEditBox::setColorFromCurrentText(bool force)
{
    Q_UNUSED(force);

    /*
            Action *action = m_chatWidgetActions->colorSelector()->action(this);
            if (!action || (!force && (InputBox->textColor() == CurrentColor)))
                    return;

            int i;
            for (i = 0; i < 16; ++i)
                    if (InputBox->textColor() == QColor(colors[i]))
                            break;

            QPixmap p(12, 12);
            if (i >= 16)
                    CurrentColor = InputBox->palette().foreground().color();
            else
                    CurrentColor = colors[i];

            p.fill(CurrentColor);

            action->QAction::setIcon(p);
    */
}

void ChatEditBox::insertPlainText(const QString &plainText)
{
    InputBox->insertPlainText(plainText);
}
