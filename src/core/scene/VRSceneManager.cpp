#include "VRSceneManager.h"
#include "VRSceneLoader.h"
#include "core/setup/VRSetup.h"
#include "core/setup/windows/VRWindow.h"
#include "core/utils/VRRate.h"
#include "core/utils/VRProfiler.h"
#include "VRScene.h"
#include "core/objects/VRCamera.h"
#include "core/objects/VRLight.h"
#include "core/objects/VRLightBeacon.h"
#include "core/gui/VRGuiManager.h"
#include "core/utils/VRTimer.h"
#include "core/utils/VRGlobals.h"
#include "core/gui/VRGuiSignals.h"
#include "core/gui/VRGuiFile.h"
#include "addons/Semantics/Reasoning/VROntology.h"
#include <OpenSG/OSGSceneFileHandler.h>
#include <gtkmm/main.h>
#include <GL/glut.h>
#include <boost/filesystem.hpp>

OSG_BEGIN_NAMESPACE
using namespace std;

VRSceneManager::VRSceneManager() {
    cout << "Init VRSceneManager..";
	original_workdir = boost::filesystem::current_path().string();
    searchExercisesAndFavorites();

    on_scene_load = VRSignal::create();
    on_scene_close = VRSignal::create();

    VROntology::setupLibrary();
    cout << " done" << endl;
}

VRSceneManager::~VRSceneManager() {}

void VRSceneManager::operator= (VRSceneManager v) {;}

VRSceneManager* VRSceneManager::get() {
    static VRSceneManager* mgr = new VRSceneManager();
    return mgr;
}

void VRSceneManager::loadScene(string path, bool write_protected) {
    if (!boost::filesystem::exists(path)) { cout << "loadScene " << path << " not found" << endl; return; }
    path = boost::filesystem::canonical(path).string();
    if (current) if (current->getPath() == path) return;
    cout << "loadScene " << path << endl;

    newEmptyScene(path);
    VRSceneLoader::get()->loadScene(path);
    current->setFlag("write_protected", write_protected);
    VRGuiSignals::get()->getSignal("scene_changed")->triggerPtr<VRDevice>(); // update gui
}

string VRSceneManager::getOriginalWorkdir() { return original_workdir; }

void VRSceneManager::closeScene() {
    if (current == 0) return;
    VRProfiler::get()->setActive(false);
    on_scene_close->triggerPtr<VRDevice>();
    current = 0;

    VRSetup::getCurrent()->resetViewports();
    VRSetup::getCurrent()->clearSignals();
    VRTransform::dynamicObjects.clear();

    // deactivate windows
    auto windows = VRSetup::getCurrent()->getWindows();
    for (auto w : windows) w.second->setContent(false);

    setWorkdir(original_workdir);
    VRGuiSignals::get()->getSignal("scene_changed")->triggerPtr<VRDevice>(); // update gui
}

void VRSceneManager::setWorkdir(string path) {
	if (path == "") return;
	if (boost::filesystem::exists(path))
		path = boost::filesystem::canonical(path).string();
	boost::filesystem::current_path(path);
}

void VRSceneManager::newEmptyScene(string path) {
    closeScene();
    VRScenePtr scene = VRScenePtr( new VRScene() );
    VRSetup::getCurrent()->setupLESCCAVELights(scene);
    scene->setPath(path);
    setWorkdir(scene->getWorkdir());
    scene->setName(scene->getFileName());
    current = scene;
}

void VRSceneManager::newScene(string path) {
    if (boost::filesystem::exists(path)) path = boost::filesystem::canonical(path).string();
    newEmptyScene(path);

    VRLightPtr headlight = VRLight::create("light");
    headlight->setType("point");
    VRLightBeaconPtr headlight_B = VRLightBeacon::create("Headlight_beacon");
    headlight->setBeacon(headlight_B);
    current->add(headlight);

    VRTransformPtr cam = VRCamera::create("Default");
    headlight->addChild(cam);

    VRTransformPtr user;
    auto setup = VRSetup::getCurrent();
    if (setup) user = setup->getUser();
    if (user) user->addChild(headlight_B);
    else cam->addChild(headlight_B);

    cam->setFrom(Vec3f(0,0,3));
    setScene(current);
}

VRSignalPtr VRSceneManager::getSignal_on_scene_load() { return on_scene_load; }
VRSignalPtr VRSceneManager::getSignal_on_scene_close() { return on_scene_close; }

void VRSceneManager::setScene(VRScenePtr scene) {
    if (!scene) return;
    current = scene;
    VRSetup::getCurrent()->setScene(scene);
    scene->setActiveCamera();
    VRProfiler::get()->setActive(true);

    on_scene_load->triggerPtr<VRDevice>();
    VRGuiSignals::get()->getSignal("scene_changed")->triggerPtr<VRDevice>(); // update gui
}

void VRSceneManager::storeFavorites() {
    string path = original_workdir + "/examples/.cfg";
    ofstream file(path);
    for (auto f : favorite_paths) file << f << endl;
    file.close();
}

void VRSceneManager::addFavorite(string path) {
    for (auto p : favorite_paths) if (p == path) return;
    favorite_paths.push_back(path);
    storeFavorites();
}

void VRSceneManager::remFavorite(string path) {
    favorite_paths.erase(std::remove(favorite_paths.begin(), favorite_paths.end(), path), favorite_paths.end());
    storeFavorites();
}

void VRSceneManager::searchExercisesAndFavorites() {
    favorite_paths.clear();
    example_paths.clear();

    // examples
	vector<string> files = VRGuiFile::listDir("examples");
	for (string file : files) {
		int N = file.size(); if (N < 6) continue;

		string ending = file.substr(N - 4, N - 1);
		if (ending != ".xml" && ending != ".pvr") continue;

		string path = boost::filesystem::canonical("examples/" + file).string();
		example_paths.push_back(path);
	}

    // favorites
    ifstream file( "examples/.cfg" );
    if (!file.is_open()) return;

    string line, path;
    while ( getline (file,line) ) {
        ifstream f(line.c_str());
        if (!f.good()) continue;
		line = boost::filesystem::canonical(line).string();
        favorite_paths.push_back(line);
    }
    file.close();
}

vector<string> VRSceneManager::getFavoritePaths() { return favorite_paths; }
vector<string> VRSceneManager::getExamplePaths() { return example_paths; }

VRScenePtr VRSceneManager::getCurrent() { return current; }

void VRSceneManager::updateScene() {
    if (!current) return;
    VRSetup::getCurrent()->updateActivatedSignals();
    //current->blockScriptThreads();
    current->update();
    //current->allowScriptThreads();
}

void VRSceneManager::update() {
    // statistics
    VRProfiler::get()->swap();
    static VRRate FPS; int fps = FPS.getRate();
    VRTimer timer; timer.start();

    // update GUI
    if (current) current->blockScriptThreads();

    VRTimer t1; t1.start();
    VRGuiManager::get()->updateGtk();
    VRGlobals::GTK1_FRAME_RATE.update(t1);

    VRTimer t4; t4.start();
    updateCallbacks();
    VRGlobals::SMCALLBACKS_FRAME_RATE.update(t4);

    VRTimer t5; t5.start();
    auto setup = VRSetup::getCurrent();
    if (setup) setup->updateTracking(); // tracking
    if (setup) setup->updateDevices(); // device beacon update
    VRGlobals::SMCALLBACKS_FRAME_RATE.update(t5);

    VRTimer t6; t6.start();
    updateScene();
    VRGlobals::SCRIPTS_FRAME_RATE.update(t6);

    if (setup) {
        VRTimer t2; t2.start();
        setup->updateWindows(); //rendering
        VRGlobals::WINDOWS_FRAME_RATE.update(t2);
    }

    VRTimer t3; t3.start();
    VRGuiManager::get()->updateGtk();
    VRGlobals::GTK2_FRAME_RATE.update(t3);

    if (current) current->allowScriptThreads();


    // statistics
    VRGlobals::CURRENT_FRAME++;
    VRGlobals::FRAME_RATE.fps = fps;
    VRTimer t7; t7.start();
    osgSleep(max(16-timer.stop(),0));
    VRGlobals::SLEEP_FRAME_RATE.update(t7);
}

OSG_END_NAMESPACE
