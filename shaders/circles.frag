/*
 * This shader was stolen from GLSLsandbox
 * https://www.glslsandbox.com
 *
 * YUV conversion added by vzvca
 *
 * Nov 2023
 */

/*
 * Original shader from: https://www.shadertoy.com/view/Wl3fzM
 */


#ifdef GL_ES
precision mediump float;
#endif

// glslsandbox uniforms
uniform float time;
uniform vec2 resolution;
uniform vec2 surfaceSize;
varying vec2 surfacePosition;

// shadertoy emulation
#define iTime time
#define iResolution resolution

uniform int   colorspace;

#define YUV 1
#define RGB 0

const mat4 rgb2yuv = mat4(0.2990, -0.1687,  0.5000, 0.000, // 1st column, R
                          0.5870,  -0.3313,  -0.4187, 0.000, // 2nd column, G
		          0.1140,  0.5000, -0.0813, 0.000, // 3rd column, B
		          0.0000,  0.5000,  0.5000, 1.000);

// Emulate some GLSL ES 3.x
float tanh(float x) {
    float ex = exp(2.0 * x);
    return ((ex - 1.) / (ex + 1.));
}

// --------[ Original ShaderToy begins here ]---------- //
// License CC0: Apollian with a twist
// Playing around with apollian fractal

#define TIME            iTime
#define RESOLUTION      iResolution
#define PI              3.141592654
#define TAU             (2.0*PI)
#define L2(x)           dot(x, x)
#define ROT(a)          mat2(cos(a), sin(a), -sin(a), cos(a))
#define PSIN(x)         (0.5+0.5*sin(x))

vec3 hsv2rgb(vec3 c) {
  const vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
  vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
  return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float apollian(vec4 p, float s) {
  float scale = 1.0;

  for(int i=0; i<7; ++i) {
    p        = -1.0 + 2.0*fract(0.5*p+0.5);

    float r2 = dot(p,p);
    
    float k  = s/(r2);
    p       *= k;
    scale   *= k;
  }
  
  return abs(p.y)/scale;
}

float weird(vec2 p) {
  float z = 4.0;
  p *= ROT(TIME*.1);
  float tm = 0.2*TIME;
  float r = 0.5;
  vec4 off = vec4(r*PSIN(tm*sqrt(3.0)), r*PSIN(tm*sqrt(1.5)), r*PSIN(tm*sqrt(2.0)), 0.0);
  vec4 pp = vec4(p.x, p.y, 0.0, 0.0)+off;
  pp.w = 0.125*(1.0-tanh(length(pp.xyz)));
  pp.yz *= ROT(tm);
  pp.xz *= ROT(tm*sqrt(0.5));
  pp /= z;
  float d = apollian(pp, 1.2);
  return d*z;
}

float df(vec2 p) {
  const float zoom = 0.5;
  p /= zoom;
  float d0 = weird(p);
  return d0*zoom;
}

vec3 color(vec2 p) {
  float aa   = 2.0/RESOLUTION.y;
  const float lw = 0.0235;
  const float lh = 1.25;

  const vec3 lp1 = vec3(0.5, lh, 0.5);
  const vec3 lp2 = vec3(-0.5, lh, 0.5);

  float d = df(p);

  float b = -0.125;
  float t = 10.0;

  vec3 ro = vec3(0.0, t, 0.0);
  vec3 pp = vec3(p.x, 0.0, p.y);

  vec3 rd = normalize(pp - ro);

  vec3 ld1 = normalize(lp1 - pp);
  vec3 ld2 = normalize(lp2 - pp);

  float bt = -(t-b)/rd.y;
  
  vec3 bp   = ro + bt*rd;
  vec3 srd1 = normalize(lp1-bp);
  vec3 srd2 = normalize(lp2-bp);
  float bl21= L2(lp1-bp);
  float bl22= L2(lp2-bp);

  float st1= (0.0-b)/srd1.y;
  float st2= (0.0-b)/srd2.y;
  vec3 sp1 = bp + srd1*st1;
  vec3 sp2 = bp + srd2*st1;

  float bd = df(bp.xz);
  float sd1= df(sp1.xz);
  float sd2= df(sp2.xz);

  vec3 col  = vec3(0.0);
  const float ss =15.0;
  
  col       += vec3(1.0, 1.0, 1.0)*(1.0-exp(-ss*(max((sd1+0.0*lw), 0.0))))/bl21;
  col       += vec3(0.5)*(1.0-exp(-ss*(max((sd2+0.0*lw), 0.0))))/bl22;
  float l   = length(p);
  float hue = fract(0.75*l-0.3*TIME)+0.3+0.15;
  float sat = 0.75*tanh(2.0*l);
  vec3 hsv  = vec3(hue, sat, 1.0);
  vec3 bcol = hsv2rgb(hsv);
  col       *= (1.0-tanh(0.75*l))*0.5;
  col       = mix(col, bcol, smoothstep(-aa, aa, -d));  
  col       += 0.5*sqrt(bcol.zxy)*(exp(-(10.0+100.0*tanh(l))*max(d, 0.0)));

  return col;
}

vec3 postProcess(vec3 col, vec2 q)  {
  col=pow(clamp(col,0.0,1.0),vec3(1.0/2.2)); 
  col=col*0.6+0.4*col*col*(3.0-2.0*col);  // contrast
  col=mix(col, vec3(dot(col, vec3(0.33))), -0.4);  // saturation
  col*=0.5+0.5*pow(19.0*q.x*q.y*(1.0-q.x)*(1.0-q.y),0.7);  // vigneting
  return col;
}

void mainImage(out vec4 fragColor, in vec2 fragCoord) {
  vec2 q = fragCoord/RESOLUTION.xy;
  vec2 p = -1. + 2. * q;
  p.x *= RESOLUTION.x/RESOLUTION.y;
	
//p = surfacePosition;
  
  vec3 col = color(p);
  col = postProcess(col, q);
  
  fragColor = vec4(col, 1.0);
  if (colorspace == YUV) fragColor = rgb2yuv*fragColor;
}

// --------[ Original ShaderToy ends here ]---------- //

void main(void)
{
    mainImage(gl_FragColor, gl_FragCoord.xy);
}
