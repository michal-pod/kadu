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

#include "matrix-room-settings-window.h"
#include "matrix-room-settings-window.moc"

#include "chat/chat-details-room.h"
#include "icons/icons-manager.h"
#include "icons/kadu-icon.h"
#include "matrix-power-level-editor.h"
#include "protocols/services/chat-service.h"
#include "widgets/chat-personal-settings-widget.h"

#include <Quotient/avatar.h>
#include <Quotient/connection.h>
#include <Quotient/csapi/list_public_rooms.h>
#include <Quotient/csapi/rooms.h>
#include <Quotient/csapi/room_state.h>
#include <Quotient/jobs/basejob.h>
#include <Quotient/room.h>
#include <Quotient/roommember.h>

#include <QtCore/QMimeDatabase>
#include <QtCore/QSet>
#include <QtCore/QSignalBlocker>
#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QCloseEvent>
#include <QtGui/QPalette>
#include <QtGui/QPixmap>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStyle>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <algorithm>

MatrixRoomSettingsWindow::MatrixRoomSettingsWindow(
    const Chat &chat, ChatService *chatService, Quotient::Connection *connection, Quotient::Room *room,
    IconsManager *iconsManager, QWidget *parent)
        : QWidget{parent, Qt::Window}, m_chat{chat}, m_chatService{chatService}, m_connection{connection},
          m_room{room}, m_iconsManager{iconsManager}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Room Settings - %1").arg(room ? room->displayName() : chat.display()));
    resize(720, 620);

    createGui();
    connectRoom();
    loadRoomData();
    loadDirectoryVisibility();
}

void MatrixRoomSettingsWindow::createGui()
{
    auto mainLayout = new QVBoxLayout{this};
    m_tabs = new QTabWidget{this};
    m_tabs->addTab(createGeneralTab(), tr("General"));
    m_personalSettings = new ChatPersonalSettingsWidget{m_tabs};
    m_tabs->addTab(m_personalSettings, m_personalSettings->tabTitle());
    m_tabs->addTab(createAccessTab(), tr("Access and privacy"));
    m_communicationTabIndex = m_tabs->addTab(createCommunicationTab(), tr("Communication"));
    m_moderationTabIndex = m_tabs->addTab(createModerationTab(), tr("Moderation"));
    m_administrationTabIndex = m_tabs->addTab(createAdministrationTab(), tr("Administration"));
    m_usersTabIndex = m_tabs->addTab(createUsersTab(), tr("Users"));
    m_tabs->addTab(createAdvancedTab(), tr("Advanced"));
    mainLayout->addWidget(m_tabs);

    auto buttons = new QDialogButtonBox{this};
    m_okButton = buttons->addButton(QDialogButtonBox::Ok);
    m_applyButton = buttons->addButton(QDialogButtonBox::Apply);
    m_cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
    m_okButton->setIcon(qApp->style()->standardIcon(QStyle::SP_DialogOkButton));
    m_applyButton->setIcon(qApp->style()->standardIcon(QStyle::SP_DialogApplyButton));
    m_cancelButton->setIcon(qApp->style()->standardIcon(QStyle::SP_DialogCancelButton));
    mainLayout->addWidget(buttons);

    connect(m_nameEdit, &QLineEdit::textChanged, this, &MatrixRoomSettingsWindow::refreshState);
    connect(m_topicEdit, &QPlainTextEdit::textChanged, this, &MatrixRoomSettingsWindow::refreshState);
    connect(m_personalSettings, &ChatPersonalSettingsWidget::changed, this,
            &MatrixRoomSettingsWindow::refreshState);
    connect(m_changeAvatarButton, &QPushButton::clicked, this, &MatrixRoomSettingsWindow::chooseAvatar);
    connect(m_removeAvatarButton, &QPushButton::clicked, this, &MatrixRoomSettingsWindow::removeAvatar);
    connect(m_joinRuleCombo, &QComboBox::currentIndexChanged, this, &MatrixRoomSettingsWindow::refreshState);
    connect(
        m_historyVisibilityCombo, &QComboBox::currentIndexChanged, this,
        &MatrixRoomSettingsWindow::refreshState);
    connect(m_guestAccessCombo, &QComboBox::currentIndexChanged, this, &MatrixRoomSettingsWindow::refreshState);
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == m_usersTabIndex)
            ensureMemberSearchModel();
    });
    connect(m_okButton, &QPushButton::clicked, this, [this] { save(true); });
    connect(m_applyButton, &QPushButton::clicked, this, [this] { save(false); });
    connect(m_cancelButton, &QPushButton::clicked, this, &QWidget::close);

    if (m_chatService)
    {
        connect(m_chatService, &ChatService::chatNotificationModeChanged, this,
                [this](const Chat &chat, ChatNotificationMode mode) {
                    if (chat != m_chat)
                        return;

                    if (m_notificationModeUpdatePending)
                    {
                        if (mode != m_personalSettings->notificationMode())
                            return;
                        m_notificationModeUpdatePending = false;
                        m_savedNotificationMode = mode;
                        m_personalSettings->setNotificationMode(mode);
                        finishOperation();
                        return;
                    }

                    if (!m_saving && m_personalSettings->notificationMode() == m_savedNotificationMode)
                    {
                        m_savedNotificationMode = mode;
                        m_personalSettings->setNotificationMode(mode);
                        refreshState();
                    }
                });
        connect(m_chatService, &ChatService::chatNotificationModeChangeFailed, this,
                [this](const Chat &chat, const QString &error) {
                    if (chat != m_chat || !m_notificationModeUpdatePending)
                        return;

                    m_notificationModeUpdatePending = false;
                    m_personalSettings->setNotificationMode(m_savedNotificationMode);
                    finishOperation(error);
                });
        connect(m_chatService, &ChatService::chatPriorityChanged, this,
                [this](const Chat &chat, ChatPriority priority) {
                    if (chat != m_chat || m_saving || m_personalSettings->priority() != m_savedPriority)
                        return;

                    m_savedPriority = priority;
                    m_personalSettings->setPriority(priority);
                    refreshState();
                });
    }
}

QWidget *MatrixRoomSettingsWindow::createGeneralTab()
{
    auto generalTab = new QWidget{m_tabs};
    auto generalLayout = new QVBoxLayout{generalTab};
    auto headerLayout = new QHBoxLayout{};

    auto nameWidget = new QWidget{generalTab};
    auto nameForm = new QFormLayout{nameWidget};
    nameForm->setContentsMargins(0, 0, 0, 0);
    m_nameEdit = new QLineEdit{generalTab};
    nameForm->addRow(tr("Room name:"), m_nameEdit);
    m_topicEdit = new QPlainTextEdit{generalTab};
    const auto descriptionHeight = m_topicEdit->fontMetrics().lineSpacing() * 4 + 16;
    m_topicEdit->setFixedHeight(descriptionHeight);
    nameForm->addRow(tr("Description:"), m_topicEdit);
    headerLayout->addWidget(nameWidget, 1, Qt::AlignTop);

    auto avatarWidget = new QWidget{generalTab};
    auto avatarLayout = new QVBoxLayout{avatarWidget};
    avatarLayout->setContentsMargins(0, 0, 0, 0);
    m_avatarPreview = new QLabel{avatarWidget};
    m_avatarPreview->setAlignment(Qt::AlignCenter);
    m_avatarPreview->setFixedSize(96, 96);
    m_avatarPreview->setFrameShape(QFrame::StyledPanel);
    avatarLayout->addWidget(m_avatarPreview, 0, Qt::AlignHCenter);

    auto avatarButtonsLayout = new QHBoxLayout{};
    avatarButtonsLayout->setContentsMargins(0, 0, 0, 0);
    m_changeAvatarButton = new QPushButton{tr("Change..."), avatarWidget};
    m_removeAvatarButton = new QPushButton{tr("Remove"), avatarWidget};
    avatarButtonsLayout->addWidget(m_changeAvatarButton);
    avatarButtonsLayout->addWidget(m_removeAvatarButton);
    avatarLayout->addLayout(avatarButtonsLayout);
    headerLayout->addWidget(avatarWidget, 0, Qt::AlignTop);
    generalLayout->addLayout(headerLayout);

    auto informationForm = new QFormLayout{};

    const auto *roomDetails = qobject_cast<ChatDetailsRoom *>(m_chat.details());
    const auto roomId = m_room ? m_room->id() : (roomDetails ? roomDetails->room() : QString{});
    auto roomIdEdit = new QLineEdit{roomId, generalTab};
    roomIdEdit->setReadOnly(true);
    roomIdEdit->setClearButtonEnabled(false);
    informationForm->addRow(tr("Internal room ID:"), roomIdEdit);

    m_accessSummaryLabel = new QLabel{generalTab};
    m_historySummaryLabel = new QLabel{generalTab};
    m_encryptionSummaryLabel = new QLabel{generalTab};
    m_directorySummaryLabel = new QLabel{tr("Loading..."), generalTab};
    m_membersSummaryLabel = new QLabel{generalTab};
    informationForm->addRow(tr("Access:"), m_accessSummaryLabel);
    informationForm->addRow(tr("History:"), m_historySummaryLabel);
    informationForm->addRow(tr("Encryption:"), m_encryptionSummaryLabel);
    informationForm->addRow(tr("Public directory:"), m_directorySummaryLabel);
    informationForm->addRow(tr("Members:"), m_membersSummaryLabel);
    generalLayout->addLayout(informationForm);
    generalLayout->addStretch();

    return generalTab;
}

QWidget *MatrixRoomSettingsWindow::createAccessTab()
{
    auto accessTab = new QWidget{m_tabs};
    auto form = new QFormLayout{accessTab};

    m_joinRuleCombo = new QComboBox{accessTab};
    m_joinRuleCombo->addItem(tr("Public - anyone can join"), QStringLiteral("public"));
    m_joinRuleCombo->addItem(tr("Private - invitation only"), QStringLiteral("invite"));
    m_joinRuleCombo->addItem(tr("Invitation or join request"), QStringLiteral("knock"));
    form->addRow(tr("Who can join:"), m_joinRuleCombo);

    m_historyVisibilityCombo = new QComboBox{accessTab};
    m_historyVisibilityCombo->addItem(tr("Members, including history before joining"), QStringLiteral("shared"));
    m_historyVisibilityCombo->addItem(tr("From the invitation"), QStringLiteral("invited"));
    m_historyVisibilityCombo->addItem(tr("From joining the room"), QStringLiteral("joined"));
    m_historyVisibilityCombo->addItem(tr("Anyone"), QStringLiteral("world_readable"));
    m_historyVisibilityCombo->setToolTip(
        tr("Changes affect future events. They do not retroactively change access to existing history."));
    form->addRow(tr("Who can read history:"), m_historyVisibilityCombo);

    m_guestAccessCombo = new QComboBox{accessTab};
    m_guestAccessCombo->addItem(tr("Forbidden"), QStringLiteral("forbidden"));
    m_guestAccessCombo->addItem(tr("Allowed"), QStringLiteral("can_join"));
    form->addRow(tr("Guest access:"), m_guestAccessCombo);

    m_accessEncryptionLabel = new QLabel{accessTab};
    form->addRow(tr("Encryption:"), m_accessEncryptionLabel);

    auto encryptionNote = new QLabel{
        tr("Encryption is shown for information only. Enabling it is irreversible and will be added separately."),
        accessTab};
    encryptionNote->setWordWrap(true);
    form->addRow(encryptionNote);
    return accessTab;
}

QWidget *MatrixRoomSettingsWindow::createCommunicationTab()
{
    auto tab = new QWidget{m_tabs};
    auto form = new QFormLayout{tab};
    addPowerLevelSetting(form, tr("Send messages:"), QStringLiteral("events_default"), PowerLevelLocation::Root);
    addPowerLevelSetting(
        form, tr("Send reactions:"), QStringLiteral("m.reaction"), PowerLevelLocation::Events, false);
    addPowerLevelSetting(
        form, tr("Remove own messages:"), QStringLiteral("m.room.redaction"), PowerLevelLocation::Events, false);
    addPowerLevelSetting(
        form, tr("Start polls:"), QStringLiteral("m.poll.start"), PowerLevelLocation::Events, false);
    addPowerLevelSetting(
        form, tr("Respond to polls:"), QStringLiteral("m.poll.response"), PowerLevelLocation::Events, false);
    addPowerLevelSetting(
        form, tr("End polls:"), QStringLiteral("m.poll.end"), PowerLevelLocation::Events, false);
    addPowerLevelSetting(
        form, tr("Pin messages:"), QStringLiteral("m.room.pinned_events"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Notify everyone (@room):"), QStringLiteral("room"), PowerLevelLocation::Notifications, false, 50);
    return tab;
}

QWidget *MatrixRoomSettingsWindow::createModerationTab()
{
    auto tab = new QWidget{m_tabs};
    auto form = new QFormLayout{tab};
    addPowerLevelSetting(form, tr("Default user role:"), QStringLiteral("users_default"), PowerLevelLocation::Root);
    addPowerLevelSetting(form, tr("Invite users:"), QStringLiteral("invite"), PowerLevelLocation::Root);
    addPowerLevelSetting(form, tr("Remove users:"), QStringLiteral("kick"), PowerLevelLocation::Root, false, 50);
    addPowerLevelSetting(form, tr("Ban users:"), QStringLiteral("ban"), PowerLevelLocation::Root, false, 50);
    addPowerLevelSetting(
        form, tr("Remove messages sent by others:"), QStringLiteral("redact"), PowerLevelLocation::Root, false, 50);
    return tab;
}

QWidget *MatrixRoomSettingsWindow::createAdministrationTab()
{
    auto tab = new QWidget{m_tabs};
    auto form = new QFormLayout{tab};
    addPowerLevelSetting(
        form, tr("Change room settings:"), QStringLiteral("state_default"), PowerLevelLocation::Root, false, 50);
    addPowerLevelSetting(
        form, tr("Change room name:"), QStringLiteral("m.room.name"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change description:"), QStringLiteral("m.room.topic"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change room avatar:"), QStringLiteral("m.room.avatar"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change room access:"), QStringLiteral("m.room.join_rules"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change history visibility:"), QStringLiteral("m.room.history_visibility"),
        PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change guest access:"), QStringLiteral("m.room.guest_access"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change main room address:"), QStringLiteral("m.room.canonical_alias"),
        PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change permissions:"), QStringLiteral("m.room.power_levels"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Enable encryption:"), QStringLiteral("m.room.encryption"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Upgrade the room:"), QStringLiteral("m.room.tombstone"), PowerLevelLocation::Events, true);
    addPowerLevelSetting(
        form, tr("Change server ACLs:"), QStringLiteral("m.room.server_acl"), PowerLevelLocation::Events, true);
    return tab;
}

QWidget *MatrixRoomSettingsWindow::createUsersTab()
{
    auto tab = new QWidget{m_tabs};
    auto layout = new QVBoxLayout{tab};
    auto searchLayout = new QHBoxLayout{};

    m_memberSearchEdit = new QLineEdit{tab};
    m_memberSearchEdit->setPlaceholderText(tr("Search room members..."));
    m_memberSearchEdit->setClearButtonEnabled(true);
    searchLayout->addWidget(m_memberSearchEdit, 1);

    m_addMemberButton = new QToolButton{tab};
    if (m_iconsManager)
        m_addMemberButton->setIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("list-add")}));
    else
        m_addMemberButton->setText(QStringLiteral("+"));
    m_addMemberButton->setToolTip(tr("Add a permission entry for the selected user"));
    m_addMemberButton->setEnabled(false);
    searchLayout->addWidget(m_addMemberButton);
    layout->addLayout(searchLayout);

    m_userPowerLevelsTable = new QTableWidget{0, 3, tab};
    m_userPowerLevelsTable->setHorizontalHeaderLabels({tr("Avatar"), tr("User"), tr("Role")});
    m_userPowerLevelsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_userPowerLevelsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_userPowerLevelsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_userPowerLevelsTable->verticalHeader()->setVisible(false);
    m_userPowerLevelsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userPowerLevelsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_userPowerLevelsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_userPowerLevelsTable->setShowGrid(false);
    layout->addWidget(m_userPowerLevelsTable, 1);

    connect(m_memberSearchEdit, &QLineEdit::textChanged, this, &MatrixRoomSettingsWindow::refreshMemberSearch);
    connect(m_memberSearchEdit, &QLineEdit::textEdited, this, [this] {
        if (!m_memberSearchLoaded && !m_memberSearchLoading)
            ensureMemberSearchModel();
    });
    connect(m_addMemberButton, &QToolButton::clicked, this, &MatrixRoomSettingsWindow::addSelectedMember);
    return tab;
}

QWidget *MatrixRoomSettingsWindow::createAdvancedTab()
{
    auto tab = new QWidget{m_tabs};
    auto layout = new QVBoxLayout{tab};
    auto roomGroup = new QGroupBox{tr("Room"), tab};
    auto roomForm = new QFormLayout{roomGroup};
    m_roomVersionLabel = new QLabel{roomGroup};
    m_canonicalAliasLabel = new QLabel{roomGroup};
    m_predecessorLabel = new QLabel{roomGroup};
    m_successorLabel = new QLabel{roomGroup};
    roomForm->addRow(tr("Version:"), m_roomVersionLabel);
    roomForm->addRow(tr("Canonical alias:"), m_canonicalAliasLabel);
    roomForm->addRow(tr("Predecessor:"), m_predecessorLabel);
    roomForm->addRow(tr("Successor:"), m_successorLabel);
    layout->addWidget(roomGroup);

    m_customPowerLevelsGroup = new QGroupBox{tr("Custom event permissions"), tab};
    m_customPowerLevelsForm = new QFormLayout{m_customPowerLevelsGroup};
    layout->addWidget(m_customPowerLevelsGroup);
    layout->addStretch();
    return tab;
}

void MatrixRoomSettingsWindow::addPowerLevelSetting(
    QFormLayout *form, const QString &label, const QString &key, PowerLevelLocation location, bool stateEvent,
    qint64 specificationDefault)
{
    auto editor = new MatrixPowerLevelEditor{form->parentWidget()};
    form->addRow(label, editor);
    m_powerLevelSettings.append({label, key, location, stateEvent, specificationDefault, editor});
    connect(editor, &MatrixPowerLevelEditor::valueChanged, this, [this] {
        refreshPowerLevelDefaults();
        refreshState();
    });
}

void MatrixRoomSettingsWindow::createCustomPowerLevelSettings()
{
    if (m_customPowerLevelsCreated || !m_customPowerLevelsForm)
        return;
    m_customPowerLevelsCreated = true;

    QSet<QString> knownEvents;
    QSet<QString> knownNotifications;
    for (const auto &setting : m_powerLevelSettings)
        if (setting.location == PowerLevelLocation::Events)
            knownEvents.insert(setting.key);
        else if (setting.location == PowerLevelLocation::Notifications)
            knownNotifications.insert(setting.key);

    const auto events = m_savedPowerLevels.value(QStringLiteral("events")).toObject();
    for (auto it = events.constBegin(); it != events.constEnd(); ++it)
        if (!knownEvents.contains(it.key()))
            addPowerLevelSetting(
                m_customPowerLevelsForm, it.key() + QStringLiteral(":"), it.key(), PowerLevelLocation::Events);

    const auto notifications = m_savedPowerLevels.value(QStringLiteral("notifications")).toObject();
    for (auto it = notifications.constBegin(); it != notifications.constEnd(); ++it)
        if (!knownNotifications.contains(it.key()))
            addPowerLevelSetting(
                m_customPowerLevelsForm, QStringLiteral("notifications.%1:").arg(it.key()), it.key(),
                PowerLevelLocation::Notifications);

    if (m_customPowerLevelsForm->rowCount() == 0)
        m_customPowerLevelsForm->addRow(
            new QLabel{tr("No custom permission entries."), m_customPowerLevelsForm->parentWidget()});
}

void MatrixRoomSettingsWindow::ensureMemberSearchModel()
{
    if (!m_connection || !m_room || m_memberSearchLoading || m_memberSearchLoaded)
        return;

    if (!m_memberSearchModel)
    {
        m_memberSearchModel = new QStandardItemModel{this};
        m_memberSearchProxy = new QSortFilterProxyModel{this};
        m_memberSearchProxy->setSourceModel(m_memberSearchModel);
        m_memberSearchProxy->setFilterRole(MemberSearchTextRole);
        m_memberSearchProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
        m_memberSearchProxy->setDynamicSortFilter(true);

        m_memberCompleter = new QCompleter{m_memberSearchProxy, m_memberSearchEdit};
        m_memberCompleter->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        m_memberCompleter->setCompletionRole(Qt::DisplayRole);
        m_memberCompleter->setMaxVisibleItems(12);
        m_memberSearchEdit->setCompleter(m_memberCompleter);
        connect(
            m_memberCompleter,
            static_cast<void (QCompleter::*)(const QModelIndex &)>(&QCompleter::activated), this,
            &MatrixRoomSettingsWindow::selectMemberSearchResult);
    }

    m_memberSearchLoading = true;
    m_memberSearchEdit->setEnabled(false);
    m_memberSearchEdit->setPlaceholderText(tr("Loading room members..."));
    m_memberSearchEdit->setToolTip({});

    // Matrix has no paginated room-member search endpoint. Keep the request lazy
    // by taking this snapshot only after the user opens the permissions tab.
    m_connection->callApi<Quotient::GetJoinedMembersByRoomJob>(m_room->id())
        .then(this, [this](Quotient::GetJoinedMembersByRoomJob *job) {
            const auto joined = job->joined();
            QStringList memberIds(joined.keyBegin(), joined.keyEnd());
            std::sort(memberIds.begin(), memberIds.end(), [&joined](const QString &left, const QString &right) {
                const auto leftName = joined.value(left).displayName;
                const auto rightName = joined.value(right).displayName;
                return (leftName.isEmpty() ? left : leftName).localeAwareCompare(
                           rightName.isEmpty() ? right : rightName) < 0;
            });

            m_memberDisplayNames.clear();
            m_memberAvatarUrls.clear();
            m_memberSearchModel->clear();
            for (const auto &memberId : memberIds)
            {
                const auto member = joined.value(memberId);
                m_memberDisplayNames.insert(memberId, member.displayName);
                m_memberAvatarUrls.insert(memberId, member.avatarUrl);

                const auto displayName = member.displayName.isEmpty() ? memberId : member.displayName;
                const auto completionLabel = member.displayName.isEmpty()
                                                 ? memberId
                                                 : QStringLiteral("%1 (%2)").arg(member.displayName, memberId);
                auto item = new QStandardItem{completionLabel};
                item->setData(memberId, MemberIdRole);
                item->setData(displayName + QLatin1Char('\n') + memberId, MemberSearchTextRole);
                m_memberSearchModel->appendRow(item);
            }

            m_memberSearchLoading = false;
            m_memberSearchLoaded = true;
            m_memberSearchEdit->setPlaceholderText(tr("Search room members..."));
            for (const auto &setting : m_userPowerLevelSettings)
                refreshUserPowerLevel(setting.userId);
            refreshPermissions();
            refreshMemberSearch();
        }, [this](Quotient::GetJoinedMembersByRoomJob *job) {
            m_memberSearchLoading = false;
            m_memberSearchLoaded = false;
            m_memberSearchEdit->setPlaceholderText(tr("Room members could not be loaded"));
            m_memberSearchEdit->setToolTip(job->errorString());
            refreshPermissions();
        });
}

void MatrixRoomSettingsWindow::refreshMemberSearch()
{
    m_selectedMemberId.clear();
    if (!m_memberSearchProxy)
    {
        m_addMemberButton->setEnabled(false);
        return;
    }

    const auto searchText = m_memberSearchEdit->text().trimmed();
    m_memberSearchProxy->setFilterFixedString(searchText);

    for (auto row = 0; row < m_memberSearchProxy->rowCount(); ++row)
    {
        const auto index = m_memberSearchProxy->index(row, 0);
        const auto memberId = index.data(MemberIdRole).toString();
        if (memberId.compare(searchText, Qt::CaseInsensitive) == 0)
        {
            m_selectedMemberId = memberId;
            break;
        }
    }

    m_addMemberButton->setEnabled(
        !m_saving && canSendState(QStringLiteral("m.room.power_levels")) && !m_selectedMemberId.isEmpty());
}

void MatrixRoomSettingsWindow::selectMemberSearchResult(const QModelIndex &index)
{
    const auto memberId = index.data(MemberIdRole).toString();
    if (memberId.isEmpty())
        return;

    m_selectedMemberId = memberId;
    const QSignalBlocker blocker{m_memberSearchEdit};
    m_memberSearchEdit->setText(memberId);
    if (m_memberSearchProxy)
        m_memberSearchProxy->setFilterFixedString(memberId);
    m_addMemberButton->setEnabled(
        !m_saving && canSendState(QStringLiteral("m.room.power_levels")));
}

void MatrixRoomSettingsWindow::addSelectedMember()
{
    if (m_selectedMemberId.isEmpty())
        return;

    addUserPowerLevel(m_selectedMemberId, std::nullopt, true);
    m_selectedMemberId.clear();
    m_memberSearchEdit->clear();
    refreshMemberSearch();
    refreshState();
}

void MatrixRoomSettingsWindow::loadUserPowerLevels()
{
    m_userPowerLevelSettings.clear();
    m_userPowerLevelsTable->setRowCount(0);

    const auto users = m_savedPowerLevels.value(QStringLiteral("users")).toObject();
    for (auto it = users.constBegin(); it != users.constEnd(); ++it)
        if (!it.key().isEmpty() && it.value().isDouble())
            addUserPowerLevel(it.key(), it.value().toInteger());
}

void MatrixRoomSettingsWindow::addUserPowerLevel(
    const QString &userId, std::optional<qint64> explicitValue, bool selectRow)
{
    for (auto row = 0; row < m_userPowerLevelSettings.size(); ++row)
    {
        if (m_userPowerLevelSettings.at(row).userId != userId)
            continue;
        if (selectRow)
        {
            m_userPowerLevelsTable->selectRow(row);
            m_userPowerLevelsTable->scrollTo(m_userPowerLevelsTable->model()->index(row, 0));
        }
        return;
    }

    const auto row = m_userPowerLevelsTable->rowCount();
    m_userPowerLevelsTable->insertRow(row);
    m_userPowerLevelsTable->setRowHeight(row, 56);

    auto avatarLabel = new QLabel{m_userPowerLevelsTable};
    avatarLabel->setFixedSize(48, 48);
    avatarLabel->setAlignment(Qt::AlignCenter);
    m_userPowerLevelsTable->setCellWidget(row, 0, avatarLabel);

    auto identityWidget = new QWidget{m_userPowerLevelsTable};
    auto identityLayout = new QVBoxLayout{identityWidget};
    identityLayout->setContentsMargins(4, 2, 4, 2);
    identityLayout->setSpacing(0);
    auto nameLabel = new QLabel{identityWidget};
    nameLabel->setTextFormat(Qt::PlainText);
    auto nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    auto mxidLabel = new QLabel{identityWidget};
    mxidLabel->setTextFormat(Qt::PlainText);
    auto mxidPalette = mxidLabel->palette();
    mxidPalette.setColor(QPalette::WindowText, mxidPalette.color(QPalette::PlaceholderText));
    mxidLabel->setPalette(mxidPalette);
    mxidLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    identityLayout->addWidget(nameLabel);
    identityLayout->addWidget(mxidLabel);
    m_userPowerLevelsTable->setCellWidget(row, 1, identityWidget);

    auto editor = new MatrixPowerLevelEditor{m_userPowerLevelsTable};
    editor->setValue(
        explicitValue, desiredRootPowerLevel(QStringLiteral("users_default"), 0));
    editor->setEditorEnabled(canSendState(QStringLiteral("m.room.power_levels")) && !m_saving);
    m_userPowerLevelsTable->setCellWidget(row, 2, editor);

    m_userPowerLevelSettings.append({userId, avatarLabel, nameLabel, mxidLabel, editor});
    connect(editor, &MatrixPowerLevelEditor::valueChanged, this, [this] {
        m_userPowerLevelsTable->resizeColumnToContents(2);
        refreshState();
    });
    refreshUserPowerLevel(userId);
    if (selectRow)
    {
        m_userPowerLevelsTable->selectRow(row);
        m_userPowerLevelsTable->scrollToBottom();
    }
}

void MatrixRoomSettingsWindow::refreshUserPowerLevel(const QString &userId)
{
    auto setting = std::find_if(
        m_userPowerLevelSettings.begin(), m_userPowerLevelSettings.end(),
        [&userId](const UserPowerLevelSetting &candidate) { return candidate.userId == userId; });
    if (setting == m_userPowerLevelSettings.end())
        return;

    auto displayName = m_memberDisplayNames.value(userId);
    auto avatarUrl = m_memberAvatarUrls.value(userId);
    QPixmap avatar;
    if (m_room)
    {
        const auto member = m_room->member(userId);
        if (!member.isEmpty())
        {
            displayName = member.name();
            if (!member.avatarUrl().isEmpty())
                avatarUrl = member.avatarUrl();
        }
    }
    if (m_connection && !avatarUrl.isEmpty())
    {
        const QPointer<MatrixRoomSettingsWindow> window{this};
        avatar = QPixmap::fromImage(m_connection->userAvatar(avatarUrl).get(48, [window, userId] {
            if (window)
                window->refreshUserPowerLevel(userId);
        }));
    }
    else if (m_room)
        avatar = QPixmap::fromImage(m_room->memberAvatar(userId, 48));

    setting->nameLabel->setText(displayName.isEmpty() ? tr("No display name") : displayName);
    setting->mxidLabel->setText(userId);
    setting->mxidLabel->show();
    if (avatar.isNull() && m_iconsManager)
        avatar = m_iconsManager->iconByPath(KaduIcon{QStringLiteral("kadu_icons/buddy0")}).pixmap(48, 48);
    setting->avatarLabel->setPixmap(
        avatar.isNull() ? QPixmap{} : avatar.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MatrixRoomSettingsWindow::connectRoom()
{
    if (!m_room)
        return;

    connect(m_room, &Quotient::Room::namesChanged, this, [this] {
        if (!hasChanges() && !m_saving)
            loadRoomData();
        else
            refreshPermissions();
    });
    connect(m_room, &Quotient::Room::topicChanged, this, [this] {
        if (!hasChanges() && !m_saving)
            loadRoomData();
    });
    connect(m_room, &Quotient::Room::avatarChanged, this, [this] {
        if (m_avatarFileName.isEmpty() && !m_removeAvatar && !m_saving)
        {
            m_savedAvatarUrl = m_room ? m_room->avatarUrl() : QUrl{};
            refreshAvatarPreview();
            refreshState();
        }
    });
    connect(m_room, &Quotient::Room::changed, this, [this](Quotient::Room::Changes) {
        if (!hasChanges() && !m_saving)
            loadRoomData();
        else
            refreshPermissions();
    });
    connect(m_room, &Quotient::Room::memberNameUpdated, this,
            [this](const Quotient::RoomMember &member) { refreshUserPowerLevel(member.id()); });
    connect(m_room, &Quotient::Room::memberAvatarUpdated, this,
            [this](const Quotient::RoomMember &member) { refreshUserPowerLevel(member.id()); });
    connect(m_room, &Quotient::Room::memberListChanged, this, [this] {
        m_memberSearchLoaded = false;
        if (!m_memberSearchLoading && m_tabs->currentIndex() == m_usersTabIndex)
            ensureMemberSearchModel();
    });
    connect(m_room, &QObject::destroyed, this, [this] {
        m_room = nullptr;
        refreshPermissions();
    });
}

void MatrixRoomSettingsWindow::loadRoomData()
{
    m_savedNotificationMode = m_chat.notificationMode();
    m_personalSettings->setNotificationMode(m_savedNotificationMode);
    m_savedPriority = m_chat.priority();
    m_personalSettings->setPriority(m_savedPriority);

    if (!m_room)
    {
        const auto *details = qobject_cast<ChatDetailsRoom *>(m_chat.details());
        m_savedName = m_chat.display();
        m_savedTopic = details ? details->description() : QString{};
        m_savedAvatarUrl = {};
        m_avatarFileName.clear();
        m_removeAvatar = false;
        m_nameEdit->setText(m_savedName);
        m_topicEdit->setPlainText(m_savedTopic);
        refreshAvatarPreview();
        refreshRoomInformation();
        refreshPermissions();
        refreshState();
        return;
    }

    m_savedName = m_room->name();
    m_savedTopic = m_room->topic();
    m_savedAvatarUrl = m_room->avatarUrl();
    m_avatarFileName.clear();
    m_removeAvatar = false;

    const auto state = m_room->currentState();
    m_savedJoinRules = state.contentJson(QStringLiteral("m.room.join_rules"));
    m_savedHistoryVisibility = state.contentJson(QStringLiteral("m.room.history_visibility"));
    m_savedGuestAccess = state.contentJson(QStringLiteral("m.room.guest_access"));
    m_savedPowerLevels = state.contentJson(QStringLiteral("m.room.power_levels"));

    m_nameEdit->setText(m_savedName);
    m_nameEdit->setPlaceholderText(m_room->displayName());
    m_topicEdit->setPlainText(m_savedTopic);

    const QSignalBlocker joinRuleBlocker{m_joinRuleCombo};
    const QSignalBlocker historyBlocker{m_historyVisibilityCombo};
    const QSignalBlocker guestBlocker{m_guestAccessCombo};
    const auto joinRule = m_savedJoinRules.value(QStringLiteral("join_rule")).toString(QStringLiteral("invite"));
    if (m_joinRuleCombo->findData(joinRule) < 0)
        m_joinRuleCombo->addItem(joinRuleName(joinRule), joinRule);
    m_joinRuleCombo->setCurrentIndex(m_joinRuleCombo->findData(joinRule));
    const auto history =
        m_savedHistoryVisibility.value(QStringLiteral("history_visibility")).toString(QStringLiteral("shared"));
    if (m_historyVisibilityCombo->findData(history) < 0)
        m_historyVisibilityCombo->addItem(historyVisibilityName(history), history);
    m_historyVisibilityCombo->setCurrentIndex(m_historyVisibilityCombo->findData(history));
    const auto guest = m_savedGuestAccess.value(QStringLiteral("guest_access")).toString(QStringLiteral("forbidden"));
    if (m_guestAccessCombo->findData(guest) < 0)
        m_guestAccessCombo->addItem(guestAccessName(guest), guest);
    m_guestAccessCombo->setCurrentIndex(m_guestAccessCombo->findData(guest));

    createCustomPowerLevelSettings();
    loadPowerLevels();
    refreshAvatarPreview();
    refreshRoomInformation();
    refreshPermissions();
    refreshState();
}

void MatrixRoomSettingsWindow::loadPowerLevels()
{
    const auto events = m_savedPowerLevels.value(QStringLiteral("events")).toObject();
    const auto notifications = m_savedPowerLevels.value(QStringLiteral("notifications")).toObject();
    const auto eventsDefault = m_savedPowerLevels.value(QStringLiteral("events_default")).toInteger(0);
    const auto stateDefault = m_savedPowerLevels.value(QStringLiteral("state_default")).toInteger(50);

    for (auto &setting : m_powerLevelSettings)
    {
        const QJsonObject *container = nullptr;
        if (setting.location == PowerLevelLocation::Root)
            container = &m_savedPowerLevels;
        else if (setting.location == PowerLevelLocation::Events)
            container = &events;
        else
            container = &notifications;

        const auto explicitValue = container->contains(setting.key)
                                       ? std::optional<qint64>{container->value(setting.key).toInteger()}
                                       : std::nullopt;
        const auto inherited = setting.location == PowerLevelLocation::Events
                                   ? (setting.stateEvent ? stateDefault : eventsDefault)
                                   : setting.specificationDefault;
        setting.editor->setValue(explicitValue, inherited);
    }
    loadUserPowerLevels();
    refreshPowerLevelDefaults();
}

void MatrixRoomSettingsWindow::loadDirectoryVisibility()
{
    if (!m_connection || !m_room)
    {
        m_directorySummaryLabel->setText(tr("Unknown"));
        return;
    }

    m_connection->callApi<Quotient::GetRoomVisibilityOnDirectoryJob>(m_room->id())
        .then(this, [this](const QString &visibility) {
            m_directorySummaryLabel->setText(
                visibility == QStringLiteral("public") ? tr("Published") : tr("Not published"));
        }, [this] { m_directorySummaryLabel->setText(tr("Unknown")); });
}

bool MatrixRoomSettingsWindow::canSendState(const QString &eventType) const
{
    return m_connection && m_connection->isLoggedIn() && m_room &&
           m_room->joinState() == Quotient::JoinState::Join &&
           m_room->memberEffectivePowerLevel() >= m_room->powerLevelFor(eventType, true);
}

void MatrixRoomSettingsWindow::refreshPermissions()
{
    const auto canSetName = canSendState(QStringLiteral("m.room.name"));
    const auto canSetTopic = canSendState(QStringLiteral("m.room.topic"));
    const auto canSetAvatar = canSendState(QStringLiteral("m.room.avatar"));
    const auto canSetJoinRules = canSendState(QStringLiteral("m.room.join_rules"));
    const auto canSetHistory = canSendState(QStringLiteral("m.room.history_visibility"));
    const auto canSetGuestAccess = canSendState(QStringLiteral("m.room.guest_access"));
    const auto canSetPowerLevels = canSendState(QStringLiteral("m.room.power_levels"));
    const auto deniedToolTip = tr("You do not have permission to change this room property.");

    m_personalSettings->setEditingEnabled(!m_saving && m_chatService);

    m_nameEdit->setReadOnly(!canSetName);
    m_nameEdit->setToolTip(canSetName ? QString{} : deniedToolTip);
    m_topicEdit->setReadOnly(!canSetTopic);
    m_topicEdit->setToolTip(canSetTopic ? QString{} : deniedToolTip);
    m_changeAvatarButton->setEnabled(canSetAvatar && !m_saving);
    m_removeAvatarButton->setEnabled(
        canSetAvatar && !m_saving && (!m_savedAvatarUrl.isEmpty() || !m_avatarFileName.isEmpty()));
    m_changeAvatarButton->setToolTip(canSetAvatar ? QString{} : deniedToolTip);
    m_removeAvatarButton->setToolTip(canSetAvatar ? QString{} : deniedToolTip);

    m_joinRuleCombo->setEnabled(canSetJoinRules && !m_saving);
    m_historyVisibilityCombo->setEnabled(canSetHistory && !m_saving);
    m_guestAccessCombo->setEnabled(canSetGuestAccess && !m_saving);
    m_joinRuleCombo->setToolTip(canSetJoinRules ? QString{} : deniedToolTip);
    m_historyVisibilityCombo->setToolTip(canSetHistory ? QString{} : deniedToolTip);
    m_guestAccessCombo->setToolTip(canSetGuestAccess ? QString{} : deniedToolTip);

    if (m_communicationTabIndex >= 0)
        m_tabs->setTabVisible(m_communicationTabIndex, canSetPowerLevels);
    if (m_moderationTabIndex >= 0)
        m_tabs->setTabVisible(m_moderationTabIndex, canSetPowerLevels);
    if (m_administrationTabIndex >= 0)
        m_tabs->setTabVisible(m_administrationTabIndex, canSetPowerLevels);
    if (m_usersTabIndex >= 0)
        m_tabs->setTabVisible(m_usersTabIndex, canSetPowerLevels);
    m_customPowerLevelsGroup->setVisible(canSetPowerLevels);
    for (const auto &setting : m_powerLevelSettings)
        setting.editor->setEditorEnabled(canSetPowerLevels && !m_saving);
    for (const auto &setting : m_userPowerLevelSettings)
        setting.editor->setEditorEnabled(canSetPowerLevels && !m_saving);
    m_memberSearchEdit->setEnabled(canSetPowerLevels && !m_saving && !m_memberSearchLoading);
    m_addMemberButton->setEnabled(canSetPowerLevels && !m_saving && !m_selectedMemberId.isEmpty());
    refreshState();
}

void MatrixRoomSettingsWindow::refreshRoomInformation()
{
    if (!m_room)
    {
        const auto unavailable = tr("Unavailable");
        m_accessSummaryLabel->setText(unavailable);
        m_historySummaryLabel->setText(unavailable);
        m_encryptionSummaryLabel->setText(unavailable);
        m_membersSummaryLabel->setText(unavailable);
        m_accessEncryptionLabel->setText(unavailable);
        m_roomVersionLabel->setText(unavailable);
        m_canonicalAliasLabel->setText(unavailable);
        m_predecessorLabel->setText(unavailable);
        m_successorLabel->setText(unavailable);
        return;
    }

    const auto joinRule = m_savedJoinRules.value(QStringLiteral("join_rule")).toString(QStringLiteral("invite"));
    const auto history =
        m_savedHistoryVisibility.value(QStringLiteral("history_visibility")).toString(QStringLiteral("shared"));
    const auto encrypted = m_room->usesEncryption() ? tr("Enabled") : tr("Disabled");
    m_accessSummaryLabel->setText(joinRuleName(joinRule));
    m_historySummaryLabel->setText(historyVisibilityName(history));
    m_encryptionSummaryLabel->setText(encrypted);
    m_accessEncryptionLabel->setText(encrypted);
    m_membersSummaryLabel->setText(
        m_room->totalMemberCount() < 0 ? tr("Unknown") : QString::number(m_room->totalMemberCount()));
    m_roomVersionLabel->setText(m_room->version());
    m_canonicalAliasLabel->setText(m_room->canonicalAlias().isEmpty() ? tr("None") : m_room->canonicalAlias());
    m_predecessorLabel->setText(m_room->predecessorId().isEmpty() ? tr("None") : m_room->predecessorId());
    m_successorLabel->setText(m_room->successorId().isEmpty() ? tr("None") : m_room->successorId());
}

void MatrixRoomSettingsWindow::refreshPowerLevelDefaults()
{
    const auto eventsDefault = desiredRootPowerLevel(QStringLiteral("events_default"), 0);
    const auto stateDefault = desiredRootPowerLevel(QStringLiteral("state_default"), 50);
    for (const auto &setting : m_powerLevelSettings)
        if (setting.location == PowerLevelLocation::Events)
            setting.editor->setInheritedValue(setting.stateEvent ? stateDefault : eventsDefault);
        else
            setting.editor->setInheritedValue(setting.specificationDefault);
    const auto usersDefault = desiredRootPowerLevel(QStringLiteral("users_default"), 0);
    for (const auto &setting : m_userPowerLevelSettings)
        setting.editor->setInheritedValue(usersDefault);
}

bool MatrixRoomSettingsWindow::hasAccessChanges() const
{
    if (!m_room)
        return false;
    return desiredJoinRules() != m_savedJoinRules || desiredHistoryVisibility() != m_savedHistoryVisibility ||
           desiredGuestAccess() != m_savedGuestAccess;
}

bool MatrixRoomSettingsWindow::hasPowerLevelChanges() const
{
    if (!m_room)
        return false;
    return desiredPowerLevels() != m_savedPowerLevels;
}

bool MatrixRoomSettingsWindow::hasChanges() const
{
    return m_nameEdit->text() != m_savedName || m_topicEdit->toPlainText() != m_savedTopic ||
           !m_avatarFileName.isEmpty() || m_removeAvatar || hasAccessChanges() || hasPowerLevelChanges() ||
           m_personalSettings->notificationMode() != m_savedNotificationMode ||
           m_personalSettings->priority() != m_savedPriority;
}

void MatrixRoomSettingsWindow::refreshState()
{
    m_okButton->setEnabled(!m_saving);
    m_applyButton->setEnabled(!m_saving && hasChanges());
    m_cancelButton->setEnabled(!m_saving);
}

qint64 MatrixRoomSettingsWindow::desiredRootPowerLevel(const QString &key, qint64 fallback) const
{
    for (const auto &setting : m_powerLevelSettings)
        if (setting.location == PowerLevelLocation::Root && setting.key == key && setting.editor->isValueValid())
            return setting.editor->explicitValue().value_or(fallback);
    return fallback;
}

QJsonObject MatrixRoomSettingsWindow::desiredJoinRules() const
{
    auto result = m_savedJoinRules;
    const auto joinRule = m_joinRuleCombo->currentData().toString();
    result.insert(QStringLiteral("join_rule"), joinRule);
    if (joinRule != QStringLiteral("restricted") && joinRule != QStringLiteral("knock_restricted"))
        result.remove(QStringLiteral("allow"));
    return result;
}

QJsonObject MatrixRoomSettingsWindow::desiredHistoryVisibility() const
{
    auto result = m_savedHistoryVisibility;
    result.insert(QStringLiteral("history_visibility"), m_historyVisibilityCombo->currentData().toString());
    return result;
}

QJsonObject MatrixRoomSettingsWindow::desiredGuestAccess() const
{
    auto result = m_savedGuestAccess;
    result.insert(QStringLiteral("guest_access"), m_guestAccessCombo->currentData().toString());
    return result;
}

QJsonObject MatrixRoomSettingsWindow::desiredPowerLevels() const
{
    auto result = m_savedPowerLevels;
    auto events = result.value(QStringLiteral("events")).toObject();
    auto notifications = result.value(QStringLiteral("notifications")).toObject();
    auto users = result.value(QStringLiteral("users")).toObject();

    for (const auto &setting : m_powerLevelSettings)
    {
        auto *container = setting.location == PowerLevelLocation::Root
                              ? &result
                              : setting.location == PowerLevelLocation::Events ? &events : &notifications;
        const auto value = setting.editor->explicitValue();
        if (value)
            container->insert(setting.key, QJsonValue{*value});
        else
            container->remove(setting.key);
    }

    for (const auto &setting : m_userPowerLevelSettings)
    {
        const auto value = setting.editor->explicitValue();
        if (value)
            users.insert(setting.userId, QJsonValue{*value});
        else
            users.remove(setting.userId);
    }

    if (events.isEmpty())
        result.remove(QStringLiteral("events"));
    else
        result.insert(QStringLiteral("events"), events);
    if (notifications.isEmpty())
        result.remove(QStringLiteral("notifications"));
    else
        result.insert(QStringLiteral("notifications"), notifications);
    if (users.isEmpty())
        result.remove(QStringLiteral("users"));
    else
        result.insert(QStringLiteral("users"), users);
    return result;
}

bool MatrixRoomSettingsWindow::powerLevelEditorsValid() const
{
    for (const auto &setting : m_powerLevelSettings)
        if (!setting.editor->isValueValid())
            return false;
    for (const auto &setting : m_userPowerLevelSettings)
        if (!setting.editor->isValueValid())
            return false;
    return true;
}

QString MatrixRoomSettingsWindow::joinRuleName(const QString &joinRule) const
{
    if (joinRule == QStringLiteral("public"))
        return tr("Public - anyone can join");
    if (joinRule == QStringLiteral("invite") || joinRule == QStringLiteral("private"))
        return tr("Private - invitation only");
    if (joinRule == QStringLiteral("knock"))
        return tr("Invitation or join request");
    if (joinRule == QStringLiteral("restricted"))
        return tr("Restricted to members of selected Spaces");
    if (joinRule == QStringLiteral("knock_restricted"))
        return tr("Restricted, with join requests");
    return tr("Unknown (%1)").arg(joinRule);
}

QString MatrixRoomSettingsWindow::historyVisibilityName(const QString &historyVisibility) const
{
    if (historyVisibility == QStringLiteral("world_readable"))
        return tr("Anyone");
    if (historyVisibility == QStringLiteral("shared"))
        return tr("Members, including history before joining");
    if (historyVisibility == QStringLiteral("invited"))
        return tr("From the invitation");
    if (historyVisibility == QStringLiteral("joined"))
        return tr("From joining the room");
    return tr("Unknown (%1)").arg(historyVisibility);
}

QString MatrixRoomSettingsWindow::guestAccessName(const QString &guestAccess) const
{
    if (guestAccess == QStringLiteral("can_join"))
        return tr("Allowed");
    if (guestAccess == QStringLiteral("forbidden"))
        return tr("Forbidden");
    return tr("Unknown (%1)").arg(guestAccess);
}

void MatrixRoomSettingsWindow::setAvatarPreview(const QPixmap &avatar)
{
    if (avatar.isNull())
    {
        m_avatarPreview->setPixmap({});
        m_avatarPreview->setText(tr("No avatar"));
        return;
    }

    m_avatarPreview->setText({});
    m_avatarPreview->setPixmap(avatar.scaled(m_avatarPreview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MatrixRoomSettingsWindow::refreshAvatarPreview()
{
    if (m_removeAvatar)
    {
        setAvatarPreview({});
        return;
    }
    if (!m_avatarFileName.isEmpty())
    {
        setAvatarPreview(QPixmap{m_avatarFileName});
        return;
    }

    const auto *details = qobject_cast<ChatDetailsRoom *>(m_chat.details());
    setAvatarPreview(details ? details->avatar() : QPixmap{});
}

void MatrixRoomSettingsWindow::chooseAvatar()
{
    const auto fileName = QFileDialog::getOpenFileName(
        this, tr("Select room avatar"), {}, tr("Images (*.png *.jpg *.jpeg *.gif *.webp);;All files (*)"));
    if (fileName.isEmpty())
        return;

    const QPixmap avatar{fileName};
    if (avatar.isNull())
    {
        QMessageBox::warning(this, tr("Room Settings"), tr("The selected file is not a valid image."));
        return;
    }

    m_avatarFileName = fileName;
    m_removeAvatar = false;
    setAvatarPreview(avatar);
    refreshPermissions();
}

void MatrixRoomSettingsWindow::removeAvatar()
{
    m_avatarFileName.clear();
    m_removeAvatar = true;
    setAvatarPreview({});
    refreshPermissions();
}

void MatrixRoomSettingsWindow::setSaving(bool saving)
{
    m_saving = saving;
    m_nameEdit->setEnabled(!saving);
    m_topicEdit->setEnabled(!saving);
    m_personalSettings->setEditingEnabled(!saving && m_chatService);
    refreshPermissions();
}

void MatrixRoomSettingsWindow::save(bool closeAfterSave)
{
    if (m_saving || !hasChanges())
    {
        if (closeAfterSave && !m_saving)
            close();
        return;
    }
    if (!m_room || !m_connection)
    {
        QMessageBox::warning(this, tr("Room Settings"), tr("The room is no longer available."));
        return;
    }
    if (!powerLevelEditorsValid())
    {
        QMessageBox::warning(
            this, tr("Room Settings"), tr("Enter a valid integer for every custom power level."));
        return;
    }

    m_closeAfterSave = closeAfterSave;
    m_pendingOperations = 0;
    m_errors.clear();
    setSaving(true);

    if (m_nameEdit->text() != m_savedName)
    {
        if (canSendState(QStringLiteral("m.room.name")))
            startStateUpdate(
                QStringLiteral("m.room.name"), QJsonObject{{QStringLiteral("name"), m_nameEdit->text()}},
                tr("Room name"));
        else
            m_errors.append(tr("The room name could not be changed because your permission has changed."));
    }
    if (m_topicEdit->toPlainText() != m_savedTopic)
    {
        if (canSendState(QStringLiteral("m.room.topic")))
            startStateUpdate(
                QStringLiteral("m.room.topic"), QJsonObject{{QStringLiteral("topic"), m_topicEdit->toPlainText()}},
                tr("Description"));
        else
            m_errors.append(tr("The description could not be changed because your permission has changed."));
    }
    if (!m_avatarFileName.isEmpty() || m_removeAvatar)
    {
        if (canSendState(QStringLiteral("m.room.avatar")))
            startAvatarUpdate();
        else
            m_errors.append(tr("The avatar could not be changed because your permission has changed."));
    }

    const auto joinRules = desiredJoinRules();
    if (joinRules != m_savedJoinRules)
    {
        if (canSendState(QStringLiteral("m.room.join_rules")))
            startStateUpdate(QStringLiteral("m.room.join_rules"), joinRules, tr("Room access"));
        else
            m_errors.append(tr("Room access could not be changed because your permission has changed."));
    }
    const auto historyVisibility = desiredHistoryVisibility();
    if (historyVisibility != m_savedHistoryVisibility)
    {
        if (canSendState(QStringLiteral("m.room.history_visibility")))
            startStateUpdate(
                QStringLiteral("m.room.history_visibility"), historyVisibility, tr("History visibility"));
        else
            m_errors.append(tr("History visibility could not be changed because your permission has changed."));
    }
    const auto guestAccess = desiredGuestAccess();
    if (guestAccess != m_savedGuestAccess)
    {
        if (canSendState(QStringLiteral("m.room.guest_access")))
            startStateUpdate(QStringLiteral("m.room.guest_access"), guestAccess, tr("Guest access"));
        else
            m_errors.append(tr("Guest access could not be changed because your permission has changed."));
    }
    const auto powerLevels = desiredPowerLevels();
    if (powerLevels != m_savedPowerLevels)
    {
        if (canSendState(QStringLiteral("m.room.power_levels")))
            startStateUpdate(QStringLiteral("m.room.power_levels"), powerLevels, tr("Permissions"));
        else
            m_errors.append(tr("Permissions could not be changed because your permission has changed."));
    }

    const auto notificationMode = m_personalSettings->notificationMode();
    if (notificationMode != m_savedNotificationMode)
    {
        if (m_chatService)
        {
            ++m_pendingOperations;
            m_notificationModeUpdatePending = true;
            if (!m_chatService->setChatNotificationMode(m_chat, notificationMode))
            {
                m_notificationModeUpdatePending = false;
                --m_pendingOperations;
                m_errors.append(tr("The notification setting could not be changed."));
            }
        }
        else
            m_errors.append(tr("The notification setting is unavailable."));
    }

    const auto priority = m_personalSettings->priority();
    if (priority != m_savedPriority)
    {
        if (m_chatService && m_chatService->setChatPriority(m_chat, priority))
        {
            m_savedPriority = m_chat.priority();
            m_personalSettings->setPriority(m_savedPriority);
        }
        else
            m_errors.append(tr("The room priority could not be changed."));
    }

    if (m_pendingOperations == 0)
        finishOperation();
}

void MatrixRoomSettingsWindow::startStateUpdate(
    const QString &eventType, const QJsonObject &content, const QString &settingName, bool countOperation)
{
    if (countOperation)
        ++m_pendingOperations;
    if (!m_room)
    {
        finishOperation(tr("%1: the room is no longer available.").arg(settingName));
        return;
    }

    auto *job = m_room->setState(eventType, QString{}, content);
    if (!job)
    {
        finishOperation(tr("%1: the server request could not be started.").arg(settingName));
        return;
    }

    connect(job, &Quotient::BaseJob::success, this, [this, eventType, content] {
        if (eventType == QStringLiteral("m.room.name"))
            m_savedName = content.value(QStringLiteral("name")).toString();
        else if (eventType == QStringLiteral("m.room.topic"))
            m_savedTopic = content.value(QStringLiteral("topic")).toString();
        else if (eventType == QStringLiteral("m.room.avatar"))
        {
            m_savedAvatarUrl = QUrl{content.value(QStringLiteral("url")).toString()};
            m_avatarFileName.clear();
            m_removeAvatar = false;
        }
        else if (eventType == QStringLiteral("m.room.join_rules"))
            m_savedJoinRules = content;
        else if (eventType == QStringLiteral("m.room.history_visibility"))
            m_savedHistoryVisibility = content;
        else if (eventType == QStringLiteral("m.room.guest_access"))
            m_savedGuestAccess = content;
        else if (eventType == QStringLiteral("m.room.power_levels"))
            m_savedPowerLevels = content;
        finishOperation();
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, job, settingName] {
        const auto details = job->errorString().isEmpty() ? tr("The server rejected the change.") : job->errorString();
        finishOperation(tr("%1: %2").arg(settingName, details));
    });
}

void MatrixRoomSettingsWindow::startAvatarUpdate()
{
    ++m_pendingOperations;
    if (m_removeAvatar)
    {
        startStateUpdate(
            QStringLiteral("m.room.avatar"), QJsonObject{{QStringLiteral("url"), QString{}}}, tr("Avatar"), false);
        return;
    }

    const auto contentType = QMimeDatabase{}.mimeTypeForFile(m_avatarFileName).name();
    m_connection->uploadFile(m_avatarFileName, contentType)
        .then(this, [this](const QUrl &url) {
            startStateUpdate(
                QStringLiteral("m.room.avatar"), QJsonObject{{QStringLiteral("url"), url.toString()}}, tr("Avatar"),
                false);
        }, [this] { finishOperation(tr("Avatar: the image could not be uploaded.")); });
}

void MatrixRoomSettingsWindow::finishOperation(const QString &error)
{
    if (!error.isEmpty())
        m_errors.append(error);

    if (m_pendingOperations > 0)
        --m_pendingOperations;
    if (m_pendingOperations > 0)
        return;

    setSaving(false);
    refreshRoomInformation();
    refreshState();
    if (!m_errors.isEmpty())
    {
        QMessageBox::warning(this, tr("Room Settings"), m_errors.join(QStringLiteral("\n")));
        return;
    }

    loadPowerLevels();

    if (m_closeAfterSave)
        close();
}

void MatrixRoomSettingsWindow::closeEvent(QCloseEvent *event)
{
    if (m_saving)
    {
        event->ignore();
        return;
    }
    QWidget::closeEvent(event);
}
