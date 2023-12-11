/*
 * Original shader from: https://www.shadertoy.com/view/tdjczK
 */


#ifdef GL_ES
precision mediump float;
#endif

// glslsandbox uniforms
uniform float time;
uniform vec2 resolution;

// shadertoy emulation
#define iTime time
#define iResolution resolution
const vec4 iMouse = vec4(0.);

uniform int   colorspace;

#define YUV 1
#define RGB 0

const mat4 rgb2yuv = mat4(0.2990, -0.1687,  0.5000, 0.000, // 1st column, R
                          0.5870,  -0.3313,  -0.4187, 0.000, // 2nd column, G
		          0.1140,  0.5000, -0.0813, 0.000, // 3rd column, B
		          0.0000,  0.5000,  0.5000, 1.000);

// Emulate some GLSL ES 3.x
#define round(x) (floor((x) + 0.5))

// --------[ Original ShaderToy begins here ]---------- //
#define CAMERA_RANGE 8.
#define CAMERA_TIME_SCALE .1
#define SHOW_CARS true
#define COLOR_WINDOW vec3(1., .9, .45)
#define COLOR_WINDOW_TINT vec3(0.9, .3, .1)
#define COLOR_BUILDING_BASE vec3(0.3,.5, .1)
#define COLOR_ROAD_DELIMETER vec3(0.87, .72, .14)
#define COLOR_ROAD vec3(0.1)
#define COLOR_ROAD_SIDEWALK vec3(.7)
#define COLOR_CAR_ROOF vec3(.3)
#define MAX_STEPS 256
#define MIN_DISTANCE 0.0001
#define MAX_DISTANCE 10.
#define defaultBaseSize .3
#define defaultBaseSpacing 2.5
#define bounds vec3(7.0, 7.0, 0.)
#define BLD_RECT 1.
#define BLD_HEX 2.
#define BLD_TUBE 4.
#define OBJ_FLOOR 3.
#define OBJ_CAMERA 5.
#define OBJ_DOME 6.
#define OBJ_CAR 7.
#define BULDING_BASE_SIZE .4
#define PI 3.14

vec2 mouse = vec2(0.);

float CELL_SIZE = (defaultBaseSize + defaultBaseSpacing/2.);
float HALF_CELL_SIZE = 0.;

struct Camera {
    float z;
    float x;
    float y;
    float verticalAngle;
    float horizontalAngle;
    float rotation;
};

Camera camera = Camera(0., 0., 0., 0., 0., 0.);

bool isScripted = true;

// #iUniform float my_scalar = 1.0 in { 0.0, 5.0 } // This will expose a slider to edit the value



struct DistBuilding {
    float d;
    vec3 size;
    float height;
    vec3 q1;
    float objId;
};

struct DistResult {
    float d;
    vec3 id;
    vec3 q1;
    vec3 p;
    float h;
    float obj;
    DistBuilding building;
    DistBuilding cars;
};

struct TraceResult {
    vec3 id;
    float dt;
    float ds;
    float h;
    vec3 p;
    vec3 q1;
    DistResult dist;
    float obj;
};




float hexDist(vec2 uv) {
    uv = abs(uv);
    return max(uv.x, dot(uv, normalize(vec2(1., 1.73))));
}


mat2 rot2d(float a) {
    float ca = cos(a);
    float sa = sin(a);
    return mat2(vec2(sa, ca), vec2(-ca, sa));
}

float n21(vec2 n) {
    return fract(sin(dot(n, vec2(12.9898, 4.1414))) * 43758.5453);
}

float smoothNoise(vec2 uv) {
    vec2 lv = smoothstep(0., 1., fract(uv));
    vec2 id = floor(uv);

    float bl = n21(id);
    float br = n21(id + vec2(1.,0.));
    float b = mix(bl, br, lv.x);

    float tl = n21(id + vec2(0.,1.));
    float tr = n21(id + vec2(1.,1.));
    float t = mix(tl, tr, lv.x);

    float n = mix(b, t, lv.y);
    return n;
}

float noise(vec2 uv, int level) {
    float n = 0.;
    float d = 1.;
    if (level > 0) {
	    n += smoothNoise(uv * 4.);
    }
    if (level > 1) {
	    n += smoothNoise(uv * 8.) * .5;
        d += .5;
    }
    if (level > 2) {
    	n += smoothNoise(uv * 16.) * .25;
        d += .25;
    }
    if (level > 3) {
	    n += smoothNoise(uv * 32.) * .125;
        d += .125;
    }
    if (level > 4) {
	    n += smoothNoise(uv * 64.) * .025;
        d += .0625;
    }
    return n / d;
}

vec3 getColorById(vec3 id) {
    float n = max(.2, n21(vec2(id.x, id.y)));
    vec3 rcol = vec3(n, fract(n*567.433), fract(n*1689.33));
    return rcol;
}

vec3 withMouse(vec3 p) {
        vec2 mouse = iMouse.xy/iResolution.xy;
        float a = max(mouse.y, .505) * 3.14 * 2.;
        p.yz *= rot2d(a);
        float aa = mouse.x * 3.14 * 2.;
        p.xy *= rot2d(aa);
        return p;
}

vec3 setSceneAngle(vec3 p) {
    vec2 mouse = iMouse.xy/iResolution.xy;
    p.yz *= rot2d(PI);
    p.xy *= rot2d(camera.horizontalAngle);
    p.x += HALF_CELL_SIZE;
    p.y += HALF_CELL_SIZE;
    // p.xy -= .77;
    p += vec3(camera.x, camera.y, camera.z);
    return p;
}

float sdBox( vec3 p, vec3 b )
{
  vec3 q = abs(p) - b;
  return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);
}

float sdCappedCylinder( vec3 p, float h, float r )
{
  vec2 d = abs(vec2(length(p.xz),p.y)) - vec2(h,r);
  return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}


float sdHexPrism( vec3 p, vec2 h )
{
  const vec3 k = vec3(-0.8660254, 0.5, 0.57735);
  p = abs(p);
  p.xy -= 2.0*min(dot(k.xy, p.xy), 0.0)*k.xy;
  vec2 d = vec2(
       length(p.xy-vec2(clamp(p.x,-k.z*h.x,k.z*h.x), h.x))*sign(p.y-h.x),
       p.z-h.y );
  return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}

DistBuilding distBuilding(vec3 q1, vec3 id) {
    DistBuilding res;
    float n = n21(id.xy);
    float n1 = fract(n*4553.33);
    float n2 = fract(n*145.33);

    float obj = BLD_RECT;

    if (n1 > .7) {
        obj = BLD_HEX;
    } else if (n2 > .8) {
        obj = BLD_TUBE;
    }

    float baseSize = defaultBaseSize;

    float h = baseSize;

    if (obj == BLD_TUBE && n1 < .2 ) {
        n = (sin(q1.x + (n1*50.)) * .5 + .5);
    }

    float ah = n * .5;

    h += ah;
    q1.z -= ah;
    q1.z -= defaultBaseSize;

    float d = 0.;



    vec3 size = vec3(baseSize, baseSize, h);

    if (obj == BLD_HEX) {
        d = sdHexPrism(q1, vec2(size.x, size.z));
    } else if (obj == BLD_TUBE) {
        float tmp = q1.z;
        q1.z = q1.y;
        q1.y = tmp;
        d = sdCappedCylinder(q1, baseSize, size.z);
    } else {
        if (n1 > .3) {
            size.x *= .5;
            size.y *= 1.5;
        } else if (n2 > .5) {
            size.y *= .5;
            size.x *= 1.5;
        }

        if (n < .6) {
            vec3 q2,nsize, nsize3, q3, d3;
            if (n2 < .2 && size.x == size.y) {
                d = sdBox(q1, size);
                q2 = vec3(q1.x, q1.y, q1.z - size.z);
                nsize = vec3(size.xy/1.5, size.z*2.);
                q3 = vec3(q1.x, q1.y, q1.z - size.z - size.z / 1.8);
                nsize3 = vec3(size.xy/(1.5*1.5), size.z*2. + size.z / 1.8);
            } else {
                q1 += vec3(0.1, -0.08, 0.);
                d = sdBox(q1, size);
                float extraH = size.z + size.z * n1;
                q2 = vec3(q1.x - .18, q1.y + .18, q1.z - extraH);
                nsize = vec3(size.xy, size.z + extraH);
                if (n1 > .4) {
                    nsize.xy = nsize.yx;
                }
            }

            float d2 = sdBox(q2, nsize);
            if (d2 < d) {
                q1 = q2;
                size = nsize;
                d = d2;
            }

            if (nsize3.x != 0.) {
                float d3 = sdBox(q3, nsize3);
                if (d3 < d) {
                    q1 = q3;
                    size = nsize3;
                    d = d3;
                }
            }

        } else {
            d = sdBox(q1, size);
        }
    }

    res.d = d;
    res.q1 = q1;
    res.size = size;
    res.objId = obj;
    res.height = size.z;

    return res;
}

DistBuilding distCars(vec3 po) {
    DistBuilding res;


    float carSize = .02;

    po -= vec3(vec2(HALF_CELL_SIZE), carSize / 2.);

    vec3 baseSpacing = vec3(CELL_SIZE);
    vec3 rc1 = vec3(baseSpacing);
    vec3 direction = vec3(-1, 0., 0.);

    float t = iTime * 2.;

    vec3 p = po;

    p.x -= t;

    vec3 id = round(p/rc1);
    if (mod(id.y, 2.) == 0.) {
        p = po;
        p.x += t;
        id = round(p/rc1);
        direction.x = 1.;
    }
    float n = n21(id.xy);

    vec3 q1 = p - rc1*vec3(id.xy, 0.);

    q1.x -= (sin(iTime*7. * n)*.5 + .5)*HALF_CELL_SIZE;
    q1.y -= (n - .5)/6.;

    float d = sdBox(q1, vec3(carSize*2., carSize, carSize));

    p = po;

    p.y -= t;
    vec3 direction2 = vec3(0., -1., 0.);

    id = round(p/rc1);
    if (mod(id.x , 2.) == 0.) {
        p = po;
        p.y += t;
        id = round(p/rc1);
        direction2.y = 1.;
    }
    n = n21(id.xy);

    q1 = p - rc1*vec3(id.xy, 0.);

    q1.y -= (sin(iTime*5. * n)*.5 + .5)*HALF_CELL_SIZE; //(n - .5)/CELL_SIZE;
    q1.x -= (n - .5)/6.;

    float d1 = sdBox(q1, vec3(carSize, carSize * 2., carSize));

    res.d = min(d, d1);
    res.q1 = d1 > d ? direction : direction2;
    return res;
}


DistResult getDist(vec3 p) {
    vec3 baseSpacing = vec3(defaultBaseSize + defaultBaseSpacing/2.);

    vec3 l = bounds;
    vec3 rc1 = vec3(baseSpacing);

    vec3 id = round(p/rc1);

    vec3 q1 = p - rc1 * clamp(id, -l, l);

    DistBuilding building = distBuilding(q1, id);

    float d = building.d;
    float obj = building.objId;
    q1 = building.q1;

    // d = 1000000.;
    float floord = p.z;

    if (floord < d) {
        obj = OBJ_FLOOR;
    }

    d = min(d, floord);

    float skyD = -(length(p) - 30.);

    if (skyD < d) {
        obj = OBJ_DOME;
        d = skyD;
    }
    DistBuilding cars;
    if (SHOW_CARS) {
        cars = distCars(p);
        if (cars.d < d) {
            d = cars.d;
            obj = OBJ_CAR;
        }
    }

    // if (!isScripted) {
    //     vec3 cameraQ = vec3(-p.x + HALF_CELL_SIZE, -p.y + HALF_CELL_SIZE, p.z-.3) + vec3(camera.x, camera.y, camera.z);
    //     cameraQ.xy *= rot2d(-camera.horizontalAngle);
    //     float cd = sdBox(cameraQ, vec3(.1, .2, .1));
    //     if (cd < d) {
    //         p = cameraQ;
    //         obj = OBJ_CAMERA;
    //     }

    //     d = min(d, cd);
    // }


    DistResult r;
    r.d = d;
    r.id = id;
    r.q1 = q1;
    r.building = building;
    r.cars = cars;
    r.p = p;
    r.obj = obj;
    return r;
}

TraceResult trace(vec3 ro, vec3 rd) {
    float ds = 0.,dt;
    float n;
    vec3 id, shift;
    vec3 p;
    vec3 q1;

    DistResult dist;


    for(int i = 0 ; i < MAX_STEPS ; i++) {
        p = ro + rd * ds;

        if (isScripted) {
            p = setSceneAngle(p);
        } else {
            p = withMouse(p);
        }

        dist = getDist(p);

        dt = dist.d;
        id = dist.id;
        q1 = dist.q1;

        ds += dt * .4;

        if (abs(dt) < MIN_DISTANCE || dt > MAX_DISTANCE) {
            break;
        }
    }

    TraceResult res;

    res.id = id;
    res.dt = dt;
    res.ds = ds;
    res.p = p;
    res.q1 = q1;
    res.obj = dist.obj;
    res.dist = dist;

    return res;
}

vec3 getNormal(vec3 p, float d) {
    vec2 e = vec2(0.00001, 0.);
    vec3 n = d - vec3(
        getDist(p - e.xyy).d,
        getDist(p - e.yxy).d,
        getDist(p - e.yyx).d
    );
    return normalize(n);
}


vec3 getCubeUV(vec3 p, vec3 normal, vec3 fsize) {
    vec2 cuv = vec2(0.);
    vec2 size = fsize.yz;
    if (normal.z != 0.) {
        // roof
        cuv.xy = vec2(p.x, p.y);
        size = fsize.xy;
        // size.y = size.x;
    }
    if (normal.x != 0.) {
        cuv.xy = vec2(p.y, p.z);
        size = fsize.yz;
    }
    if (normal.y != 0.) {
        cuv = vec2(p.x, p.z);
        size = fsize.xz;
    }

    cuv /= size*2.;

    cuv += vec2(.5, .5);

    float r = size.x / size.y;

    cuv -= vec2((r-1.)*.03, 0.);

    cuv.x *= r;

    return vec3(cuv, r);
}

vec2 getTubeUV(vec3 p, vec3 normal, vec2 size) {
    float x = atan(p.x, p.z) / 6.28;
    float y = p.y/size.y;

    vec2 cuv = vec2(x, y);
    if (normal.z != 0.) {
        cuv = p.xz / size.x;
    }
    return cuv;
}

vec2 getHexUV(vec3 p, vec3 normal, vec2 size) {

    float x = atan(p.x, p.y) / 6.28;
    float y = p.z/size.y;

    vec2 cuv = vec2(x, y);

    if (normal.z != 0.) {
        cuv = p.xy / size.x;
    }

    cuv += vec2(.5, .5);

    return cuv;
}

vec4 allWindowsSkyscraperTexture(vec3 p, vec2 uv, vec3 normal, vec3 bid, float xr, float obj, float w, vec3 size) {
    vec3 col = vec3(0.15);
    vec2 wuv = uv;

    float frameWidth = .03;

    float frame = 0.;

    float fogMultiplier = 0.;

    if (obj == BLD_RECT) {
        if (normal.z == 0.) {
            frame = step(uv.x, frameWidth) +  step((1. - frameWidth) * xr, uv.x) + step((1. - frameWidth), uv.y);
            frame += step(uv.y, frameWidth);
        } else {
            if (size.x > size.y) {
                frame = step(uv.x, frameWidth - .15) +  step((.95 - frameWidth) * xr, uv.x) + step((1. - frameWidth), uv.y);
            } else {
                frame = step(uv.x, frameWidth) +  step((1. - frameWidth) * xr, uv.x) + step((1. - frameWidth), uv.y);
            }
            frame += step(uv.y, frameWidth);
        }
        uv *= 40. * w / xr;
    } else if (obj == BLD_HEX) {
        vec2 huv = uv;
        if (normal.z == 0.) {
            frame = step(fract(huv.x*6. + .53), .1);
            frame += step(1.46, huv.y);
        } else {
            frame = step(1. - frameWidth * 2., hexDist((uv - .5)*rot2d(3.14)));
        }
        float scaleY = 20. * defaultBaseSize / xr;
        uv *= vec2(scaleY*(6.*xr), scaleY);
    } else if (obj == BLD_TUBE) {
        vec2 huv = uv;
        if (normal.z != 0.) {
            float hl = length(huv);
            frame = step(hl, 1.1) - step(hl, 1. - frameWidth * 2.);
        } else {
            frame = step(1. - frameWidth * 2., huv.y);
        }
        float scaleY = 20. * defaultBaseSize / xr;
        uv *= vec2(scaleY*(6.*xr), scaleY);
    }

    col += frame;

    if (normal.z == 0. && frame == 0.) {

        float bn = fract(n21(bid.xy)*567.85);
        float distToBuilding = distance(bid*CELL_SIZE, vec3(camera.x, camera.y, camera.z));

        bool isLight = bn > .6 && distToBuilding > 6. ? true : false;
        col = vec3(0.);
        vec2 id = floor(uv);
        uv = fract(uv);
        float n = n21(id + bid.xy + 22.*floor(normal.xy));
        float borders = (step(uv.x, .3) + step(uv.y, .3));
        if (!isLight && n > .7 && abs(sin(bid.x + bid.y + fract(n*23422.)*110. + iTime/50.)) > fract(n*123.22)) {
            col += COLOR_WINDOW * (1. - borders);
            col += borders * COLOR_WINDOW_TINT;
            fogMultiplier = .3;
        } else {
            if (borders != 0.) {
                col = vec3(0.2);
                if (isLight) {
                    vec2 lights = vec2(sin(wuv.x + iTime + fract(bn * 3322.)*10.), sin(wuv.y + iTime + fract(bn * 3322.)*10.));
                    if (bn > .85) {
                        col.rb += lights;
                    } else {
                        col.rg += lights;
                    }
                }
            }
        }

    }

    return vec4(col, fogMultiplier);
}

vec3 domeTexture(vec3 p) {
    vec3 q1 = p;
    q1.yz *= rot2d(PI);
    p = q1;
    vec3 col = vec3(.01);
    float x = acos(p.y/length(p));
    float y = atan(p.z, p.x) / 6.28;
    vec2 uv = vec2(x, y) + .5;

    float rize = .1 + sin(iTime/6.)*.1;

    vec2 muv = uv*vec2(1., 5.);
    vec2 id = floor(muv);
    muv = fract(muv) - .5;
    muv += vec2(rize, 0.);

    bool isMoon = false;

    if (id.y == 2.) {
        float muvl = length(muv);
        float ml = muvl * 1.5;
        vec3 mc = step(ml, .1) * vec3(noise(5. + muv*4. + iTime/50., 5));
        if (ml > .1) {
            mc += pow(.05 / muvl, 6.0);
        }
        if (ml < .15) {
            isMoon = true;
        }
        col += mc * vec3(.9, .6, .1);
    }

    vec2 suv = uv * vec2(30., 150.);
    vec2 sid = floor(suv);
    suv = fract(suv) - .5;

    float n = n21(sid);
    if (n > .7 && !isMoon) {
        col += step(length(suv + vec2(fract(n*3432.33) - .5, fract(n*78953.2) - .5)), .04*fract(n*123.123));
    }

    return col;
}

vec4 floorTexture(vec3 p, vec3 q1) {
    vec3 col = vec3(0.);

    float fogMultiplier = 0.;

    vec2 uv = mod((p.xy - HALF_CELL_SIZE), CELL_SIZE) / CELL_SIZE - .5;
    vec2 roadUV = mod((p.xy), CELL_SIZE) / CELL_SIZE;
    vec2 blockID = floor(p.xy / CELL_SIZE);

    if (abs(blockID.x) > bounds.x || abs(blockID.y) > bounds.y) {
        return vec4(COLOR_BUILDING_BASE, fogMultiplier);
    }

    float roadX = step(BULDING_BASE_SIZE, roadUV.x) - step(1. - BULDING_BASE_SIZE, roadUV.x);
    float roadY = step(BULDING_BASE_SIZE, roadUV.y) - step(1. - BULDING_BASE_SIZE, roadUV.y);

    float road = max(roadX, roadY);


    col += road * COLOR_ROAD;

    // col.rg = roadUV;
    // col.rg = uv;

    vec2 baseUV = abs(uv);
    col += step(max(baseUV.x, baseUV.y), BULDING_BASE_SIZE*.9) * COLOR_BUILDING_BASE;

    uv = fract((uv + .26)*8.);

    if (roadX == 0.) {
        uv.x /= 4.;
    }
    if (roadY == 0.) {
        uv.y /= 4.;
    }

    float delimeter = step(max(uv.x, uv.y), .1) * road;

    fogMultiplier = delimeter * 1.5;

    col += delimeter * COLOR_ROAD_DELIMETER;

    if (col.x == 0.) {
        col = COLOR_ROAD_SIDEWALK;
    }

    vec2 zebraUV = roadUV * 12.;
    vec2 zebraID = floor(zebraUV);
    zebraUV = fract(zebraUV);

    float n = n21(blockID);

    if (zebraID.x == 3. && road > 0. && n > .7) {
        col += step(fract((zebraUV - .08)*vec2(1., 3.)).y + .1, .4);
    }
    if (zebraID.y == 3. && road > 0. && fract(n*123.33) > .7) {
        col += step(fract((zebraUV - .08)*vec2(3., 1.)).x + .1, .4);
    }

    return vec4(col, fogMultiplier);
}

vec3 carTexture(TraceResult tr, vec3 normal) {
    vec3 col = vec3(0.);
    vec3 dir = tr.dist.cars.q1;

    if (normal.y > 0. && dir.x == 0.) {
        if (dir.y > 0.) {
            col.r += 1.;
        } else {
            col.rgb += 1.;
        }
    } else
    if (normal.y < 0. && dir.x == 0.) {
        if (dir.y < 0.) {
            col.r += 1.;
        } else {
            col.rgb += 1.;
        }
    } else

    if (normal.x > 0. && dir.y == 0.) {
        if (dir.x > 0.) {
            col.r += 1.;
        } else {
            col.rgb += 1.;
        }
    } else
    if (normal.x < 0. && dir.y == 0.) {
        if (dir.x < 0.) {
            col.r += 1.;
        } else {
            col.rgb += 1.;
        }
    } else {
        col = COLOR_CAR_ROOF;
    }

    return col;
}

vec4 getBuildingTexture(TraceResult tr, vec3 normal) {
    vec3 col = vec3(0.);

    vec3 id = tr.id;
    float objId = tr.obj;

    vec3 p = tr.p;

    float baseSize = normal.x == 0. ? tr.dist.building.size.x : tr.dist.building.size.y;

    vec2 size = vec2(baseSize, tr.dist.building.height);

    vec3 cubeUV = getCubeUV(tr.q1, normal, tr.dist.building.size);
    vec2 uv = cubeUV.xy;

    if (objId == BLD_HEX) {
        uv = getHexUV(tr.q1, normal, size);
    }
    if (objId == BLD_TUBE) {
        uv = getTubeUV(tr.q1, normal, size);
    }

    vec4 tc = allWindowsSkyscraperTexture(p, uv, normal, id, cubeUV.z, tr.obj, baseSize, tr.dist.building.size);

    col += tc.rgb;

    return vec4(col, tc.w);
}

vec2 getCameraIterationPosition(float iteration) {
    if (iteration == 0.) {
        return vec2(0.);
    }
    float n = n21(vec2(iteration));

    float m = CAMERA_RANGE;
    return round(vec2((n - .5) * m, (fract(n*113.43) - .5) * m));
}

vec3 cameraNextPosition() {
    float t = iTime * CAMERA_TIME_SCALE;

    vec2 center = vec2(.0);

    float iterationDuration = 1.;

    float iteration = floor(t / iterationDuration) + 1.;
    float stepSize = 1.;

    float cellStepSize = CELL_SIZE*stepSize;

    vec2 prevPosition = getCameraIterationPosition(iteration - 1.)*cellStepSize;
    vec2 nextPosition = getCameraIterationPosition(iteration)*cellStepSize;


    float iterationTime = mod(t, iterationDuration) / iterationDuration;

    float iterationSteps = 2.;
    float stepDuration = iterationDuration / iterationSteps;
    float iterationStep = floor(iterationTime / stepDuration) + 1.;

    float iterationStepTime = mod(iterationTime, stepDuration) / stepDuration;

    vec2 f = (nextPosition - prevPosition) * vec2(iterationStepTime);

    if (iterationStep == 1.) {
        f.y = 0.;
    } else if (iterationStep == 2.) {
        prevPosition.x = nextPosition.x;
        f.x = 0.;
    }


    center = prevPosition + f;

    return vec3(center, iteration*2. - iterationStep);
}

void scriptCamera() {
    float t = iTime / 18.;

    vec3 nextPosition = cameraNextPosition();

    camera.x = nextPosition.x;
    camera.y = nextPosition.y;

    camera.z = .5 - cos(iTime/2.) * .5;

    float verticalA = 1.0;
    float horizA = 0.;
    horizA = sin(t*8.);

    verticalA = 1. - (sin(t*8.)*.02 + .01);

    camera.rotation = sin(iTime) * PI/64.;

    float n = n21(vec2(nextPosition.z + 13.));

    float topDown = step(.2, n);

    horizA  *= topDown;
    verticalA *= topDown;
    camera.z -= (topDown - 1.) * n/2.;

    if (length(mouse.xy - .5) > .1) {
        horizA = PI * mouse.x;
        verticalA = PI/3. * mouse.y;
    }

    camera.horizontalAngle = horizA;
    camera.verticalAngle = verticalA;
}

void setupCamera() {
    camera.x = 0.;
    camera.y = 0.;
    camera.z = 0.;
    camera.rotation = 0.;
    scriptCamera();
}


void mainImage(out vec4 fragColor, in vec2 fragCoords) {

    HALF_CELL_SIZE = CELL_SIZE / 2.;

    mouse = iMouse.xy/iResolution.xy;
    if (mouse.x == 0. || mouse.y == 0.) {
        mouse += .5;
    }
    isScripted = true; //!isKeyDown(Key_Shift);

    setupCamera();

    vec2 uv = ((fragCoords.xy / iResolution.xy) - .5) * vec2(iResolution.x / iResolution.y, 1.);

    vec3 col = vec3(.0);

    float a = 0.;

    vec3 ro = vec3(0., 1. , 0.1);
    vec3 lookat = vec3(0., 1.*camera.verticalAngle, 0.);

    if (!isScripted) {
        ro = vec3(0., 0. , -4.);
        lookat = vec3(0., 0., 0.);
    }

    float zoom = .9;
    vec3 up = vec3(0., 1., 0.);
    up.xy *= rot2d(PI / 2. + camera.rotation);

    vec3 f = normalize(lookat - ro);
    vec3 r = normalize(cross(up, f));
    vec3 u = cross(f, r);

    vec3 c = ro + f * zoom;
    vec3 I = c + uv.x * r + uv.y * u;

    vec3 rd = normalize(I - ro);

    TraceResult tr = trace(ro, rd);
    float fogMultiplier = 0.;

    if (tr.dt < MIN_DISTANCE) {
        float objId = tr.obj;

        vec3 normal;

        if (objId == OBJ_FLOOR) {
            vec4 fc = floorTexture(tr.p, tr.q1);
            col += fc.rgb;
            normal = vec3(0., -1., 0.);
            fogMultiplier = fc.w;
        } else if (objId == OBJ_CAMERA) {
            col += mix(vec3(0., 0., 1.), vec3(1., 0., 0.), (.7 + tr.dist.p.y*10.));
        } else if (objId == OBJ_DOME) {
            col += domeTexture(tr.p);
        } else if (objId == OBJ_CAR) {
            normal = getNormal(tr.p, tr.dt);
            col += carTexture(tr, normal);
            fogMultiplier = 2.;
        } else {
            normal = getNormal(tr.p, tr.dt);
            vec4 bt = getBuildingTexture(tr, normal);
            col += bt.rgb;
            fogMultiplier = bt.w;
        }

    }

    float d = length(tr.p - vec3(camera.x, camera.y, camera.z))/(1. + camera.z/8.);
    float fog = (2.2) - d/1.8;

    fragColor = vec4(min(vec3(1.), col), 1.);
    if (tr.obj != OBJ_DOME) {
        if (fogMultiplier != 0.) {
             fragColor *= fog > -fogMultiplier ?  (1. - (fog < 0. ? abs(fog)/fogMultiplier : 0.)) : fog;
        } else {
            fragColor *= fog;
        }
    }
    if (colorspace == YUV) {
       fragColor = rgb2yuv*fragColor;
    }
}
// --------[ Original ShaderToy ends here ]---------- //

void main(void)
{
    mainImage(gl_FragColor, gl_FragCoord.xy);
}