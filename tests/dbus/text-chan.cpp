#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include <QtDBus/QtDBus>

#include <QtTest/QtTest>

#include <TelepathyQt4/Client/Connection>
#include <TelepathyQt4/Client/PendingReadyChannel>
#include <TelepathyQt4/Client/TextChannel>
#include <TelepathyQt4/Debug>

#include <telepathy-glib/debug.h>

#include <tests/lib/contacts-conn.h>
#include <tests/lib/echo/chan.h>
#include <tests/lib/echo2/chan.h>
#include <tests/lib/test.h>

using namespace Telepathy::Client;
using Telepathy::UIntList;

struct SentMessageDetails
{
    SentMessageDetails(const Message &message,
            const Telepathy::MessageSendingFlags flags,
            const QString &token)
        : message(message), flags(flags), token(token) { }
    Message message;
    Telepathy::MessageSendingFlags flags;
    QString token;
};

class TestTextChan : public Test
{
    Q_OBJECT

public:
    TestTextChan(QObject *parent = 0)
        : Test(parent),
          // service side (telepathy-glib)
          mConnService(0), mBaseConnService(0), mContactRepo(0),
            mTextChanService(0), mMessagesChanService(0),
          // client side (Telepathy-Qt4)
          mConn(0), mChan(0)
    { }

protected Q_SLOTS:
    void onMessageReceived(const Telepathy::Client::ReceivedMessage &);
    void onMessageRemoved(const Telepathy::Client::ReceivedMessage &);
    void onMessageSent(const Telepathy::Client::Message &,
            Telepathy::MessageSendingFlags, const QString &);
    void expectConnReady(uint, uint);

private Q_SLOTS:
    void initTestCase();
    void init();

    void testMessages();
    void testLegacyText();

    void cleanup();
    void cleanupTestCase();

private:
    void commonTest(bool withMessages);
    void sendText(const char *text);

    QList<SentMessageDetails> sent;
    QList<ReceivedMessage> received;
    QList<ReceivedMessage> removed;

    ContactsConnection *mConnService;
    TpBaseConnection *mBaseConnService;
    TpHandleRepoIface *mContactRepo;
    ExampleEchoChannel *mTextChanService;
    ExampleEcho2Channel *mMessagesChanService;

    Connection *mConn;
    TextChannel *mChan;
    QString mTextChanPath;
    QString mMessagesChanPath;
    QString mConnName;
    QString mConnPath;
};

void TestTextChan::onMessageReceived(const ReceivedMessage &message)
{
    received << message;
}

void TestTextChan::onMessageRemoved(const ReceivedMessage &message)
{
    removed << message;
}

void TestTextChan::onMessageSent(const Telepathy::Client::Message &message,
        Telepathy::MessageSendingFlags flags, const QString &token)
{
    sent << SentMessageDetails(message, flags, token);
}

void TestTextChan::expectConnReady(uint newStatus, uint newStatusReason)
{
    qDebug() << "connection changed to status" << newStatus;
    switch (newStatus) {
    case Connection::StatusDisconnected:
        qWarning() << "Disconnected";
        mLoop->exit(1);
        break;
    case Connection::StatusConnecting:
        /* do nothing */
        break;
    case Connection::StatusConnected:
        qDebug() << "Ready";
        mLoop->exit(0);
        break;
    default:
        qWarning().nospace() << "What sort of status is "
            << newStatus << "?!";
        mLoop->exit(2);
        break;
    }
}

void TestTextChan::sendText(const char *text)
{
    // FIXME: there's no high-level API for Send() yet, so...
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(
            mChan->textInterface()->Send(
                Telepathy::ChannelTextMessageTypeNormal,
                QLatin1String(text)));
    QVERIFY(connect(watcher,
                SIGNAL(finished(QDBusPendingCallWatcher *)),
                SLOT(expectSuccessfulCall(QDBusPendingCallWatcher *))));
    QCOMPARE(mLoop->exec(), 0);
    delete watcher;
}

void TestTextChan::initTestCase()
{
    initTestCaseImpl();

    g_type_init();
    g_set_prgname("text-chan");
    tp_debug_set_flags("all");
    dbus_g_bus_get(DBUS_BUS_STARTER, 0);

    gchar *name;
    gchar *connPath;
    GError *error = 0;

    mConnService = CONTACTS_CONNECTION(g_object_new(
            CONTACTS_TYPE_CONNECTION,
            "account", "me@example.com",
            "protocol", "example",
            0));
    QVERIFY(mConnService != 0);
    mBaseConnService = TP_BASE_CONNECTION(mConnService);
    QVERIFY(mBaseConnService != 0);

    QVERIFY(tp_base_connection_register(mBaseConnService,
                "example", &name, &connPath, &error));
    QVERIFY(error == 0);

    QVERIFY(name != 0);
    QVERIFY(connPath != 0);

    mConnName = QString::fromAscii(name);
    mConnPath = QString::fromAscii(connPath);

    g_free(name);
    g_free(connPath);

    mConn = new Connection(mConnName, mConnPath);
    QCOMPARE(mConn->isReady(), false);

    mConn->requestConnect();

    QVERIFY(connect(mConn->requestConnect(),
                    SIGNAL(finished(Telepathy::Client::PendingOperation*)),
                    SLOT(expectSuccessfulCall(Telepathy::Client::PendingOperation*))));
    QCOMPARE(mLoop->exec(), 0);
    QCOMPARE(mConn->isReady(), true);
#if 0
    // should be able to do this, when Connection is no longer broken
    QCOMPARE(static_cast<uint>(mConn->status()),
            static_cast<uint>(Connection::StatusConnected));
#else
    if (mConn->status() != Connection::StatusConnected) {
        QVERIFY(connect(mConn,
                        SIGNAL(statusChanged(uint, uint)),
                        SLOT(expectConnReady(uint, uint))));
        QCOMPARE(mLoop->exec(), 0);
        QVERIFY(disconnect(mConn,
                           SIGNAL(statusChanged(uint, uint)),
                           this,
                           SLOT(expectConnReady(uint, uint))));
        QCOMPARE(mConn->status(), (uint) Connection::StatusConnected);
    }
#endif

    // create a Channel by magic, rather than doing D-Bus round-trips for it

    mContactRepo = tp_base_connection_get_handles(mBaseConnService,
            TP_HANDLE_TYPE_CONTACT);
    guint handle = tp_handle_ensure(mContactRepo, "someone@localhost", 0, 0);

    mTextChanPath = mConnPath + QLatin1String("/TextChannel");
    QByteArray chanPath(mTextChanPath.toAscii());
    mTextChanService = EXAMPLE_ECHO_CHANNEL(g_object_new(
                EXAMPLE_TYPE_ECHO_CHANNEL,
                "connection", mConnService,
                "object-path", chanPath.data(),
                "handle", handle,
                NULL));

    mMessagesChanPath = mConnPath + QLatin1String("/MessagesChannel");
    chanPath = mMessagesChanPath.toAscii();
    mMessagesChanService = EXAMPLE_ECHO_2_CHANNEL(g_object_new(
                EXAMPLE_TYPE_ECHO_2_CHANNEL,
                "connection", mConnService,
                "object-path", chanPath.data(),
                "handle", handle,
                NULL));

    tp_handle_unref(mContactRepo, handle);
}

void TestTextChan::init()
{
    initImpl();

    mChan = 0;
}

void TestTextChan::commonTest(bool withMessages)
{
    Q_ASSERT(mChan != 0);
    Channel *asChannel = mChan;

    QVERIFY(connect(asChannel->becomeReady(),
                SIGNAL(finished(Telepathy::Client::PendingOperation *)),
                SLOT(expectSuccessfulCall(Telepathy::Client::PendingOperation *))));
    QCOMPARE(mLoop->exec(), 0);

    QVERIFY(asChannel->isReady());
    QVERIFY(mChan->isReady());
    QVERIFY(!mChan->isReady(0, TextChannel::FeatureMessageQueue));
    // Implementation detail: in legacy text channels, capabilities arrive
    // early, so don't assert about that
    QVERIFY(!mChan->isReady(0, TextChannel::FeatureMessageSentSignal));

    QVERIFY(connect(mChan,
                SIGNAL(messageReceived(const Telepathy::Client::ReceivedMessage &)),
                SLOT(onMessageReceived(const Telepathy::Client::ReceivedMessage &))));
    QCOMPARE(received.size(), 0);
    QVERIFY(connect(mChan,
                SIGNAL(pendingMessageRemoved(const Telepathy::Client::ReceivedMessage &)),
                SLOT(onMessageRemoved(const Telepathy::Client::ReceivedMessage &))));
    QCOMPARE(removed.size(), 0);

    QVERIFY(connect(mChan,
                SIGNAL(messageSent(const Telepathy::Client::Message &,
                        Telepathy::MessageSendingFlags,
                        const QString &)),
                SLOT(onMessageSent(const Telepathy::Client::Message &,
                        Telepathy::MessageSendingFlags,
                        const QString &))));
    QCOMPARE(sent.size(), 0);

    sendText("One");

    // Make the Sent signal become ready
    QVERIFY(connect(mChan->becomeReady(0, TextChannel::FeatureMessageSentSignal),
                SIGNAL(finished(Telepathy::Client::PendingOperation *)),
                SLOT(expectSuccessfulCall(Telepathy::Client::PendingOperation *))));
    QCOMPARE(mLoop->exec(), 0);

    QVERIFY(asChannel->isReady());
    QVERIFY(mChan->isReady());
    QVERIFY(mChan->isReady(0, TextChannel::FeatureMessageSentSignal));
    QVERIFY(!mChan->isReady(0, TextChannel::FeatureMessageQueue));

    sendText("Two");

    // We should have received "Two", but not "One"
    QCOMPARE(sent.size(), 1);
    QCOMPARE(static_cast<uint>(sent.at(0).flags), 0U);
    QCOMPARE(sent.at(0).token, QString::fromAscii(""));

    Message m(sent.at(0).message);
    QCOMPARE(static_cast<uint>(m.messageType()),
            static_cast<uint>(Telepathy::ChannelTextMessageTypeNormal));
    QVERIFY(!m.isTruncated());
    QVERIFY(!m.hasNonTextContent());
    QCOMPARE(m.messageToken(), QString::fromAscii(""));
    QVERIFY(!m.isSpecificToDBusInterface());
    QCOMPARE(m.dbusInterface(), QString::fromAscii(""));
    QCOMPARE(m.size(), 2);
    QCOMPARE(m.header().value(QLatin1String("message-type")).variant().toUInt(),
            0U);
    QCOMPARE(m.part(1).value(QLatin1String("content-type")).variant().toString(),
            QString::fromAscii("text/plain"));
    QCOMPARE(m.text(), QString::fromAscii("Two"));

    // Make capabilities become ready
    QVERIFY(connect(mChan->becomeReady(0, TextChannel::FeatureMessageCapabilities),
                SIGNAL(finished(Telepathy::Client::PendingOperation *)),
                SLOT(expectSuccessfulCall(Telepathy::Client::PendingOperation *))));
    QCOMPARE(mLoop->exec(), 0);

    QVERIFY(asChannel->isReady());
    QVERIFY(mChan->isReady());
    QVERIFY(mChan->isReady(0, TextChannel::FeatureMessageCapabilities));
    QVERIFY(!mChan->isReady(0, TextChannel::FeatureMessageQueue));

    if (withMessages) {
        QCOMPARE(mChan->supportedContentTypes(), QStringList() << "*/*");
        QCOMPARE(static_cast<uint>(mChan->messagePartSupport()),
                static_cast<uint>(Telepathy::MessagePartSupportFlagOneAttachment |
                    Telepathy::MessagePartSupportFlagMultipleAttachments));
        QCOMPARE(static_cast<uint>(mChan->deliveryReportingSupport()), 0U);
    } else {
        QCOMPARE(mChan->supportedContentTypes(), QStringList() << "text/plain");
        QCOMPARE(static_cast<uint>(mChan->messagePartSupport()), 0U);
        QCOMPARE(static_cast<uint>(mChan->deliveryReportingSupport()), 0U);
    }

    // Make the message queue become ready too
    QCOMPARE(received.size(), 0);
    QCOMPARE(mChan->messageQueue().size(), 0);
    QVERIFY(connect(mChan->becomeReady(0, TextChannel::FeatureMessageQueue),
                SIGNAL(finished(Telepathy::Client::PendingOperation *)),
                SLOT(expectSuccessfulCall(Telepathy::Client::PendingOperation *))));
    QCOMPARE(mLoop->exec(), 0);

    QVERIFY(asChannel->isReady());
    QVERIFY(mChan->isReady());
    QVERIFY(mChan->isReady(0, TextChannel::FeatureMessageQueue));
    QVERIFY(mChan->isReady(0, TextChannel::FeatureMessageCapabilities));

    // Assert that both our sent messages were echoed by the remote contact
    QCOMPARE(received.size(), 2);
    QCOMPARE(mChan->messageQueue().size(), 2);
    QVERIFY(mChan->messageQueue().at(0) == received.at(0));
    QVERIFY(mChan->messageQueue().at(1) == received.at(1));

    m = received.at(0);
    QCOMPARE(static_cast<uint>(m.messageType()),
            static_cast<uint>(Telepathy::ChannelTextMessageTypeNormal));
    QVERIFY(!m.isTruncated());
    QVERIFY(!m.hasNonTextContent());
    QCOMPARE(m.messageToken(), QString::fromAscii(""));
    QVERIFY(!m.isSpecificToDBusInterface());
    QCOMPARE(m.dbusInterface(), QString::fromAscii(""));
    QCOMPARE(m.size(), 2);
    QCOMPARE(m.header().value(QLatin1String("message-type")).variant().toUInt(),
            0U);
    QCOMPARE(m.part(1).value(QLatin1String("content-type")).variant().toString(),
            QString::fromAscii("text/plain"));

    // one "echo" implementation echoes the message literally, the other edits
    // it slightly
    if (withMessages) {
        QCOMPARE(m.text(), QString::fromAscii("One"));
    } else {
        QCOMPARE(m.text(), QString::fromAscii("You said: One"));
    }

    m = received.at(1);
    QCOMPARE(static_cast<uint>(m.messageType()),
            static_cast<uint>(Telepathy::ChannelTextMessageTypeNormal));
    QVERIFY(!m.isTruncated());
    QVERIFY(!m.hasNonTextContent());
    QCOMPARE(m.messageToken(), QString::fromAscii(""));
    QVERIFY(!m.isSpecificToDBusInterface());
    QCOMPARE(m.dbusInterface(), QString::fromAscii(""));
    QCOMPARE(m.size(), 2);
    QCOMPARE(m.header().value(QLatin1String("message-type")).variant().toUInt(),
            0U);
    QCOMPARE(m.part(1).value(QLatin1String("content-type")).variant().toString(),
            QString::fromAscii("text/plain"));

    if (withMessages) {
        QCOMPARE(m.text(), QString::fromAscii("Two"));
    } else {
        QCOMPARE(m.text(), QString::fromAscii("You said: Two"));
    }

    uint id = received.at(0).header().value(
            QLatin1String("pending-message-id")).variant().toUInt();
    // go behind the TextChannel's back to acknowledge the first message:
    // this emulates another client doing so
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(
            mChan->textInterface()->AcknowledgePendingMessages(
                UIntList() << id));
    QVERIFY(connect(watcher,
                SIGNAL(finished(QDBusPendingCallWatcher *)),
                SLOT(expectSuccessfulCall(QDBusPendingCallWatcher *))));
    QCOMPARE(mLoop->exec(), 0);
    delete watcher;

    // on a channel with Messages, we're notified; on a channel with only Text,
    // we're not notified
    if (withMessages) {
        QCOMPARE(mChan->messageQueue().size(), 1);
        QVERIFY(mChan->messageQueue().at(0) == received.at(1));
        QCOMPARE(removed.size(), 1);
        QVERIFY(removed.at(0) == received.at(0));
    } else {
        QCOMPARE(mChan->messageQueue().size(), 2);
        QVERIFY(mChan->messageQueue().at(0) == received.at(0));
        QVERIFY(mChan->messageQueue().at(1) == received.at(1));
        QCOMPARE(removed.size(), 0);
    }
}

void TestTextChan::testMessages()
{
    mChan = new TextChannel(mConn, mMessagesChanPath, QVariantMap(), this);

    commonTest(true);
}

void TestTextChan::testLegacyText()
{
    mChan = new TextChannel(mConn, mTextChanPath, QVariantMap(), this);

    commonTest(false);
}

void TestTextChan::cleanup()
{
    if (mChan != 0) {
        delete mChan;
        mChan = 0;
    }
    received.clear();
    removed.clear();
    sent.clear();

    cleanupImpl();
}

void TestTextChan::cleanupTestCase()
{
    if (mConn != 0) {
        // Disconnect and wait for the readiness change
        QVERIFY(connect(mConn->requestDisconnect(),
                        SIGNAL(finished(Telepathy::Client::PendingOperation*)),
                        SLOT(expectSuccessfulCall(Telepathy::Client::PendingOperation*))));
        QCOMPARE(mLoop->exec(), 0);

        if (mConn->isValid()) {
            QVERIFY(connect(mConn,
                            SIGNAL(invalidated(Telepathy::Client::DBusProxy *,
                                               const QString &, const QString &)),
                            mLoop,
                            SLOT(quit())));
            QCOMPARE(mLoop->exec(), 0);
        }

        delete mConn;
        mConn = 0;
    }

    if (mTextChanService != 0) {
        g_object_unref(mTextChanService);
        mTextChanService = 0;
    }

    if (mMessagesChanService != 0) {
        g_object_unref(mMessagesChanService);
        mMessagesChanService = 0;
    }

    if (mConnService != 0) {
        mBaseConnService = 0;
        g_object_unref(mConnService);
        mConnService = 0;
    }

    cleanupTestCaseImpl();
}

QTEST_MAIN(TestTextChan)
#include "_gen/text-chan.cpp.moc.hpp"
