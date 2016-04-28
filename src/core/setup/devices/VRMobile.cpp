#include "VRMobile.h"
#include "VRSignal.h"
#include "core/utils/VRFunction.h"
#include "core/utils/toString.h"
#include "core/networking/VRNetworkManager.h"
#include "core/networking/VRSocket.h"
#include "core/objects/VRTransform.h"
#include "addons/CEF/CEF.h"
#include "core/scene/VRSceneManager.h"
#include "core/scene/VRScene.h"
#include "core/utils/VRLogger.h"
#include <boost/bind.hpp>

OSG_BEGIN_NAMESPACE;
using namespace std;

VRMobile::VRMobile(int port) : VRDevice("mobile") {
    soc = VRSceneManager::get()->getSocket(port);
    this->port = port;
    cb = new VRHTTP_cb( "Mobile_server_callback", boost::bind(&VRMobile::callback, this, _1) );
    soc->setHTTPCallback(cb);
    soc->setType("http receive");
}

VRMobile::~VRMobile() {
    //cout << "~VRMobile " << getName() << endl;
}

VRMobilePtr VRMobile::create(int p) {
    auto d = VRMobilePtr(new VRMobile(p));
    d->initIntersect(d);
    d->clearSignals();
    return d;
}

VRMobilePtr VRMobile::ptr() { return static_pointer_cast<VRMobile>( shared_from_this() ); }

void VRMobile::callback(void* _args) { // TODO: implement generic button trigger of device etc..
    //args->print();
	HTTP_args* args = (HTTP_args*)_args;

	int button, state;
	button = state = 0;

    if (args->websocket) {
        button = args->ws_id;
        state = 0;
        setMessage(args->ws_data);
        if (args->ws_data.size() == 0) return;
    } else {
        if (args->params->count("button") == 0) { cout << "VRMobile::callback warning, no button passed\n"; return; }
        if (args->params->count("state") == 0) { cout << "VRMobile::callback warning, no state passed\n"; return; }
        if (args->params->count("message")) setMessage((*args->params)["message"]);

        button = toInt((*args->params)["button"]);
        state = toInt((*args->params)["state"]);
    }

    VRLog::log("net", "VRMobile::callback button: " + toString(button) + " state: " + toString(state) + "\n");
    change_button(button, state);
}

void VRMobile::answerWebSocket(int id, string msg) {
    soc->answerWebSocket(id, msg);
}

void VRMobile::clearSignals() {
    VRDevice::clearSignals();

    addSignal( 0, 0)->add( getDrop() );
    addSignal( 0, 1)->add( addDrag( getBeacon() ) );
}

void VRMobile::setPort(int port) { this->port = port; soc->setPort(port); }
int VRMobile::getPort() { return port; }

void VRMobile::updateMobilePage() {
    return; //TODO: this induces a segfault when closing PolyVR
    string page = "<html><body>";
    for (auto w : websites) page += "<a href=\"" + w.first + "\">" + w.first + "</a>";
    page += "</body></html>";
    if (websites.size()) soc->addHTTPPage(getName(), page);
    else soc->remHTTPPage(getName());
}

void VRMobile::addWebSite(string uri, string website) {
    websites[uri] = website;
    soc->addHTTPPage(uri, website);
    updateMobilePage();
}

void VRMobile::remWebSite(string uri) {
    if (websites.count(uri)) websites.erase(uri);
    soc->remHTTPPage(uri);
    updateMobilePage();
}

void VRMobile::updateClients(string path) { CEF::reloadScripts(path); }

OSG_END_NAMESPACE;
