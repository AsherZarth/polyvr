#ifndef VRStroke_H_INCLUDED
#define VRStroke_H_INCLUDED

#include "VRGeometry.h"
#include "core/objects/VRObjectFwd.h"
#include "core/math/VRMathFwd.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

class VRStroke : public VRGeometry {
    public:
        enum CAP {
            NONE,
            ARROW
        };

    private:
        vector<pathPtr> paths;

        int mode = -1;
        vector<Vec3f> profile;
        bool closed = false;
        bool doColor = true;

        CAP cap_beg = NONE;
        CAP cap_end = NONE;

        VRGeometryPtr strewGeo = 0;

    public:
        VRStroke(string name);

        static VRStrokePtr create(string name = "None");
        VRStrokePtr ptr();

        void setPath(pathPtr p);
        void addPath(pathPtr p);
        void setPaths(vector<pathPtr> p);

        vector<pathPtr>& getPaths();

        void strokeProfile(vector<Vec3f> profile, bool closed, bool doColor = true, CAP l = NONE, CAP r = NONE);
        void strokeStrew(VRGeometryPtr geo);

        void update();
};

OSG_END_NAMESPACE;

#endif // VRStroke_H_INCLUDED
