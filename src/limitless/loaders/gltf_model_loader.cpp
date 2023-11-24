#include "cgltf.h"

#include <chrono>
#include <cstdio>
#include <iostream>
#include <limitless/assets.hpp>
#include <limitless/camera.hpp>
#include <limitless/core/context.hpp>
#include <limitless/core/context_observer.hpp>
#include <limitless/core/indexed_stream.hpp>
#include <limitless/core/skeletal_stream.hpp>
#include <limitless/core/vertex.hpp>
#include <limitless/instances/model_instance.hpp>
#include <limitless/instances/skeletal_instance.hpp>
#include <limitless/loaders/gltf_model_loader.hpp>
#include <limitless/models/abstract_mesh.hpp>
#include <limitless/models/abstract_model.hpp>
#include <limitless/models/bones.hpp>
#include <limitless/models/line.hpp>
#include <limitless/models/mesh.hpp>
#include <limitless/models/model.hpp>
#include <limitless/models/skeletal_model.hpp>
#include <limitless/ms/material_builder.hpp>
#include <limitless/ms/property.hpp>
#include <limitless/pipeline/shader_type.hpp>
#include <limitless/renderer/renderer.hpp>
#include <limitless/scene.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace Limitless;

static std::string toString(cgltf_type type) {
	switch (type) {
	case cgltf_type_invalid:
		return "invalid";
	case cgltf_type_scalar:
		return "scalar";
	case cgltf_type_vec2:
		return "vec2";
	case cgltf_type_vec3:
		return "vec3";
	case cgltf_type_vec4:
		return "vec4";
	case cgltf_type_mat2:
		return "mat2";
	case cgltf_type_mat3:
		return "mat3";
	case cgltf_type_mat4:
		return "mat4";
	default:
		return "unknown(" + std::to_string(static_cast<int>(type)) + ")";
	}
}

static std::string toString(cgltf_component_type component_type) {
	switch (component_type) {
	case cgltf_component_type_invalid:
		return "invalid";
	case cgltf_component_type_r_8:
		return "signed byte";
	case cgltf_component_type_r_8u:
		return "unsigned byte";
	case cgltf_component_type_r_16:
		return "signed short";
	case cgltf_component_type_r_16u:
		return "unsigned short";
	case cgltf_component_type_r_32u:
		return "unsigned int";
	case cgltf_component_type_r_32f:
		return "float";
	default:
		return "unknown(" + std::to_string(static_cast<int>(component_type)) + ")";
	}
}

template <typename ElemType>
static std::vector<ElemType> copyFromAccessor(const cgltf_accessor& accessor) {
	if (accessor.is_sparse) {
		throw ModelLoadError {"sparse accessors not supported"};
	}

	if (std::is_same_v<ElemType, GLuint>
	    && (accessor.type != cgltf_type_scalar
	        || accessor.component_type != cgltf_component_type_r_32u)) {
		throw ModelLoadError {
			"GLuint should use scalar u32 type accessor, got " + toString(accessor.component_type)};
	}

	if (std::is_same_v<ElemType, glm::vec2>
	    && (accessor.type != cgltf_type_vec2
	        || accessor.component_type != cgltf_component_type_r_32f)) {
		throw ModelLoadError {"vec2 should use vec2 float type accessor"};
	}

	if (std::is_same_v<ElemType, glm::vec3>
	    && (accessor.type != cgltf_type_vec3
	        || accessor.component_type != cgltf_component_type_r_32f)) {
		throw ModelLoadError {
			"vec3 should use vec3 float type accessor, got " + toString(accessor.type) + ", "
			+ toString(accessor.component_type)};
	}

	if (std::is_same_v<ElemType, glm::vec4>
	    && (accessor.type != cgltf_type_vec4
	        || accessor.component_type != cgltf_component_type_r_32f)) {
		throw ModelLoadError {
			"vec4 should use vec4 float type accessor, got " + toString(accessor.type) + ", "
			+ toString(accessor.component_type)};
	}

	if (std::is_same_v<ElemType, unsigned short[4]>
	    && (accessor.type != cgltf_type_vec4
	        || accessor.component_type != cgltf_component_type_r_16u)) {
		throw ModelLoadError {
			"ushort4 should use vec4 short type accessor, got " + toString(accessor.type) + ", "
			+ toString(accessor.component_type)};
	}

	std::vector<ElemType> result;
	result.reserve(accessor.count);

	const uint8_t* data = static_cast<const uint8_t*>(accessor.buffer_view->buffer->data)
	                      + accessor.buffer_view->offset + accessor.offset;

	for (cgltf_size i = 0; i < accessor.count; ++i) {
		result.emplace_back(*reinterpret_cast<const ElemType*>(data));
		data += accessor.stride;
	}

	return result;
}

static glm::vec4 toVec4(const float (&src)[4]) {
	return glm::vec4 {src[0], src[1], src[2], src[3]};
}

static glm::vec3 toVec3(const float (&src)[3]) {
	return glm::vec3 {src[0], src[1], src[2]};
}

static glm::quat toQuat(const float (&src)[4]) {
	glm::quat result;
	// GLM quaternion storage depends on GLM_FORCE_QUAT_DATA_XYZW macro
	// definition. Enforce proper assignment in any case.
	result.x = src[0];
	result.y = src[1];
	result.z = src[2];
	result.w = src[3];

	return result;
}

// Convert GLTF quaternion to GLM one.
// GLTF quaternion array has (x, y, z, w) order, where w is the scalar.
static glm::quat toQuat(const std::array<float, 4>& src) {
	glm::quat result;
	// GLM quaternion storage depends on GLM_FORCE_QUAT_DATA_XYZW macro
	// definition. Enforce proper assignment in any case.
	result.x = src[0];
	result.y = src[1];
	result.z = src[2];
	result.w = src[3];

	return result;
}

static glm::mat4 toMat4(const float (&src)[16]) {
	return glm::mat4 {
		src[0],
		src[1],
		src[2],
		src[3],
		src[4],
		src[5],
		src[6],
		src[7],
		src[8],
		src[9],
		src[10],
		src[11],
		src[12],
		src[13],
		src[14],
		src[15]};
}

static glm::mat4 toMat4(const std::array<float, 16>& src) {
	return glm::mat4 {
		src[0],
		src[1],
		src[2],
		src[3],
		src[4],
		src[5],
		src[6],
		src[7],
		src[8],
		src[9],
		src[10],
		src[11],
		src[12],
		src[13],
		src[14],
		src[15]};
}

static glm::mat4 getNodeMatrix(const cgltf_node& joint) {
	if (joint.has_matrix) {
		return toMat4(joint.matrix);

	} else {
		glm::mat4 translation = glm::mat4(1.f);
		glm::mat4 rotation    = glm::mat4(1.f);
		glm::mat4 scaling     = glm::mat4(1.f);

		if (joint.has_translation) {
			translation = glm::translate(glm::mat4(1.f), toVec3(joint.translation));
		}

		if (joint.has_rotation) {
			glm::quat rot_quat = glm::normalize(toQuat(joint.rotation));
			rotation           = glm::mat4_cast(rot_quat);
		}

		if (joint.has_scale) {
			scaling = glm::scale(glm::mat4(1.f), toVec3(joint.scale));
		}

		glm::mat4 node_transform = translation * rotation * scaling;

		return node_transform;
	}
}

static std::string generateMeshName(const std::string& model_name, size_t mesh_index) {
	return model_name + "_mesh" + std::to_string(mesh_index);
}

// TODO: return std::vector of mesh + material.
// Note that material pointer can be empty if mesh does not have material.
static std::pair<std::vector<std::shared_ptr<AbstractMesh>>, std::vector<std::shared_ptr<ms::Material>>>
loadMeshes(
	const cgltf_node& node,
	const cgltf_mesh& mesh,
	const cgltf_skin* skin,
	const std::string& model_name,
	size_t mesh_index,
	const std::vector<std::shared_ptr<ms::Material>>& materials,
	const cgltf_data& data
) {
	auto base_mesh_name =
		std::string(mesh.name ? mesh.name : generateMeshName(model_name, mesh_index));
	std::vector<std::shared_ptr<AbstractMesh>> meshes;
	std::vector<std::shared_ptr<ms::Material>> mesh_materials;

	auto select_mesh_material = [&](const cgltf_primitive& primitive) -> std::shared_ptr<ms::Material> {
		if (!primitive.material) {
			return {nullptr};
		}
		return materials.at(cgltf_material_index(&data, primitive.material));
	};

	auto mesh_matrix = getNodeMatrix(node);

	for (cgltf_size i = 0, n = mesh.primitives_count; i < n; ++i) {
		auto mesh_name = base_mesh_name + (n == 1 ? std::string() : std::to_string(i));
		std::vector<glm::vec3> positions;
		std::vector<glm::vec4> tangents;
		std::vector<glm::vec3> normals;
		std::vector<glm::vec2> uvs;
		std::vector<GLuint> indices;
		std::vector<VertexNormalTangent> vertices;
		std::vector<std::array<GLuint, 4>> bone_indices;
		std::vector<std::array<float, 4>> bone_weights;

		const cgltf_primitive& primitive = mesh.primitives[i];

		if (primitive.type != cgltf_primitive_type_triangles) {
			throw ModelLoadError {"non-triangle primitives are not supported yet"};
		}

		if (primitive.indices) {
			if (primitive.indices->count % 3 != 0) {
				throw ModelLoadError {"triangle indices count is not divisible by 3"};
			}

			switch (primitive.indices->component_type) {
			case cgltf_component_type_r_32u:
				indices = copyFromAccessor<GLuint>(*primitive.indices);
				break;
			case cgltf_component_type_r_16u: {
				auto trash_indices = copyFromAccessor<uint16_t>(*primitive.indices);
				for (auto trash_indice : trash_indices) {
					indices.emplace_back(static_cast<GLuint>(trash_indice));
				}
				break;
			}
			case cgltf_component_type_r_8u: {
				auto trash_indices = copyFromAccessor<uint8_t>(*primitive.indices);
				for (auto trash_indice : trash_indices) {
					indices.emplace_back(static_cast<GLuint>(trash_indice));
				}
				break;
			}
			case cgltf_component_type_invalid:
			case cgltf_component_type_r_8:
			case cgltf_component_type_r_16:
			case cgltf_component_type_r_32f:
			case cgltf_component_type_max_enum:
				throw ModelLoadError {"invalid indice component type"};
			}
		} else {
			// has no indices.
			throw ModelLoadError {"no indices in model"};
		}

		for (cgltf_size j = 0; j < primitive.attributes_count; ++j) {
			const cgltf_attribute& attribute = primitive.attributes[j];

			switch (attribute.type) {
			case cgltf_attribute_type_tangent:
				tangents = copyFromAccessor<glm::vec4>(*attribute.data);
				break;
			case cgltf_attribute_type_normal:
				normals = copyFromAccessor<glm::vec3>(*attribute.data);
				break;
			case cgltf_attribute_type_position:
				positions = copyFromAccessor<glm::vec3>(*attribute.data);
				break;
			case cgltf_attribute_type_texcoord:
				// TODO: load other texture coords.
				if (attribute.index == 0) {
					uvs = copyFromAccessor<glm::vec2>(*attribute.data);
				} else {
					std::cerr << "model has multiple UVs" << std::endl;
				}
				break;
			case cgltf_attribute_type_joints:
				// TODO: handle host big endianess, as gltf data is little
				// endian.
				// TODO: make this more efficient by promoting in
				// copyFromAccessor.
				if (attribute.data->component_type == cgltf_component_type_r_16u) {
					auto loaded_bone_indices =
						copyFromAccessor<std::array<unsigned short, 4>>(*attribute.data);
					for (auto& [u1, u2, u3, u4] : loaded_bone_indices) {
						bone_indices.emplace_back(std::array<GLuint, 4> {u1, u2, u3, u4});
					}
				} else if (attribute.data->component_type == cgltf_component_type_r_8u) {
					auto loaded_bone_indices =
						copyFromAccessor<std::array<unsigned char, 4>>(*attribute.data);
					for (auto& [u1, u2, u3, u4] : loaded_bone_indices) {
						bone_indices.emplace_back(std::array<GLuint, 4> {u1, u2, u3, u4});
					}
				} else {
					throw "unsupported accessor type "
						+ std::to_string(attribute.data->component_type) + " for bone joint IDs";
				}

				break;

			case cgltf_attribute_type_weights:
				if (attribute.data->component_type == cgltf_component_type_r_16u) {
					auto loaded_bone_weights =
						copyFromAccessor<std::array<unsigned short, 4>>(*attribute.data);

					for (auto& [u1, u2, u3, u4] : loaded_bone_weights) {
						bone_weights.emplace_back(std::array<float, 4> {
							u1 / 65535.f, u2 / 65535.f, u3 / 65535.f, u4 / 65535.f});
					}
				} else if (attribute.data->component_type == cgltf_component_type_r_8u) {
					auto loaded_bone_weights =
						copyFromAccessor<std::array<unsigned char, 4>>(*attribute.data);
					for (auto& [u1, u2, u3, u4] : loaded_bone_weights) {
						bone_weights.emplace_back(std::array<float, 4> {
							u1 / 255.f, u2 / 255.f, u3 / 255.f, u4 / 255.f});
					}
				} else if (attribute.data->component_type == cgltf_component_type_r_32f) {
					bone_weights = copyFromAccessor<std::array<float, 4>>(*attribute.data);
				} else {
					throw ModelLoadError {
						"unsupported accessor type "
						+ std::to_string(attribute.data->component_type) + " for bone weights"};
				}
				break;

			default:
				throw ModelLoadError {
					"unsupported attribute type " + std::to_string(attribute.type)};
				break;
			}
		}

		if (normals.empty()) {
			// model has no normals, generating dummy ones.
			normals = std::vector<glm::vec3>(positions.size(), glm::vec3 {0.0f, 1.0f, 0.0f});
		}

		if (tangents.empty()) {
			// model has no tangents, generating dummy ones.
			// TODO: calculate?
			tangents = std::vector<glm::vec4>(positions.size(), glm::vec4 {0.0f, 1.0f, 0.0f, 0.0f});
		}

		if (uvs.empty()) {
			uvs = std::vector<glm::vec2>(positions.size(), glm::vec2 {0.0f, 0.0f});
		}

		if (positions.size() != normals.size() || positions.size() != tangents.size()
		    || positions.size() != uvs.size()) {
			throw ModelLoadError {
				"mismatching count of vertex attributes: " + std::to_string(positions.size())
				+ " positions, " + std::to_string(normals.size()) + " normals, "
				+ std::to_string(tangents.size()) + " tangents, " + std::to_string(uvs.size())
				+ " and UVs"};
		}

		vertices.reserve(positions.size());
		for (size_t i = 0; i < positions.size(); ++i) {
			vertices.emplace_back(VertexNormalTangent {
				positions[i],
				normals[i],
				glm::vec3 {tangents[i]}, // TODO: handle tangent basis handedness in W.
				uvs[i]});
		}

		if (!skin) {
			// plain mesh.

			// move all vertices from mesh space into model space.
			//　TODO: do we have to rotate normals here?
			for (auto& vertice : vertices) {
				auto model_position = mesh_matrix * glm::vec4(vertice.position, 1.f);
				vertice.position = glm::vec3(model_position.x, model_position.y, model_position.z);
			}

			auto stream = std::make_unique<IndexedVertexStream<VertexNormalTangent>>(
				std::move(vertices),
				std::move(indices),
				VertexStreamUsage::Static,
				VertexStreamDraw::Triangles
			);

			auto result = std::make_shared<Mesh>(std::move(stream), mesh_name + std::to_string(i));

			meshes.emplace_back(std::move(result));
			mesh_materials.emplace_back(select_mesh_material(primitive));

		} else {
			// skeletal mesh.
			std::vector<VertexBoneWeight> vertex_bone_weights;
			if (positions.size() != bone_weights.size()
			    || positions.size() != bone_indices.size()) {
				throw "mismatching count of vertex bone attributes: "
					+ std::to_string(positions.size()) + " positions, "
					+ std::to_string(bone_weights.size()) + " bone weights,"
					+ std::to_string(bone_indices.size()) + " bone indices";
			}

			vertex_bone_weights.reserve(positions.size());
			for (size_t i = 0; i < positions.size(); ++i) {
				vertex_bone_weights.emplace_back(VertexBoneWeight {bone_indices[i], bone_weights[i]}
				);
			}

			auto stream = std::make_unique<SkinnedVertexStream<VertexNormalTangent>>(
				std::move(vertices),
				std::move(indices),
				std::move(vertex_bone_weights),
				VertexStreamUsage::Static,
				VertexStreamDraw::Triangles
			);

			auto result = std::make_shared<Mesh>(std::move(stream), mesh_name + std::to_string(i));

			meshes.emplace_back(std::move(result));
			mesh_materials.emplace_back(select_mesh_material(primitive));
		}
	}

	return {meshes, mesh_materials};
}

Animation loadAnimation(
	std::string anim_name,
	const cgltf_animation& animation,
	const std::unordered_map<const cgltf_node*, Bone*>& bone_map
) {
	std::unordered_map<cgltf_node*, AnimationNode> anim_nodes;
	double max_time = 0.f;

	for (size_t i = 0; i < animation.channels_count; ++i) {
		const cgltf_animation_channel& channel = animation.channels[i];
		if (channel.sampler == nullptr) {
			throw ModelLoadError {"one of animation " + anim_name + " channels has no sampler"};
		}

		const auto& sampler = *channel.sampler;
		if (sampler.input == nullptr || sampler.output == nullptr) {
			throw ModelLoadError {"sampler input or output accessor is missing"};
		}

		auto keyframe_times = copyFromAccessor<float>(*sampler.input);

		if (bone_map.find(channel.target_node) == bone_map.end()) {
			throw ModelLoadError {"failed to find bone for this node"};
		}

		Bone& bone = *bone_map.at(channel.target_node);
		// Get or create animation node for this bone if missing.
		auto it = anim_nodes.emplace(channel.target_node, AnimationNode({}, {}, {}, bone));
		AnimationNode& anim_node = it.first->second;

		switch (sampler.interpolation) {
		case cgltf_interpolation_type_linear:
			// do nothing, only linear is supported anyway.
			break;

		case cgltf_interpolation_type_step:
			throw ModelLoadError {"step interpolation is not supported yet"};

		case cgltf_interpolation_type_cubic_spline:
			throw ModelLoadError {"cubic spline interpolation is not supported yet"};

		default:
			throw ModelLoadError {
				"invalid interpolation type " + std::to_string(sampler.interpolation)};
		}

		switch (channel.target_path) {
		case cgltf_animation_path_type_translation: {
			if (!anim_node.positions.empty()) {
				throw ModelLoadError {"multiple translation animation tracks for single bone"};
			}

			auto translations = copyFromAccessor<glm::vec3>(*sampler.output);
			if (translations.size() != keyframe_times.size()) {
				throw ModelLoadError {"translations count != keyframe_times count"};
			}

			for (size_t i = 0; i < translations.size(); ++i) {
				anim_node.positions.emplace_back(KeyFrame(translations[i], keyframe_times[i]));
				max_time = std::max(max_time, anim_node.positions[i].time);
			}

			break;
		}

		case cgltf_animation_path_type_rotation: {
			if (!anim_node.rotations.empty()) {
				throw ModelLoadError {"multiple rotation animation tracks for single bone"};
			}

			auto rotations = copyFromAccessor<std::array<float, 4>>(*sampler.output);

			if (rotations.size() != keyframe_times.size()) {
				throw ModelLoadError {"rotations count != keyframe_times count"};
			}

			for (size_t i = 0; i < rotations.size(); ++i) {
				anim_node.rotations.emplace_back(
					KeyFrame(glm::normalize(toQuat(rotations[i])), keyframe_times[i])
				);
				max_time = std::max(max_time, anim_node.rotations[i].time);
			}
			break;
		}

		case cgltf_animation_path_type_scale: {
			if (!anim_node.scales.empty()) {
				throw ModelLoadError {"multiple scaling animation tracks for single bone"};
			}

			auto scales = copyFromAccessor<glm::vec3>(*sampler.output);

			if (scales.size() != keyframe_times.size()) {
				throw ModelLoadError {"scales count != keyframe_times count"};
			}

			for (size_t i = 0; i < scales.size(); ++i) {
				anim_node.scales.emplace_back(KeyFrame(scales[i], keyframe_times[i]));
				max_time = std::max(max_time, anim_node.scales[i].time);
			}
			break;
		}

		case cgltf_animation_path_type_weights:
			throw ModelLoadError {"morph weights not supported"};

		case cgltf_animation_path_type_invalid:
		case cgltf_animation_path_type_max_enum:
			throw ModelLoadError {"invalid animation path type"};
		}
	}

	std::vector<AnimationNode> flat_animation_nodes;
	for (auto& [_, anim_node] : anim_nodes) {
		flat_animation_nodes.emplace_back(std::move(anim_node));
	}

	return Animation {std::move(anim_name), max_time, 1.0f, std::move(flat_animation_nodes)};
}

static std::vector<Tree<uint32_t>> makeBoneIndiceTrees(
	const std::vector<const cgltf_node*>& roots,
	const std::unordered_map<const cgltf_node*, Bone*>& bone_map
) {
	std::vector<Tree<uint32_t>> result;

	std::function<void(Tree<uint32_t>&, const cgltf_node&, int)> dfs;

	dfs = [&](Tree<uint32_t>& tree, const cgltf_node& node, int depth) {
		for (size_t i = 0; i < node.children_count; ++i) {
			const auto& child_node = *node.children[i];
			const auto& child_bone = *bone_map.at(&child_node);

			auto& child_tree = tree.add(child_bone.index);
			dfs(child_tree, child_node, depth + 1);
		}
	};

	for (const auto* root : roots) {
		const auto& bone = bone_map.at(root);
		result.emplace_back(bone->index);
		dfs(result.back(), *root, 0);
	}

	return result;
}

static std::vector<const cgltf_node*> findRootNodes(const cgltf_data& src) {
	std::vector<const cgltf_node*> result;
	for (size_t i = 0; i < src.nodes_count; ++i) {
		const cgltf_node& node = src.nodes[i];
		if (node.parent == nullptr) {
			result.emplace_back(&node);
		}
	}

	return result;
}

// Generate a unique material name if it's missing.
static std::string generateMaterialName(const std::string& model_name, size_t material_index) {
	return model_name + "_material" + std::to_string(material_index);
}

static std::shared_ptr<ms::Material> loadMaterial(
	Assets& assets,
	const InstanceTypes& instance_types,
	const fs::path& base_path,
	const cgltf_material& material,
	const std::string& model_name,
	size_t material_index
) {
	ms::MaterialBuilder builder {assets};

	builder
		.setName(
			material.name ? std::string(material.name)
						  : generateMaterialName(model_name, material_index)
		)
		.setShading(material.unlit ? ms::Shading::Unlit : ms::Shading::Lit)
		.setTwoSided(material.double_sided);

	switch (material.alpha_mode) {
	case cgltf_alpha_mode_opaque:
		builder.setBlending(ms::Blending::Opaque);
		break;
	case cgltf_alpha_mode_blend:
		builder.setBlending(ms::Blending::Translucent);
		break;
	default:
		throw ModelLoadError {
			"alpha mode " + std::to_string(material.alpha_mode) + " not supported"};
	}

	if (!material.has_pbr_metallic_roughness) {
		throw ModelLoadError {"missing PBR metallic roughness"};
	}

	const auto& pbr_mr   = material.pbr_metallic_roughness;
	auto* base_color_tex = pbr_mr.base_color_texture.texture;

	if (base_color_tex == nullptr) {
		// no texture, this means that base color factor is the color.
		builder.add(ms::Property::Color, toVec4(pbr_mr.base_color_factor));

	} else {
		if (base_color_tex->image == nullptr) {
			throw ModelLoadError {"material has no base color texture image despite having PBR"};
		}

		const auto path = base_path / fs::path(base_color_tex->image->uri);

		// The base color texture MUST contain 8-bit values encoded with the
		// sRGB opto-electronic transfer function.
		const auto flags = TextureLoaderFlags(TextureLoaderFlags::Space::sRGB);
		// TODO: deduce other flags from cgltf sampler.

		builder.add(ms::Property::Diffuse, TextureLoader::load(assets, path, flags));
		builder.add(ms::Property::Color, toVec4(pbr_mr.base_color_factor));
		// TODO: recall why this was here.
		// builder.setFragmentSnippet(
		// 	"data.baseColor.rgb = getMaterialDiffuse(getVertexUV()).rrr;"
		// );
	}

	auto* mr_tex = pbr_mr.metallic_roughness_texture.texture;
	if (mr_tex && mr_tex->image) {
		const auto path = base_path / fs::path(mr_tex->image->uri);

		// These values MUST be encoded with a linear transfer function.
		const auto flags = TextureLoaderFlags(TextureLoaderFlags::Space::Linear);

		builder.add(ms::Property::ORM, TextureLoader::load(assets, path, flags));
		builder.add(ms::Property::Metallic, pbr_mr.metallic_factor);
		builder.add(ms::Property::Roughness, pbr_mr.roughness_factor);
	}

	auto* normal_tex = material.normal_texture.texture;
	if (normal_tex && normal_tex->image) {
		const auto path = base_path / fs::path(normal_tex->image->uri);

		// These values MUST be encoded with a linear transfer function.
		const auto flags = TextureLoaderFlags(TextureLoaderFlags::Space::Linear);

		builder.add(ms::Property::Normal, TextureLoader::load(assets, path, flags));
	}

	if (material.has_ior) {
		builder.add(ms::Property::IoR, material.ior.ior);
	}

	auto* emissive_tex = material.emissive_texture.texture;
	if (emissive_tex && emissive_tex->image) {
		const auto path = base_path / fs::path(emissive_tex->image->uri);

		// This texture contains RGB components encoded with the sRGB transfer
		// function
		const auto flags = TextureLoaderFlags(TextureLoaderFlags::Space::sRGB);

		builder.add(ms::Property::EmissiveMask, TextureLoader::load(assets, path, flags));
	}

	glm::vec3 emissive_rgb = toVec3(material.emissive_factor);
	if (emissive_rgb != glm::vec3(0.f)) {
		glm::vec4 emissive_color(toVec3(material.emissive_factor), 1.0f);
		if (material.has_emissive_strength) {
			emissive_color.w = material.emissive_strength.emissive_strength;
		}

		builder.add(ms::Property::EmissiveColor, emissive_color);
	}

	return builder.setModelShaders(instance_types).build();
}

static std::vector<std::shared_ptr<ms::Material>> loadMaterials(
	const std::string& model_name,
	Assets& assets,
	const InstanceTypes& instance_types,
	const fs::path& path,
	const cgltf_data& src
) {
	std::vector<std::shared_ptr<ms::Material>> materials;

	for (size_t i = 0; i < src.materials_count; ++i) {
		materials.emplace_back(loadMaterial(
			assets, instance_types, path.parent_path(), src.materials[i], model_name, i
		));
	}

	return materials;
}

// Generate material for meshes that do not have one.
static std::shared_ptr<ms::Material> makeDummyMaterial(
	Assets& assets, const std::string& model_name, const InstanceTypes& instance_types
) {
	return ms::MaterialBuilder(assets)
	    .setName(generateMaterialName(model_name, 0))
	    .setTwoSided(true)
	    .setShading(ms::Shading::Unlit)
	    .add(ms::Property::Color, glm::vec4(1.f, 0.f, 1.f, 1.f))
	    .setModelShaders(instance_types)
	    .build();
}

// Fill in missing mesh materials with a dummy one if required.
static void fixMissingMaterials(
	std::vector<std::shared_ptr<ms::Material>>& mesh_materials,
	Assets& assets,
	const std::string& model_name,
	const InstanceTypes& instance_types
) {
	std::shared_ptr<ms::Material> dummy_material;

	// Lazily create dummy material if required.
	auto use_dummy_material = [&]() {
		if (!dummy_material) {
			dummy_material = makeDummyMaterial(assets, model_name, instance_types);
		}
		return dummy_material;
	};

	for (auto& maybe_material : mesh_materials) {
		if (!maybe_material) {
			maybe_material = use_dummy_material();
		}
	}
}

static SkeletalModel* loadSkeletalModel(
	Assets& assets, const fs::path& path, const cgltf_data& src, const std::string& model_name
) {
	std::vector<Bone> bones;
	std::unordered_map<const cgltf_node*, size_t> bone_indice_map;

	auto root_nodes = findRootNodes(src);

	for (size_t i = 0; i < src.nodes_count; ++i) {
		const cgltf_node& node = src.nodes[i];
		auto bone_name =
			node.name ? std::string(node.name) : model_name + "_bone" + std::to_string(i);
		bones.emplace_back(Bone(i, bone_name, getNodeMatrix(node), glm::mat4(1.f)));
		bone_indice_map.emplace(&node, i);

		auto& bone = bones.back();
		if (node.has_translation) {
			bone.position = toVec3(node.translation);
		}
		if (node.has_rotation) {
			bone.rotation = toQuat(node.rotation);
		}
		if (node.has_scale) {
			bone.scale = toVec3(node.scale);
		}
	}

	// we do this last to avoid ptrs being invalidated if bones vector
	// reallocates.
	std::unordered_map<const cgltf_node*, Bone*> bone_map;

	for (const auto& [node, bone_index] : bone_indice_map) {
		bone_map.emplace(node, &bones[bone_index]);
	}

	for (size_t i = 0; i < src.skins_count; ++i) {
		const cgltf_skin& skin = src.skins[i];
		// Accessors of matrix type have data stored in column-major order.
		// glm stores them in column-major order.
		auto inverse_bind_matrices =
			copyFromAccessor<std::array<float, 16>>(*skin.inverse_bind_matrices);

		if (inverse_bind_matrices.size() != skin.joints_count) {
			throw ModelLoadError {"mismatched number of skin joints and inverse bind matrices"};
		}

		for (size_t j = 0; j < skin.joints_count; ++j) {
			const cgltf_node& joint_node = *skin.joints[j];
			auto* bone                   = bone_map.at(&joint_node);
			bone->offset_matrix          = toMat4(inverse_bind_matrices[j]);
			bone->joint_index            = j;
		}
	}

	std::vector<Animation> animations;
	for (size_t i = 0; i < src.animations_count; ++i) {
		auto anim_name = src.animations[i].name ? std::string(src.animations[i].name)
		                                        : "anim" + std::to_string(i);
		animations.emplace_back(loadAnimation(std::move(anim_name), src.animations[i], bone_map));
	}

	auto bone_indices_tree = makeBoneIndiceTrees(root_nodes, bone_map);

	const InstanceTypes instance_types {InstanceType::Skeletal};
	auto loaded_materials = loadMaterials(model_name, assets, instance_types, path, src);

	std::vector<std::shared_ptr<AbstractMesh>> meshes;
	std::vector<std::shared_ptr<ms::Material>> mesh_materials;

	for (size_t i = 0; i < src.nodes_count; ++i) {
		const cgltf_node& node = src.nodes[i];

		if (node.mesh) {
			auto [more_meshes, more_mesh_materials] = loadMeshes(
				node, *node.mesh, node.skin, model_name, meshes.size(), loaded_materials, src
			);
			meshes.insert(meshes.end(), more_meshes.begin(), more_meshes.end());
			mesh_materials.insert(
				mesh_materials.end(), more_mesh_materials.begin(), more_mesh_materials.end()
			);
		}
	}

	fixMissingMaterials(mesh_materials, assets, model_name, instance_types);

	std::unordered_map<std::string, uint32_t> bone_indices_map;
	for (size_t i = 0; i < bones.size(); ++i) {
		bone_indices_map.emplace(bones[i].name, i);
	}

	return new SkeletalModel(
		std::move(meshes),
		std::move(mesh_materials),
		std::move(bones),
		std::move(bone_indices_map),
		std::move(bone_indices_tree),
		std::move(animations),
		model_name
	);
}

static Model* loadPlainModel(
	Assets& assets, const fs::path& path, const cgltf_data& src, const std::string& model_name
) {
	std::vector<std::shared_ptr<AbstractMesh>> meshes;
	std::vector<std::shared_ptr<ms::Material>> mesh_materials;
	const InstanceTypes instance_types {InstanceType::Model};

	auto loaded_materials = loadMaterials(model_name, assets, instance_types, path, src);

	for (size_t i = 0; i < src.nodes_count; ++i) {
		const cgltf_node& node = src.nodes[i];

		if (node.mesh) {
			auto [more_meshes, more_mesh_materials] = loadMeshes(
				node, *node.mesh, nullptr, model_name, meshes.size(), loaded_materials, src
			);
			meshes.insert(meshes.end(), more_meshes.begin(), more_meshes.end());
			mesh_materials.insert(
				mesh_materials.end(), more_mesh_materials.begin(), more_mesh_materials.end()
			);
		}
	}

	fixMissingMaterials(mesh_materials, assets, model_name, instance_types);

	return new Model(std::move(meshes), std::move(mesh_materials), model_name);
}

static std::shared_ptr<AbstractModel>
loadModel(Assets& assets, const fs::path& path, const cgltf_data& src) {
	auto model_name = path.stem().string();

	if (src.skins_count > 0) {
		return std::shared_ptr<AbstractModel>(loadSkeletalModel(assets, path, src, model_name));
	} else {
		return std::shared_ptr<AbstractModel>(loadPlainModel(assets, path, src, model_name));
	}
}

std::shared_ptr<AbstractModel>
GltfModelLoader::loadModel(Assets& assets, const fs::path& path, const ModelLoaderFlags& flags) {
	cgltf_options opts = cgltf_options {
		cgltf_file_type_invalid, // autodetect
		0, // auto json token count
		cgltf_memory_options {nullptr, nullptr, nullptr},
		cgltf_file_options {nullptr, nullptr, nullptr}
    };
	cgltf_data* out_data = nullptr;

	cgltf_result gltf = cgltf_parse_file(&opts, reinterpret_cast<const char *>(path.c_str()), &out_data);
	if (gltf != cgltf_result_success) {
		throw ModelLoadError {
			"failed to parse GLTF model file " + path.string() + ": "
			+ std::to_string(static_cast<int>(gltf))};
	}

	auto result = cgltf_load_buffers(&opts, out_data, reinterpret_cast<const char *>(path.c_str()));
	if (result != cgltf_result_success) {
		throw ModelLoadError {
			"failed to load buffers: " + std::to_string(static_cast<int>(result))};
	}

	if (out_data->scenes == nullptr) {
		throw ModelLoadError {"no scene"};
	}

	return ::loadModel(assets, path, *out_data);
}