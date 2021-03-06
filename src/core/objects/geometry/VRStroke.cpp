#include "VRStroke.h"
#include "VRGeoData.h"
#include "core/math/path.h"
#include "core/objects/material/VRMaterial.h"

#include <OpenSG/OSGMatrixUtility.h>
#include <OpenSG/OSGGeoProperties.h>
#include <OpenSG/OSGTriangleIterator.h>

OSG_BEGIN_NAMESPACE;
using namespace std;

VRStroke::VRStroke(string name) : VRGeometry(name) { }
VRStrokePtr VRStroke::create(string name) { return shared_ptr<VRStroke>(new VRStroke(name) ); }
VRStrokePtr VRStroke::ptr() { return static_pointer_cast<VRStroke>( shared_from_this() ); }

void VRStroke::setPath(pathPtr p) {
    paths.clear();
    paths.push_back(p);
}

void VRStroke::addPath(pathPtr p) { paths.push_back(p); }
void VRStroke::setPaths(vector<pathPtr> p) { paths = p; }
vector<pathPtr>& VRStroke::getPaths() { return paths; }

void VRStroke::strokeProfile(vector<Vec3f> profile, bool closed, bool doColor, CAP l, CAP r) {
    mode = 0;
    this->profile = profile;
    this->closed = closed;
    this->doColor = doColor;
    cap_beg = l;
    cap_end = r;

    Vec3f pCenter;
    for (auto p : profile) pCenter += p;
    pCenter *= 1.0/profile.size();

    VRGeoData data;
    bool doCaps = closed && profile.size() > 1;
    Vec3f z = Vec3f(0,0,1);

    clearChildren();
    for (auto path : paths) {
        vector<Vec3f> pnts = path->getPositions();
        vector<Vec3f> directions = path->getDirections();
        vector<Vec3f> up_vectors = path->getUpvectors();
        vector<Vec3f> cols = path->getColors();

        Vec3f _p;
        for (uint j=0; j<pnts.size(); j++) {
            Vec3f p = pnts[j];
            Vec3f n = directions[j];
            Vec3f u = up_vectors[j];
            Vec3f c = cols[j];

            Matrix m;
            MatrixLookAt(m, Vec3f(0,0,0), n, u);

            bool begArrow1 = (j == 0) && (cap_beg == ARROW);
            bool begArrow2 = (j == 1) && (cap_beg == ARROW);
            bool endArrow1 = (j == pnts.size()-2) && (cap_end == ARROW);
            bool endArrow2 = (j == pnts.size()-1) && (cap_end == ARROW);

            // add new profile points and normals
            for (Vec3f pos : profile) {
                if (endArrow1 || begArrow2) pos += (pos-pCenter)*2.5;
                if (endArrow2 || begArrow1) pos = pCenter;
                m.mult(pos, pos);

                Vec3f norm = pos; norm.normalize();
                if (!doColor) data.pushVert(p+pos, norm);
                else data.pushVert(p+pos, norm, c);
            }

            if (j==0) continue;

            if (profile.size() == 1) data.pushLine();
            else { // add quad
                for (uint k=0; k<profile.size()-1; k++) {
                    int N1 = data.size() - 2*profile.size() + k;
                    int N2 = data.size() -   profile.size() + k;
                    data.pushQuad(N1, N2, N2+1, N1+1);
                }

                if (closed) {
                    int N0 = data.size() - 2*profile.size();
                    int N1 = data.size() - profile.size() - 1;
                    int N2 = data.size() - 1;
                    data.pushQuad(N1, N2, N1+1, N0);
                }
            }
        }
    }

    // caps
    if (doCaps) {
        for (uint i=0; i<paths.size(); i++) {
            if (paths[i]->isClosed()) continue;

            vector<Vec3f> pnts = paths[i]->getPositions();
            vector<Vec3f> directions = paths[i]->getDirections();
            vector<Vec3f> up_vectors = paths[i]->getUpvectors();
            vector<Vec3f> cols = paths[i]->getColors();

            if (pnts.size() == 0) { cout << "VRStroke::strokeProfile path size 0!\n"; continue; }

            Matrix m;

             // first cap
            Vec3f p = pnts[0];
            Vec3f n = directions[0];
            Vec3f u = up_vectors[0];
            Vec3f c = cols[0];
            MatrixLookAt(m, Vec3f(0,0,0), n, u);
            Vec3f tmp; m.mult(pCenter, tmp);

            int Ni = data.size();
            if (!doColor) data.pushVert(p + tmp, -n);
            else data.pushVert(p + tmp, -n, c);

            for (uint k=0; k<profile.size(); k++) {
                Vec3f tmp = profile[k];
                m.mult(tmp, tmp);

                if (!doColor) data.pushVert(p + tmp, -n);
                else data.pushVert(p + tmp, -n, c);
            }

            for (uint k=1; k<=profile.size(); k++) {
                int j = k+1;
                if (k == profile.size()) j = 1;
                data.pushTri(Ni, Ni+k, Ni+j);
            }

             // last cap
            int N = pnts.size()-1;
            Ni = data.size();
            p = pnts[N];
            n = directions[N];
            u = up_vectors[N];
            c = cols[N];
            MatrixLookAt(m, Vec3f(0,0,0), n, u);
            m.mult(pCenter, tmp);

            if (!doColor) data.pushVert(p + tmp, n);
            else data.pushVert(p + tmp, n, c);

            for (uint k=0; k<profile.size(); k++) {
                Vec3f tmp = profile[k];
                m.mult(tmp, tmp);

                if (!doColor) data.pushVert(p + tmp, n);
                else data.pushVert(p + tmp, n, c);
            }

            for (uint k=1; k<=profile.size(); k++) {
                int j = k+1;
                if (k == profile.size()) j = 1;
                data.pushTri(Ni, Ni+j, Ni+k);
            }
        }
    }

    data.apply( ptr() );
}

void VRStroke::strokeStrew(VRGeometryPtr geo) {
    if (geo == 0) return;

    mode = 1;
    strewGeo = geo;

    clearChildren();
    for (uint i=0; i<paths.size(); i++) {
        vector<Vec3f> pnts = paths[i]->getPositions();
        for (uint j=0; j<pnts.size(); j++) {
            Vec3f p = pnts[j];
            VRGeometryPtr g = static_pointer_cast<VRGeometry>(geo->duplicate());
            addChild(g);
            g->translate(p);
        }
    }
}

void VRStroke::update() {
    switch (mode) {
        case 0:
            strokeProfile(profile, closed, doColor);
            break;
        case 1:
            strokeStrew(strewGeo);
            break;
        default:
            break;
    }
}

OSG_END_NAMESPACE;
