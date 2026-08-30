/*
 * %kadu copyright begin%
 * Copyright 2009, 2011 Piotr Galiszewski (piotr.galiszewski@kadu.im)
 * Copyright 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2010 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2009, 2010, 2011, 2012, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#pragma once

#include "chat/chat.h"
#include "configuration/configuration-aware-object.h"
#include "windows/main-window.h"

#include "exports.h"

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QUrl>
#include <QtCore/QtGlobal>
#include <injeqt/injeqt.h>

class Action;
class BaseActionContext;
class BoldAction;
class ChatConfigurationHolder;
class ChatWidget;
class CustomInput;
class IconsManager;
class ItalicAction;
class StatusConfigurationHolder;
class StatusContainerManager;
class UnderlineAction;
class QFrame;
class QFileInfo;
class QLabel;
class QToolButton;
class QWidget;

class KADUAPI ChatEditBox : public MainWindow, public ConfigurationAwareObject
{
    Q_OBJECT

    QPointer<BoldAction> m_boldAction;
    QPointer<ChatConfigurationHolder> m_chatConfigurationHolder;
    QPointer<IconsManager> m_iconsManager;
    QPointer<ItalicAction> m_italicAction;
    QPointer<StatusConfigurationHolder> m_statusConfigurationHolder;
    QPointer<StatusContainerManager> m_statusContainerManager;
    QPointer<UnderlineAction> m_underlineAction;

    Chat CurrentChat;
    CustomInput *InputBox;
    QColor CurrentColor;
    QFrame *m_attachmentPreview = nullptr;
    QLabel *m_attachmentIcon = nullptr;
    QLabel *m_attachmentName = nullptr;
    QString m_attachmentPath;
    QString m_attachmentDescription;
    bool m_attachmentsEnabled = true;

    static constexpr qint64 AttachmentWarningSize = 2 * 1024 * 1024;

    BaseActionContext *Context;

    void setColorFromCurrentText(bool force);
    bool canAttachFile(const QFileInfo &fileInfo);

private slots:
    INJEQT_SET void setBoldAction(BoldAction *boldAction);
    INJEQT_SET void setChatConfigurationHolder(ChatConfigurationHolder *chatConfigurationHolder);
    INJEQT_SET void setIconsManager(IconsManager *iconsManager);
    INJEQT_SET void setItalicAction(ItalicAction *italicAction);
    INJEQT_SET void setStatusConfigurationHolder(StatusConfigurationHolder *statusConfigurationHolder);
    INJEQT_SET void setStatusContainerManager(StatusContainerManager *statusContainerManager);
    INJEQT_SET void setUnderlineAction(UnderlineAction *underlineAction);
    INJEQT_INIT void init();

    void configurationUpdated();

    void updateContext();

    void fontChanged(QFont font);
    void colorSelectorActionCreated(Action *action);
    void cursorPositionChanged();
    void setAttachment(const QUrl &fileUrl);

public:
    static void createDefaultToolbars(Configuration *configuration, QDomElement parentConfig);

    ChatEditBox(const Chat &chat, QWidget *parent = nullptr);
    virtual ~ChatEditBox();

    // TODO: remove?
    CustomInput *inputBox();
    QString attachmentPath() const;
    bool attachmentsEnabled() const;
    void clearAttachment();

    virtual bool supportsActionType(ActionDescription::ActionType type);
    virtual TalkableProxyModel *talkableProxyModel();

    ChatWidget *chatWidget();

    void openInsertImageDialog();
    void openAttachFileDialog();
    void openLocationDialog();

    void setAutoSend(bool autoSend);
    void setAttachmentsEnabled(bool enabled);

public slots:
    void changeColor(const QColor &newColor);
    void insertPlainText(const QString &plainText);

signals:
    void keyPressed(QKeyEvent *e, CustomInput *sender, bool &handled);
    void locationSelected(const QString &geoUri);
};
