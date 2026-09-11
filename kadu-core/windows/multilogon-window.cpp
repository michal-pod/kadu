/*
 * %kadu copyright begin%
 * Copyright 2011 Piotr Dąbrowski (ultr@ultr.pl)
 * Copyright 2011, 2012, 2013, 2014 Bartosz Brachaczek (b.brachaczek@gmail.com)
 * Copyright 2011, 2012, 2013, 2014 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include <QtGui/QKeyEvent>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>

#include <memory>

#include "accounts/filter/have-multilogon-filter.h"
#include "configuration/config-file-variant-wrapper.h"
#include "core/injected-factory.h"
#include "icons/icons-manager.h"
#include "model/roles.h"
#include "multilogon/model/multilogon-model.h"
#include "multilogon/multilogon-session.h"
#include "os/generic/window-geometry-manager.h"
#include "protocols/protocol.h"
#include "protocols/services/multilogon-service.h"
#include "widgets/accounts-combo-box.h"

#include "multilogon-window.h"
#include "multilogon-window.moc"

MultilogonWindow::MultilogonWindow(QWidget *parent) : QWidget(parent), DesktopAwareObject(this)
{
    setWindowRole("kadu-multilogon");

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Sessions"));
}

MultilogonWindow::~MultilogonWindow()
{
}

void MultilogonWindow::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void MultilogonWindow::setInjectedFactory(InjectedFactory *injectedFactory)
{
    m_injectedFactory = injectedFactory;
}

void MultilogonWindow::init()
{
    createGui();

    new WindowGeometryManager(
        new ConfigFileVariantWrapper(m_configuration, "General", "MultilogonWindowGeometry"), QRect(0, 50, 450, 300),
        this);
}

void MultilogonWindow::createGui()
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    QWidget *selectAccountWidget = new QWidget(this);
    QHBoxLayout *selectAccountLayout = new QHBoxLayout(selectAccountWidget);

    selectAccountLayout->addWidget(new QLabel(tr("Account:"), selectAccountWidget));
    selectAccountLayout->setContentsMargins(0, 0, 0, 0);

    Accounts = m_injectedFactory->makeInjected<AccountsComboBox>(
        true, AccountsComboBox::NotVisibleWithOneRowSourceModel, selectAccountWidget);
    Accounts->addFilter(new HaveMultilogonFilter(Accounts));
    Accounts->setIncludeIdInDisplay(true);
    selectAccountLayout->addWidget(Accounts);
    selectAccountLayout->addStretch(1);

    connect(Accounts, SIGNAL(currentIndexChanged(int)), this, SLOT(accountChanged()));

    layout->addWidget(selectAccountWidget);

    SessionsTable = new QTableView(this);
    SessionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    SessionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    SessionsTable->setSortingEnabled(true);
    SessionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    SessionsTable->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(SessionsTable);

    StatusLabel = new QLabel(this);
    StatusLabel->setTextFormat(Qt::PlainText);
    StatusLabel->setWordWrap(true);
    layout->addWidget(StatusLabel);

    QDialogButtonBox *buttons = new QDialogButtonBox(this);
    RefreshButton =
        new QPushButton(qApp->style()->standardIcon(QStyle::SP_BrowserReload), tr("Refresh"), buttons);
    KillSessionButton =
        new QPushButton(qApp->style()->standardIcon(QStyle::SP_DialogCloseButton), tr("Disconnect session"), buttons);
    VerifySessionButton = new QPushButton(tr("Verify device"), buttons);
    QPushButton *closeButton =
        new QPushButton(qApp->style()->standardIcon(QStyle::SP_DialogCancelButton), tr("Close"), buttons);

    KillSessionButton->setEnabled(false);
    connect(RefreshButton, SIGNAL(clicked()), this, SLOT(refreshSessions()));
    connect(KillSessionButton, SIGNAL(clicked()), this, SLOT(killSession()));
    connect(VerifySessionButton, &QPushButton::clicked, this, [this] {
        if (CurrentService)
            CurrentService->verifySession(multilogonSession());
    });
    connect(closeButton, SIGNAL(clicked()), this, SLOT(close()));

    buttons->addButton(RefreshButton, QDialogButtonBox::ActionRole);
    buttons->addButton(VerifySessionButton, QDialogButtonBox::ActionRole);
    buttons->addButton(KillSessionButton, QDialogButtonBox::DestructiveRole);
    buttons->addButton(closeButton, QDialogButtonBox::RejectRole);

    layout->addSpacing(16);
    layout->addWidget(buttons);

    accountChanged();
}

void MultilogonWindow::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
    {
        e->accept();
        close();
    }
    else
        QWidget::keyPressEvent(e);
}

MultilogonService *MultilogonWindow::multilogonService()
{
    if (CurrentService)
        return CurrentService;

    Protocol *protocol = Accounts->currentAccount().protocolHandler();
    if (!protocol)
        return 0;

    return protocol->multilogonService();
}

MultilogonSession MultilogonWindow::multilogonSession()
{
    QItemSelectionModel *selectionModel = SessionsTable->selectionModel();
    if (!selectionModel)
        return {};

    QModelIndex index = selectionModel->currentIndex();
    return index.data(MultilogonSessionRole).value<MultilogonSession>();
}

void MultilogonWindow::accountChanged()
{
    if (CurrentService)
        disconnect(CurrentService, nullptr, this, nullptr);
    CurrentService = nullptr;
    SessionsLoading = false;
    SessionOperationRunning = false;
    StatusLabel->clear();
    Accounts->setEnabled(true);
    VerifySessionButton->setVisible(false);
    VerifySessionButton->setEnabled(false);
    delete SessionsTable->model();

    Protocol *protocol = Accounts->currentAccount().protocolHandler();
    MultilogonService *service = protocol ? protocol->multilogonService() : nullptr;
    if (!service)
    {
        RefreshButton->setEnabled(false);
        KillSessionButton->setEnabled(false);
        return;
    }

    CurrentService = service;
    VerifySessionButton->setVisible(service->supportsSessionVerification());

    SessionsTable->setModel(new MultilogonModel(service, this));
    SessionsTable->setColumnHidden(1, !service->supportsSessionVerification());

    connect(
        SessionsTable->selectionModel(), SIGNAL(currentChanged(QModelIndex, QModelIndex)), this,
        SLOT(selectionChanged()));
    connect(service, SIGNAL(sessionsLoadingChanged(bool)), this, SLOT(sessionsLoadingChanged(bool)));
    connect(service, SIGNAL(sessionsReset()), this, SLOT(sessionsReset()));
    connect(service, SIGNAL(sessionsRefreshFailed(QString)), this, SLOT(sessionsRefreshFailed(QString)));
    connect(service, SIGNAL(sessionKillStarted(MultilogonSession)), this,
            SLOT(sessionKillStarted(MultilogonSession)));
    connect(service, SIGNAL(sessionKillFinished(MultilogonSession)), this,
            SLOT(sessionKillFinished(MultilogonSession)));
    connect(service, SIGNAL(sessionKillFailed(MultilogonSession,QString)), this,
            SLOT(sessionKillFailed(MultilogonSession,QString)));
    connect(service, SIGNAL(sessionKillPasswordRequired(MultilogonSession,QString)), this,
            SLOT(sessionKillPasswordRequired(MultilogonSession,QString)));

    RefreshButton->setEnabled(true);
    sessionsReset();
    sessionsLoadingChanged(service->sessionsLoading());
    refreshSessions();
}

void MultilogonWindow::selectionChanged()
{
    const auto session = multilogonSession();
    KillSessionButton->setEnabled(
        !SessionsLoading && !SessionOperationRunning && CurrentService && CurrentService->canKillSession(session));
    VerifySessionButton->setEnabled(
        !SessionsLoading && !SessionOperationRunning && CurrentService && CurrentService->canVerifySession(session));
}

void MultilogonWindow::killSession()
{
    const QPointer<MultilogonService> service{multilogonService()};
    if (!service)
        return;

    const auto session = multilogonSession();
    if (!service->canKillSession(session))
        return;

    const auto result = QMessageBox::question(
        this, tr("Disconnect session"),
        tr("Disconnect %1? This will sign the session out of the account.").arg(session.name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result == QMessageBox::Yes && service && CurrentService == service)
        service->killSession(session);
}

void MultilogonWindow::refreshSessions()
{
    if (!CurrentService || SessionsLoading || SessionOperationRunning)
        return;

    StatusLabel->clear();
    CurrentService->refreshSessions();
}

void MultilogonWindow::sessionsLoadingChanged(bool loading)
{
    SessionsLoading = loading;
    RefreshButton->setEnabled(!loading && !SessionOperationRunning);
    if (loading)
        StatusLabel->setText(tr("Loading sessions..."));
    selectionChanged();
}

void MultilogonWindow::sessionsReset()
{
    if (SessionOperationRunning)
    {
        selectionChanged();
        return;
    }

    if (!SessionsTable->model() || SessionsTable->model()->rowCount() == 0)
        StatusLabel->setText(tr("No sessions available."));
    else
        StatusLabel->clear();
    selectionChanged();
}

void MultilogonWindow::sessionsRefreshFailed(const QString &details)
{
    StatusLabel->setText(
        details.isEmpty() ? tr("Could not load sessions.") : tr("Could not load sessions: %1").arg(details));
}

void MultilogonWindow::sessionKillStarted(MultilogonSession session)
{
    Q_UNUSED(session)
    SessionOperationRunning = true;
    Accounts->setEnabled(false);
    RefreshButton->setEnabled(false);
    StatusLabel->setText(tr("Disconnecting session..."));
    selectionChanged();
}

void MultilogonWindow::sessionKillFinished(MultilogonSession session)
{
    Q_UNUSED(session)
    SessionOperationRunning = false;
    Accounts->setEnabled(true);
    RefreshButton->setEnabled(!SessionsLoading);
    StatusLabel->setText(tr("Session disconnected."));
    selectionChanged();
}

void MultilogonWindow::sessionKillFailed(MultilogonSession session, const QString &details)
{
    Q_UNUSED(session)
    SessionOperationRunning = false;
    Accounts->setEnabled(true);
    RefreshButton->setEnabled(!SessionsLoading);
    StatusLabel->setText(
        details.isEmpty() ? tr("Could not disconnect the session.")
                          : tr("Could not disconnect the session: %1").arg(details));
    selectionChanged();
}

void MultilogonWindow::sessionKillPasswordRequired(
    MultilogonSession session, const QString &authenticationSession)
{
    if (!CurrentService)
        return;

    const QPointer<MultilogonService> service{CurrentService};
    auto *dialog = new QInputDialog{this};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("Authentication required"));
    dialog->setLabelText(tr("Enter the password for %1 to disconnect this session:").arg(session.account.id()));
    dialog->setTextEchoMode(QLineEdit::Password);
    const auto answered = std::make_shared<bool>(false);
    connect(dialog, &QDialog::finished, service, [service, dialog, session, authenticationSession, answered](int result) {
        *answered = true;
        if (service)
            service->provideSessionKillPassword(
                session, authenticationSession, result == QDialog::Accepted ? dialog->textValue() : QString{});
    });
    connect(dialog, &QObject::destroyed, service, [service, session, authenticationSession, answered] {
        if (service && !*answered)
            service->provideSessionKillPassword(session, authenticationSession, {});
    });
    connect(service, &MultilogonService::sessionKillFailed, dialog,
            [dialog, session](const MultilogonSession &failed, const QString &) {
                if (failed.id == session.id)
                    dialog->reject();
            });
    dialog->open();
}
