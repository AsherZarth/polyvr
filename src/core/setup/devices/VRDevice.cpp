#include "VRDevice.h"
#include "VRSignal.h"
#include "core/utils/VRFunction.h"
#include "core/scene/VRSceneManager.h"
#include <libxml++/nodes/element.h>
#include <boost/bind.hpp>

OSG_BEGIN_NAMESPACE;

VRDevice::VRDevice(string _type) : VRAvatar(_type) {
    type = _type;
    setName(_type);

    store("type", &type);
    store("name", &name);
}

VRDevice::~VRDevice() {}

VRDevicePtr VRDevice::create(string type) {
    auto d = VRDevicePtr(new VRDevice(type));
    d->initIntersect(d);
    return d;
}

VRDevicePtr VRDevice::ptr() { return static_pointer_cast<VRDevice>( shared_from_this() ); }

VRSignalPtr VRDevice::signalExist(int key, int state) {
    stringstream ss;
    ss << "on_" << name << "_" << key << "_" << state;
    string sig_name = ss.str();
    if ( callbacks.count(sig_name) ==  0) return 0;
    return callbacks[sig_name];
}

VRSignalPtr VRDevice::createSignal(int key, int state) {
    stringstream ss;
    ss << "on_" << name << "_" << key << "_" << state;
    string sig_name = ss.str();

    if ( callbacks.count(sig_name) ==  0) callbacks[sig_name] = VRSignal::create(ptr());
    VRSignalPtr sig = callbacks[sig_name];
    sig->setName(sig_name);
    return sig;
}

void VRDevice::triggerSignal(int key, int state) {
    VRSignalPtr sig = signalExist(key, state);
    if (sig) {
        sig->triggerPtr<VRDevice>();
        if (sig->doUpdate()) addUpdateSignal(sig);
    }
}

VRSignalPtr VRDevice::getToEdgeSignal() { return 0; }
VRSignalPtr VRDevice::getFromEdgeSignal() { return 0; }

void VRDevice::addUpdateSignal(VRSignalPtr sig) {
    activatedSignals[sig.get()] = sig;
}

void VRDevice::remUpdateSignal(VRSignalPtr sig, VRDeviceWeakPtr dev) {
    auto k = sig.get();
    if (activatedSignals.count(k) == 0) return;
    activatedSignals[k]->triggerPtr<VRDevice>();
    activatedSignals.erase(k);
}

void VRDevice::updateSignals() {
    for (auto a : activatedSignals) a.second->triggerPtr<VRDevice>();
}

void VRDevice::clearSignals() {
    callbacks.clear();
    activatedSignals.clear();
    deactivationCallbacks.clear();
}

string VRDevice::getType() { return type; }

// signal event setup
VRSignalPtr VRDevice::addSignal(int key, int state) {
    VRSignalPtr sig = createSignal(key, state);
    BStates[key] = false;
    return sig;
}

// signal activation setup
VRSignalPtr VRDevice::addToggleSignal(int key) { // TODO: this is not ok, an active signal may also be used by someone who does not want it to be active!!
    BStates[key] = false;

    // activation signal
    VRSignalPtr sigA = createSignal(key, 1);
    sigA->setUpdate(true);

    // deactivation signal
    VRSignalPtr sigB = createSignal(key, 0);
    auto fkt = VRFunction<VRDeviceWeakPtr>::create("Device_remUpdate", boost::bind(&VRDevice::remUpdateSignal, ptr(), sigA, _1)); // TODO: check if passing ptr() induces a ptr cycle!!
    sigB->add( fkt );
    deactivationCallbacks[sigA.get()] = fkt;

    return sigA;
}

void VRDevice::addSignal(VRSignalPtr sig, int key, int state) {
    stringstream ss;
    ss << "on_" << name << "_" << key << "_" << state;
    sig->setName(ss.str());
    callbacks[sig->getName()] = sig;
    BStates[key] = false;
}

VRSignalPtr VRDevice::addSlider(int key) {
    VRSignalPtr sig = createSignal(key, 0);
    SStates[key] = 0;
    return sig;
}

void VRDevice::change_button(int key, int state) {
    if (BStates.count(key) == 0) BStates[key] = state;
    sig_key = key;
    sig_state = state;
    BStates[key] = state;
    triggerSignal(key, state);
    triggerSignal(-1, state);
}

void VRDevice::change_slider(int key, float state) {
    if (SStates.count(key) == 0) SStates[key] = state;
    SStates[key] = state;
    sig_key = key;
    sig_state = state;
    if (abs(state) > 0.001) triggerSignal(key, 0);
}

int VRDevice::key() { return sig_key; }
int VRDevice::getState() { return sig_state; }
string VRDevice::getMessage() { return message; }
void VRDevice::setMessage(string s) { message = s; }

void VRDevice::b_state(int key, int* b_state) { if (BStates.count(key)) *b_state = BStates[key]; }
int VRDevice::b_state(int key) { if (BStates.count(key)) return BStates[key]; else return -1; }

void VRDevice::s_state(int key, float* s_state) { if (SStates.count(key)) *s_state = SStates[key];}
float VRDevice::s_state(int key) { if (SStates.count(key)) return SStates[key]; else return 0; }

void VRDevice::setTarget(VRTransformPtr e) { target = e; }
VRTransformPtr VRDevice::getTarget() { return target.lock(); }

map<string, VRSignalPtr> VRDevice::getSignals() { return callbacks; }
VRSignalPtr VRDevice::getSignal(string name) { if (callbacks.count(name)) return callbacks[name]; else return 0; }

void VRDevice::printMap() {
    cout << "\nDevice " << name << " signals:";
    for (auto c : callbacks) cout << "\n Signal " << c.second->getName();
    for (auto b : BStates) cout << "\n BState key " << b.first << " " << b.second;
    for (auto s : SStates) cout << "\n SState key " << s.first << " " << s.second;
    cout << endl;
}

void VRDevice::setSpeed(Vec2f s) { speed = s; }
Vec2f VRDevice::getSpeed() { return speed; }

OSG_END_NAMESPACE;
