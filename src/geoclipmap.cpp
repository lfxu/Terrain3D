// Copyright © 2023 Cory Petkovsek, Roope Palmroos, and Contributors.

#include <godot_cpp/classes/rendering_server.hpp>

#include "geoclipmap.h"
#include "logger.h"

///////////////////////////
// Private Functions
///////////////////////////

RID GeoClipMap::_create_mesh(PackedVector3Array p_vertices, PackedInt32Array p_indices, AABB p_aabb) {
	Array arrays;
	arrays.resize(RenderingServer::ARRAY_MAX);
	arrays[RenderingServer::ARRAY_VERTEX] = p_vertices;
	arrays[RenderingServer::ARRAY_INDEX] = p_indices;

	PackedVector3Array normals;
	normals.resize(p_vertices.size());
	normals.fill(Vector3(0, 1, 0));
	arrays[RenderingServer::ARRAY_NORMAL] = normals;

	PackedFloat32Array tangents;
	tangents.resize(p_vertices.size() * 4);
	tangents.fill(0.0f);
	arrays[RenderingServer::ARRAY_TANGENT] = tangents;

	LOG(DEBUG, "Creating mesh via the Rendering server");
	RID mesh = RS->mesh_create();
	RS->mesh_add_surface_from_arrays(mesh, RenderingServer::PRIMITIVE_TRIANGLES, arrays);

	LOG(DEBUG, "Setting custom aabb: ", p_aabb.position, ", ", p_aabb.size);
	RS->mesh_set_custom_aabb(mesh, p_aabb);

	return mesh;
}

///////////////////////////
// Public Functions
///////////////////////////

/* Generate clipmap meshes originally by Mike J Savage
 * Article https://mikejsavage.co.uk/blog/geometry-clipmaps.html
 * Code http://git.mikejsavage.co.uk/medfall/file/clipmap.cc.html#l197
 * In email communication with Cory, Mike clarified that the code in his
 * repo can be considered either MIT or public domain.
 */
Vector<RID> GeoClipMap::generate(int p_size, int p_levels) {
	LOG(DEBUG, "Generating meshes of size: ", p_size, " levels: ", p_levels);

	RID tile_mesh;
	AABB aabb;
	int n = 0;

	// Create a tile mesh.
	// A p_size * p_size grid plus normal or subdividing stripes on four sides.
	// Below is a example of a p_size = 2.
	// Different portions of the mesh are displayed at different part of
	// geometry clipmap by dropping vertexes in shader.
	// ┌───┬───┬───┬───┐
	// │ \ │ \ │ \ │ > │
	// ├───┼───┼───┼───┤
	// │ \ │ \ │ \ │ > │
	// ├───┼───o───┼───┤
	// │ \ │ \ │ \ │ > │
	// ├───┼───┼───┼───┘
	// │ v │ v │ v │
	// └───┴───┴───┘
	{
		PackedVector3Array vertices;
		vertices.resize((p_size + 3) * (p_size + 3) + (p_size + 1) * 2 - 1);
		PackedInt32Array indices;
		indices.resize((p_size + 1) * (p_size + 1) * 6 + (p_size + 1) * 9 * 2);

		Vector3 offset = Vector3(-p_size * 0.5f - 1.0f, 0, -p_size * 0.5f - 1.0f);
		n = 0;

		for (int y = 0; y < p_size + 3; y++) {
			for (int x = 0; x < p_size + 3; x++) {
				if (x == p_size + 2 && y == p_size + 2) {
					break;
				}
				vertices[n++] = Vector3(x, 0, y) + offset;
			}
		}
		for (int y = 0; y < p_size + 1; y++) {
			vertices[n++] = Vector3(p_size + 2, 0, y + 0.5f) + offset;
		}
		for (int x = 0; x < p_size + 1; x++) {
			vertices[n++] = Vector3(x + 0.5f, 0, p_size + 2) + offset;
		}

		n = 0;
		for (int y = 0; y < p_size + 1; y++) {
			for (int x = 0; x < p_size + 1; x++) {
				indices[n++] = _patch_2d(x, y, p_size + 3);
				indices[n++] = _patch_2d(x + 1, y + 1, p_size + 3);
				indices[n++] = _patch_2d(x, y + 1, p_size + 3);

				indices[n++] = _patch_2d(x, y, p_size + 3);
				indices[n++] = _patch_2d(x + 1, y, p_size + 3);
				indices[n++] = _patch_2d(x + 1, y + 1, p_size + 3);
			}
		}
		int m = (p_size + 3) * (p_size + 3) - 1;
		for (int y = 0; y < p_size + 1; y++) {
			indices[n++] = _patch_2d(p_size + 1, y, p_size + 3);
			indices[n++] = _patch_2d(p_size + 2, y, p_size + 3);
			indices[n++] = m;

			indices[n++] = _patch_2d(p_size + 1, y + 1, p_size + 3);
			indices[n++] = _patch_2d(p_size + 1, y, p_size + 3);
			indices[n++] = m;

			indices[n++] = _patch_2d(p_size + 2, y + 1, p_size + 3);
			indices[n++] = _patch_2d(p_size + 1, y + 1, p_size + 3);
			indices[n++] = m++;
		}
		for (int x = 0; x < p_size + 1; x++) {
			indices[n++] = _patch_2d(x, p_size + 2, p_size + 3);
			indices[n++] = _patch_2d(x, p_size + 1, p_size + 3);
			indices[n++] = m;

			indices[n++] = _patch_2d(x, p_size + 1, p_size + 3);
			indices[n++] = _patch_2d(x + 1, p_size + 1, p_size + 3);
			indices[n++] = m;

			indices[n++] = _patch_2d(x + 1, p_size + 1, p_size + 3);
			indices[n++] = _patch_2d(x + 1, p_size + 2, p_size + 3);
			indices[n++] = m++;
		}

		aabb = AABB(offset, Vector3(p_size + 2, 1, p_size + 2));
		tile_mesh = _create_mesh(vertices, indices, aabb);
	}

	Vector<RID> meshes = {
		tile_mesh,
	};

	return meshes;
}
