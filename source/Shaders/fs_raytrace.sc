$input TexCoord

/*
 * Raytracing Fragment Shader
 * Applies raytraced shadows and AO to the scene
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);        // Main scene color
SAMPLER2D(s_texShadowRT, 1);     // Raytraced shadow map
SAMPLER2D(s_texAORT, 2);         // Raytraced AO map

uniform vec4 u_lightData[4];      // Light data: xyz=position/direction, w=intensity
uniform vec4 u_lightColor[4];     // Light color: rgb=color, w=attenuation
uniform vec4 u_lightCount;        // x: light count (0-4)
uniform vec4 u_raytraceParams;    // x: AO radius, y: AO samples, z: shadow intensity, w: AO intensity

void main()
{
	vec2 uv = TexCoord;

	// Sample main scene color
	vec3 color = texture2D(s_texColor, uv).rgb;

	// Sample raytraced shadows
	float shadow = 1.0;
	if (u_lightCount.x > 0.0) {
		shadow = texture2D(s_texShadowRT, uv).r;
		// Apply shadow intensity
		float shadowIntensity = u_raytraceParams.z > 0.0 ? u_raytraceParams.z : 1.0;
		shadow = mix(1.0, shadow, shadowIntensity);
	}

	// Sample raytraced AO
	float ao = 1.0;
	ao = texture2D(s_texAORT, uv).r;
	// Apply AO intensity
	float aoIntensity = u_raytraceParams.w > 0.0 ? u_raytraceParams.w : 1.0;
	ao = mix(1.0, ao, aoIntensity);

	// Apply lighting contributions from raytraced lights
	vec3 lightContribution = vec3_splat(0.0);
	int lightCount = int(u_lightCount.x);

	for (int i = 0; i < 4 && i < lightCount; ++i) {
		vec3 lightPos = u_lightData[i].xyz;
		float lightIntensity = u_lightData[i].w;
		vec3 lightColor = u_lightColor[i].rgb;
		float attenuation = u_lightColor[i].w;

		// Simple lighting contribution (would use normal from G-buffer in production)
		// For now, just add ambient light contribution
		lightContribution += lightColor * lightIntensity * 0.1; // Ambient term
	}

	// Combine: color * shadow * AO + light contributions
	color = color * shadow * ao + lightContribution;

	// Clamp to valid range
	color = clamp(color, vec3_splat(0.0), vec3_splat(10.0));

	gl_FragColor = vec4(color, 1.0);
}
