#ifndef AIS_STREAM_CLIENT_H
#define AIS_STREAM_CLIENT_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include <wx/string.h>

#include <ixwebsocket/IXWebSocket.h>

// Connects to the OpenWaters.io AIS websocket feed for a given search area
// and delivers decoded NMEA sentences via callback until stopped.
//
// Uses IXWebSocket internally, which owns its own background thread for
// the socket I/O and dispatches messages via a callback. Start()/Stop()/
// Restart() are safe to call from the GUI thread. The sentence callback is
// invoked from the IXWebSocket worker thread, so callers must marshal back
// to the GUI thread themselves if they touch wx widgets from it (e.g. via
// wxTheApp->CallAfter or a wx event).
class AisStreamClient
{
    public:
        using SentenceCallback = std::function<void(const wxString&)>;

        AisStreamClient();
        ~AisStreamClient();

        AisStreamClient(const AisStreamClient&) = delete;
        AisStreamClient& operator=(const AisStreamClient&) = delete;

        // Starts streaming for the given search area. No-op if already running.
        void Start(double latitude, double longitude, double boxSizeDegrees, SentenceCallback onSentence);

        // Stops streaming and blocks until the socket has closed. No-op
        // if not running.
        void Stop();

        // Stop() + Start() with new parameters; no-op (does not start) if not
        // already streaming.
        void Restart(double latitude, double longitude, double boxSizeDegrees, SentenceCallback onSentence);

        bool IsStreaming() const;

    private:
        void OnMessage(const ix::WebSocketMessagePtr& msg);
        std::string BuildSubscribeMessage(double latitude, double longitude, double boxSizeDegrees) const;

        std::unique_ptr<ix::WebSocket> m_webSocket;
        std::atomic<bool> m_streaming{false};
        SentenceCallback m_onSentence;

        double m_latitude = 0.0;
        double m_longitude = 0.0;
        double m_boxSizeDegrees = 0.0;
};

#endif // AIS_STREAM_CLIENT_H
