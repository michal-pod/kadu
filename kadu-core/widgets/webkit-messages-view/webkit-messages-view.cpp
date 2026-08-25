/*
 * %kadu copyright begin%
 * Copyright 2014, 2015 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "webkit-messages-view.h"
#include "webkit-messages-view.moc"

#include "chat-style/chat-style-manager.h"
#include "chat-style/engine/chat-style-renderer-configuration.h"
#include "chat-style/engine/chat-style-renderer-factory.h"
#include "chat-style/engine/chat-style-renderer.h"
#include "chat/chat-service-repository.h"
#include "contacts/contact-set.h"
#include "core/injected-factory.h"
#include "gui/configuration/chat-configuration-holder.h"
#include "gui/scoped-updates-disabler.h"
#include "misc/paths-provider.h"
#include "protocols/protocol.h"
#include "protocols/services/chat-image-service.h"
#include "protocols/services/chat-service.h"
#include "services/chat-image-request-service.h"
#include "widgets/webkit-messages-view/message-limit-policy.h"
#include "widgets/webkit-messages-view/webkit-messages-view-handler-factory.h"
#include "widgets/webkit-messages-view/webkit-messages-view-handler.h"

#include <QtCore/QFile>
#include <QtGui/QKeyEvent>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineScript>
#include <QtWebEngineCore/QWebEngineScriptCollection>

WebkitMessagesView::WebkitMessagesView(const Chat &chat, bool supportTransparency, QWidget *parent)
        : KaduWebView{parent}, m_chat{chat}, m_forcePruneDisabled{}, m_supportTransparency{supportTransparency},
          m_atBottom{true}
{
}

WebkitMessagesView::~WebkitMessagesView()
{
    disconnectChat();
}

void WebkitMessagesView::setChatConfigurationHolder(ChatConfigurationHolder *chatConfigurationHolder)
{
    m_chatConfigurationHolder = chatConfigurationHolder;
}

void WebkitMessagesView::setChatImageRequestService(ChatImageRequestService *chatImageRequestService)
{
    m_chatImageRequestService = chatImageRequestService;
}

void WebkitMessagesView::setChatServiceRepository(ChatServiceRepository *chatServiceRepository)
{
    m_chatServiceRepository = chatServiceRepository;
}

void WebkitMessagesView::setChatStyleManager(ChatStyleManager *chatStyleManager)
{
    m_chatStyleManager = chatStyleManager;
}

void WebkitMessagesView::setInjectedFactory(InjectedFactory *injectedFactory)
{
    m_injectedFactory = injectedFactory;
}

void WebkitMessagesView::setPathsProvider(PathsProvider *pathsProvider)
{
    m_pathsProvider = pathsProvider;
}

void WebkitMessagesView::setWebkitMessagesViewHandlerFactory(
    WebkitMessagesViewHandlerFactory *webkitMessagesViewHandlerFactory)
{
    m_webkitMessagesViewHandlerFactory = webkitMessagesViewHandlerFactory;
}

void WebkitMessagesView::init()
{
    connect(
        m_chatImageRequestService.data(), SIGNAL(chatImageStored(ChatImage, QString)), this,
        SLOT(chatImageStored(ChatImage, QString)));

    // TODO: for me with empty styleSheet if has artifacts on scrollbars...
    // maybe Qt bug?
    setStyleSheet("QWidget { }");
    setFocusPolicy(Qt::NoFocus);
    setMinimumSize(QSize(100, 100));
    // JavaScript is enabled on the shared profile; plugins no longer exist in QtWebEngine.

    applyPalette();
    updatePageBackground();

    // Messages are written by other people. Neutering XMLHttpRequest keeps rendered content from
    // reaching the network. Injected as a script so it covers every document, not just this one.
    QWebEngineScript blockXhr;
    blockXhr.setName(QStringLiteral("kadu-block-xhr"));
    blockXhr.setInjectionPoint(QWebEngineScript::DocumentCreation);
    blockXhr.setWorldId(QWebEngineScript::MainWorld);
    blockXhr.setRunsOnSubFrames(true);
    blockXhr.setSourceCode(
        QStringLiteral("XMLHttpRequest.prototype.open = function() { return false; };"
                       "XMLHttpRequest.prototype.send = function() { return false; };"));
    page()->scripts().insert(blockXhr);

    updateScrollBarStyle();
    updateEmoticonStyle();

    connect(page(), &QWebEnginePage::contentsSizeChanged, this, &WebkitMessagesView::scrollToBottom);

    // QtWebEngine renders into a native child widget, so mouse and wheel events never reach this
    // widget. Tracking the scroll position directly is both simpler and more reliable than the
    // event handlers this replaces.
    connect(page(), &QWebEnginePage::scrollPositionChanged, this, &WebkitMessagesView::updateAtBottom);
    connect(m_chatStyleManager, SIGNAL(chatStyleConfigurationUpdated()), this, SLOT(chatStyleConfigurationUpdated()));

    configurationUpdated();
    connectChat();
    refreshView();
}

void WebkitMessagesView::resizeEvent(QResizeEvent *e)
{
    QWebEngineView::resizeEvent(e);

    scrollToBottom();
}

void WebkitMessagesView::updateAtBottom()
{
    // QtWebEngine exposes no scroll bars; what is left below the viewport takes their place.
    // Contents size and scroll position are reported in CSS pixels and can be fractional, all the
    // more so on a fractionally scaled display, so the comparison needs a little slack.
    auto const belowViewport = page()->contentsSize().height() - page()->scrollPosition().y();
    m_atBottom = belowViewport <= height() + 2;

    const auto atTop = page()->scrollPosition().y() <= 2;
    if (atTop && !m_atTop)
        emit scrolledToTop();
    m_atTop = atTop;
}

void WebkitMessagesView::connectChat()
{
    for (auto const &contact : m_chat.contacts())
        connect(contact, SIGNAL(buddyUpdated()), this, SLOT(refreshView()));

    auto chatService = m_chatServiceRepository->chatService(m_chat.chatAccount());
    if (chatService)
        connect(
            chatService, SIGNAL(sentMessageStatusChanged(const Message &)), this,
            SLOT(sentMessageStatusChanged(const Message &)));
}

void WebkitMessagesView::disconnectChat()
{
    if (m_chat.isNull())
        return;

    for (auto const &contact : m_chat.contacts())
        disconnect(contact, nullptr, this, nullptr);

    if (m_chat.chatAccount().isNull() || !m_chat.chatAccount().protocolHandler())
        return;

    auto chatImageService = m_chat.chatAccount().protocolHandler()->chatImageService();
    if (chatImageService)
        disconnect(chatImageService, nullptr, this, nullptr);

    auto chatService = m_chatServiceRepository->chatService(m_chat.chatAccount());
    if (chatService)
        disconnect(chatService, nullptr, this, nullptr);
}

void WebkitMessagesView::setChat(const Chat &chat)
{
    disconnectChat();
    m_chat = chat;
    connectChat();

    refreshView();
}

void WebkitMessagesView::setForcePruneDisabled(bool disable)
{
    m_forcePruneDisabled = disable;
    if (disable)
        m_handler->setMessageLimitPolicy(MessageLimitPolicy::None);
    else
    {
        m_handler->setMessageLimitPolicy(MessageLimitPolicy::Value);
        chatStyleConfigurationUpdated();
    }
}

void WebkitMessagesView::chatStyleConfigurationUpdated()
{
    m_handler->setMessageLimit(m_chatStyleManager->prune());
}

void WebkitMessagesView::refreshView()
{
    if (!m_chatStyleRendererFactory || !m_webkitMessagesViewHandlerFactory)
        return;

    auto chatStyleRenderer = m_chatStyleRendererFactory->createChatStyleRenderer(rendererConfiguration());
    auto handler = m_webkitMessagesViewHandlerFactory.data()->createWebkitMessagesViewHandler(
        std::move(chatStyleRenderer), page());
    setWebkitMessagesViewHandler(std::move(handler));
}

ChatStyleRendererConfiguration WebkitMessagesView::rendererConfiguration()
{
    QFile file{m_pathsProvider->dataPath() + QStringLiteral("scripts/chat-scripts.js")};
    auto javaScript = file.open(QIODevice::ReadOnly | QIODevice::Text) ? file.readAll() : QString{};
    auto transparency = m_chatConfigurationHolder->useTransparency() && supportTransparency() && isCompositingEnabled();
    return ChatStyleRendererConfiguration{chat(), *page(), javaScript, transparency};
}

void WebkitMessagesView::setWebkitMessagesViewHandler(owned_qptr<WebkitMessagesViewHandler> handler)
{
    ScopedUpdatesDisabler updatesDisabler{*this};
    auto scrollBarPosition = page()->scrollPosition().y();

    auto messages = m_handler ? m_handler->messages() : SortedMessages{};
    m_handler = std::move(handler);
    setForcePruneDisabled(m_forcePruneDisabled);
    m_handler->add(messages);

    page()->runJavaScript(QStringLiteral("window.scrollTo(0, %1);").arg(scrollBarPosition));
}

void WebkitMessagesView::pageUp()
{
    auto event = QKeyEvent{QEvent::KeyPress, Qt::Key_PageUp, Qt::NoModifier};
    keyPressEvent(&event);
}

void WebkitMessagesView::pageDown()
{
    auto event = QKeyEvent{QEvent::KeyPress, Qt::Key_PageDown, Qt::NoModifier};
    keyPressEvent(&event);
}

void WebkitMessagesView::chatImageStored(const ChatImage &chatImage, const QString &fullFilePath)
{
    m_handler->displayChatImage(chatImage, fullFilePath);
}

void WebkitMessagesView::add(const Message &message)
{
    ScopedUpdatesDisabler updatesDisabler{*this};
    m_handler->add(message);
    emit messagesUpdated();
}

void WebkitMessagesView::add(const SortedMessages &messages)
{
    ScopedUpdatesDisabler updatesDisabler{*this};
    m_handler->add(messages);
    emit messagesUpdated();
}

SortedMessages WebkitMessagesView::messages() const
{
    return m_handler->messages();
}

void WebkitMessagesView::setChatStyleRendererFactory(std::shared_ptr<ChatStyleRendererFactory> chatStyleRendererFactory)
{
    m_chatStyleRendererFactory = chatStyleRendererFactory;

    refreshView();
}

void WebkitMessagesView::clearMessages()
{
    ScopedUpdatesDisabler updatesDisabler{*this};
    m_handler->clear();
    emit messagesUpdated();
    m_atBottom = true;
    m_atTop = true;
}

int WebkitMessagesView::countMessages()
{
    return m_handler ? m_handler->messages().size() : 0;
}

void WebkitMessagesView::sentMessageStatusChanged(const Message &message)
{
    if (m_chat != message.messageChat())
        return;
    m_handler->displayMessageStatus(message.id(), message.status());
}

void WebkitMessagesView::contactActivityChanged(const Contact &contact, ChatState state)
{
    m_handler->displayChatState(contact, state);
}

void WebkitMessagesView::scrollToTop()
{
    page()->runJavaScript(QStringLiteral("window.scrollTo(0, 0);"));

    // Not updateAtBottom(), for the reason forceScrollToBottom() gives below: runJavaScript() is
    // asynchronous, so the position it read would be the one from before the scroll. Coming from
    // the bottom that leaves the flag set, and the next message drags the view straight back down.
    m_atBottom = false;
}

void WebkitMessagesView::scrollToBottom()
{
    if (m_atBottom)
        forceScrollToBottom();
}

void WebkitMessagesView::forceScrollToBottom()
{
    // No scroll bar API in QtWebEngine; scrolling is done from the document itself.
    page()->runJavaScript(QStringLiteral("window.scrollTo(0, document.body.scrollHeight);"));

    // Deliberately not updateAtBottom(): runJavaScript() is asynchronous, so it would read the
    // position from before the scroll and, with the content having just grown, conclude the view
    // is no longer at the bottom -- stopping the next message from scrolling it. Going to the
    // bottom is what this method means, so the flag simply says so.
    m_atBottom = true;
}

void WebkitMessagesView::configurationUpdated()
{
    applyPalette();
    updateScrollBarStyle();
    updatePageBackground();
    setUserFont(m_chatConfigurationHolder->chatFont().toString(), m_chatConfigurationHolder->forceCustomChatFont());
    refreshView();
}

void WebkitMessagesView::updateScrollBarStyle()
{
    // The page is drawn by a browser engine, which draws its own scroll bar and knows nothing of
    // the application's colours -- so on a dark theme the conversation had a bright bar down its
    // side. Measured against the engine in use: color-scheme alone darkens the page but leaves the
    // bar as it was, and only the ::-webkit-scrollbar rules reach it.
    auto const &colours = palette();
    auto const track = colours.color(QPalette::Base);
    auto const thumb = colours.color(QPalette::Mid);
    auto const thumbHover = colours.color(QPalette::Dark);
    auto const text = colours.color(QPalette::Text);

    // The colour of anything the style did not colour itself. A page whose text is left alone is
    // black, which was fine while conversations were on white and is not now that the background
    // follows the desktop. Chat styles colour what they think of -- the nick, the message -- and
    // leave the rest: the two colons Arvenil puts between the nick and the date sit outside both
    // its coloured elements, ultr colours nothing at all. Those took the page's black.
    //
    // Set on the body, not on everything: colour is inherited, so this reaches whatever was left
    // alone and nothing that was not. A style that names a colour still gets the colour it named.
    auto const style = QStringLiteral(
                           "body { color: %4; }"
                           "::-webkit-scrollbar { width: 12px; height: 12px; }"
                           "::-webkit-scrollbar-track { background: %1; }"
                           "::-webkit-scrollbar-thumb { background: %2; border-radius: 6px;"
                           " border: 3px solid %1; }"
                           "::-webkit-scrollbar-thumb:hover { background: %3; }"
                           "::-webkit-scrollbar-corner { background: %1; }")
                           .arg(track.name(), thumb.name(), thumbHover.name(), text.name());

    // Embedded the way the style renderers do it: escaped, then quoted.
    auto quoted = style;
    quoted.replace('\\', QStringLiteral("\\\\"));
    quoted.replace('\'', QStringLiteral("\\'"));
    quoted = QStringLiteral("'") + quoted + QStringLiteral("'");

    // Inserted as a script rather than into the styles' own sheets: the appearance belongs to the
    // application, not to the chat style, and every style gets it this way.
    QWebEngineScript scrollBarStyle;
    scrollBarStyle.setName(QStringLiteral("kadu-scrollbar-style"));
    scrollBarStyle.setInjectionPoint(QWebEngineScript::DocumentReady);
    scrollBarStyle.setWorldId(QWebEngineScript::MainWorld);
    scrollBarStyle.setRunsOnSubFrames(false);
    scrollBarStyle.setSourceCode(
        QStringLiteral("(function() {"
                       "  var id = 'kadu-scrollbar-style';"
                       "  var previous = document.getElementById(id);"
                       "  if (previous) previous.remove();"
                       "  var sheet = document.createElement('style');"
                       "  sheet.id = id;"
                       "  sheet.textContent = %1;"
                       "  document.head.appendChild(sheet);"
                       "})();")
            .arg(quoted));

    for (auto const &existing : page()->scripts().find(QStringLiteral("kadu-scrollbar-style")))
        page()->scripts().remove(existing);
    page()->scripts().insert(scrollBarStyle);

    // The page already loaded keeps the sheet it was given, so it is replaced there too.
    page()->runJavaScript(scrollBarStyle.sourceCode());
}

void WebkitMessagesView::updateEmoticonStyle()
{
    // Emoticons are small bitmaps drawn pixel by pixel, and they come in one resolution only. The
    // page had been left to scale them by whatever the screen's is, smoothly: measured against the
    // engine in use, a twenty pixel emoticon whose file holds a hundred and three colours reached
    // the screen carrying five hundred, none of the extra ones its own.
    //
    // So the size is stated instead of inherited: zoom multiplies the image's own size, and asking
    // for two screen pixels per pixel of the file leaves the emoticon the same size it was on a
    // screen scaled by two, while on one scaled by one and a half it grows by a third and stops
    // being resampled. Either way it is drawn at a whole number of screen pixels per file pixel,
    // which is what image-rendering: pixelated needs in order to look deliberate rather than ragged.
    //
    // Below a scale of one there is nothing to correct -- the image is already drawn one for one --
    // and doubling it there would only make it bigger for no gain, so that case is left alone.
    //
    // The ratio is read in the page rather than passed in from here, because it is the page that
    // knows it, and it is read again on resize, which is what the engine reports when a window is
    // moved to a screen of another scale.
    QWebEngineScript emoticonStyle;
    emoticonStyle.setName(QStringLiteral("kadu-emoticon-style"));
    emoticonStyle.setInjectionPoint(QWebEngineScript::DocumentReady);
    emoticonStyle.setWorldId(QWebEngineScript::MainWorld);
    emoticonStyle.setRunsOnSubFrames(false);
    emoticonStyle.setSourceCode(QStringLiteral(
        "(function() {"
        "  var id = 'kadu-emoticon-style';"
        "  var apply = function() {"
        "    var previous = document.getElementById(id);"
        "    if (previous) previous.remove();"
        "    var ratio = window.devicePixelRatio;"
        "    var zoom = ratio > 1 ? 2 / ratio : 1;"
        "    var sheet = document.createElement('style');"
        "    sheet.id = id;"
        "    sheet.textContent = 'img[emoticon], img.emoticon"
        " { zoom: ' + zoom + '; image-rendering: pixelated; }';"
        "    document.head.appendChild(sheet);"
        "  };"
        "  if (window.kaduEmoticonStyle)"
        "    window.removeEventListener('resize', window.kaduEmoticonStyle);"
        "  window.kaduEmoticonStyle = apply;"
        "  window.addEventListener('resize', apply);"
        "  apply();"
        "})();"));

    for (auto const &existing : page()->scripts().find(QStringLiteral("kadu-emoticon-style")))
        page()->scripts().remove(existing);
    page()->scripts().insert(emoticonStyle);

    page()->runJavaScript(emoticonStyle.sourceCode());
}

void WebkitMessagesView::applyPalette()
{
    // Built from the application's palette every time rather than from this widget's own, and
    // applied again whenever the desktop changes its colours. Setting a palette makes every colour
    // in it the widget's own, which Qt then stops revising -- so doing this once at construction
    // left the conversation's scroll bar and its background at the colours of whatever scheme was
    // current when the window opened, however dark the desktop went afterwards.
    auto palette = QGuiApplication::palette();

    // This widget never has focus anyway, so there's no need for distinction
    // between active and inactive, and active highlight colors have way better
    // contrast, especially on Windows. See Kadu bug #2605.
    palette.setBrush(QPalette::Inactive, QPalette::Highlight, palette.brush(QPalette::Active, QPalette::Highlight));
    palette.setBrush(
        QPalette::Inactive, QPalette::HighlightedText, palette.brush(QPalette::Active, QPalette::HighlightedText));

    setPalette(palette);
}

void WebkitMessagesView::updatePageBackground()
{
    // QWebEnginePage has no palette, so the page background stands in for QPalette::Base.
    //
    // Asking for a transparent one makes QtWebEngine composite every repaint against whatever is
    // behind the widget, which shows as flicker while scrolling. It is only worth that when
    // transparency is actually in use -- the same condition the renderer is given.
    auto const transparent =
        m_chatConfigurationHolder->useTransparency() && supportTransparency() && isCompositingEnabled();

    page()->setBackgroundColor(transparent ? QColor{Qt::transparent} : palette().color(QPalette::Base));
    setAttribute(Qt::WA_OpaquePaintEvent, !transparent);
}

void WebkitMessagesView::compositingEnabled()
{
    refreshView();
}

void WebkitMessagesView::compositingDisabled()
{
    refreshView();
}
