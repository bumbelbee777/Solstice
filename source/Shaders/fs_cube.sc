$input v_position, v_normal
#include <bgfx_shader.sh>

// Material uniform (albedo color)
uniform vec4 u_albedoColor; // RGB + roughness/metallic

void main()
{
    vec3 n = normalize(v_normal);
    
    // Directional Light (Sun)
    vec3 lightDir = normalize(vec3(0.5, 1.0, -0.5));
    vec3 lightColor = vec3(1.0, 0.98, 0.95); // Warm sun
    
    // Hemispheric Ambient
    vec3 skyColor = vec3(0.16, 0.20, 0.28); // Blueish sky
    vec3 groundColor = vec3(0.1, 0.1, 0.1); // Dark ground
    float hemiMix = n.y * 0.5 + 0.5;
    vec3 ambient = mix(groundColor, skyColor, hemiMix);

    // Diffuse (Lambert)
    float diff = max(dot(n, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular (Blinn-Phong)
    // Simplified view direction (assuming camera is somewhat above/front)
    vec3 halfDir = normalize(lightDir + vec3(0, 1, 0)); 
    float spec = pow(max(dot(n, halfDir), 0.0), 64.0);
    vec3 specular = spec * lightColor * 0.4;

    // Combine
    vec3 albedo = u_albedoColor.rgb;
    vec3 linearColor = (ambient + diffuse) * albedo + specular;
    
    // Gamma Correction (Linear -> sRGB)
    vec3 finalColor = pow(linearColor, vec3(1.0/2.2, 1.0/2.2, 1.0/2.2));

    gl_FragColor = vec4(finalColor, 1.0);
}