$input v_clip

#include <bgfx_shader.sh>

SAMPLER3D(s_texVolume, 0);

uniform mat4 u_fluidInvViewProj;
uniform vec4 u_fluidCameraPos;
uniform vec4 u_fluidAabbMin;
uniform vec4 u_fluidAabbMax;
/// xy: viewport w,h; zw: 1/w, 1/h — NDC from gl_FragCoord (matches scene camera / post blit).
uniform vec4 u_fluidViewport;
/// x: mode (0 temp colormap, 1 schlieren grayscale), yzw unused
uniform vec4 u_fluidVisParams;
/// World AABB; x>0.5 in u_fluidClipParams enables clipping (samples outside contribute nothing).
uniform vec4 u_fluidClipMin;
uniform vec4 u_fluidClipMax;
uniform vec4 u_fluidClipParams;

bool intersectAABB(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float tnear, out float tfar)
{
	vec3 invRd = 1.0 / (rd + 1e-20);
	vec3 t0 = (bmin - ro) * invRd;
	vec3 t1 = (bmax - ro) * invRd;
	vec3 tsm = min(t0, t1);
	vec3 tlg = max(t0, t1);
	tnear = max(max(tsm.x, tsm.y), tsm.z);
	tfar = min(min(tlg.x, tlg.y), tlg.z);
	return tfar > max(tnear, 0.0);
}

vec3 heatmap(float t)
{
	t = clamp(t, 0.0, 1.0);
	// Saturated cool → warm (temperature mode): blue → cyan → yellow → red.
	vec3 c0 = vec3(0.06, 0.12, 0.92);
	vec3 c1 = vec3(0.15, 0.75, 1.0);
	vec3 c2 = vec3(0.98, 0.92, 0.18);
	vec3 c3 = vec3(0.98, 0.14, 0.06);
	if (t < 0.34) {
		return mix(c0, c1, t / 0.34);
	}
	if (t < 0.67) {
		return mix(c1, c2, (t - 0.34) / 0.33);
	}
	return mix(c2, c3, (t - 0.67) / 0.33);
}

void main()
{
	// Pixel-centered NDC; Y flip for top-left fragment origin (matches D3D/bgfx + our Perspective).
	vec2 ndc;
	ndc.x = gl_FragCoord.x * u_fluidViewport.z * 2.0 - 1.0;
	ndc.y = 1.0 - gl_FragCoord.y * u_fluidViewport.w * 2.0;
	vec4 farCS = vec4(ndc, 1.0, 1.0);
	vec4 farWS = mul(u_fluidInvViewProj, farCS);
	farWS.xyz /= max(farWS.w, 1e-5);
	vec3 ro = u_fluidCameraPos.xyz;
	vec3 rd = normalize(farWS.xyz - ro);

	float t0, t1;
	if (!intersectAABB(ro, rd, u_fluidAabbMin.xyz, u_fluidAabbMax.xyz, t0, t1))
	{
		gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);
		return;
	}

	const int STEPS = 128;
	float len = max(t1 - t0, 1e-4);
	float dt = len / float(STEPS);

	// Break up coherent “checkerboard / slab” artifacts from a fixed step count vs coarse voxels.
	float jitter = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
	float tStart = t0 + jitter * dt;

	vec3 acc = vec3(0.0, 0.0, 0.0);
	float a = 0.0;
	float mode = u_fluidVisParams.x;

	for (int i = 0; i < STEPS; ++i)
	{
		float t = tStart + float(i) * dt;
		vec3 p = ro + rd * t;
		if (u_fluidClipParams.x > 0.5) {
			if (any(lessThan(p, u_fluidClipMin.xyz)) || any(greaterThan(p, u_fluidClipMax.xyz))) {
				continue;
			}
		}
		vec3 uvw = (p - u_fluidAabbMin.xyz) / max(u_fluidAabbMax.xyz - u_fluidAabbMin.xyz, vec3(1e-5, 1e-5, 1e-5));
		if (any(lessThan(uvw, vec3(0.0, 0.0, 0.0))) || any(greaterThan(uvw, vec3(1.0, 1.0, 1.0))))
			continue;

		float s = texture3D(s_texVolume, uvw).r;
		// Light edge softening only; strong smoothstep was washing out narrow R8 bands.
		s = smoothstep(0.005, 0.995, s);
		// Temperature: boost contrast vs vertical stratification (R8 often clusters near mid-gray).
		if (mode < 0.5) {
			s = clamp(0.5 + (s - 0.5) * 2.4, 0.0, 1.0);
		}
		vec3 col = (mode < 0.5) ? heatmap(s) : vec3(s, s, s);
		// Stronger per-step density so the plume reads clearly over the scene.
		float dens = (0.2 + 0.8 * s) * (1.45 / float(STEPS));
		col *= 1.38;
		acc += col * dens * (1.0 - a);
		a += dens * (1.0 - a);
		if (a > 0.985) break;
	}

	gl_FragColor = vec4(acc, clamp(a * 0.97, 0.0, 0.96));
}
