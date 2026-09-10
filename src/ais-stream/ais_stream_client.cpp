#include "ais_stream_client.h"

#include <utility>

#include <json/json.h>

#include <ixwebsocket/IXNetSystem.h>


namespace {

constexpr char kAisUrl[] = "wss://ais.openwaters.io/v1/stream";

void ProcessAisEvent(const Json::Value& ev, const std::function<void(const wxString&)>& sendSentence)
{
    if (!ev.isMember("type") || ev["type"].asString() != "event")
    {
        return; // welcome/control message, not an AIS report
    }

    if (!ev.isMember("nmea") || !ev["nmea"].isArray())
    {
        return;
    }

    for (const auto& sentence : ev["nmea"])
    {
        if (!sentence.isString())
        {
            continue;
        }

        wxString nmea = wxString::FromUTF8(sentence.asString().c_str());
        if (!nmea.EndsWith("\r\n"))
        {
            nmea += "\r\n";
        }

        sendSentence(nmea);
    }
}

} // namespace


AisStreamClient::AisStreamClient()
{
    // IXWebSocket requires ix::initNetSystem()/uninitNetSystem() to be
    // called once per process on Windows (initializes Winsock). It is a
    // harmless no-op on other platforms. Reference-counted internally by
    // IXWebSocket, so it is safe to call this from multiple client
    // instances.
    ix::initNetSystem();
}

AisStreamClient::~AisStreamClient()
{
    Stop();
    ix::uninitNetSystem();
}

std::string AisStreamClient::BuildSubscribeMessage(double latitude, double longitude, double boxSizeDegrees) const
{
    Json::Value box(Json::arrayValue);
    box.append(latitude  - boxSizeDegrees / 2.0);
    box.append(longitude - boxSizeDegrees / 2.0);
    box.append(latitude  + boxSizeDegrees / 2.0);
    box.append(longitude + boxSizeDegrees / 2.0);

    Json::Value bbox(Json::arrayValue);
    bbox.append(box);

    Json::Value sub_msg;
    sub_msg["type"] = "subscribe";
    sub_msg["bbox"] = bbox;

    Json::FastWriter writer;
    std::string sub_str = writer.write(sub_msg);
    // FastWriter appends a trailing newline; trim it since this is sent
    // as a single websocket text frame.
    if (!sub_str.empty() && sub_str.back() == '\n')
    {
        sub_str.pop_back();
    }
    return sub_str;
}

void AisStreamClient::OnMessage(const ix::WebSocketMessagePtr& msg)
{
    switch (msg->type)
    {
        case ix::WebSocketMessageType::Open:
        {
            // Connection established (and, for wss://, TLS handshake
            // completed) - send the subscribe message for our search area.
            const std::string sub = BuildSubscribeMessage(m_latitude, m_longitude, m_boxSizeDegrees);
            if (m_webSocket)
            {
                m_webSocket->send(sub);
            }
            break;
        }

        case ix::WebSocketMessageType::Message:
        {
            if (!msg->binary)
            {
                Json::Value ev;
                Json::Reader reader;
                if (reader.parse(msg->str, ev) && m_onSentence)
                {
                    ProcessAisEvent(ev, m_onSentence);
                }
                // else: ignore malformed frames.
            }
            break;
        }

        case ix::WebSocketMessageType::Error:
        case ix::WebSocketMessageType::Close:
        case ix::WebSocketMessageType::Ping:
        case ix::WebSocketMessageType::Pong:
        case ix::WebSocketMessageType::Fragment:
        default:
            // Ping/Pong are handled internally by IXWebSocket. Errors and
            // closes are surfaced only via disconnection; IXWebSocket's
            // built-in automatic reconnection (enabled by default) takes
            // care of re-establishing the socket, at which point Open
            // fires again and we re-subscribe.
            break;
    }
}

void AisStreamClient::Start(double latitude, double longitude, double boxSizeDegrees, SentenceCallback onSentence)
{
    if (m_streaming.load())
    {
        return; // already running
    }

    m_latitude = latitude;
    m_longitude = longitude;
    m_boxSizeDegrees = boxSizeDegrees;
    m_onSentence = std::move(onSentence);

    m_webSocket = std::make_unique<ix::WebSocket>();
    m_webSocket->setUrl(kAisUrl);

    // Automatic reconnection with capped exponential backoff, handled
    // internally by IXWebSocket - no manual retry loop needed.
    m_webSocket->setMinWaitBetweenReconnectionRetries(1000);   // 1s
    m_webSocket->setMaxWaitBetweenReconnectionRetries(30000);  // 30s

    // Respond to server pings automatically to keep the connection alive;
    // this is IXWebSocket's default behavior, kept explicit for clarity.
    m_webSocket->setPingInterval(45);

    m_webSocket->setOnMessageCallback(
        [this](const ix::WebSocketMessagePtr& msg)
        {
            OnMessage(msg);
        });

    m_streaming = true;
    m_webSocket->start();
}

void AisStreamClient::Stop()
{
    if (!m_streaming.load())
    {
        return;
    }

    m_streaming = false;

    if (m_webSocket)
    {
        // Blocks until the background thread has stopped and the socket
        // is closed.
        m_webSocket->stop();
        m_webSocket.reset();
    }
}

void AisStreamClient::Restart(double latitude, double longitude, double boxSizeDegrees, SentenceCallback onSentence)
{
    if (!m_streaming.load())
    {
        return;
    }

    Stop();
    Start(latitude, longitude, boxSizeDegrees, std::move(onSentence));
}

bool AisStreamClient::IsStreaming() const
{
    return m_streaming.load();
}
