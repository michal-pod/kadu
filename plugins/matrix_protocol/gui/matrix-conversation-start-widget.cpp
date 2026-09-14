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

#include "matrix-conversation-start-widget.h"
#include "matrix-conversation-start-widget.moc"

#include "../matrix-chat-service.h"
#include "icons/icons-manager.h"
#include "icons/kadu-icon.h"
#include "matrix-user-directory-search.h"

#include <Quotient/avatar.h>
#include <Quotient/connection.h>
#include <Quotient/csapi/create_room.h>
#include <Quotient/csapi/joining.h>
#include <Quotient/csapi/list_public_rooms.h>
#include <Quotient/events/roomcreateevent.h>
#include <Quotient/jobs/basejob.h>
#include <Quotient/room.h>
#include <Quotient/roomstateview.h>
#include <Quotient/uri.h>
#include <Quotient/user.h>

#include <QtCore/QJsonObject>
#include <QtCore/QSet>
#include <QtCore/QTimer>
#include <QtGui/QPixmap>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QTreeWidgetItemIterator>
#include <QtWidgets/QVBoxLayout>

#include <optional>
#include <utility>

enum MatrixConversationResultRole
{
    MatrixConversationKindRole = Qt::UserRole + 1,
    MatrixConversationIdRole,
    MatrixConversationDisplayNameRole,
    MatrixConversationSelectionKeyRole,
    MatrixConversationAvatarKeyRole
};

MatrixConversationStartWidget::MatrixConversationStartWidget(
    Quotient::Connection *connection, MatrixChatService *chatService, IconsManager *iconsManager,
    QWidget *parent)
    : ConversationStartForm{parent}, m_connection{connection}, m_chatService{chatService},
      m_iconsManager{iconsManager}
{
    createGui();

    m_userSearch = new MatrixUserDirectorySearch{m_connection, this};
    m_roomSearchTimer = new QTimer{this};
    m_roomSearchTimer->setSingleShot(true);
    m_roomSearchTimer->setInterval(300);

    connect(m_userSearch, &MatrixUserDirectorySearch::changed, this, [this] {
        if (m_operation == Operation::Idle)
        {
            rebuildResults();
            updateStatus();
        }
    });
    connect(m_roomSearchTimer, &QTimer::timeout, this, &MatrixConversationStartWidget::searchPublicRooms);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &MatrixConversationStartWidget::queryChanged);
    connect(m_results, &QTreeWidget::currentItemChanged, this,
            [this](QTreeWidgetItem *, QTreeWidgetItem *) { selectionChanged(); });
    connect(m_results, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) {
        if (primaryActionEnabled())
            performPrimaryAction();
    });

    if (m_connection)
    {
        connect(m_connection, &Quotient::Connection::connected, this, [this] {
            queryChanged();
            updateStatus();
        });
        connect(m_connection, &Quotient::Connection::loggedOut, this, [this] {
            if (m_operationJob)
                m_operationJob->abandon();
            if (m_roomSearchJob)
                m_roomSearchJob->abandon();
            m_operationJob = nullptr;
            m_roomSearchJob = nullptr;
            m_pendingRoomId.clear();
            setOperation(Operation::Idle, tr("The Matrix account disconnected."));
        });
        connect(m_connection, &Quotient::Connection::newRoom, this,
                [this](Quotient::Room *room) { tryOpenRoom(room); });
        connect(m_connection, &Quotient::Connection::joinedRoom, this,
                [this](Quotient::Room *room, Quotient::Room *) { tryOpenRoom(room); });
        connect(m_connection, &Quotient::Connection::directChatsListChanged,
                this, [this] { rebuildResults(); });
    }

    queryChanged();
    m_searchEdit->setFocus();
}

MatrixConversationStartWidget::~MatrixConversationStartWidget()
{
    if (m_operationJob)
        m_operationJob->abandon();
    if (m_roomSearchJob)
        m_roomSearchJob->abandon();
}

void MatrixConversationStartWidget::createGui()
{
    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_searchEdit = new QLineEdit{this};
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setPlaceholderText(tr("Search conversations, people and rooms…"));
    layout->addWidget(m_searchEdit);

    m_results = new QTreeWidget{this};
    m_results->setHeaderHidden(true);
    m_results->setRootIsDecorated(false);
    m_results->setUniformRowHeights(false);
    m_results->setIconSize(QSize{40, 40});
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_results, 1);

    m_statusLabel = new QLabel{this};
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_directOptions = createDirectOptions();
    layout->addWidget(m_directOptions);

    m_roomOptions = createRoomOptions();
    layout->addWidget(m_roomOptions);
}

QWidget *MatrixConversationStartWidget::createDirectOptions()
{
    auto *widget = new QWidget{this};
    auto *layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    m_directEncryptionCheckBox = new QCheckBox{tr("Enable end-to-end encryption"), widget};
    m_directEncryptionCheckBox->setChecked(m_connection && m_connection->encryptionEnabled());
    m_directEncryptionCheckBox->setEnabled(m_connection && m_connection->encryptionEnabled());
    layout->addWidget(m_directEncryptionCheckBox);
    connect(m_directEncryptionCheckBox, &QCheckBox::toggled, this, [this] { emit stateChanged(); });
    return widget;
}

QGroupBox *MatrixConversationStartWidget::createRoomOptions()
{
    auto *group = new QGroupBox{tr("Create room"), this};
    auto *layout = new QFormLayout{group};

    m_nameEdit = new QLineEdit{group};
    layout->addRow(tr("Name:"), m_nameEdit);

    m_topicEdit = new QPlainTextEdit{group};
    m_topicEdit->setPlaceholderText(tr("Optional room description"));
    m_topicEdit->setFixedHeight(m_topicEdit->fontMetrics().lineSpacing() * 3 + 12);
    layout->addRow(tr("Description:"), m_topicEdit);

    auto *aliasWidget = new QWidget{group};
    auto *aliasLayout = new QHBoxLayout{aliasWidget};
    aliasLayout->setContentsMargins(0, 0, 0, 0);
    aliasLayout->setSpacing(3);
    aliasLayout->addWidget(new QLabel{QStringLiteral("#"), aliasWidget});
    m_aliasEdit = new QLineEdit{aliasWidget};
    m_aliasEdit->setPlaceholderText(tr("optional-alias"));
    aliasLayout->addWidget(m_aliasEdit, 1);
    m_aliasServerLabel = new QLabel{aliasWidget};
    const auto host = serverName();
    m_aliasServerLabel->setText(host.isEmpty() ? QString{} : QStringLiteral(":") + host);
    aliasLayout->addWidget(m_aliasServerLabel);
    layout->addRow(tr("Room alias:"), aliasWidget);

    m_accessCombo = new QComboBox{group};
    m_accessCombo->addItem(tr("Invitation required"), QStringLiteral("private_chat"));
    m_accessCombo->addItem(tr("Anyone can join"), QStringLiteral("public_chat"));
    layout->addRow(tr("Access:"), m_accessCombo);

    m_directoryVisibilityCombo = new QComboBox{group};
    m_directoryVisibilityCombo->addItem(tr("Private — not listed"), false);
    m_directoryVisibilityCombo->addItem(tr("Public — listed in the room directory"), true);
    layout->addRow(tr("Directory visibility:"), m_directoryVisibilityCombo);

    m_historyVisibilityCombo = new QComboBox{group};
    m_historyVisibilityCombo->addItem(tr("Members since they joined"), QStringLiteral("joined"));
    m_historyVisibilityCombo->addItem(tr("Members since they were invited"), QStringLiteral("invited"));
    m_historyVisibilityCombo->addItem(tr("Members can see earlier messages"), QStringLiteral("shared"));
    m_historyVisibilityCombo->addItem(tr("Anyone can see the history"), QStringLiteral("world_readable"));
    m_historyVisibilityCombo->setCurrentIndex(2);
    layout->addRow(tr("History visibility:"), m_historyVisibilityCombo);

    m_roomEncryptionCheckBox = new QCheckBox{tr("Enable end-to-end encryption"), group};
    m_roomEncryptionCheckBox->setChecked(m_connection && m_connection->encryptionEnabled());
    m_roomEncryptionCheckBox->setEnabled(m_connection && m_connection->encryptionEnabled());
    layout->addRow(tr("Encryption:"), m_roomEncryptionCheckBox);

    auto *encryptionHint = new QLabel{
        m_roomEncryptionCheckBox->isEnabled()
            ? tr("Encryption cannot be disabled after the room is created.")
            : tr("Encryption is unavailable for this account session."),
        group};
    encryptionHint->setTextFormat(Qt::PlainText);
    encryptionHint->setWordWrap(true);
    layout->addRow(QString{}, encryptionHint);

    connect(m_nameEdit, &QLineEdit::textChanged, this, [this] { emit stateChanged(); });
    connect(m_topicEdit, &QPlainTextEdit::textChanged, this, [this] { emit stateChanged(); });
    connect(m_aliasEdit, &QLineEdit::textChanged, this, [this] {
        updateStatus();
        emit stateChanged();
    });
    connect(m_roomEncryptionCheckBox, &QCheckBox::clicked, this, [this] {
        m_roomEncryptionChoiceChanged = true;
    });
    connect(m_accessCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (!m_roomEncryptionChoiceChanged && m_roomEncryptionCheckBox->isEnabled())
            m_roomEncryptionCheckBox->setChecked(
                m_accessCombo->currentData().toString() == QStringLiteral("private_chat"));
    });
    return group;
}

QString MatrixConversationStartWidget::primaryActionText() const
{
    if (m_operation != Operation::Idle)
    {
        if (m_operation == Operation::Joining)
            return tr("Joining…");
        if (m_operation == Operation::CreatingDirectChat)
            return tr("Starting…");
        if (m_operation == Operation::CreatingRoom)
            return tr("Creating…");
        return tr("Opening…");
    }

    if (m_selectedKind == ResultKind::CreateRoom)
        return tr("Create room");
    if (m_selectedKind == ResultKind::InvitedRoom)
        return tr("Accept invitation");
    if (m_selectedKind == ResultKind::JoinedRoom)
        return tr("Open");
    if (m_selectedKind == ResultKind::PublicRoom || m_selectedKind == ResultKind::ExactRoom)
        return tr("Join");
    if (m_selectedKind == ResultKind::User)
    {
        if (const auto *direct = directRoom(m_selectedId))
            return direct->joinState() == Quotient::JoinState::Invite ? tr("Accept invitation") : tr("Open");
        return tr("Start conversation");
    }
    return tr("Open");
}

bool MatrixConversationStartWidget::primaryActionEnabled() const
{
    if (m_operation != Operation::Idle || m_selectedKind == ResultKind::None)
        return false;
    if (m_selectedKind == ResultKind::JoinedRoom)
        return m_chatService && room(m_selectedId);
    if (m_selectedKind == ResultKind::User)
    {
        const auto *existingDirectRoom = directRoom(m_selectedId);
        if (existingDirectRoom && existingDirectRoom->joinState() == Quotient::JoinState::Join)
            return m_chatService;
    }
    if (!accountOnline())
        return false;
    if (m_selectedKind == ResultKind::CreateRoom)
        return !m_nameEdit->text().trimmed().isEmpty() && validLocalAlias(m_aliasEdit->text());
    return !m_selectedId.isEmpty();
}

bool MatrixConversationStartWidget::operationInProgress() const
{
    return m_operation != Operation::Idle;
}

void MatrixConversationStartWidget::performPrimaryAction()
{
    if (!primaryActionEnabled())
        return;

    switch (m_selectedKind)
    {
    case ResultKind::JoinedRoom:
        openRoom(room(m_selectedId));
        break;
    case ResultKind::InvitedRoom:
    case ResultKind::PublicRoom:
    case ResultKind::ExactRoom:
        openOrJoinSelectedRoom();
        break;
    case ResultKind::User:
        openOrCreateDirectChat();
        break;
    case ResultKind::CreateRoom:
        createRoom();
        break;
    case ResultKind::None:
        break;
    }
}

bool MatrixConversationStartWidget::accountOnline() const
{
    return m_connection && m_chatService && m_connection->isLoggedIn() && m_connection->isOnline();
}

QString MatrixConversationStartWidget::serverName() const
{
    if (!m_connection)
        return {};
    const auto separator = m_connection->userId().indexOf(QLatin1Char(':'));
    return separator >= 0 ? m_connection->userId().mid(separator + 1) : QString{};
}

bool MatrixConversationStartWidget::validUserId(const QString &text) const
{
    return !MatrixUserDirectorySearch::completeUserId(text).isEmpty();
}

bool MatrixConversationStartWidget::validRoomId(const QString &text) const
{
    const auto value = text.trimmed();
    return value.size() > 3 && value.startsWith(QLatin1Char('!'))
           && value.indexOf(QLatin1Char(':'), 2) > 1;
}

bool MatrixConversationStartWidget::validRoomAlias(const QString &text) const
{
    const auto uri = Quotient::Uri::fromUserInput(text.trimmed());
    return uri.isValid() && uri.type() == Quotient::Uri::RoomAlias
           && uri.secondaryType() == Quotient::Uri::NoSecondaryId;
}

bool MatrixConversationStartWidget::validLocalAlias(const QString &text) const
{
    const auto alias = text.trimmed();
    if (alias.isEmpty())
        return true;
    for (const auto character : alias)
        if (character.isSpace() || character == QLatin1Char('#') || character == QLatin1Char(':')
            || character.category() == QChar::Other_Control)
            return false;
    return true;
}

bool MatrixConversationStartWidget::roomMatches(Quotient::Room *room, const QString &query) const
{
    if (!room || roomIsSpace(room))
        return false;
    if (query.isEmpty())
        return true;
    if (room->id().contains(query, Qt::CaseInsensitive)
        || room->displayName().contains(query, Qt::CaseInsensitive)
        || room->topic().contains(query, Qt::CaseInsensitive)
        || room->canonicalAlias().contains(query, Qt::CaseInsensitive))
        return true;
    for (const auto &alias : room->aliases())
        if (alias.contains(query, Qt::CaseInsensitive))
            return true;
    return false;
}

bool MatrixConversationStartWidget::roomIsSpace(Quotient::Room *room) const
{
    const auto *createEvent = room ? room->currentState().get<Quotient::RoomCreateEvent>() : nullptr;
    return createEvent && createEvent->roomType() == Quotient::RoomType::Space;
}

Quotient::Room *MatrixConversationStartWidget::room(const QString &roomId) const
{
    return m_connection ? m_connection->room(
                              roomId, Quotient::JoinState::Invite | Quotient::JoinState::Join)
                        : nullptr;
}

Quotient::Room *MatrixConversationStartWidget::directRoom(const QString &userId) const
{
    if (!m_connection || userId.isEmpty())
        return nullptr;
    const auto *user = m_connection->user(userId);
    const auto directChats = m_connection->directChats();
    const auto roomIds = directChats.values(user);
    Quotient::Room *invited = nullptr;
    for (const auto &roomId : roomIds)
    {
        auto *candidate = room(roomId);
        if (!candidate)
            continue;
        if (candidate->joinState() == Quotient::JoinState::Join)
            return candidate;
        if (candidate->joinState() == Quotient::JoinState::Invite)
            invited = candidate;
    }
    return invited;
}

void MatrixConversationStartWidget::queryChanged()
{
    if (m_operation != Operation::Idle)
        return;

    m_userSearch->setQuery(m_searchEdit->text());
    scheduleRoomSearch();
    rebuildResults();
    updateStatus();
}

void MatrixConversationStartWidget::scheduleRoomSearch()
{
    ++m_roomSearchGeneration;
    m_roomSearchTimer->stop();
    if (m_roomSearchJob)
        m_roomSearchJob->abandon();
    m_roomSearchJob = nullptr;
    m_publicRooms.clear();
    m_roomSearchError.clear();
    m_roomSearchLoading = false;
    m_roomSearchFinished = false;

    const auto query = m_searchEdit->text().trimmed();
    if (accountOnline() && query.size() >= 2 && !query.startsWith(QLatin1Char('@')))
    {
        m_roomSearchLoading = true;
        m_roomSearchTimer->start();
    }
}

void MatrixConversationStartWidget::searchPublicRooms()
{
    if (!accountOnline())
        return;

    const auto query = m_searchEdit->text().trimmed();
    const auto generation = m_roomSearchGeneration;
    Quotient::QueryPublicRoomsJob::Filter filter;
    filter.genericSearchTerm = query;
    auto job = m_connection->callApi<Quotient::QueryPublicRoomsJob>(
        QString{}, std::optional<int>{30}, QString{},
        std::optional<Quotient::QueryPublicRoomsJob::Filter>{filter});
    m_roomSearchJob = job.data();
    connect(job, &Quotient::BaseJob::success, this, [this, job, query, generation] {
        if (generation != m_roomSearchGeneration || query != m_searchEdit->text().trimmed())
            return;

        m_roomSearchJob = nullptr;
        m_publicRooms.clear();
        for (const auto &room : job->chunk())
        {
            if (room.roomId.isEmpty() || room.roomType == QStringLiteral("m.space"))
                continue;
            m_publicRooms.append({
                room.roomId, room.canonicalAlias, room.name, room.topic, room.avatarUrl,
                room.numJoinedMembers});
        }
        m_roomSearchLoading = false;
        m_roomSearchFinished = true;
        m_roomSearchError.clear();
        rebuildResults();
        updateStatus();
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, job, query, generation] {
        if (generation != m_roomSearchGeneration || query != m_searchEdit->text().trimmed())
            return;

        m_roomSearchJob = nullptr;
        m_publicRooms.clear();
        m_roomSearchLoading = false;
        m_roomSearchFinished = true;
        m_roomSearchError = job->errorString();
        rebuildResults();
        updateStatus();
    });
}

void MatrixConversationStartWidget::rebuildResults()
{
    const auto oldSelection = itemSelectionKey(m_results->currentItem());
    const auto query = m_searchEdit->text().trimmed();
    m_results->clear();

    QSet<QString> knownRoomIds;
    auto exactRoomMatch = false;
    auto *conversations = static_cast<QTreeWidgetItem *>(nullptr);
    if (m_connection)
    {
        for (auto *knownRoom : m_connection->rooms(Quotient::JoinState::Invite | Quotient::JoinState::Join))
        {
            if (!roomMatches(knownRoom, query))
                continue;
            if (!conversations)
                conversations = addSection(tr("Conversations"));
            knownRoomIds.insert(knownRoom->id());
            const auto invited = knownRoom->joinState() == Quotient::JoinState::Invite;
            const auto direct = m_connection->isDirectChat(knownRoom->id());
            auto title = knownRoom->displayName();
            if (title.isEmpty())
                title = knownRoom->canonicalAlias().isEmpty() ? knownRoom->id() : knownRoom->canonicalAlias();
            auto subtitle = direct ? tr("Direct conversation") : tr("Room");
            if (invited)
                subtitle += tr(" — invitation");
            subtitle += QStringLiteral(" · ") + knownRoom->id();
            addResult(
                conversations, invited ? ResultKind::InvitedRoom : ResultKind::JoinedRoom,
                knownRoom->id(), title, subtitle, knownRoom->avatarUrl());
            exactRoomMatch = exactRoomMatch || title.compare(query, Qt::CaseInsensitive) == 0
                             || knownRoom->id().compare(query, Qt::CaseInsensitive) == 0
                             || knownRoom->canonicalAlias().compare(query, Qt::CaseInsensitive) == 0;
        }
    }

    QSet<QString> knownUserIds;
    auto *people = static_cast<QTreeWidgetItem *>(nullptr);
    for (const auto &user : m_userSearch->results())
    {
        if (user.userId == (m_connection ? m_connection->userId() : QString{}))
            continue;
        if (!people)
            people = addSection(tr("People"));
        knownUserIds.insert(user.userId);
        addResult(
            people, ResultKind::User, user.userId,
            user.displayName.isEmpty() ? user.userId : user.displayName, user.userId, user.avatarUrl);
    }
    if (validUserId(query) && query != (m_connection ? m_connection->userId() : QString{})
        && !knownUserIds.contains(query))
    {
        if (!people)
            people = addSection(tr("People"));
        addResult(people, ResultKind::User, query, query, tr("Exact Matrix ID"));
    }

    auto *publicRooms = static_cast<QTreeWidgetItem *>(nullptr);
    for (const auto &publicRoom : std::as_const(m_publicRooms))
    {
        if (knownRoomIds.contains(publicRoom.roomId))
            continue;
        if (!publicRooms)
            publicRooms = addSection(tr("Public rooms"));
        const auto title = publicRoom.name.isEmpty()
                               ? (publicRoom.alias.isEmpty() ? publicRoom.roomId : publicRoom.alias)
                               : publicRoom.name;
        auto subtitle = publicRoom.alias.isEmpty() ? publicRoom.roomId : publicRoom.alias;
        subtitle += tr(" · %n member(s)", nullptr, publicRoom.joinedMembers);
        addResult(
            publicRooms, ResultKind::PublicRoom, publicRoom.roomId, title, subtitle,
            publicRoom.avatarUrl);
        exactRoomMatch = exactRoomMatch || title.compare(query, Qt::CaseInsensitive) == 0
                         || publicRoom.alias.compare(query, Qt::CaseInsensitive) == 0
                         || publicRoom.roomId.compare(query, Qt::CaseInsensitive) == 0;
    }

    if ((validRoomId(query) || validRoomAlias(query)) && !exactRoomMatch)
    {
        auto *actions = addSection(tr("Room address"));
        addResult(actions, ResultKind::ExactRoom, query, query, tr("Open or join this room"));
        exactRoomMatch = true;
    }

    if (!query.isEmpty() && !validUserId(query) && !validRoomId(query) && !validRoomAlias(query)
        && m_roomSearchFinished && !exactRoomMatch)
    {
        auto *actions = addSection(tr("Create"));
        addResult(
            actions, ResultKind::CreateRoom, query, tr("Create room “%1”").arg(query),
            tr("No accessible room with this exact name was found"));
    }

    m_results->expandAll();
    restoreOrSelectFirst(oldSelection);
    selectionChanged();
}

QTreeWidgetItem *MatrixConversationStartWidget::addSection(const QString &title)
{
    auto *section = new QTreeWidgetItem{m_results};
    section->setText(0, title);
    section->setFlags(Qt::ItemIsEnabled);
    auto font = section->font(0);
    font.setBold(true);
    section->setFont(0, font);
    return section;
}

QTreeWidgetItem *MatrixConversationStartWidget::addResult(
    QTreeWidgetItem *section, ResultKind kind, const QString &id, const QString &title,
    const QString &subtitle, const QUrl &avatarUrl)
{
    auto *item = new QTreeWidgetItem{section};
    item->setText(0, title + QLatin1Char('\n') + subtitle);
    item->setData(0, MatrixConversationKindRole, static_cast<int>(kind));
    item->setData(0, MatrixConversationIdRole, id);
    item->setData(0, MatrixConversationDisplayNameRole, title);
    item->setData(0, MatrixConversationSelectionKeyRole,
                  QStringLiteral("%1:%2").arg(static_cast<int>(kind)).arg(id));
    item->setSizeHint(0, QSize{0, 50});

    if (m_iconsManager)
    {
        const auto iconName = kind == ResultKind::User
                                  ? QStringLiteral("contact-new")
                              : kind == ResultKind::CreateRoom
                                  ? QStringLiteral("list-add")
                                  : QStringLiteral("internet-group-chat");
        item->setIcon(0, m_iconsManager->iconByPath(KaduIcon{iconName}));
    }
    if (!avatarUrl.isEmpty())
        applyAvatar(item, itemSelectionKey(item), avatarUrl);
    return item;
}

void MatrixConversationStartWidget::restoreOrSelectFirst(const QString &selectionKey)
{
    QTreeWidgetItem *first = nullptr;
    QTreeWidgetItemIterator it{m_results};
    while (*it)
    {
        auto *item = *it;
        if (item->data(0, MatrixConversationKindRole).isValid())
        {
            if (!first)
                first = item;
            if (!selectionKey.isEmpty() && itemSelectionKey(item) == selectionKey)
            {
                m_results->setCurrentItem(item);
                return;
            }
        }
        ++it;
    }
    m_results->setCurrentItem(first);
}

QString MatrixConversationStartWidget::itemSelectionKey(const QTreeWidgetItem *item) const
{
    return item ? item->data(0, MatrixConversationSelectionKeyRole).toString() : QString{};
}

void MatrixConversationStartWidget::selectionChanged()
{
    const auto previousKind = m_selectedKind;
    const auto previousId = m_selectedId;
    const auto *item = m_results->currentItem();
    if (!item || !item->data(0, MatrixConversationKindRole).isValid())
    {
        m_selectedKind = ResultKind::None;
        m_selectedId.clear();
        m_selectedDisplayName.clear();
    }
    else
    {
        m_selectedKind = static_cast<ResultKind>(item->data(0, MatrixConversationKindRole).toInt());
        m_selectedId = item->data(0, MatrixConversationIdRole).toString();
        m_selectedDisplayName = item->data(0, MatrixConversationDisplayNameRole).toString();
    }

    if (m_selectedKind == ResultKind::CreateRoom
        && (previousKind != ResultKind::CreateRoom || previousId != m_selectedId))
    {
        m_nameEdit->setText(m_selectedId);
        m_aliasEdit->clear();
    }
    updateOptionVisibility();
    updateStatus();
    emit stateChanged();
}

void MatrixConversationStartWidget::updateOptionVisibility()
{
    const auto newDirectChat = m_selectedKind == ResultKind::User && !directRoom(m_selectedId);
    m_directOptions->setVisible(newDirectChat);
    m_roomOptions->setVisible(m_selectedKind == ResultKind::CreateRoom);
}

void MatrixConversationStartWidget::updateStatus()
{
    if (m_operation != Operation::Idle)
        return;
    if (!accountOnline())
    {
        m_statusLabel->setText(
            m_selectedKind == ResultKind::JoinedRoom
                || (m_selectedKind == ResultKind::User && directRoom(m_selectedId)
                    && directRoom(m_selectedId)->joinState() == Quotient::JoinState::Join)
                ? tr("The account is offline. Existing conversations can still be opened.")
                : tr("Connect the selected Matrix account to start a new conversation."));
        return;
    }
    if (m_selectedKind == ResultKind::CreateRoom && !validLocalAlias(m_aliasEdit->text()))
    {
        m_statusLabel->setText(tr("The local room alias cannot contain spaces, # or :."));
        return;
    }
    if (m_roomSearchLoading || m_userSearch->loading())
    {
        m_statusLabel->setText(tr("Searching the Matrix server…"));
        return;
    }
    if (!m_roomSearchError.isEmpty() && !m_userSearch->error().isEmpty())
    {
        m_statusLabel->setText(tr("Server-side search is currently unavailable. Exact Matrix IDs and room aliases can still be used."));
        return;
    }
    if (m_results->topLevelItemCount() == 0)
    {
        m_statusLabel->setText(
            m_searchEdit->text().trimmed().isEmpty()
                ? tr("Enter a name, Matrix ID or room alias.")
                : tr("No accessible matching conversation was found."));
        return;
    }
    m_statusLabel->clear();
}

void MatrixConversationStartWidget::setOperation(Operation operation, const QString &status)
{
    if (operation != Operation::Idle)
    {
        ++m_roomSearchGeneration;
        m_roomSearchTimer->stop();
        if (m_roomSearchJob)
            m_roomSearchJob->abandon();
        m_roomSearchJob = nullptr;
        m_roomSearchLoading = false;
    }
    m_operation = operation;
    const auto idle = operation == Operation::Idle;
    m_searchEdit->setEnabled(idle);
    m_results->setEnabled(idle);
    m_directOptions->setEnabled(idle);
    m_roomOptions->setEnabled(idle);
    m_statusLabel->setText(status);
    emit stateChanged();
}

void MatrixConversationStartWidget::openRoom(Quotient::Room *room)
{
    if (!room || room->joinState() != Quotient::JoinState::Join || !m_chatService)
        return;
    const auto chat = m_chatService->roomChat(room);
    if (chat)
        emit chatReady(chat);
}

void MatrixConversationStartWidget::openOrJoinSelectedRoom()
{
    if (auto *knownRoom = room(m_selectedId);
        knownRoom && knownRoom->joinState() == Quotient::JoinState::Join)
    {
        openRoom(knownRoom);
        return;
    }

    setOperation(Operation::Joining, tr("Joining the room…"));
    auto job = m_connection->joinRoom(m_selectedId);
    m_operationJob = job.data();
    connect(job, &Quotient::BaseJob::success, this, [this, job] {
        m_operationJob = nullptr;
        waitForRoom(job->roomId());
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, job] {
        m_operationJob = nullptr;
        if (m_selectedKind == ResultKind::ExactRoom
            && job->jsonData().value(QStringLiteral("errcode")).toString() == QStringLiteral("M_NOT_FOUND")
            && validRoomAlias(m_selectedId))
        {
            offerRoomCreationAfterMissingAlias(m_selectedId);
            return;
        }
        operationFailed(job, false);
    });
}

void MatrixConversationStartWidget::openOrCreateDirectChat()
{
    if (auto *knownRoom = directRoom(m_selectedId))
    {
        if (knownRoom->joinState() == Quotient::JoinState::Join)
        {
            openRoom(knownRoom);
            return;
        }

        setOperation(Operation::Joining, tr("Accepting the direct-chat invitation…"));
        auto job = m_connection->joinRoom(knownRoom->id());
        m_operationJob = job.data();
        connect(job, &Quotient::BaseJob::success, this, [this, job] {
            m_operationJob = nullptr;
            waitForRoom(job->roomId());
        });
        connect(job, &Quotient::BaseJob::failure, this, [this, job] {
            m_operationJob = nullptr;
            operationFailed(job, false);
        });
        return;
    }
    createDirectChat(m_selectedId);
}

void MatrixConversationStartWidget::createDirectChat(const QString &userId)
{
    setOperation(Operation::CreatingDirectChat, tr("Starting the direct conversation…"));
    QVector<Quotient::CreateRoomJob::StateEvent> initialState;
    if (m_directEncryptionCheckBox->isChecked())
    {
        Quotient::CreateRoomJob::StateEvent encryption;
        encryption.type = QStringLiteral("m.room.encryption");
        encryption.content.insert(QStringLiteral("algorithm"), QStringLiteral("m.megolm.v1.aes-sha2"));
        initialState.append(encryption);
    }

    auto job = m_connection->createRoom(
        Quotient::Connection::UnpublishRoom, QString{}, QString{}, QString{}, QStringList{userId},
        QStringLiteral("trusted_private_chat"), QString{}, true, initialState);
    m_operationJob = job.data();
    connect(job, &Quotient::BaseJob::success, this, [this, job] {
        m_operationJob = nullptr;
        waitForRoom(job->roomId());
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, job] {
        m_operationJob = nullptr;
        operationFailed(job, true);
    });
}

void MatrixConversationStartWidget::createRoom()
{
    setOperation(Operation::CreatingRoom, tr("Creating the room…"));
    QVector<Quotient::CreateRoomJob::StateEvent> initialState;
    Quotient::CreateRoomJob::StateEvent history;
    history.type = QStringLiteral("m.room.history_visibility");
    history.content.insert(
        QStringLiteral("history_visibility"), m_historyVisibilityCombo->currentData().toString());
    initialState.append(history);

    if (m_roomEncryptionCheckBox->isChecked())
    {
        Quotient::CreateRoomJob::StateEvent encryption;
        encryption.type = QStringLiteral("m.room.encryption");
        encryption.content.insert(QStringLiteral("algorithm"), QStringLiteral("m.megolm.v1.aes-sha2"));
        initialState.append(encryption);
    }

    const auto visibility = m_directoryVisibilityCombo->currentData().toBool()
                                ? Quotient::Connection::PublishRoom
                                : Quotient::Connection::UnpublishRoom;
    auto job = m_connection->createRoom(
        visibility, m_aliasEdit->text().trimmed(), m_nameEdit->text().trimmed(),
        m_topicEdit->toPlainText().trimmed(), QStringList{}, m_accessCombo->currentData().toString(),
        QString{}, false, initialState);
    m_operationJob = job.data();
    connect(job, &Quotient::BaseJob::success, this, [this, job] {
        m_operationJob = nullptr;
        waitForRoom(job->roomId());
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, job] {
        m_operationJob = nullptr;
        operationFailed(job, true);
    });
}

void MatrixConversationStartWidget::waitForRoom(const QString &roomId)
{
    if (roomId.isEmpty())
    {
        setOperation(Operation::Idle, tr("The server did not return the new room ID."));
        return;
    }
    m_pendingRoomId = roomId;
    setOperation(Operation::WaitingForRoom, tr("The server accepted the operation. Opening the conversation…"));
    tryOpenRoom(m_connection ? m_connection->room(roomId, Quotient::JoinState::Join) : nullptr);
}

void MatrixConversationStartWidget::tryOpenRoom(Quotient::Room *room)
{
    if (!room || room->joinState() != Quotient::JoinState::Join || !m_chatService)
        return;
    if (m_pendingRoomId.isEmpty() || room->id() != m_pendingRoomId)
        return;
    m_pendingRoomId.clear();
    openRoom(room);
}

void MatrixConversationStartWidget::operationFailed(Quotient::BaseJob *job, bool creating)
{
    setOperation(Operation::Idle, operationError(job, creating));
}

QString MatrixConversationStartWidget::operationError(Quotient::BaseJob *job, bool creating) const
{
    if (!job)
        return creating ? tr("The conversation could not be created.") : tr("The room could not be joined.");

    const auto code = job->jsonData().value(QStringLiteral("errcode")).toString();
    const auto details = job->errorString().trimmed();
    const auto withDetails = [this, &details](const QString &summary) {
        return details.isEmpty() ? summary : tr("%1\nServer response: %2").arg(summary, details);
    };
    if (code == QStringLiteral("M_FORBIDDEN"))
        return withDetails(
            creating
                ? tr("The homeserver does not allow this conversation to be created with the selected settings.")
                : tr("You cannot join this room. It may require an invitation, restrict membership, or have banned this account."));
    if (code == QStringLiteral("M_NOT_FOUND"))
        return withDetails(tr("The room or its alias could not be found."));
    if (code == QStringLiteral("M_ROOM_IN_USE"))
        return withDetails(tr("This room alias is already in use."));
    if (code == QStringLiteral("M_UNSUPPORTED_ROOM_VERSION"))
        return withDetails(tr("The homeserver does not support the requested room version."));
    if (code == QStringLiteral("M_LIMIT_EXCEEDED"))
        return withDetails(tr("The homeserver is temporarily limiting room operations. Try again later."));
    if (code == QStringLiteral("M_BAD_ALIAS") || code == QStringLiteral("M_INVALID_PARAM"))
        return withDetails(tr("The Matrix identifier or room alias is invalid."));
    if (!details.isEmpty())
        return creating ? tr("The conversation could not be created: %1").arg(details)
                        : tr("The room could not be joined: %1").arg(details);
    return creating ? tr("The conversation could not be created.") : tr("The room could not be joined.");
}

void MatrixConversationStartWidget::offerRoomCreationAfterMissingAlias(const QString &alias)
{
    const auto separator = alias.indexOf(QLatin1Char(':'));
    const auto aliasServer = separator >= 0 ? alias.mid(separator + 1) : QString{};
    if (aliasServer.compare(serverName(), Qt::CaseInsensitive) != 0)
    {
        setOperation(Operation::Idle, tr("The remote room alias does not exist and cannot be created on this homeserver."));
        return;
    }

    const auto localAlias = alias.mid(1, separator - 1);
    m_selectedKind = ResultKind::CreateRoom;
    m_selectedId = localAlias;
    m_selectedDisplayName = localAlias;
    m_nameEdit->setText(localAlias);
    m_aliasEdit->setText(localAlias);
    setOperation(Operation::Idle, tr("This room does not exist. Choose its settings and create it."));
    updateOptionVisibility();
    emit stateChanged();
}

void MatrixConversationStartWidget::applyAvatar(
    QTreeWidgetItem *item, const QString &key, const QUrl &avatarUrl)
{
    if (!m_connection || !Quotient::Avatar::isUrlValid(avatarUrl))
        return;
    auto avatar = std::make_shared<Quotient::Avatar>(m_connection, avatarUrl);
    m_avatarLoaders.insert(key, avatar);
    item->setData(0, MatrixConversationAvatarKeyRole, key);
    const QPointer<MatrixConversationStartWidget> widget{this};
    const auto image = avatar->get(40, [widget, key] {
        if (widget)
            widget->refreshAvatar(key);
    });
    if (!image.isNull())
        item->setIcon(0, QPixmap::fromImage(image));
}

void MatrixConversationStartWidget::refreshAvatar(const QString &key)
{
    const auto avatar = m_avatarLoaders.value(key);
    if (!avatar)
        return;
    const auto image = avatar->get(40, {});
    if (image.isNull())
        return;

    QTreeWidgetItemIterator it{m_results};
    while (*it)
    {
        if ((*it)->data(0, MatrixConversationAvatarKeyRole).toString() == key)
            (*it)->setIcon(0, QPixmap::fromImage(image));
        ++it;
    }
}
