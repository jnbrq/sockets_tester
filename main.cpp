#include <iostream>
#include <memory>

#if defined(__MINGW32__)
#include <wx/wx.h>
#include <winsock2.h>
#include "client.hpp"
#elif defined(_MSC_VER)
#include "client.hpp"
#include <wx/wx.h>
#else
#include "client.hpp"
#include <wx/wx.h>
#endif

class MainFrame : public wxFrame {
public:
    MainFrame(wxWindow *parent) {
        wxFrame::Create(parent, wxID_ANY, "Sockets Tester");
        InitializeComponents();
    }

private:
    wxTextCtrl *tcAddress, *tcPort, *tcSend, *tcReceived;
    wxButton *btnConnect, *btnAbort;
    wxCheckBox *cbEnterClears;

    std::weak_ptr<client::client> c;

    void Abort() {
        if (auto cl = c.lock()) {
            cl->stop();
        }
        SetStatusText("", 0);
        SetStatusText("", 1);
        tcReceived->Clear();
    }

    struct ThreadMessage {
        enum {
            connected,
            received,
            error
        } type;
        std::string data;
    };

    void Connect() {
        auto cl = std::make_shared<client::client>(
            [this](const std::string &ep) {
                auto evt = new wxThreadEvent();
                evt->SetPayload(ThreadMessage{ThreadMessage::connected, ep });
                wxQueueEvent(this, evt);
            },
            [this](const std::string &payload) {
                auto evt = new wxThreadEvent();
                evt->SetPayload(ThreadMessage{ ThreadMessage::received, payload });
                wxQueueEvent(this, evt);
            },
            [this](const std::string &error) {
                auto evt = new wxThreadEvent();
                evt->SetPayload(ThreadMessage{ ThreadMessage::error, error });
                wxQueueEvent(this, evt);
            });
        cl->start(tcAddress->GetValue().ToStdString(), tcPort->GetValue().ToStdString());
        c = cl;
    }

    void InitializeComponents() {
        wxPanel *root = new wxPanel(this);

        tcAddress = new wxTextCtrl(root, wxID_ANY);
        tcPort = new wxTextCtrl(root, wxID_ANY);
        btnConnect = new wxButton(root, wxID_ANY, "&Connect");
        tcReceived = new wxTextCtrl(root, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
        tcSend = new wxTextCtrl(root, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        cbEnterClears = new wxCheckBox(root, wxID_ANY, "[ENTER] clears the text box");
        btnAbort = new wxButton(root, wxID_ANY, "&Abort");

        btnConnect->Bind(wxEVT_BUTTON, [this](wxCommandEvent &evt) {
            Abort();
            Connect();
        });

        btnAbort->Bind(wxEVT_BUTTON, [this](wxCommandEvent &evt) {
            Abort();
        });

        tcSend->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent &evt) {
            if (auto cl = c.lock()) {
                cl->send((const char *) tcSend->GetValue().ToUTF8());
                if (cbEnterClears->GetValue())
                    tcSend->Clear();
            }
        });

        wxStaticText *lblAddress = new wxStaticText(root, wxID_ANY, "Address:");
        wxStaticText *lblPort = new wxStaticText(root, wxID_ANY, "Port:");
        wxStaticText *lblReceived = new wxStaticText(root, wxID_ANY, "Received Data:");
        wxStaticText *lblSend = new wxStaticText(root, wxID_ANY, "Send:");

        wxBoxSizer *szrRoot = new wxBoxSizer(wxVERTICAL);
        wxBoxSizer *szrButtons = new wxBoxSizer(wxHORIZONTAL);

        szrButtons->Add(btnAbort, wxSizerFlags(0).Expand().Border());
        szrButtons->AddStretchSpacer(1);
        szrButtons->Add(btnConnect, wxSizerFlags(0).Expand().Border());

        szrRoot->Add(lblAddress, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(tcAddress, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(lblPort, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(tcPort, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(szrButtons, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(lblReceived, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(tcReceived, wxSizerFlags(1).Expand().Border());
        szrRoot->Add(lblSend, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(tcSend, wxSizerFlags(0).Expand().Border());
        szrRoot->Add(cbEnterClears, wxSizerFlags(0).Right().Border());

        root->SetSizer(szrRoot);
        SetSize(640, 480);

        CreateStatusBar(2);
        int widths[] = { 150, -1 };
        GetStatusBar()->SetStatusWidths(2, widths);

        Bind(wxEVT_THREAD, [this](wxThreadEvent &evt) {
            auto &&pl = evt.GetPayload<ThreadMessage>();
            switch (pl.type) {
            case ThreadMessage::connected: {
                SetStatusText("Connection established.", 0);
                SetStatusText(pl.data, 1);
                break;
            }
            case ThreadMessage::received: {
                tcReceived->AppendText(wxString::FromUTF8(pl.data.c_str()));
                tcReceived->AppendText("\r\n");
                break;
            }
            case ThreadMessage::error: {
                SetStatusText("Error.", 0);
                SetStatusText(pl.data, 1);
                break;
            }
            }
        });
    }

    virtual ~MainFrame() {
        Abort();
    }
};

class App : public wxApp {
public:
    bool OnInit() override {
#if defined(WIN32) && !defined(NO_CONSOLE)
        AllocConsole();
        AttachConsole(GetCurrentProcessId());
        SetConsoleOutputCP(CP_UTF8);
        freopen("CON", "w", stderr);
#endif
        auto frame = new MainFrame(nullptr);
        frame->Show();
        return true;
    }
};

wxDECLARE_APP(App);
wxIMPLEMENT_APP(App);
