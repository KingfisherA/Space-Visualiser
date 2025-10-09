#version 460 core
#auto_defines

vec3  FragPos;
float FragR;
float FragH;
float FragMu;
vec3  EyePosM;
float EyeR;
float EyeH;
float EyeMu;
vec3  eyeVec;
float eyeVecLength;
float HorizonMu;

#include "atmo_common.glsl"
#include "eclipse_common.glsl"

uniform sampler2D DiffMap;
uniform sampler2D BumpMap;
uniform sampler2D GlowMap;
uniform sampler2D RingsMap;
uniform sampler1D PlanckFunction;
uniform sampler2D NoiseMap;

uniform vec4   NodeCenter;    // (Node center offset, heightmap offset)
uniform mat4x4 ModelViewProj; // mvp matrix
uniform vec3   Light0Pos;     // Object-space light position
uniform vec4   Light0Color;   // Light color, light specular power
uniform vec4   AmbientColor;  // Ambient color, eclipse shadow intensity
uniform vec4   ModulateColor; // Modulate color
uniform vec4   GlowColor;     // Glow color, glow mode
uniform vec3   GlowColorAtmo; // Glow color of the atmosphere
uniform vec4   WaterFogColor; // Underwater fog color, water fade
uniform vec4   EyePos;        // Object-space camera position, minEyeMu
uniform vec4   EyePosLocal;   // Object-space camera position relative to the node center, surface radius
uniform vec4   SurfParams1;   // (ice specular bright, water specular bright, Hapke to Lambert coefficient, exposure)
uniform vec4   SurfParams2;   // (heightmap scale, day ambient coefficient, Hapke spot bright, 1 / Hapke spot width)
uniform vec4   SurfParams3;   // (detail texture frequency, lava temp factor, sunheat temp factor, thermal emission brightness)
uniform vec4   DiffTexTransf; // Diffuse texture transform
uniform vec4   BumpTexTransf; // Bump texture transform
uniform vec4   GlowTexTransf; // Glow texture transform
uniform vec4   RingsParams;   // (inv rings width, rings inner radius, rings density, face)
uniform vec4   WaterParams;   // (water depth, water layer radius, inv water fade height, water depth correction)
uniform vec4   EllipsGrav;    // (planet ellipsoid oblateness, ellipsoid gravity coefficient)
uniform vec4   LightLuminosity;
uniform vec4   LightRadius;
uniform mat4x4 Light0Casters;

#ifdef _VERTEX_

/****************** Vertex shader input ******************/
layout(location = 0) in  vec3  VertexPos;
layout(location = 1) in  vec2  TexCoord;
layout(location = 2) in  vec3  Tangent;

/****************** Vertex shader output ******************/
out vec4 vPosition;
out vec3 vPositionLocal;
out vec3 vTangent;
out vec4 vTexCoord;
out vec4 vTexCoord2;
out vec4 vRingShadowCoord;
out vec4 vRingShadowFade;

void main()
{
    // Transfer vertex parameters
    vPositionLocal = VertexPos * EyePosLocal.w;
    vPosition.xyz  = VertexPos + NodeCenter.xyz;
    vTangent       = Tangent;

    // Transfer main texture coordinates
    vTexCoord.xy  = TexCoord * DiffTexTransf.zw + DiffTexTransf.xy;

    // Transfer bump texture coordinates
    vTexCoord.zw  = TexCoord * BumpTexTransf.zw + BumpTexTransf.xy;

    // Transfer glow texture coordinates
    vTexCoord2.xy = TexCoord * GlowTexTransf.zw + GlowTexTransf.xy;

    // Transfer detail texture coordinates
    vTexCoord2.zw = TexCoord * SurfParams3.x;

    // Calculate output position
    gl_Position = ModelViewProj * vec4(VertexPos, 1.0);

    // Calculate rings shadows parameters
    vec3  lightPos;
    vec2  shadowProj;
    float sina;
    if (RingsParams.w == 0.0)
    {
        lightPos = Light0Pos - vPosition.xyz;
        shadowProj = vPosition.xz - lightPos.xz * min(vPosition.y / lightPos.y, 0.0);
        vRingShadowCoord.x = (length(shadowProj) - RingsParams.y) * RingsParams.x;
        sina = abs(Light0Pos.y) * inversesqrt(dot(Light0Pos, Light0Pos));
        vRingShadowFade.x = RingsParams.z * smoothstep(0.0003, 0.001, sina);

    }
    else
    {
        lightPos = Light0Pos - vPosition.xyz;
        shadowProj = vPosition.xy - lightPos.xy * min(vPosition.z / lightPos.z, 0.0);
        vRingShadowCoord.x = (length(shadowProj) - RingsParams.y) * RingsParams.x;
        sina = abs(Light0Pos.z) * inversesqrt(dot(Light0Pos, Light0Pos));
        vRingShadowFade.x = RingsParams.z * smoothstep(0.0003, 0.001, sina);

    }
}

#else

/****************** Fragment shader input ******************/
in vec4 vPosition;
in vec3 vPositionLocal;
in vec3 vTangent;
in vec4 vTexCoord;
in vec4 vTexCoord2;
in vec4 vRingShadowCoord;
in vec4 vRingShadowFade;

/****************** Fragment shader output ******************/
layout(location = 0) out vec4 FragColor;

// The Hapke hot spot correction function
float Hapke(float mu)
{
    return 1.0 + SurfParams2.z / (1.0 + sqrt((1.0 - mu) / (1.0 + mu)) * SurfParams2.w);
}

void main()
{
    // Get diffuse color in rgb and transparency in alpha
    vec4 diffColor = texture(DiffMap, vTexCoord.xy);
    if (diffColor.a < 0.05) discard;
    float cloudsFade = (1.0 - 0.05 / diffColor.a) / 0.95;
    diffColor *= ModulateColor * cloudsFade;
    diffColor.rgb *= ModulateColor.a;

    // Init atmosphere parameters
    planRadius2 = Radiuses.x * Radiuses.x;
    mieG2  = AtmoParams2.r * AtmoParams2.r;
    atmoH2 = Radiuses.y * Radiuses.y - planRadius2;
    atmoH  = sqrt(atmoH2);

    // Get normal and tangent
    vec3  Normal  = normalize(vPosition.xyz);
    vec3  Tangent = normalize(vTangent);

    // Calculate matrix of transformation to tangent space
    mat3x3 Rotation = transpose(mat3x3(Tangent, cross(Tangent, Normal), Normal));

    // Get normal map vector, expanded from [0, 1] to [-1, 1]
    vec4  bumpData = texture(BumpMap, vTexCoord.zw);
    vec3  normVec;
    normVec.xy = 2.0 * bumpData.xy - 1.0;
    normVec.z  = sqrt(1.0 - dot(normVec.xy, normVec.xy));
    float heightN = bumpData.w + bumpData.z * 0.00390625;
    float height = heightN * SurfParams2.x;

    // Calculate precise fragment position and eye vector in object space
    EyeR    = length(EyePos.xyz);
    FragR   = (1.0 + height) * Radiuses.w;
    FragPos = Normal * FragR;
    eyeVec  = FragPos - EyePos.xyz;
    eyeVecLength = length(eyeVec);
    eyeVec /= eyeVecLength;

    // Calculate eye vector in tangent space
    vec3  eyeVecTS = Rotation * eyeVec;

    // Calculate fragment position for eclipse shadow
    vec3 FragPosS = FragPos * EllipsGrav.xyz;

    // Calculate fragment and eye parameters for atmosphere
    FragH  = (FragR - Radiuses.x) / Radiuses.z;
    EyeH   = (EyeR  - Radiuses.x) / Radiuses.z;
    FragMu = dot(FragPos,    eyeVec) / FragR;
    EyeMu  = dot(EyePos.xyz, eyeVec) / EyeR;

    // If EyePos in space, move it to nearest intersection of ray with top atmosphere boundary
    EyePosM = EyePos.xyz;
    float d = -EyeR * EyeMu - sqrt(EyeR * EyeR * (EyeMu * EyeMu - 1.0) + Radiuses.y * Radiuses.y);
    if (d > 0.0)
    {
        EyePosM += d * eyeVec;
        eyeVecLength -= d;
        EyeMu = (EyeR * EyeMu + d) / Radiuses.y;
        EyeR = Radiuses.y;
        EyeH = 1.0;
    }

    // Calculate light vectors in object space
    vec3  Light0PosS = Light0Pos - FragPos;
    vec3  light0Vec  = normalize(Light0PosS);

    // Calculate light vectors in tangent space
    vec3  light0VecTS = Rotation * light0Vec;

    // Calculate direct sun lighting
    vec4  cosIN = vec4(0.0);
    cosIN.x = dot(Normal, light0Vec);
    vec3  sunLight0 = Light0Color.rgb;
    vec4  cosINC = clamp(cosIN, 0.0, 1.0);

    // Calculate inverse light distance
    vec3 light0VecSN = Light0PosS * EllipsGrav.xyz;
    float invLight0Dist = inversesqrt(dot(light0VecSN, light0VecSN));

    // Calculate shadow value
    float Shadow0 = 1.0;

    // Calculate rings shadow
    Shadow0 *= 1.0 - vRingShadowFade.x * texture(RingsMap, vec2(vRingShadowCoord.x, 0.75)).a;

    // Calculate eclipse shadow
    float lightAngularRadius;
    lightAngularRadius = asin(clamp(LightRadius.x * invLight0Dist, 0.0, 1.0));
    Shadow0 *= 1.0 - AmbientColor.a * EclipseShadow(Light0Casters, FragPosS, light0VecSN * invLight0Dist, lightAngularRadius);

    // Modulate the sunlight with the shadow value
    sunLight0 *= Shadow0;

    // Calculate planetary sphere shadow
    float sinHor = 1.0 / (1.0 + height);
    float cosHor = sqrt(1.0 - sinHor * sinHor) * 500.0;
    float HorShadow0 = clamp(cosHor + cosIN.x * 500.0, 0.0, 1.0);

    // Calculate lighting value, mix between the Lambert model and 1.0
    float litFactor, cosI;
    float cosO = clamp(-dot(normVec, eyeVecTS), 0.001, 1.0);
    float rr = Radiuses.x / FragR;
    float mu0 = -sqrt(1.0 - rr*rr);
    float sqrtFragH = sqrt(FragH);
    float sunHeating = 0.0;

    // Calculate lighting value for light 0
    cosI = clamp(dot(normVec, light0VecTS), 0.0, 1.0);
    litFactor = cosI;
    // Get sunlight attenuation in the atmosphere
    sunLight0 *= transmittanceWithShadowDens(sqrtFragH, cosIN.x, mu0);
    litFactor += SurfParams2.y * cosINC.x;
    // Get normal lighting color
    vec3  sunLightS0 = sunLight0 * litFactor;
    // Get flat lighting color
    vec3  sunLightW0 = sunLight0 * cosINC.x;
    // Get sun heating value
    sunHeating += cosI * Shadow0 * HorShadow0 * LightLuminosity.x * invLight0Dist * invLight0Dist;
    // Get sky irradiance
    vec3 skyIrrad0 = irradiance(FragH, cosIN.x) * Light0Color.rgb * AtmoParams1.z * Shadow0;
    sunLightS0 += skyIrrad0;
    sunLightW0 += skyIrrad0;
    vec3  LightingColorS = sunLightS0;
    vec3  LightingColorW = sunLightW0;
    vec3  groundColor = diffColor.rgb * (AmbientColor.rgb + LightingColorS);

    // Get glow color in rgb and the glow mode in alpha
    vec4  glowData = texture(GlowMap, vTexCoord2.xy);
    float glowMode = (GlowColor.a >= 0) ? GlowColor.a : glowData.a;

    // City lights - off in the daytime
    float LightingBrightness = dot(LightingColorW, vec3(0.3, 0.59, 0.11));
    vec3  glowColorCity = glowData.rgb * (1.0 - smoothstep(0.01, 0.05, LightingBrightness));

    // Permanent lights
    vec3  glowColorPerm = glowData.rgb;

    // Thermal emission light
    float surfTemp = dot(glowData.rgb, vec3(1.0, 1.0/255.0, 1.0/65025.0)); // unpack 24 bit to float
    surfTemp = exp((surfTemp - 0.1316) / 0.188); // temperature in thousand Kelvins
    // Heating by the sun(s)
    surfTemp = pow(0.010673289 * sunHeating + pow(surfTemp, 4), 0.25);
    // Gravity darkening for ellipsoidal stars
    float sinLat2 = (RingsParams.w == 0) ? Normal.y * Normal.y : Normal.z * Normal.z;
    float gravity = EllipsGrav.w * sinLat2 + 1.0;
    surfTemp *= pow(gravity, 0.25);
    // Get thermal emission color by sampling the Planck function texture
    vec3 glowColorTherm = texture(PlanckFunction, log(surfTemp + SurfParams3.y) * 0.188 + 0.1316).rgb * SurfParams3.w;
    // Star edge darkening
    glowColorTherm *= vec3(0.65, 0.35, 0.20) + vec3(0.35, 0.65, 0.80) * cosO;

    // Mix all glow colors using the glow mode value
    const float lerpWidthCP = 1.0/6.0;
    const float lerpWidthPT = 1.0/6.0;
    float lerpCityToPerm  = 1.0 - smoothstep(1.0/3.0 - lerpWidthCP, 1.0/3.0,               glowMode);
    float lerpPermToCity  =       smoothstep(1.0/3.0,               1.0/3.0 + lerpWidthCP, glowMode);
    float lerpPermToTherm = 1.0 - smoothstep(2.0/3.0 - lerpWidthPT, 2.0/3.0,               glowMode);
    float lerpThermToPerm =       smoothstep(2.0/3.0,               2.0/3.0 + lerpWidthPT, glowMode);
    vec3  glowColor = glowColorCity  * lerpCityToPerm +
                      glowColorPerm  * lerpPermToCity * lerpPermToTherm +
                      glowColorTherm * lerpThermToPerm;
    groundColor += glowColor.rgb * GlowColor.rgb * diffColor.a;

    // Atmospheric attenuation along ray from the ground to the viewer
    Attenuation = transmittanceAnalytic(EyeR, max(EyeMu, EyePos.w), eyeVecLength);

    // Atmospheric scattering along ray from the ground to the viewer
    vec3 Inscatter = vec3(0.0);
    float invR = Radiuses.x / EyeR;
    HorizonMu = -sqrt(1.0 - invR * invR);
    HorizonFixEps = AtmoParams1.w;
    if (abs(EyeMu - HorizonMu) < HorizonFixEps)
    {
        Inscatter += inscatterGroundFix(vec3(0.0)) * GlowColorAtmo;
        Inscatter += inscatterGroundFix(light0Vec) * Light0Color.rgb * Shadow0;
    }
    else
    {
        Inscatter += inscatterGround(vec3(0.0)) * GlowColorAtmo;
        Inscatter += inscatterGround(light0Vec) * Light0Color.rgb * Shadow0;
    }
    groundColor  = groundColor * Attenuation + Inscatter;

    // Subtract atmospheric scattering from the cloud layer to the eye, because it was already computed via previous surface (landscape)
    groundColor -= Inscatter * (1.0 - diffColor.a);

    // Calculate the result color
    FragColor.rgb = groundColor * SurfParams1.w;
    FragColor.a   = diffColor.a;
}

#endif
