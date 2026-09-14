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

#pragma once

#include "windows/conversation-start-form.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <QtCore/QVector>

#include <memory>

class IconsManager;
class MatrixChatService;
class MatrixUserDirectorySearch;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;

namespace Quotient
{
class Avatar;
class BaseJob;
class Connection;
class Room;
}

class MatrixConversationStartWidget final : public ConversationStartForm
{
    Q_OBJECT

public:
    MatrixConversationStartWidget(
        Quotient::Connection *connection, MatrixChatService *chatService, IconsManager *iconsManager,
        QWidget *parent = nullptr);
    ~MatrixConversationStartWidget() override;

    QString primaryActionText() const override;
    bool primaryActionEnabled() const override;
    bool operationInProgress() const override;
    void performPrimaryAction() override;

private:
    enum class ResultKind
    {
        None,
        JoinedRoom,
        InvitedRoom,
        User,
        PublicRoom,
        ExactRoom,
        CreateRoom
    };

    enum class Operation
    {
        Idle,
        Joining,
        CreatingRoom,
        CreatingDirectChat,
        WaitingForRoom
    };

    struct PublicRoomResult
    {
        QString roomId;
        QString alias;
        QString name;
        QString topic;
        QUrl avatarUrl;
        int joinedMembers = 0;
    };

    QPointer<Quotient::Connection> m_connection;
    QPointer<MatrixChatService> m_chatService;
    QPointer<IconsManager> m_iconsManager;
    QPointer<Quotient::BaseJob> m_operationJob;
    QPointer<Quotient::BaseJob> m_roomSearchJob;
    MatrixUserDirectorySearch *m_userSearch = nullptr;
    QTimer *m_roomSearchTimer = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QTreeWidget *m_results = nullptr;
    QLabel *m_statusLabel = nullptr;
    QWidget *m_directOptions = nullptr;
    QCheckBox *m_directEncryptionCheckBox = nullptr;
    QGroupBox *m_roomOptions = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QPlainTextEdit *m_topicEdit = nullptr;
    QLineEdit *m_aliasEdit = nullptr;
    QLabel *m_aliasServerLabel = nullptr;
    QComboBox *m_accessCombo = nullptr;
    QComboBox *m_directoryVisibilityCombo = nullptr;
    QComboBox *m_historyVisibilityCombo = nullptr;
    QCheckBox *m_roomEncryptionCheckBox = nullptr;
    QVector<PublicRoomResult> m_publicRooms;
    QHash<QString, std::shared_ptr<Quotient::Avatar>> m_avatarLoaders;
    QString m_roomSearchError;
    QString m_pendingRoomId;
    QString m_selectedId;
    QString m_selectedDisplayName;
    ResultKind m_selectedKind = ResultKind::None;
    Operation m_operation = Operation::Idle;
    int m_roomSearchGeneration = 0;
    bool m_roomSearchLoading = false;
    bool m_roomSearchFinished = false;
    bool m_roomEncryptionChoiceChanged = false;

    void createGui();
    QWidget *createDirectOptions();
    QGroupBox *createRoomOptions();
    bool accountOnline() const;
    QString serverName() const;
    bool validUserId(const QString &text) const;
    bool validRoomId(const QString &text) const;
    bool validRoomAlias(const QString &text) const;
    bool validLocalAlias(const QString &text) const;
    bool roomMatches(Quotient::Room *room, const QString &query) const;
    bool roomIsSpace(Quotient::Room *room) const;
    Quotient::Room *room(const QString &roomId) const;
    Quotient::Room *directRoom(const QString &userId) const;
    void queryChanged();
    void scheduleRoomSearch();
    void searchPublicRooms();
    void rebuildResults();
    QTreeWidgetItem *addSection(const QString &title);
    QTreeWidgetItem *addResult(
        QTreeWidgetItem *section, ResultKind kind, const QString &id, const QString &title,
        const QString &subtitle, const QUrl &avatarUrl = {});
    void restoreOrSelectFirst(const QString &selectionKey);
    QString itemSelectionKey(const QTreeWidgetItem *item) const;
    void selectionChanged();
    void updateOptionVisibility();
    void updateStatus();
    void setOperation(Operation operation, const QString &status);
    void openRoom(Quotient::Room *room);
    void openOrJoinSelectedRoom();
    void openOrCreateDirectChat();
    void createDirectChat(const QString &userId);
    void createRoom();
    void waitForRoom(const QString &roomId);
    void tryOpenRoom(Quotient::Room *room);
    void operationFailed(Quotient::BaseJob *job, bool creating);
    QString operationError(Quotient::BaseJob *job, bool creating) const;
    void offerRoomCreationAfterMissingAlias(const QString &alias);
    void applyAvatar(QTreeWidgetItem *item, const QString &key, const QUrl &avatarUrl);
    void refreshAvatar(const QString &key);
};
