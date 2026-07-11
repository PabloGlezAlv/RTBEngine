#include "Mesh.h"
#include <cstdint>
#include <limits>

RTBEngine::Rendering::Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
	:VAO(0), VBO(0), EBO(0), vertexCount(static_cast<unsigned int>(vertices.size())), indexCount(static_cast<unsigned int>(indices.size()))
{
	SetupMesh(vertices, indices);
	CalculateAABB(vertices);
}

RTBEngine::Rendering::Mesh::~Mesh()
{
	if (VAO != 0) {
		glDeleteVertexArrays(1, &VAO);
		VAO = 0;
	}

	if (VBO != 0) {
		glDeleteBuffers(1, &VBO);
		VBO = 0;
	}

	if (EBO != 0) {
		glDeleteBuffers(1, &EBO);
		EBO = 0;
	}

	if (instanceVBO != 0) {
		glDeleteBuffers(1, &instanceVBO);
		instanceVBO = 0;
	}
}

void RTBEngine::Rendering::Mesh::Draw() const
{
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

}

void RTBEngine::Rendering::Mesh::UploadInstanceData(const Math::Matrix4* matrices, std::size_t count)
{
	if (!matrices || count == 0) {
		return;
	}

	glBindVertexArray(VAO);

	// Configure the mat4 instance attribute (locations 5-8) the first time this mesh is instanced.
	// A mat4 attribute occupies four vec4 slots; the whole attribute advances once per instance.
	const bool needsAttribSetup = (instanceVBO == 0);
	if (needsAttribSetup) {
		glGenBuffers(1, &instanceVBO);
	}

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

	if (needsAttribSetup) {
		const GLsizei stride = static_cast<GLsizei>(sizeof(Math::Matrix4));
		for (GLuint i = 0; i < 4; ++i) {
			const GLuint location = 5 + i;
			glEnableVertexAttribArray(location);
			glVertexAttribPointer(
				location, 4, GL_FLOAT, GL_FALSE, stride,
				reinterpret_cast<void*>(static_cast<std::uintptr_t>(i * sizeof(float) * 4)));
			glVertexAttribDivisor(location, 1);
		}
	}

	glBufferData(
		GL_ARRAY_BUFFER,
		static_cast<GLsizeiptr>(count * sizeof(Math::Matrix4)),
		matrices,
		GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}

void RTBEngine::Rendering::Mesh::DrawInstanced(GLsizei instanceCount) const
{
	if (instanceCount <= 0) {
		return;
	}

	glBindVertexArray(VAO);
	glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
	glBindVertexArray(0);
}

void RTBEngine::Rendering::Mesh::SetupMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
{
	//Create buffers/arrays
	glGenVertexArrays(1, &VAO); //Vertex Array Object 
	glGenBuffers(1, &VBO); //Vertex Buffer Object
	glGenBuffers(1, &EBO); //Element Buffer Object

	//Activate VAO
	glBindVertexArray(VAO);

	// Load data into vertex buffers
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

	// Load data into element buffer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	// VAO save info Position (location 0) -> glVertexAttribPointer for float
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(0);

	// VAO save info Normal (location 1)
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
	glEnableVertexAttribArray(1);

	// VAO save info TexCoords (location 2)
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
	glEnableVertexAttribArray(2);

	// VAO save info BoneIndices (location 3) -> glVertexAttribIPointer for integers
	glVertexAttribIPointer(3, 4, GL_INT, sizeof(Vertex), (void*)offsetof(Vertex, boneIndices));
	glEnableVertexAttribArray(3);

	// VAO save info BoneWeights (location 4)
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, boneWeights));
	glEnableVertexAttribArray(4);

	glBindVertexArray(0);
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
