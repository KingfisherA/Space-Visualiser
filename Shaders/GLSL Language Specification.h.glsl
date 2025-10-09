struct vec2
{
    vec2(float x);
    vec2(float x, float y);
    float x, y;
    float r, g;
    float s, t;
    float xy;
    float rg;
    float st;
    float &operator[](int i) {}
}
//
struct vec3
{
    vec3(float x);
    vec3(float x, float y, float z);
    vec3(vec2 v, float z);
    float x, y, z;
    float r, g, b;
    float s, t, r;
    //
    float xy;
    float rg;
    float st;
    float xyz;
    float rgb;
    float str;
    //
    float &operator[](int i) {}
}
//
struct vec4
{
    vec4(float x);
    vec4(float x, float y, float z, float w);
    vec4(vec3 v, float w);
    float x, y, z, w;
    float r, g, b, a;
    float s, t, r, q;
    //
    float xy;
    float rg;
    float st;
    float xyz;
    float rgb;
    float str;
    float xyzw;
    float rgba;
    //
    float &operator[](int i) {}
}
//
struct mat3
{
    mat3(float x);
    mat3(vec3 v1, vec3 v2, vec3 v3);
    mat3(float a1, float a2, float a3,
         float a4, float a5, float a6,
         float a7, float a8, float a9);
    vec3 data[3];
    vec3 &operator[](int i) { return data[i]; }
}
//
struct mat4
{
    mat4(float x);
    mat4(vec4 v1, vec4 v2, vec4 v3, vec4 v4);
    mat4(float a1, float a2, float a3, float a4,
         float a5, float a6, float a7, float a8,
         float a9, float a10, float a11, float a12,
         float a13, float a14, float a15, float a16);

    vec3 data[4];
    vec4 &operator[](int i) { return data[i]; }
}
//
struct sampler2D
{
};
struct sampler3D
{
};

extern unsigned int location;
#define layout(x)

#define uniform
#define in
#define out
#define inout
#define flat
#define smooth
#define noperspective

float radians(float degrees);
vec2 radians(vec2 degrees);
vec3 radians(vec3 degrees);
vec4 radians(vec4 degrees);

float degrees(float radians);
vec2 degrees(vec2 radians);
vec3 degrees(vec3 radians);
vec4 degrees(vec4 radians);

float sin(float angle);
vec2 sin(vec2 angle);
vec3 sin(vec3 angle);
vec4 sin(vec4 angle);

float cos(float angle);
vec2 cos(vec2 angle);
vec3 cos(vec3 angle);
vec4 cos(vec4 angle);

float tan(float angle);
vec2 tan(vec2 angle);
vec3 tan(vec3 angle);
vec4 tan(vec4 angle);

float asin(float x);
vec2 asin(vec2 x);
vec3 asin(vec3 x);
vec4 asin(vec4 x);

float acos(float x);
vec2 acos(vec2 x);
vec3 acos(vec3 x);
vec4 acos(vec4 x);

float atan(float y_over_x);
vec2 atan(vec2 y_over_x);
vec3 atan(vec3 y_over_x);
vec4 atan(vec4 y_over_x);

float atan(float y, float x);
vec2 atan(vec2 y, vec2 x);
vec3 atan(vec3 y, vec3 x);
vec4 atan(vec4 y, vec4 x);

float pow(float x, float y);
vec2 pow(vec2 x, vec2 y);
vec3 pow(vec3 x, vec3 y);
vec4 pow(vec4 x, vec4 y);

float exp(float x);
vec2 exp(vec2 x);
vec3 exp(vec3 x);
vec4 exp(vec4 x);

float log(float x);
vec2 log(vec2 x);
vec3 log(vec3 x);
vec4 log(vec4 x);

float log2(float x);
vec2 log2(vec2 x);
vec3 log2(vec3 x);
vec4 log2(vec4 x);

float exp2(float x);
vec2 exp2(vec2 x);
vec3 exp2(vec3 x);
vec4 exp2(vec4 x);

float sqrt(float x);
vec2 sqrt(vec2 x);
vec3 sqrt(vec3 x);
vec4 sqrt(vec4 x);

float inversesqrt(float x);
vec2 inversesqrt(vec2 x);
vec3 inversesqrt(vec3 x);
vec4 inversesqrt(vec4 x);

float abs(float x);
vec2 abs(vec2 x);
vec3 abs(vec3 x);
vec4 abs(vec4 x);

float sign(float x);
vec2 sign(vec2 x);
vec3 sign(vec3 x);
vec4 sign(vec4 x);

float floor(float x);
vec2 floor(vec2 x);
vec3 floor(vec3 x);
vec4 floor(vec4 x);

float ceil(float x);
vec2 ceil(vec2 x);
vec3 ceil(vec3 x);
vec4 ceil(vec4 x);

float fract(float x);
vec2 fract(vec2 x);
vec3 fract(vec3 x);
vec4 fract(vec4 x);

float mod(float x, float y);
vec2 mod(vec2 x, vec2 y);
vec3 mod(vec3 x, vec3 y);
vec4 mod(vec4 x, vec4 y);

float mod(float x, float y);
vec2 mod(vec2 x, float y);
vec3 mod(vec3 x, float y);
vec4 mod(vec4 x, float y);

float min(float x, float y);
vec2 min(vec2 x, vec2 y);
vec3 min(vec3 x, vec3 y);
vec4 min(vec4 x, vec4 y);
float min(float x, float y);
vec2 min(vec2 x, float y);
vec3 min(vec3 x, float y);
vec4 min(vec4 x, float y);

float max(float x, float y);
vec2 max(vec2 x, vec2 y);
vec3 max(vec3 x, vec3 y);
vec4 max(vec4 x, vec4 y);
float max(float x, float y);
vec2 max(vec2 x, float y);
vec3 max(vec3 x, float y);
vec4 max(vec4 x, float y);

float clamp(float x, float minVal, float maxVal);
vec2 clamp(vec2 x, vec2 minVal, vec2 maxVal);
vec3 clamp(vec3 x, vec3 minVal, vec3 maxVal);
vec4 clamp(vec4 x, vec4 minVal, vec4 maxVal);
float clamp(float x, float minVal, float maxVal);
vec2 clamp(vec2 x, float minVal, float maxVal);
vec3 clamp(vec3 x, float minVal, float maxVal);
vec4 clamp(vec4 x, flfloat minVal, float maxVal);

float mix(float x, float y, float a);
vec2 mix(vec2 x, vec2 y, vec2 a);
vec3 mix(vec3 x, vec3 y, vec3 a);
vec4 mix(vec4 x, vec4 y, vec4 a);
float mix(float x, float y, float a);
vec2 mix(vec2 x, vec2 y, float a);
vec3 mix(vec3 x, vec3 y, float a);
vec4 mix(vec4 x, vec4 y, float a);

float step(float edge, float x);
vec2 step(vec2 edge, vec2 x);
vec3 step(vec3 edge, vec3 x);
vec4 step(vec4 edge, vec4 x);
float step(float edge, float x);
vec2 step(float edge, vec2 x);
vec3 step(float edge, vec3 x);
vec4 step(float edge, vec4 x);

float smoothstep(float edge0, float edge1, float x);
vec2 smoothstep(vec2 edge0, vec2 edge1, vec2 x);
vec3 smoothstep(vec3 edge0, vec3 edge1, vec3 x);
vec4 smoothstep(vec4 edge0, vec4 edge1, vec4 x);
float smoothstep(float edge0, float edge1, float x);
vec2 smoothstep(float edge0, float edge1, vec2 x);
vec3 smoothstep(float edge0, float edge1, vec3 x);
vec4 smoothstep(float edge0, float edge1, vec4 x);

float length(float x);
float length(vec2 x);
float length(vec3 x);
float length(vec4 x);

float distance(float p0, float p1);
float distance(vec2 p0, vec2 p1);
float distance(vec3 p0, vec3 p1);
float distance(vec4 p0, vec4 p1);

float dot(float x, float y);
float dot(vec2 x, vec2 y);
float dot(vec3 x, vec3 y);
float dot(vec4 x, vec4 y);

vec3 cross(vec3 x, vec3 y);

float normalize(float x);
vec2 normalize(vec2 x);
vec3 normalize(vec3 x);
vec4 normalize(vec4 x);

float faceforward(float N, float I, float Nref);
vec2 faceforward(vec2 N, vec2 I, vec2 Nref);
vec3 faceforward(vec3 N, vec3 I, vec3 Nref);
vec4 faceforward(vec4 N, vec4 I, vec4 Nref);

float reflect(float I, float N);
vec2 reflect(vec2 I, vec2 N);
vec3 reflect(vec3 I, vec3 N);
vec4 reflect(vec4 I, vec4 N);

float refract(float I, float N, float eta);
vec2 refract(vec2 I, vec2 N, float eta);
vec3 refract(vec3 I, vec3 N, float eta);
vec4 refract(vec4 I, vec4 N, float eta);

vec4 texture(sampler2D sampler, vec2 coord);
vec4 texture(sampler3D sampler, vec2 coord);

#define gl_Position
#define gl_VertexID
#define gl_InstanceID
#define gl_DrawID
#define gl_BaseVertex
#define gl_BaseInstance

#define gl_FragCoord
#define gl_FrontFacing
#define gl_PointCoord
#define discard

#define insert(key)