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


uniform sampler2D DiffMap;
uniform sampler2D BumpMap;
uniform sampler2D RingsMap;
uniform sampler2D NoiseMap;

uniform vec4   Radiuses;      // (planet radius, atmosphere radius, atmosphere height, surface radius)
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

    // Get detail noise
    vec4 detNoise =
         texture(NoiseMap, vTexCoord2.zw) +
         texture(NoiseMap, vTexCoord2.zw *  2.0) * 0.5 +
         texture(NoiseMap, vTexCoord2.zw *  8.0) * 0.25 +
         texture(NoiseMap, vTexCoord2.zw * 16.0) * 0.125;

    // Add detail normal noise normal (0.9375 because normal vectors are packed into [0, 1] range)
    vec3 normDetail = normalize(detNoise.xyz - 0.9375);
    normVec = normalize(normVec + 0.5 * normDetail);

    // Calculate precise fragment position and eye vector in object space
    EyeR    = length(EyePos.xyz);
    FragR   = (1.0 + height) * Radiuses.w;
    FragPos = Normal * FragR;
    eyeVec  = FragPos - EyePos.xyz;
    eyeVecLength = length(eyeVec);

    // Switch to vertex-precise coordinates close to the camera
    if (eyeVecLength < 10.0) // km
    {
        float t = smoothstep(1.0, 10.0, eyeVecLength);
        vec3  FragPosP = vPositionLocal.xyz + NodeCenter.xyz * EyePosLocal.w;
        vec3  eyeVecP  = vPositionLocal.xyz - EyePosLocal.xyz;
        float FragRP   = length(FragPosP);
        FragPos = mix(FragPosP, FragPos, t);
        eyeVec  = mix(eyeVecP,  eyeVec,  t);
        FragR   = mix(FragRP,   FragR,   t);
        eyeVecLength = length(eyeVec);
    }

    eyeVec /= eyeVecLength;

    // Calculate eye vector in tangent space
    vec3  eyeVecTS = Rotation * eyeVec;

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
    float eclMask = 1.0;

    // Calculate shadow value
    float Shadow0 = 1.0;

    // Calculate rings shadow
    Shadow0 *= 1.0 - vRingShadowFade.x * texture(RingsMap, vec2(vRingShadowCoord.x, 0.75)).a;

    // Modulate the sunlight with the shadow value
    sunLight0 *= Shadow0;

    // Calculate planetary sphere shadow
    float sinHor = 1.0 / (1.0 + height);
    float cosHor = sqrt(1.0 - sinHor * sinHor) * 500.0;
    float HorShadow0 = clamp(cosHor + cosIN.x * 500.0, 0.0, 1.0);

    // Calculate lighting value, mix between the Lambert and Hapke models
    float litFactor, cosI, mu;
    float cosO = clamp(-dot(normVec, eyeVecTS), 0.001, 1.0);

    // Calculate lighting value for light 0
    cosI = clamp(dot(normVec, light0VecTS), 0.0, 1.0);
    mu = clamp(-dot(light0Vec, eyeVec), -0.999, 1.0);
    litFactor = cosI / (cosI + cosO) * Hapke(mu); // Simplified Hapke BRDF
    litFactor = mix(cosI, litFactor, SurfParams1.z);  // mix between the Lambert and Hapke models
    sunLight0 *= HorShadow0;
    litFactor += SurfParams2.y * cosINC.x;
    // Get normal lighting color
    vec3  sunLightS0 = sunLight0 * litFactor;

    // Get diffuse color in rgb and specular intensity in alpha
    vec4  diffColor = texture(DiffMap, vTexCoord.xy) * ModulateColor;

    // Add detail noise texture
    diffColor.rgb *= 0.7 + 0.3 * detNoise.w;
    vec3  LightingColorS = sunLightS0;
    vec3  groundColor = diffColor.rgb * (AmbientColor.rgb + LightingColorS);

    // Calculate the result color
    FragColor.rgb = groundColor * SurfParams1.w;
    FragColor.a   = 1.0;
}

#endif
