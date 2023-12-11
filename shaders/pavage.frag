/*
 * This shader was stolen from GLSLsandbox
 * https://www.glslsandbox.com
 *
 * YUV conversion added by vzvca
 *
 * Nov 2023
 */

#ifdef GL_ES
precision highp float;
#endif
uniform float time;
uniform vec2 resolution;
//#extension GL_OES_standard_derivatives : enable
#define iTime time
#define iResolution resolution

uniform int   colorspace;

#define YUV 1
#define RGB 0

const mat4 rgb2yuv = mat4(0.2990, -0.1687,  0.5000, 0.000, // 1st column, R
                          0.5870,  -0.3313,  -0.4187, 0.000, // 2nd column, G
		          0.1140,  0.5000, -0.0813, 0.000, // 3rd column, B
		          0.0000,  0.5000,  0.5000, 1.000);


mat2 testinverse(mat2 m)
{
  return mat2(m[1][1],-m[0][1],-m[1][0], m[0][0]) / (m[0][0]*m[1][1] - m[0][1]*m[1][0]);
}

vec4 HexGrid(vec2 uv, out vec2 id)
{
    uv *= mat2(1.1547,0.0,-0.5773503,1.0);
    vec2 f = fract(uv);
    float triid = 1.0;
    if((f.x+f.y) > 1.0)
    {
        f = 1.0 - f;
     	triid = -1.0;
    }
    vec2 co = step(f.yx,f) * step(1.0-f.x-f.y,max(f.x,f.y));
    id = floor(uv) + (triid < 0.0 ? 1.0 - co : co);
    co = (f - co) * triid * mat2(0.866026,0.0,0.5,1.0);
    uv = abs(co);
    id*=testinverse(mat2(1.1547,0.0,-0.5773503,1.0));
    return vec4(0.5-max(uv.y,abs(dot(vec2(0.866026,0.5),uv))),length(co),co);
}
vec4 TriGrid(vec2 uv, out vec2 id)
{
    float scaler = 0.866026;
    uv *= mat2(1,-1./1.73, 0,2./1.73)*scaler;
    vec3 g = vec3(uv,1.-uv.x-uv.y);
    vec3 _id = floor(g)+0.5;
    g = fract(g);
    float lg = length(g);
    if (lg>1.)
        g = 1.-g;
    vec3 g2 = abs(2.*fract(g)-1.);
    vec2 triuv = (g.xy-ceil(1.-g.z)/3.) * mat2(1,.5, 0,1.73/2.);
    float edge = max(max(g2.x,g2.y),g2.z);
    id = _id.xy;
    id*= mat2(1,.5, 0,1.73/2.);
    id.xy += sign(lg-1.)*0.1;
    return vec4(((1.0-edge)*0.43)/scaler,length(triuv),triuv);
}
float hbar(vec2 p, float nline, float t)
{
return 0.5+sin((p.y*nline)+t)*0.5;
}
float smin( float a, float b, float k )
{
float h = clamp( 0.5 + 0.5*(b-a)/k, 0.0, 1.0 );
return mix( b, a, h ) - k*h*(1.0-h);
}


mat2 _rot(float th)
{
	vec2 a = sin(vec2(1.5707963, 0) + th);
	return mat2(a.xy, -a.y, a.x);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    float tt = iTime;
    vec2 uv = (fragCoord.xy - 0.5 * iResolution.xy) / iResolution.y;
    uv *= _rot(sin(uv.x*uv.y)*0.25+iTime*0.1);
    vec2 id = vec2(0.0);
    vec2 id2 = vec2(0.0);
    float zoom = 10.;
    zoom += sin(uv.x*2.0+tt) * 0.5;
    vec4 h = HexGrid(uv*zoom, id)*0.9;
    vec4 h2 = TriGrid(uv*zoom, id2)*1.4;
    h.x = min(h.x+0.5, h2.x*2.75);
    float vvv1 = abs(h2.x-h.x)*0.4;
    float vvv2 = abs(h2.y-h.y)*0.25;
//h.x = min(h.x,h2.x);
	
	h.x = smin(h.x,h2.y-0.05,0.8);
	
    id = mix(id,id2,0.5);
    float vvv = min(vvv1,vvv2)*(1.75+sin(time*1.5+length(id*16.0)));
    vec3 shapecol = vec3(0.125, 0.275, 0.155)*.95;
    vec3 shapecol2 = vec3(0.83,.95,.83) * 4.0;
    shapecol = mix(shapecol,shapecol2,vvv);
    float cm = 1.0 + pow(abs(sin(length(id)*.25 + tt*0.65)), 64.0);
    cm *= 1.0 + (hbar(h.zw,100.0,tt*12.0)*0.1);
    shapecol *= cm;
    vec3 bordercol = shapecol;
    vec3 finalcol = mix(bordercol*0.2,shapecol,smoothstep(-0.05, 0.045, h.x));
    fragColor.xyz =finalcol*0.8;
    fragColor.w = 1.0;
    if (colorspace == YUV) {
       fragColor = rgb2yuv*fragColor;
    }
}
void main(void)
{
    mainImage(gl_FragColor, gl_FragCoord.xy);
}
