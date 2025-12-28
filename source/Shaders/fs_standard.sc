$input PixelPosition, PixelNormal, ShadowCoord, TexCoord, PixelTangent, PixelBitangent
#include <bgfx_shader.sh>

uniform vec4 u_AlbedoColor; // RGB + roughness
uniform vec4 u_MaterialParams; // x: metallic, y: unused, z: unused, w: unused
uniform vec4 u_Emission; // rgb: emission color, w: emission strength
uniform vec4 u_LightDir;    // xyz: light direction (normalized) - Directional Light (Sun)
uniform vec4 u_LightColor;   // rgb: light color, w: intensity - Directional Light (Sun)
uniform vec4 u_CameraPos;    // xyz: camera position in world space, w: unused
uniform vec4 u_TextureBlend; // x: blend mode, y: blend factor, z: unused, w: unused
uniform vec4 u_NormalMapParams; // x: normal map strength, y: has normal map (0 or 1), z: unused, w: unused
uniform vec4 u_LightmapParams; // x: lightmap scale X, y: lightmap scale Y, z: lightmap offset X, w: lightmap offset Y

// Multi-Light Support (32 Point Lights)
uniform vec4 u_PointLightPos[32];   // xyz: position, w: range
uniform vec4 u_PointLightColor[32]; // rgb: color, w: intensity
uniform vec4 u_PointLightParams[32]; // x: attenuation, y: padding, z: padding, w: padding
uniform vec4 u_NumPointLights;      // x: count (int cast)

SAMPLER2D(s_TexShadow, 1);
SAMPLER2D(s_TexAlbedo, 2); // Albedo texture (base layer)
SAMPLERCUBE(s_TexEnvironment, 3); // Environment map (skybox) for reflections
SAMPLER2D(s_TexAlbedo2, 4); // Detail texture (tiled)
SAMPLER2D(s_TexBlendMask, 5); // Blend mask texture
SAMPLER2D(s_TexRoughness, 6); // Roughness texture map
SAMPLER2D(s_TexMetallic, 7); // Metallic texture map
SAMPLER2D(s_TexNormal, 8); // Normal map texture
SAMPLER2D(s_TexLightmap, 9); // Lightmap (radiosity) texture

float SampleShadow(sampler2D _sampler, vec3 _coord, float _bias)
{
    float Visible = 1.0;
    if (_coord.z > texture2D(_sampler, _coord.xy).r + _bias) { Visible = 0.0; }
    return Visible;
}

float SampleShadowPCF(sampler2D _sampler, vec3 _coord, float _bias, vec2 _texelSize)
{
    float Visibility = 0.0;
    // Optimized 2x2 PCF (4 samples)
    for(int x = 0; x <= 1; ++x)
    {
        for(int y = 0; y <= 1; ++y)
        {
            Visibility += SampleShadow(_sampler, _coord + vec3(vec2(float(x)-0.5, float(y)-0.5) * _texelSize, 0.0), _bias);
        }
    }
    return Visibility * 0.25;
}

vec3 CalculatePointLight(vec3 WorldPos, vec3 Normal, vec3 ViewDir,
                        vec3 LightPos, vec3 LightColor, float Intensity, float Range, float Attenuation,
                        vec3 Albedo, float Roughness, float Metallic, vec3 F0)
{
    vec3 L = LightPos - WorldPos;
    float Dist = length(L);

    // Range check
    if (Dist > Range && Range > 0.0) return vec3_splat(0.0);

    L = normalize(L);
    vec3 H = normalize(L + ViewDir);

    // Attenuation (Inverse Square + Range fade)
    float Atten = 1.0 / (1.0 + Attenuation * Dist * Dist);
    // Smooth falloff at range limit
    if (Range > 0.0)
    {
        float Falloff = clamp(1.0 - pow(Dist / Range, 4.0), 0.0, 1.0);
        Atten *= Falloff * Falloff;
    }

    vec3 Radiance = LightColor * Intensity * Atten;

    // Cook-Torrance BRDF (simplified)
    float NdotL = max(dot(Normal, L), 0.0);
    float NdotH = max(dot(Normal, H), 0.0);
    float NdotV = max(dot(Normal, ViewDir), 0.0);

    // Specular (Blinn-Phong approximation for performance, matching main light style)
    float SpecPower = 2.0 / (Roughness * Roughness) - 2.0;
    float Spec = pow(NdotH, SpecPower);

    // Fresnel-Schlick
    vec3 F = F0 + (vec3_splat(1.0) - F0) * pow(1.0 - max(dot(H, ViewDir), 0.0), 5.0);

    // Diffuse vs Specular
    vec3 KD = (vec3_splat(1.0) - F) * (1.0 - Metallic);
    vec3 Diffuse = KD * Albedo / 3.14159;

    // Specular Term (Energy conserved)
    // Using simple Blinn-Phong specular term to match existing style
    vec3 Specular = F * Spec * (1.0 + Metallic * 2.0) * 0.5;

    return (Diffuse + Specular) * Radiance * NdotL;
}

void main()
{
    // Build TBN matrix for normal mapping
    vec3 N = normalize(PixelNormal);
    vec3 T = normalize(PixelTangent);
    vec3 B = normalize(PixelBitangent);

    // Ensure orthonormal basis (Gram-Schmidt re-orthogonalization)
    T = normalize(T - N * dot(T, N));
    B = normalize(B - N * dot(B, N) - T * dot(B, T));

    // TBN matrix: transforms from tangent space to world space
    mat3 TBN = mat3(T, B, N);

    // Sample normal map if available
    bool HasNormalMap = u_NormalMapParams.y > 0.5;
    vec3 FinalNormal = N; // Default to vertex normal

    if (HasNormalMap) {
        // Sample normal map (stored in [0,1] range, decode to [-1,1])
        vec3 NormalMapSample = texture2D(s_TexNormal, TexCoord).rgb;
        NormalMapSample = NormalMapSample * 2.0 - 1.0; // Decode from [0,1] to [-1,1]

        // Apply normal map strength
        float NormalStrength = u_NormalMapParams.x;
        NormalMapSample.xy *= NormalStrength;
        NormalMapSample = normalize(NormalMapSample);

        // Transform from tangent space to world space
        FinalNormal = normalize(mul(TBN, NormalMapSample));
    }

    // Use final normal (either from normal map or vertex normal) for all lighting
    vec3 N_Final = FinalNormal;

    // Sample lightmap (radiosity) if available
    vec3 LightmapColor = vec3(0.0, 0.0, 0.0);
    bool HasLightmap = u_LightmapParams.x > 0.0 || u_LightmapParams.y > 0.0;
    if (HasLightmap) {
        // Calculate lightmap UV coordinates from material scale/offset
        vec2 LightmapUV = TexCoord * vec2(u_LightmapParams.x, u_LightmapParams.y) + vec2(u_LightmapParams.z, u_LightmapParams.w);
        vec4 LightmapSample = texture2D(s_TexLightmap, LightmapUV);

        // Decode RGBM format
        float Multiplier = 8.0;
        LightmapColor = LightmapSample.rgb * (LightmapSample.a * Multiplier);
    }

    // Lighting Setup - use uniforms from renderer
    vec3 SunLightDir = normalize(u_LightDir.xyz);
    vec3 SunLightColor = u_LightColor.rgb * u_LightColor.w;

    // Ambient lighting - improved for PBR materials
    vec3 SkyColor = vec3(0.4, 0.6, 0.9); // Sky Blue
    vec3 GroundColor = vec3(0.1, 0.1, 0.12); // Slightly brighter ground ambient
    float HemiMix = N_Final.y * 0.5 + 0.5;
    float AmbientFactor = mix(0.15, 0.25, HemiMix);
    vec3 Ambient = mix(GroundColor, SkyColor, HemiMix) * AmbientFactor;

    // Diffuse lighting - standard PBR approach
    float Diff = max(dot(N_Final, SunLightDir), 0.0);
    vec3 Diffuse = Diff * SunLightColor;

    // View direction
    vec3 ViewDir = normalize(u_CameraPos.xyz - PixelPosition);
    vec3 HalfDir = normalize(SunLightDir + ViewDir);

    // Reflection vector
    vec3 ReflectDir = reflect(-ViewDir, N_Final);

    // Get material properties
    float Metallic = u_MaterialParams.x;
    float Roughness = max(0.05, u_MaterialParams.y);
    float Opacity = u_AlbedoColor.w;

    // Sample roughness texture
    float RoughnessTex = texture2D(s_TexRoughness, TexCoord).r;
    if (RoughnessTex < 0.98) {
        Roughness = max(0.05, RoughnessTex);
    }

    // Sample metallic texture
    float MetallicTex = texture2D(s_TexMetallic, TexCoord).r;
    if (MetallicTex > 0.02) {
        Metallic = MetallicTex;
    }

    // Check if material is transparent/glass
    bool IsGlass = Opacity < 0.95;

    // Specular (Blinn-Phong)
    float SpecPower = 2.0 / (Roughness * Roughness) - 2.0;
    float Spec = pow(max(dot(N_Final, HalfDir), 0.0), SpecPower);
    vec3 Specular = Spec * SunLightColor * 0.5;

    // Rim Light
    float Rim = 1.0 - max(dot(N_Final, ViewDir), 0.0);
    Rim = pow(Rim, 3.0);
    vec3 RimColor = vec3(0.5, 0.6, 0.8) * Rim * 0.5;

    // Fresnel
    float Fresnel = pow(1.0 - max(dot(N_Final, ViewDir), 0.0), 2.0);

    // Environment reflection
    vec3 ReflectionColor = vec3_splat(0.0);
    if (IsGlass) {
        ReflectionColor = textureCube(s_TexEnvironment, ReflectDir).rgb;
        ReflectionColor *= Fresnel * 0.8;
    }

    // Shadow calculation
    float Shadow = 1.0;
    vec3 Coords = ShadowCoord.xyz;
    if (abs(ShadowCoord.w) > 0.0001) {
        Coords = ShadowCoord.xyz / ShadowCoord.w;
    }
    Coords = Coords * 0.5 + 0.5;

    bool IsGround = N_Final.y > 0.7;
    bool InBounds = Coords.x >= -2.0 && Coords.x <= 3.0 &&
                    Coords.y >= -2.0 && Coords.y <= 3.0 &&
                    Coords.z >= -1.0 && Coords.z <= 2.0;

    if (InBounds) {
        Coords.xy = clamp(Coords.xy, 0.001, 0.999);
        Coords.z = clamp(Coords.z, 0.0, 1.0);

        float Bias;
        if (IsGround) {
            Bias = 0.0001;
        } else {
            float SurfaceAngle = dot(N_Final, SunLightDir);
            Bias = max(0.0005, 0.003 * (1.0 - SurfaceAngle));
        }

        // 2x2 PCF
        vec2 TexelSize = vec2(1.0/1024.0, 1.0/1024.0);
        float Visibility = 0.0;
        float Samples = 0.0;
        for(int x = 0; x <= 1; ++x) {
            for(int y = 0; y <= 1; ++y) {
                vec2 Offset = vec2(float(x) - 0.5, float(y) - 0.5) * TexelSize * 1.5;
                vec2 SampleCoord = clamp(Coords.xy + Offset, 0.001, 0.999);
                float Depth = texture2D(s_TexShadow, SampleCoord).r;
                Visibility += step(Coords.z, Depth + Bias);
                Samples += 1.0;
            }
        }
        Shadow = Visibility / Samples;

        if (IsGround) {
            Shadow = mix(0.2, 1.0, Shadow);
        } else {
            Shadow = mix(0.25, 1.0, Shadow);
        }
    } else if (IsGround) {
        // Fallback for ground
        Coords.xy = clamp(Coords.xy, 0.001, 0.999);
        float Depth = texture2D(s_TexShadow, Coords.xy).r;
        if (Depth > 0.001 && Depth < 0.999) {
            Shadow = step(Coords.z, Depth + 0.0001);
        } else {
            Shadow = 1.0;
        }
        Shadow = mix(0.2, 1.0, Shadow);
    }

    // Material Albedo
    vec3 TexAlbedo = texture2D(s_TexAlbedo, TexCoord).rgb;
    bool HasTexture = (TexAlbedo.r < 0.99 || TexAlbedo.g < 0.99 || TexAlbedo.b < 0.99);

    float BlendMode = u_TextureBlend.x;
    float BlendFactor = u_TextureBlend.y;

    if (BlendMode > 0.5 && BlendFactor > 0.0) {
        vec3 DetailColor = texture2D(s_TexAlbedo2, TexCoord * 4.0).rgb;
        vec3 BlendMask = texture2D(s_TexBlendMask, TexCoord).rgb;
        float MaskValue = BlendMask.r;

        if (BlendMode < 1.5) {
            TexAlbedo = mix(TexAlbedo, TexAlbedo * DetailColor, MaskValue * BlendFactor);
        } else if (BlendMode < 2.5) {
            vec3 Overlay = mix(
                TexAlbedo * 2.0 * DetailColor,
                1.0 - 2.0 * (1.0 - TexAlbedo) * (1.0 - DetailColor),
                step(0.5, TexAlbedo)
            );
            TexAlbedo = mix(TexAlbedo, Overlay, MaskValue * BlendFactor);
        } else if (BlendMode < 3.5) {
            TexAlbedo = mix(TexAlbedo, TexAlbedo + DetailColor * 0.5, MaskValue * BlendFactor);
        } else {
            TexAlbedo = mix(TexAlbedo, DetailColor, MaskValue * BlendFactor);
        }
    }

    vec3 Albedo;
    if (HasTexture) {
        Albedo = mix(TexAlbedo, TexAlbedo * u_AlbedoColor.rgb, 0.1);
    } else {
        Albedo = u_AlbedoColor.rgb;
    }

    // PBR Setup
    vec3 F0 = mix(vec3_splat(0.04), Albedo, Metallic);
    float NdotV = max(dot(N_Final, ViewDir), 0.0);

    // Fresnel-Schlick
    vec3 F = F0 + (vec3_splat(1.0) - F0) * pow(1.0 - NdotV, 5.0);

    vec3 DiffuseTerm = (1.0 - Metallic) * Albedo * Diffuse;
    vec3 SpecularTerm = F * Specular * (1.0 + Metallic * 2.0);

    // Dynamic Lighting (Directional + Point Lights)
    vec3 DirectionalLighting = mix(
        (Ambient + (Diffuse + Specular) * Shadow) * Albedo,
        Ambient * Albedo * 0.5 + (DiffuseTerm + SpecularTerm * 2.0) * Shadow,
        Metallic
    );

    // Ensure minimum
    DirectionalLighting = max(DirectionalLighting, Albedo * 0.15);

    // Accumulate Point Lights
    vec3 PointLighting = vec3_splat(0.0);
    int NumPointLights = int(u_NumPointLights.x);
    // Limit to 32
    if (NumPointLights > 32) NumPointLights = 32;

    for (int i = 0; i < NumPointLights; ++i)
    {
        PointLighting += CalculatePointLight(
            PixelPosition, N_Final, ViewDir,
            u_PointLightPos[i].xyz,
            u_PointLightColor[i].rgb,
            u_PointLightColor[i].w, // Intensity
            u_PointLightPos[i].w,   // Range
            u_PointLightParams[i].x, // Attenuation
            Albedo, Roughness, Metallic, F0
        );
    }

    vec3 TotalLighting = DirectionalLighting + PointLighting + LightmapColor * Albedo;

    // Add emission
    vec3 Emission = u_Emission.rgb * u_Emission.w;

    vec3 LinearColor = TotalLighting + Emission;
    float FinalAlpha = 1.0;

    if (IsGlass) {
        LinearColor = mix(TotalLighting, ReflectionColor, Fresnel * 0.9);
        LinearColor += Specular * (1.0 - Fresnel) * 0.5;
        FinalAlpha = mix(Opacity, 1.0, Fresnel * 0.8);
    }

    vec3 FinalColor = LinearColor + RimColor * (1.0 - Metallic * 0.3);

    gl_FragColor = vec4(FinalColor, FinalAlpha);
}

