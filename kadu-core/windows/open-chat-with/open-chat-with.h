/*
 * %kadu copyright begin%
 * Copyright 2016 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#pragma once

#include "chat/chat.h"
#include "exports.h"

#include <QtCore/QPointer>
#include <QtWidgets/QDialog>
#include <injeqt/injeqt.h>

class AccountsComboBox;
class ChatWidgetManager;
class Configuration;
class ConversationStartForm;
class IconsManager;
class InjectedFactory;
class QLabel;
class QPushButton;
class QVBoxLayout;

class KADUAPI OpenChatWith : public QDialog
{
    Q_OBJECT

public:
    explicit OpenChatWith(QWidget *parent = nullptr);
    ~OpenChatWith() override;

public slots:
    void show();

private:
    QPointer<ChatWidgetManager> m_chatWidgetManager;
    QPointer<Configuration> m_configuration;
    QPointer<IconsManager> m_iconsManager;
    QPointer<InjectedFactory> m_injectedFactory;
    AccountsComboBox *m_accountCombo = nullptr;
    QWidget *m_formHost = nullptr;
    QVBoxLayout *m_formLayout = nullptr;
    ConversationStartForm *m_form = nullptr;
    QLabel *m_placeholder = nullptr;
    QPushButton *m_primaryButton = nullptr;

    void createGui();
    void refreshState();
    void openChat(const Chat &chat);

private slots:
    void rebuildForm();
    INJEQT_SET void setChatWidgetManager(ChatWidgetManager *chatWidgetManager);
    INJEQT_SET void setConfiguration(Configuration *configuration);
    INJEQT_SET void setIconsManager(IconsManager *iconsManager);
    INJEQT_SET void setInjectedFactory(InjectedFactory *injectedFactory);
    INJEQT_INIT void init();
};
