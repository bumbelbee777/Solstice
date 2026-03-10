$input PixelPosition, PixelNormal, ShadowCoord, TexCoord, PixelTangent, PixelBitangent
#include <bgfx_shader.sh>

uniform vec4 u_AlbedoColor; // RGB + roughness
uniform vec4 u_MaterialParams; // x: metallic, y: roughness (if not from texture), z: IOR (Index of Refraction), w: unused
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

// GGX Normal Distribution Function (Trowbridge-Reitz)
float DistributionGGX(float NdotH, float Roughness)
{
    float Alpha = Roughness * Roughness;
    float Alpha2 = Alpha * Alpha;
    float NdotH2 = NdotH * NdotH;
    float Denom = (NdotH2 * (Alpha2 - 1.0) + 1.0);
    Denom = 3.14159 * Denom * Denom;
    return Alpha2 / max(Denom, 0.0000001);
}

// Smith Geometry Term (simplified, branchless)
float GeometrySchlickGGX(float NdotV, float Roughness)
{
    float R = Roughness + 1.0;
    float K = (R * R) / 8.0; // Direct lighting
    float Denom = NdotV * (1.0 - K) + K;
    return NdotV / max(Denom, 0.0000001);
}

// Smith Geometry (combined)
float GeometrySmith(float NdotV, float NdotL, float Roughness)
{
    float GGXV = GeometrySchlickGGX(NdotV, Roughness);
    float GGXL = GeometrySchlickGGX(NdotL, Roughness);
    return GGXV * GGXL;
}

// Fresnel-Schlick (already optimized, branchless)
vec3 FresnelSchlick(float CosTheta, vec3 F0)
{
    return F0 + (vec3_splat(1.0) - F0) * pow(clamp(1.0 - CosTheta, 0.0, 1.0), 5.0);
}

vec3 CalculatePointLight(vec3 WorldPos, vec3 Normal, vec3 ViewDir,
                        vec3 LightPos, vec3 LightColor, float Intensity, float Range, float Attenuation,
                        vec3 Albedo, float Roughness, float Metallic, vec3 F0)
{
    vec3 L = LightPos - WorldPos;
    float Dist = length(L);

    // Branchless range check
    float RangeCheck = step(Dist, Range) + step(Range, 0.0);
    RangeCheck = mix(0.0, 1.0, RangeCheck);
    if (RangeCheck < 0.5) return vec3_splat(0.0);

    L = normalize(L);
    vec3 H = normalize(L + ViewDir);

    // Attenuation (Inverse Square + Range fade) - branchless
    float Atten = 1.0 / (1.0 + Attenuation * Dist * Dist);
    float RangeFactor = mix(1.0, clamp(1.0 - pow(Dist / Range, 4.0), 0.0, 1.0), step(0.0, Range));
    Atten *= RangeFactor * RangeFactor;

    vec3 Radiance = LightColor * Intensity * Atten;

    // Cook-Torrance BRDF (proper implementation)
    float NdotL = max(dot(Normal, L), 0.0);
    float NdotH = max(dot(Normal, H), 0.0);
    float NdotV = max(dot(Normal, ViewDir), 0.0);
    float VdotH = max(dot(ViewDir, H), 0.0);

    // GGX Normal Distribution
    float D = DistributionGGX(NdotH, Roughness);

    // Smith Geometry Term
    float G = GeometrySmith(NdotV, NdotL, Roughness);

    // Fresnel-Schlick
    vec3 F = FresnelSchlick(VdotH, F0);

    // Cook-Torrance Specular BRDF: (D * G * F) / (4 * NdotV * NdotL)
    vec3 Numerator = D * G * F;
    float Denominator = 4.0 * NdotV * NdotL + 0.001; // Avoid division by zero
    vec3 Specular = Numerator / Denominator;

    // Energy conservation: KD = (1.0 - F) * (1.0 - Metallic)
    vec3 KD = (vec3_splat(1.0) - F) * (vec3_splat(1.0) - Metallic);
    vec3 Diffuse = KD * Albedo / 3.14159;

    // Final BRDF
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

    // Sample roughness texture (branchless)
    float RoughnessTex = texture2D(s_TexRoughness, TexCoord).r;
    Roughness = mix(Roughness, max(0.05, RoughnessTex), step(RoughnessTex, 0.98));

    // Sample metallic texture (branchless)
    float MetallicTex = texture2D(s_TexMetallic, TexCoord).r;
    Metallic = mix(Metallic, MetallicTex, step(0.02, MetallicTex));

    // Material Albedo (compute early for PBR)
    vec3 TexAlbedo = texture2D(s_TexAlbedo, TexCoord).rgb;
    bool HasTexture = (TexAlbedo.r < 0.99 || TexAlbedo.g < 0.99 || TexAlbedo.b < 0.99);

    float BlendMode = u_TextureBlend.x;
    float BlendFactor = u_TextureBlend.y;

    // Texture blending
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

    // Branchless albedo selection
    vec3 Albedo = mix(u_AlbedoColor.rgb, mix(TexAlbedo, TexAlbedo * u_AlbedoColor.rgb, 0.1),
                      step(0.01, float(HasTexture)));

    // For glass materials, zero out albedo (glass is transparent, color comes from reflections)
    float IsGlass = 1.0 - step(0.95, Opacity); // 1.0 if glass, 0.0 if not glass
    float IsGlassFactor = step(0.95, Opacity); // 1.0 if not glass, 0.0 if glass (inverse of IsGlass)
    Albedo = mix(Albedo, vec3_splat(0.0), IsGlass);

    // PBR Setup (compute F0 for Fresnel)
    // Glass should use dielectric F0 (0.04), not albedo-based F0
    vec3 F0 = mix(vec3_splat(0.04), Albedo, Metallic);
    F0 = mix(vec3_splat(0.04), F0, IsGlass); // Force F0 to 0.04 for glass
    float NdotV = max(dot(N_Final, ViewDir), 0.0);

    // Cook-Torrance BRDF for directional light (proper PBR)
    float NdotL = max(dot(N_Final, SunLightDir), 0.0);
    float NdotH = max(dot(N_Final, HalfDir), 0.0);
    float VdotH = max(dot(ViewDir, HalfDir), 0.0);

    // GGX Normal Distribution
    float Alpha = Roughness * Roughness;
    float Alpha2 = Alpha * Alpha;
    float NdotH2 = NdotH * NdotH;
    float Denom = (NdotH2 * (Alpha2 - 1.0) + 1.0);
    Denom = 3.14159 * Denom * Denom;
    float D = Alpha2 / max(Denom, 0.0000001);

    // Smith Geometry Term (branchless)
    float R = Roughness + 1.0;
    float K = (R * R) / 8.0;
    float GGXV = NdotV / max(NdotV * (1.0 - K) + K, 0.0000001);
    float GGXL = NdotL / max(NdotL * (1.0 - K) + K, 0.0000001);
    float G = GGXV * GGXL;

    // Fresnel-Schlick
    vec3 F = FresnelSchlick(VdotH, F0);

    // Cook-Torrance Specular: (D * G * F) / (4 * NdotV * NdotL)
    vec3 Numerator = vec3_splat(D * G) * F;
    float Denominator = 4.0 * NdotV * NdotL + 0.001;
    vec3 Specular = (Numerator / Denominator) * SunLightColor;

    // Rim Light
    float Rim = 1.0 - max(dot(N_Final, ViewDir), 0.0);
    Rim = pow(Rim, 3.0);
    vec3 RimColor = vec3(0.5, 0.6, 0.8) * Rim * 0.5;

    // Fresnel
    float Fresnel = pow(1.0 - max(dot(N_Final, ViewDir), 0.0), 2.0);

    // Get IOR from material params (default 1.0 for non-glass materials)
    float IOR = u_MaterialParams.z;
    if (IOR < 1.0) IOR = 1.0; // Clamp minimum IOR

    // Environment reflection (branchless)
    vec3 ReflectionColor = textureCube(s_TexEnvironment, ReflectDir).rgb;
    // Glass materials need stronger reflections to show geometry
    float ReflectionStrength = mix(1.2, 0.8, IsGlassFactor); // Stronger for glass (1.2), weaker for opaque (0.8)
    ReflectionColor *= Fresnel * ReflectionStrength;
    ReflectionColor = mix(ReflectionColor, vec3_splat(0.0), step(0.95, Opacity)); // Keep reflections for glass, remove for opaque

    // Refraction for glass materials
    vec3 RefractionColor = vec3_splat(0.0);
    if (IsGlass > 0.5) {
        // Calculate refraction direction using Snell's law
        // refract(I, N, eta) where eta = IOR_air / IOR_glass = 1.0 / IOR
        float eta = 1.0 / IOR;
        vec3 RefractDir = refract(-ViewDir, N_Final, eta);
        
        // If refraction fails (total internal reflection), use reflection instead
        float refractValid = step(0.0, dot(RefractDir, RefractDir)); // 1.0 if valid, 0.0 if invalid
        RefractDir = mix(ReflectDir, RefractDir, refractValid);
        
        // Sample environment with refracted direction
        RefractionColor = textureCube(s_TexEnvironment, RefractDir).rgb;
    }

    // Shadow calculation (optimized with branchless conditionals)
    float Shadow = 1.0;
    vec3 Coords = ShadowCoord.xyz;
    // Branchless perspective divide
    float WCheck = step(0.0001, abs(ShadowCoord.w));
    Coords = mix(Coords, ShadowCoord.xyz / ShadowCoord.w, WCheck);
    Coords = Coords * 0.5 + 0.5;

    // Branchless ground check
    float IsGround = step(0.7, N_Final.y);
    float InBounds = step(Coords.x, 3.0) * step(-2.0, Coords.x) *
                     step(Coords.y, 3.0) * step(-2.0, Coords.y) *
                     step(Coords.z, 2.0) * step(-1.0, Coords.z);

    // Branchless bias calculation
    float SurfaceAngle = dot(N_Final, SunLightDir);
    float BiasGround = 0.0001;
    float BiasOther = max(0.0005, 0.003 * (1.0 - SurfaceAngle));
    float Bias = mix(BiasOther, BiasGround, IsGround);

    // 2x2 PCF (always compute, use InBounds to mask)
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
    float RawShadow = Visibility / Samples;

    // Branchless shadow mixing
    float ShadowMinGround = 0.2;
    float ShadowMinOther = 0.25;
    float ShadowMin = mix(ShadowMinOther, ShadowMinGround, IsGround);
    float InBoundsShadow = mix(ShadowMin, 1.0, RawShadow);

    // Fallback for out-of-bounds ground (branchless)
    float OutOfBoundsGround = (1.0 - InBounds) * IsGround;
    vec2 FallbackCoords = clamp(Coords.xy, 0.001, 0.999);
    float FallbackDepth = texture2D(s_TexShadow, FallbackCoords).r;
    float FallbackShadow = mix(1.0, step(Coords.z, FallbackDepth + 0.0001),
                               step(0.001, FallbackDepth) * step(FallbackDepth, 0.999));
    FallbackShadow = mix(ShadowMinGround, 1.0, FallbackShadow);

    // Combine shadows (branchless)
    Shadow = mix(InBoundsShadow, FallbackShadow, OutOfBoundsGround);
    Shadow = mix(Shadow, 1.0, 1.0 - InBounds - OutOfBoundsGround); // Default for out-of-bounds non-ground

    // Albedo and PBR setup already computed earlier (lines 218-249)
    // Energy-conserving diffuse term (F already computed above for directional light)
    vec3 KD = (vec3_splat(1.0) - F) * (vec3_splat(1.0) - Metallic);
    vec3 DiffuseTerm = KD * Albedo * Diffuse;

    // Proper Cook-Torrance specular term (already computed above)
    vec3 SpecularTerm = Specular * Shadow;

    // Dynamic Lighting (Directional + Point Lights) - proper PBR
    vec3 DirectionalLighting = (Ambient * Albedo * 0.5 + (DiffuseTerm + SpecularTerm)) * Shadow;

    // Ensure minimum (but not for glass - glass should be dark/transparent)
    vec3 MinLighting = mix(Albedo * 0.15, vec3_splat(0.0), IsGlass);
    DirectionalLighting = max(DirectionalLighting, MinLighting);

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

    // Branchless glass handling
    // IsGlassFactor already declared earlier, reuse it here
    // For glass, blend refraction and reflection based on Fresnel
    // More reflection at grazing angles (Fresnel), more refraction at normal angles
    vec3 GlassColor = mix(RefractionColor, ReflectionColor, Fresnel) * 1.2;
    GlassColor += SpecularTerm * (1.0 - Fresnel) * 0.3;
    LinearColor = mix(GlassColor, LinearColor, IsGlassFactor);

    // Pack roughness into alpha for opaque materials (used by post reflections).
    float OpaqueAlpha = clamp(Roughness, 0.05, 1.0);
    float TransparentAlpha = mix(Opacity, 1.0, Fresnel * 0.8);
    float FinalAlpha = mix(TransparentAlpha, OpaqueAlpha, IsGlassFactor);

    vec3 FinalColor = LinearColor + RimColor * (1.0 - Metallic * 0.3);

    gl_FragColor = vec4(FinalColor, FinalAlpha);
}

