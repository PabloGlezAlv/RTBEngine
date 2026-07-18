#include "Mesh.h"
#include "RHI/RenderDevice.h"
#include <cstdint>
#include <limits>

namespace {
    RTBEngine::Rendering::RHI::IRenderDevice& Device()
    {
        return RTBEngine::Rendering::RHI::RenderDevice::Get();
    }
}

RTBEngine::Rendering::Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
	: vertexCount(static_cast<unsigned int>(vertices.size())), indexCount(static_cast<unsigned int>(indices.size()))
{
	SetupMesh(vertices, indices);
	CalculateAABB(vertices);
}

RTBEngine::Rendering::Mesh::~Mesh()
{
	auto& device = Device();
	if (VAO != RHI::kInvalidGpuId) {
		device.DestroyVertexArray(VAO);
		VAO = RHI::kInvalidGpuId;
	}
	if (VBO != RHI::kInvalidGpuId) {
		device.DestroyBuffer(VBO);
		VBO = RHI::kInvalidGpuId;
	}
	if (EBO != RHI::kInvalidGpuId) {
		device.DestroyBuffer(EBO);
		EBO = RHI::kInvalidGpuId;
	}
	if (instanceVBO != RHI::kInvalidGpuId) {
		device.DestroyBuffer(instanceVBO);
		instanceVBO = RHI::kInvalidGpuId;
	}
	if (instanceColorVBO != RHI::kInvalidGpuId) {
		device.DestroyBuffer(instanceColorVBO);
		instanceColorVBO = RHI::kInvalidGpuId;
	}
}

void RTBEngine::Rendering::Mesh::Draw() const
{
	auto& device = Device();
	device.BindVertexArray(VAO);
	device.DrawIndexed(RHI::PrimitiveTopology::Triangles, static_cast<int>(indexCount), RHI::IndexType::UInt32);
	device.UnbindVertexArray();
}

void RTBEngine::Rendering::Mesh::UploadInstanceData(const Math::Matrix4* matrices, std::size_t count)
{
	if (!matrices || count == 0) {
		return;
	}

	auto& device = Device();
	device.BindVertexArray(VAO);

	const bool needsAttribSetup = (instanceVBO == RHI::kInvalidGpuId);
	if (needsAttribSetup) {
		instanceVBO = device.CreateBuffer();
	}

	device.SetArrayBufferData(
		instanceVBO,
		matrices,
		count * sizeof(Math::Matrix4),
		RHI::BufferUsage::Dynamic);

	if (needsAttribSetup) {
		const int stride = static_cast<int>(sizeof(Math::Matrix4));
		for (unsigned int i = 0; i < 4; ++i) {
			const unsigned int location = 5 + i;
			device.EnableVertexAttribFloat(
				location, 4, stride,
				static_cast<std::size_t>(i * sizeof(float) * 4));
			device.SetVertexAttribDivisor(location, 1);
		}
	}

	device.BindArrayBuffer(RHI::kInvalidGpuId);
	device.UnbindVertexArray();
}

void RTBEngine::Rendering::Mesh::UploadInstanceColors(const Math::Vector4* colors, std::size_t count)
{
	if (!colors || count == 0) {
		return;
	}

	auto& device = Device();
	device.BindVertexArray(VAO);

	const bool needsAttribSetup = (instanceColorVBO == RHI::kInvalidGpuId);
	if (needsAttribSetup) {
		instanceColorVBO = device.CreateBuffer();
	}

	device.SetArrayBufferData(
		instanceColorVBO,
		colors,
		count * sizeof(Math::Vector4),
		RHI::BufferUsage::Dynamic);

	if (needsAttribSetup) {
		constexpr unsigned int location = 9;
		device.EnableVertexAttribFloat(location, 4, static_cast<int>(sizeof(Math::Vector4)), 0);
		device.SetVertexAttribDivisor(location, 1);
	}

	device.BindArrayBuffer(RHI::kInvalidGpuId);
	device.UnbindVertexArray();
}

void RTBEngine::Rendering::Mesh::DrawInstanced(int instanceCount) const
{
	if (instanceCount <= 0) {
		return;
	}

	auto& device = Device();
	device.BindVertexArray(VAO);
	device.DrawIndexedInstanced(
		RHI::PrimitiveTopology::Triangles,
		static_cast<int>(indexCount),
		RHI::IndexType::UInt32,
		instanceCount);
	device.UnbindVertexArray();
}

void RTBEngine::Rendering::Mesh::SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
	auto& device = Device();
	VAO = device.CreateVertexArray();
	VBO = device.CreateBuffer();
	EBO = device.CreateBuffer();

	device.BindVertexArray(VAO);
	device.SetArrayBufferData(VBO, vertices.data(), vertices.size() * sizeof(Vertex), RHI::BufferUsage::Static);
	device.SetElementBufferData(EBO, indices.data(), indices.size() * sizeof(unsigned int), RHI::BufferUsage::Static);

	const int stride = static_cast<int>(sizeof(Vertex));
	device.EnableVertexAttribFloat(0, 3, stride, offsetof(Vertex, position));
	device.EnableVertexAttribFloat(1, 3, stride, offsetof(Vertex, normal));
	device.EnableVertexAttribFloat(2, 2, stride, offsetof(Vertex, texCoords));
	device.EnableVertexAttribInt(3, 4, stride, offsetof(Vertex, boneIndices));
	device.EnableVertexAttribFloat(4, 4, stride, offsetof(Vertex, boneWeights));

	device.UnbindVertexArray();
}

void RTBEngine::Rendering::Mesh::CalculateAABB(const std::vector<Vertex>& vertices)
{
	if (vertices.empty()) {
		aabbMin = Math::Vector3(0.0f, 0.0f, 0.0f);
		aabbMax = Math::Vector3(0.0f, 0.0f, 0.0f);
		return;
	}

	float minX = std::numeric_limits<float>::max();
	float minY = std::numeric_limits<float>::max();
	float minZ = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float maxY = std::numeric_limits<float>::lowest();
	float maxZ = std::numeric_limits<float>::lowest();

	for (const auto& vertex : vertices) {
		if (vertex.position.x < minX) minX = vertex.position.x;
		if (vertex.position.y < minY) minY = vertex.position.y;
		if (vertex.position.z < minZ) minZ = vertex.position.z;
		if (vertex.position.x > maxX) maxX = vertex.position.x;
		if (vertex.position.y > maxY) maxY = vertex.position.y;
		if (vertex.position.z > maxZ) maxZ = vertex.position.z;
	}

	aabbMin = Math::Vector3(minX, minY, minZ);
	aabbMax = Math::Vector3(maxX, maxY, maxZ);
}
