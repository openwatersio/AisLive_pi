#include <cmath>

#include "main_ui_derived.h"
#include "settings/global_settings.h"
#include "plugin/plugin.h"


////////////////////////////
/// Class Initialization ///
////////////////////////////

DialogMainGui::DialogMainGui(
    wxWindow* parent,
    wxWindowID id,
    const wxString& title,
    const wxPoint& pos,
    const wxSize& size,
    long style)
    : DialogMainGuiBase(parent)
{
    // Keep the UI in sync with the initial stream state.
    m_staticText_streamState->SetLabel(_("Stopped"));
}

DialogMainGui::~DialogMainGui()
{
    StopAisStream();
}


/////////////////////
/// Input updates ///
/////////////////////

void DialogMainGui::updateSearchPosition(double lat, double lon)
{
    m_searchLatitude = lat;
    m_searchLongitude = lon;

    const wxString latDir = lat >= 0.0 ? "N" : "S";
    const wxString lonDir = lon >= 0.0 ? "E" : "W";

    m_staticText_searchLatitude->SetLabel(
        wxString::Format("%.6f°%s", std::abs(lat), latDir));

    m_staticText_searchLongitude->SetLabel(
        wxString::Format("%.6f°%s", std::abs(lon), lonDir));

    // Only restart an already-running stream.
    RestartAisStream();
}

void DialogMainGui::updateSearchBoxSize(double degrees)
{
    m_searchBoxSize = degrees;

    m_slider_searchBoxSize->SetValue(
        static_cast<int>(degrees));

    m_staticText_searchBoxSize->SetLabel(
        wxString::Format("%.0f° x %.0f°", degrees, degrees));

    // Only restart an already-running stream.
    RestartAisStream();
}

void DialogMainGui::updateBoatPosition(double lat, double lon)
{
    m_boatLatitude = lat;
    m_boatLongitude = lon;

    // Use the first valid boat position as the initial AIS search position.
    if (!m_initialBoatPositionSet)
    {
        m_initialBoatPositionSet = true;
        updateSearchPosition(lat, lon);
    }
}


///////////////
/// Getters ///
///////////////

double DialogMainGui::getSearchBoxSize()
{
    return m_searchBoxSize;
}


/////////////////
/// UI events ///
/////////////////

void DialogMainGui::OnClose(wxCloseEvent& event)
{
    StopAisStream();

    if (plugin)
    {
        plugin->OnGuiClosed();
    }
}

void DialogMainGui::OnButtonClick_startStream(wxCommandEvent& event)
{
    StartAisStream();
}

void DialogMainGui::OnButtonClick_stopStream(wxCommandEvent& event)
{
    StopAisStream();
}

void DialogMainGui::OnScroll_UpdateSearchBoxSize(wxScrollEvent& event)
{
    const double degrees =
        static_cast<double>(m_slider_searchBoxSize->GetValue());

    updateSearchBoxSize(degrees);
}


////////////////////
/// AIS streaming ///
////////////////////

void DialogMainGui::StartAisStream()
{
    if (m_aisStream.IsStreaming())
    {
        return;
    }

    m_aisStream.Start(
        m_searchLatitude,
        m_searchLongitude,
        m_searchBoxSize,

        [this](const wxString& sentence)
        {
            // AisStreamClient invokes this callback from its websocket
            // worker thread. Never directly access wxWidgets from there.
            this->CallAfter(
                [this, sentence]()
                {
                    if (!m_aisStream.IsStreaming())
                    {
                        return;
                    }

                    if (plugin)
                    {
                        plugin->sendNmeaSentence(sentence);
                    }
                });
        });

    m_staticText_streamState->SetLabel(_("Running"));
}

void DialogMainGui::StopAisStream()
{
    if (!m_aisStream.IsStreaming())
    {
        m_staticText_streamState->SetLabel(_("Stopped"));
        return;
    }

    m_aisStream.Stop();

    m_staticText_streamState->SetLabel(_("Stopped"));
}

void DialogMainGui::RestartAisStream()
{
    if (!m_aisStream.IsStreaming())
    {
        return;
    }

    m_aisStream.Restart(
        m_searchLatitude,
        m_searchLongitude,
        m_searchBoxSize,

        [this](const wxString& sentence)
        {
            // Callback originates from the IXWebSocket worker thread.
            this->CallAfter(
                [this, sentence]()
                {
                    if (!m_aisStream.IsStreaming())
                    {
                        return;
                    }

                    if (plugin)
                    {
                        plugin->sendNmeaSentence(sentence);
                    }
                });
        });

    m_staticText_streamState->SetLabel(_("Running"));
}

