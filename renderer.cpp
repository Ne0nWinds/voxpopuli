#include "template.h"

static float3 MouseLocation;

float3 RandomPointInLight(float3 Normal) {
	float3 Result = MouseLocation;

	Result += Normal / WORLDSIZE * 8;

	Result.x += RandomFloat() / WORLDSIZE;
	Result.y += RandomFloat() / WORLDSIZE;
	Result.z += RandomFloat() / WORLDSIZE;
	return Result;
}

// -----------------------------------------------------------
// Calculate light transport via a ray
// -----------------------------------------------------------
float3 Renderer::Trace( Ray& ray, int, int, int /* we'll use these later */ )
{
	scene.FindNearest( ray );
	if (ray.voxel == 0) return float3( 0.1f, 0.1f, 0.1f ); // or a fancy sky color
	float3 I = ray.IntersectionPoint();
	float3 L = RandomPointInLight(ray.GetNormal()) - I;
	float distance = length(L);
	L = normalize(L);
	float cosa = max(0.0f, dot(ray.GetNormal(), L));
	Ray shadowRay(I, L, distance);

	float3 ambient = 1.0 / 16.0 * ray.GetAlbedo();
	// ambient = 0.0f;

	if (scene.IsOccluded(shadowRay)) return ambient;
	float distanceSquared = max(pow2f(distance), 1.0f);

	float3 result = 1.0f * ray.GetAlbedo() * cosa / distanceSquared;
	result.x = max(result.x, ambient.x);
	result.y = max(result.y, ambient.y);
	result.z = max(result.z, ambient.z);
	return result;
}

// -----------------------------------------------------------
// Application initialization - Executed once, at app start
// -----------------------------------------------------------
void Renderer::Init()
{
	accumulator = new float3[SCRWIDTH * SCRHEIGHT];
	memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
}

// -----------------------------------------------------------
// Main application tick function - Executed every frame
// -----------------------------------------------------------
void Renderer::Tick( float deltaTime )
{
	// high-resolution timer, see template.h
	Timer t;
	static int spp = 1;
	const float scale = 1.0f / spp++;

	// pixel loop: lines are executed as OpenMP parallel tasks (disabled in DEBUG)
#pragma omp parallel for schedule(dynamic)
	for (int y = 0; y < SCRHEIGHT; y++)
	{
		// trace a primary ray for each pixel on the line
		for (int x = 0; x < SCRWIDTH; x++)
		{
#if 1
			Ray r = camera.GetPrimaryRay( x + RandomFloat(), y + RandomFloat());
			float3 sample = Trace( r );
			float3 &previousSample = accumulator[x + y * SCRWIDTH];
			previousSample += sample;
			float3 average = previousSample * scale;
			screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8(average);
#else
			Ray r = camera.GetPrimaryRay( x, y );
			float3 sample = Trace( r );
			screen->pixels[x + y * SCRWIDTH] = RGBF32_to_RGB8(sample);
#endif
		}
	}
	// performance report - running average - ms, MRays/s
	static float avg = 10, alpha = 1;
	avg = (1 - alpha) * avg + alpha * t.elapsed() * 1000;
	if (alpha > 0.05f) alpha *= 0.5f;
	float fps = 1000.0f / avg, rps = (SCRWIDTH * SCRHEIGHT) / avg;
	printf( "%5.2fms (%.1ffps) - %.1fMrays/s\n", avg, fps, rps / 1000 );
	// handle user input
	static uint32_t mousePosX;
	static uint32_t mousePosY;

	bool MouseLocationChanged = mousePos.x != mousePosX || mousePos.y != mousePosY;

	if (camera.HandleInput(deltaTime) || MouseLocationChanged) {
		spp = 1;
		mousePosX = mousePos.x;
		mousePosY = mousePos.y;
		memset(accumulator, 0, SCRWIDTH * SCRHEIGHT * sizeof(float3));
	}
}

// -----------------------------------------------------------
// Update user interface (imgui)
// -----------------------------------------------------------
void Renderer::UI()
{
	// ray query on mouse
	Ray r = camera.GetPrimaryRay( (float)mousePos.x, (float)mousePos.y );
	scene.FindNearest( r );
	MouseLocation = r.IntersectionPoint();

	MouseLocation.x = max(min(MouseLocation.x, 1.0f), 0.0f);
	MouseLocation.y = max(min(MouseLocation.y, 1.0f), 0.0f);
	MouseLocation.z = max(min(MouseLocation.z, 1.0f), 0.0f);

	ImGui::Text( "voxel: %i", r.voxel );

	float3 I = MouseLocation;
	ImGui::Text( "intersection: %.3f, %.3f, %.3f", I.x, I.y, I.z );
}