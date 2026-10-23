#ifndef WATER_H
#define WATER_H

#include "utils.h"
#include "detection.h"
#include "sky.h"
#include "clouds.h"
#include "noise.h"

// fresnel - Schlick's approximation
float calculateFresnel(float cosR, float r0) {
  float a = 1.0-cosR;
  float a2 = a*a;
  return r0 + (1.0-r0)*a2*a2*a;
}

// Ray Traced 2.0.0: ondas direcionais somadas (portadas do YSS Craft RTX)
// devolve o gradiente da superfície; só as 3 ondas longas (o vértice só vê 1 amostra por bloco)
vec2 nlWaveGradient(vec2 p, highp float t) {
  float s = t*NL_WATER_WAVE_SPEED*1.25;
  vec2 g = vec2_splat(0.0);
  vec2 d;
  d = normalize(vec2(1.0, 0.4));  g += d*(0.9*0.06)*cos(dot(d,p)*0.9 + s*1.2);
  d = normalize(vec2(-0.6, 1.0)); g += d*(1.7*0.035)*cos(dot(d,p)*1.7 + s*1.6);
  d = normalize(vec2(0.3, -1.0)); g += d*(3.1*0.02)*cos(dot(d,p)*3.1 - s*2.1);
  return g*0.45;
}

// brilho do sol na água: quadrado nítido + halo (como no YSS Craft RTX)
float nlSunGlint(vec3 refl, vec3 L) {
  vec3 sr = normalize(cross(vec3(0.0, 0.0, 1.0), L));
  vec3 su = cross(L, sr);
  vec2 sp = vec2(dot(refl, sr), dot(refl, su));
  float sq = (1.0 - step(0.043, max(abs(sp.x), abs(sp.y)))) * step(0.0, dot(refl, L));
  return sq*1.6 + pow(max(dot(refl, L), 0.0), 80.0)*0.12;
}

vec4 nlWater(
  inout vec4 color, inout vec3 wPos, nl_skycolor skycol, nl_environment env, vec4 COLOR, vec3 viewDir,
  vec3 cPos, vec3 tiledCpos, vec3 gPos, vec3 CAMERA_POS, vec3 light, vec3 torchColor, vec2 lit,
  float fractCposY, float camDist, highp float t
) {

  vec2 bump = vec2_splat(movingNoise2D(gPos.xz + gPos.yy, NL_WATER_WAVE_SPEED*t, 0.6));

  vec2 waveG = nlWaveGradient(gPos.xz, t);

  vec3 nrm;
  if (fractCposY > 0.0) { // top plane
    nrm.xz = bump*NL_WATER_BUMP + waveG;
    nrm.y = -1.0;
    /*if (fractCposY>0.8 || fractCposY<0.9) { // flat plane
    } else { // slanted plane and highly slanted plane
    }*/
  } else { // reflection for side plane
    bump *= 0.5 + 0.5*sin(3.0*t*NL_WATER_WAVE_SPEED + cPos.y*PI_HALF);
    nrm.xz = normalize(viewDir.xz) + bump.y*(1.0-viewDir.xz*viewDir.xz)*NL_WATER_BUMP;
    nrm.y = bump.x*NL_WATER_BUMP;
  }
  nrm = normalize(nrm);

  float cosR = dot(nrm, viewDir);
  viewDir = viewDir - 2.0*cosR*nrm ; // reflect(viewDir, nrm)

  vec3 waterRefl = nlRenderSky(skycol, env, viewDir, t, false);

  #if defined(NL_CLOUD_AURORA_REFLECTION)
    if (viewDir.y < 0.0) {
      vec4 cloudRefl = nlCloudAuroraReflection(skycol, env, viewDir, wPos, CAMERA_POS, t);
      waterRefl = mix(waterRefl, cloudRefl.rgb, cloudRefl.a);
    }
  #endif

  // torch light reflection
  float tc = 0.5+0.5*sin(16.0*viewDir.x)*sin(16.0*viewDir.z);
  waterRefl += torchColor*NL_TORCHLIGHT_INTENSITY*lit.x*tc*tc;

  // mask sky reflection under shade
  if (!env.end) {
    waterRefl *= 0.05 + lit.y*1.14;
  }

  // brilho do sol/lua refletido na água
  if (!env.end && !env.nether) {
    vec3 mainDir = env.sunDir.y > 0.0 ? env.sunDir : env.moonDir;
    float glintPower = env.sunDir.y > 0.0 ? clamp(env.dayFactor*2.0, 0.0, 1.0) : 0.25;
    vec3 glintCol = env.sunDir.y > 0.0 ? vec3(1.0, 0.97, 0.92) : vec3(0.6, 0.7, 1.0);
    // usa o mesmo vetor que nlRenderSky recebe, então o brilho cai onde o céu mostra o sol
    waterRefl += glintCol * (NL_WATER_SUN_GLINT * glintPower * (1.0-0.8*env.rainFactor) * lit.y) * nlSunGlint(viewDir, mainDir);
  }

  #ifdef NL_WATER_REFL_MASK
    float mask = 0.05+0.05*sin(viewDir.x*12.0)*sin(viewDir.z*6.0);
    waterRefl *= smoothstep(mask-0.2,mask+0.13,viewDir.y*viewDir.y);
  #endif

  cosR = abs(cosR);
  float fresnel = calculateFresnel(cosR, 0.10);
  float opacity = 1.0-cosR;

  color.rgb *= 0.22*NL_WATER_TINT*(1.0-0.8*fresnel);
  color.a = mix(COLOR.a*NL_WATER_TRANSPARENCY, 1.0, opacity*opacity);

  #ifdef NL_WATER_WAVE
    if (camDist < 14.0) {
      wPos.y -= 0.5*(bump.x+0.5)*NL_WATER_BUMP;
    }
  #endif

  return vec4(waterRefl, fresnel);
}

#endif
