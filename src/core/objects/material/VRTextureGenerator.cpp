#include "VRTextureGenerator.h"
#include "VRPerlin.h"
#include "VRBricks.h"
#include "core/networking/VRSharedMemory.h"

#include <OpenSG/OSGImage.h>
#include <OpenSG/OSGMatrix.h>
#include <OpenSG/OSGMatrixUtility.h>
#include "core/objects/material/VRTexture.h"
#include "core/math/path.h"
#include "core/math/polygon.h"

OSG_BEGIN_NAMESPACE;
using namespace std;

VRTextureGenerator::VRTextureGenerator() {}
VRTextureGenerator::~VRTextureGenerator() {}
shared_ptr<VRTextureGenerator> VRTextureGenerator::create() { return shared_ptr<VRTextureGenerator>(new VRTextureGenerator()); }

void VRTextureGenerator::setSize(Vec3i dim, bool a) { width = dim[0]; height = dim[1]; depth = dim[2]; hasAlpha = a; }
void VRTextureGenerator::setSize(int w, int h, int d) { width = w; height = h; depth = d; }
Vec3i VRTextureGenerator::getSize() { return Vec3i(width, height, depth); };

void VRTextureGenerator::add(string type, float amount, Vec3f c1, Vec3f c2) {
    GEN_TYPE t = PERLIN;
    if (type == "Bricks") t = BRICKS;
    add(t, amount, c1, c2);
}

void VRTextureGenerator::add(GEN_TYPE type, float amount, Vec3f c1, Vec3f c2) {
    Layer l;
    l.amount = amount;
    l.type = type;
    l.c31 = c1;
    l.c32 = c2;
    l.Nchannels = 3;
    layers.push_back(l);
}

void VRTextureGenerator::add(string type, float amount, Vec4f c1, Vec4f c2) {
    GEN_TYPE t = PERLIN;
    if (type == "Bricks") t = BRICKS;
    add(t, amount, c1, c2);
}

void VRTextureGenerator::add(GEN_TYPE type, float amount, Vec4f c1, Vec4f c2) {
    Layer l;
    l.amount = amount;
    l.type = type;
    l.c41 = c1;
    l.c42 = c2;
    l.Nchannels = 4;
    layers.push_back(l);
}

void VRTextureGenerator::drawFill(Vec4f c) {
    Layer l;
    l.type = FILL;
    l.c41 = c;
    layers.push_back(l);
}

void VRTextureGenerator::drawPixel(Vec3i p, Vec4f c) {
    Layer l;
    l.type = PIXEL;
    l.p1 = p;
    l.c41 = c;
    layers.push_back(l);
}

void VRTextureGenerator::drawLine(Vec3f p1, Vec3f p2, Vec4f c, float w) {
    Layer l;
    l.type = LINE;
    l.c31 = p1;
    l.c32 = p2;
    l.c41 = c;
    l.amount = w;
    layers.push_back(l);
}

void VRTextureGenerator::drawPath(pathPtr p, Vec4f c, float w) {
    Layer l;
    l.type = PATH;
    l.p = p;
    l.c41 = c;
    l.amount = w;
    layers.push_back(l);
}

void VRTextureGenerator::drawPolygon(polygonPtr p, Vec4f c, float h) {
    Layer l;
    l.type = POLYGON;
    l.pgon = p;
    l.c41 = c;
    l.amount = h;
    layers.push_back(l);
}

void VRTextureGenerator::applyFill(Vec3f* data, Vec4f c) {
    for (int k=0; k<depth; k++) {
        for (int j=0; j<height; j++) {
            for (int i=0; i<width; i++) {
                int d = k*height*width + j*width + i;
                data[d] = Vec3f(c[0], c[1], c[2])*c[3];
            }
        }
    }
}

void VRTextureGenerator::applyFill(Vec4f* data, Vec4f c) {
    for (int k=0; k<depth; k++) {
        for (int j=0; j<height; j++) {
            for (int i=0; i<width; i++) {
                int d = k*height*width + j*width + i;
                data[d] = c;
            }
        }
    }
}

bool VRTextureGenerator::inBox(Pnt3f& p, Vec3f& s) {
    if (abs(p[0]) > s[0]) return false;
    if (abs(p[1]) > s[1]) return false;
    if (abs(p[2]) > s[2]) return false;
    return true;
}

void VRTextureGenerator::applyPixel(Vec4f* data, Vec3i p, Vec4f c) {
    int d = p[2]*height*width + p[1]*width + p[0];
    int N = depth*height*width;
    if (d >= N || d < 0) { cout << "Warning: applyPixel failed, pixel " << d << " " << p << " " << height << " " << width << " " << depth << " out of range! (buffer size is " << N << ")" << endl; return; }
    data[d] = c;
}

void VRTextureGenerator::applyPixel(Vec3f* data, Vec3i p, Vec4f c) {
    int d = p[2]*height*width + p[1]*width + p[0];
    int N = depth*height*width;
    if (d >= N|| d < 0) { cout << "Warning: applyPixel failed, pixel " << d << " " << p << " " << height << " " << width << " " << depth << " out of range! (buffer size is " << N << ")" << endl; return; }
    data[d] = Vec3f(c[0], c[1], c[2])*c[3] + data[d]*(1.0-c[3]);
}

Vec3i VRTextureGenerator::clamp(Vec3i p) {
    if (p[0] < 0) p[0] = 0;
    if (p[1] < 0) p[1] = 0;
    if (p[2] < 0) p[2] = 0;
    if (p[0] >= width)  p[0] = width-1;
    if (p[1] >= height) p[1] = height-1;
    if (p[2] >= depth)  p[2] = depth-1;
    return p;
}

void VRTextureGenerator::applyLine(Vec3f* data, Vec3f p1, Vec3f p2, Vec4f c, float w) {
    auto upscale = [&](Vec3f& p) {
        p = Vec3f(p[0]*width, p[1]*height, p[2]*depth);
    };

    auto getLeadDim = [](Vec3f d) {
        Vec3i iDs = Vec3i(0,1,2);
        if (abs(d[1]) > abs(d[0]) && abs(d[1]) > abs(d[2])) iDs = Vec3i(1,0,2);
        if (abs(d[2]) > abs(d[0]) && abs(d[2]) > abs(d[1])) iDs = Vec3i(2,0,1);
        return iDs;
    };

    auto BresenhamPixels = [&](Vec3f p1, Vec3f p2) {
        vector<Vec3i> pixels;
        Vec3f d = p2-p1;
        Vec3i pi = Vec3i(p1);
        Vec3i iDs = getLeadDim(d);
        int I = iDs[0];

        Vec2f derr; // slopes
        if (I == 0) derr = Vec2f(abs(d[1]/d[0]), abs(d[2]/d[0]));
        if (I == 1) derr = Vec2f(abs(d[0]/d[1]), abs(d[2]/d[1]));
        if (I == 2) derr = Vec2f(abs(d[0]/d[2]), abs(d[1]/d[2]));

        Vec2f err = derr - Vec2f(0.5,0.5);

        int k = 1;
        if (d[I] < 0) k = -1;

        for (; abs(pi[I]) < abs(p1[I]+d[I]); pi[I] += k) {
            pixels.push_back(pi);
            err += derr;
            if (err[0] >= 0.5) { pi[iDs[1]]++; err[0] -= 1.0; }
            if (err[1] >= 0.5) { pi[iDs[2]]++; err[1] -= 1.0; }
        }

        return pixels;
    };

    upscale(p1);
    upscale(p2);
    Vec3f d = p2-p1;
    Vec3i iDs = getLeadDim(d);
    Vec3f u = Vec3f(0,1,0);
    if (iDs[0] == 1) u = Vec3f(0,0,1);
    Vec3f p3 = d.cross(u);
    Vec3f p4 = d.cross(p3);

    p3.normalize(); p3 *= w*0.5; upscale(p3);
    p4.normalize(); p4 *= w*0.5; upscale(p4);

    auto pixels1 = BresenhamPixels(p1,p2);
    auto pixels2 = BresenhamPixels(-p3,p3);
    auto pixels3 = BresenhamPixels(-p4,p4);

    for (auto pi : pixels1) {
        for (auto pj : pixels2) {
            for (auto pk : pixels3) {
                applyPixel(data, clamp(pi+pj+pk), c);
            }
        }
    }
}

void VRTextureGenerator::applyLine(Vec4f* data, Vec3f p1, Vec3f p2, Vec4f c, float w) { // Bresenham's
    auto upscale = [&](Vec3f& p) {
        p = Vec3f(p[0]*width, p[1]*height, p[2]*depth);
    };

    auto getLeadDim = [](Vec3f d) {
        Vec3i iDs = Vec3i(0,1,2);
        if (abs(d[1]) > abs(d[0]) && abs(d[1]) > abs(d[2])) iDs = Vec3i(1,0,2);
        if (abs(d[2]) > abs(d[0]) && abs(d[2]) > abs(d[1])) iDs = Vec3i(2,0,1);
        return iDs;
    };

    auto BresenhamPixels = [&](Vec3f p1, Vec3f p2) {
        vector<Vec3i> pixels;
        Vec3f d = p2-p1;
        Vec3i iDs = getLeadDim(d);
        int I = iDs[0];

        Vec2f derr; // slopes
        if (I == 0) derr = Vec2f(abs(d[1]/d[0]), abs(d[2]/d[0]));
        if (I == 1) derr = Vec2f(abs(d[0]/d[1]), abs(d[2]/d[1]));
        if (I == 2) derr = Vec2f(abs(d[0]/d[2]), abs(d[1]/d[2]));
        Vec2f err = derr - Vec2f(0.5,0.5);

        Vec3i pi1 = Vec3i(p1);
        Vec3i pi2 = Vec3i(p2);
        if (pi1[I] > pi2[I]) swap(pi1, pi2);
        if (pi1[I] == pi2[I]) pi2[I]++;
        Vec3i pi = pi1;

        for (; pi[I] < pi2[I]; pi[I]++) {
            pixels.push_back(pi);
            err += derr;
            if (err[0] >= 0.5) { pi[iDs[1]]++; err[0] -= 1.0; }
            if (err[1] >= 0.5) { pi[iDs[2]]++; err[1] -= 1.0; }
        }
        return pixels;
    };

    upscale(p1);
    upscale(p2);
    Vec3f d = p2-p1;
    Vec3i iDs = getLeadDim(d);
    Vec3f u = Vec3f(0,1,0);
    if (iDs[0] == 1) u = Vec3f(0,0,1);
    Vec3f p3 = d.cross(u);
    Vec3f p4 = d.cross(p3);

    p3.normalize(); p3 *= w*0.5; upscale(p3);
    p4.normalize(); p4 *= w*0.5; upscale(p4);

    auto pixels1 = BresenhamPixels(p1,p2);
    auto pixels2 = BresenhamPixels(-p3,p3);
    auto pixels3 = BresenhamPixels(-p4,p4);

    for (auto pi : pixels1) {
        for (auto pj : pixels2) {
            for (auto pk : pixels3) {
                applyPixel(data, clamp(pi+pj+pk), c);
            }
        }
    }
}

// TODO: fix holes between curved path segments
void VRTextureGenerator::applyPath(Vec3f* data, pathPtr p, Vec4f c, float w) {
    auto pos = p->getPositions();
    for (uint i=1; i<pos.size(); i++) applyLine(data, pos[i-1], pos[i], c, w);
}

void VRTextureGenerator::applyPath(Vec4f* data, pathPtr p, Vec4f c, float w) {
    auto pos = p->getPositions();
    for (uint i=1; i<pos.size(); i++) applyLine(data, pos[i-1], pos[i], c, w);
}

void VRTextureGenerator::applyPolygon(Vec3f* data, polygonPtr p, Vec4f c, float h) {
    for (int j=0; j<height; j++) {
        for (int i=0; i<width; i++) {
            Vec2f pos = Vec2f(float(i)/width, float(j)/height);
            if (p->isInside(pos)) {
                for (int k=0; k<depth; k++) applyPixel(data, Vec3i(i,j,k), c);
            }
        }
    }
}

void VRTextureGenerator::applyPolygon(Vec4f* data, polygonPtr p, Vec4f c, float h) {
    for (int j=0; j<height; j++) {
        for (int i=0; i<width; i++) {
            Vec2f pos = Vec2f(float(i)/width, float(j)/height);
            if (p->isInside(pos)) {
                for (int k=0; k<depth; k++) applyPixel(data, Vec3i(i,j,k), c);
            }
        }
    }
}

void VRTextureGenerator::clearStage() { layers.clear(); }

VRTexturePtr VRTextureGenerator::compose(int seed) {
    srand(seed);
    Vec3i dims(width, height, depth);

    Vec3f* data3 = new Vec3f[width*height*depth];
    Vec4f* data4 = new Vec4f[width*height*depth];
    for (int i=0; i<width*height*depth; i++) data3[i] = Vec3f(1,1,1);
    for (int i=0; i<width*height*depth; i++) data4[i] = Vec4f(1,1,1,1);
    for (auto l : layers) {
        if (!hasAlpha) {
            if (l.type == BRICKS) VRBricks::apply(data3, dims, l.amount, l.c31, l.c32);
            if (l.type == PERLIN) VRPerlin::apply(data3, dims, l.amount, l.c31, l.c32);
            if (l.type == LINE) applyLine(data3, l.c31, l.c32, l.c41, l.amount);
            if (l.type == FILL) applyFill(data3, l.c41);
            if (l.type == PIXEL) applyPixel(data3, l.p1, l.c41);
            if (l.type == PATH) applyPath(data3, l.p, l.c41, l.amount);
            if (l.type == POLYGON) applyPolygon(data3, l.pgon, l.c41, l.amount);
        }
        if (hasAlpha) {
            if (l.type == BRICKS) VRBricks::apply(data4, dims, l.amount, l.c41, l.c42);
            if (l.type == PERLIN) VRPerlin::apply(data4, dims, l.amount, l.c41, l.c42);
            if (l.type == LINE) applyLine(data4, l.c31, l.c32, l.c41, l.amount);
            if (l.type == FILL) applyFill(data4, l.c41);
            if (l.type == PIXEL) applyPixel(data4, l.p1, l.c41);
            if (l.type == PATH) applyPath(data4, l.p, l.c41, l.amount);
            if (l.type == POLYGON) applyPolygon(data4, l.pgon, l.c41, l.amount);
        }
    }

    img = VRTexture::create();
    auto format = hasAlpha ? OSG::Image::OSG_RGBA_PF : OSG::Image::OSG_RGB_PF;
    if (hasAlpha) img->getImage()->set(format, width, height, depth, 0, 1, 0.0, (const uint8_t*)data4, OSG::Image::OSG_FLOAT32_IMAGEDATA, true, 1);
    else       img->getImage()->set(format, width, height, depth, 0, 1, 0.0, (const uint8_t*)data3, OSG::Image::OSG_FLOAT32_IMAGEDATA, true, 1);
    delete[] data3;
    delete[] data4;
    return img;
}

struct tex_params {
    Vec3i dims;
    int pixel_format = OSG::Image::OSG_RGB_PF;
    int data_type = OSG::Image::OSG_FLOAT32_IMAGEDATA;
    int internal_pixel_format = -1;
};

VRTexturePtr VRTextureGenerator::readSharedMemory(string segment, string object) {
    VRSharedMemory sm(segment, false);

    // add texture example
    /*auto s = sm.addObject<Vec3i>(object+"_size");
    *s = Vec3i(2,2,1);
    auto vec = sm.addVector<Vec3f>(object);
    vec->push_back(Vec3f(1,0,1));
    vec->push_back(Vec3f(1,1,0));
    vec->push_back(Vec3f(0,0,1));
    vec->push_back(Vec3f(1,0,0));*/

    // read texture
    auto tparams = sm.getObject<tex_params>(object+"_size");
    auto vs = tparams.dims;
    auto vdata = sm.getVector<float>(object);

    //cout << "read shared texture " << object << " " << vs << "   " << tparams.pixel_format << "  " << tparams.data_type << "  " << vdata.size() << endl;

    img = VRTexture::create();
    img->getImage()->set(tparams.pixel_format, vs[0], vs[1], vs[2], 0, 1, 0.0, (const uint8_t*)&vdata[0], tparams.data_type, true, 1);
    img->setInternalFormat( tparams.internal_pixel_format );
    return img;
}

OSG_END_NAMESPACE;
