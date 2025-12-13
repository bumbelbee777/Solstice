$input v_position, v_normal, v_shadowcoord
#include <bgfx_shader.sh>

uniform vec4 u_albedoColor; // RGB + roughness
SAMPLER2D(s_texShadow, 1);

float SampleShadow(sampler2D _sampler, vec3 _coord, float _bias)
{
    float visible = 1.0;
    if (_coord.z > texture2D(_sampler, _coord.xy).r + _bias) { visible = 0.0; }
    return visible;
}

float SampleShadowPCF(sampler2D _sampler, vec3 _coord, float _bias, vec2 _texelSize)
{
    float visibility = 0.0;
    // Optimized 2x2 PCF (4 samples)
    for(int x = 0; x <= 1; ++x)
    {
        for(int y = 0; y <= 1; ++y)
        {
            visibility += SampleShadow(_sampler, _coord + vec3(vec2(float(x)-0.5, float(y)-0.5) * _texelSize, 0.0), _bias);
        }
    }
    return visibility * 0.25;
}

void main()
{
    vec3 n = normalize(v_normal);
    
    // Lighting Setup
    vec3 lightDir = normalize(vec3(0.5, 1.0, -0.5));
    vec3 lightColor = vec3(1.0, 0.95, 0.90) * 1.5; // Brighter sun
    
    // Ambient
    // Ambient
    vec3 skyColor = vec3(0.4, 0.6, 0.9); // Sky Blue
    vec3 groundColor = vec3(0.05, 0.05, 0.05); // Dark ground
    float hemiMix = n.y * 0.5 + 0.5;
    vec3 ambient = mix(groundColor, skyColor, hemiMix) * 0.3;

    // Diffuse
    float diff = max(dot(n, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    vec3 viewDir = normalize(vec3(0, 5, 20) - v_position); // Approx view pos
    vec3 halfDir = normalize(lightDir + viewDir); 
    float roughness = max(0.05, u_albedoColor.w);
    float specPower = 2.0 / (roughness * roughness) - 2.0;
    float spec = pow(max(dot(n, halfDir), 0.0), specPower);
    vec3 specular = spec * lightColor * 0.5;

    // Rim Light (Fresnel-ish)
    float rim = 1.0 - max(dot(n, viewDir), 0.0);
    rim = pow(rim, 3.0);
    vec3 rimColor = vec3(0.5, 0.6, 0.8) * rim * 0.5;

    // Shadow
    float shadow = 1.0;
    vec3 shadowCoord = v_shadowcoord.xyz / v_shadowcoord.w;
    shadowCoord = shadowCoord * 0.5 + 0.5;
    
    if (shadowCoord.x >= 0.0 && shadowCoord.x <= 1.0 &&
        shadowCoord.y >= 0.0 && shadowCoord.y <= 1.0 &&
        shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0) 
    {
        // PCF
        float bias = 0.002;
        shadow = SampleShadowPCF(s_texShadow, shadowCoord, bias, vec2(1.0/1024.0, 1.0/1024.0));
        // Tone down shadow a bit (GI approximation)
        shadow = mix(0.2, 1.0, shadow); 
    }

    // Material
    vec3 albedo = u_albedoColor.rgb;
    
    // Final Composition (Linear Space)
    vec3 linearColor = (ambient + (diffuse + specular) * shadow) * albedo + rimColor;

    // NO Gamma Correction here! We write to RGBA16F for PostProcessing.
    gl_FragColor = vec4(linearColor, 1.0);
}