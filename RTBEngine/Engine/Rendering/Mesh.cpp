#include "Mesh.h"
#include <limits>

RTBEngine::Rendering::Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
	:VAO(0), VBO(0), EBO(0), vertexCount(static_cast<unsigned int>(vertices.size())), indexCount(static_cast<unsigned int>(indices.size()))
{
	SetupMesh(vertices, indices);
	CalculateAABB(vertices);
}

RTBEngine::Rendering::Mesh::~Mesh()
{
}

void RTBEngine::Rendering::Mesh::Draw() const
{
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
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

    // VAO save info Position (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    // VAO save info Normal (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    // VAO save info TexCoords (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(2);

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
