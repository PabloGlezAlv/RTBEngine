#include "Texture.h"

RTBEngine::Rendering::Texture::Texture()
	: textureID(0), width(0), height(0), channels(0)
{
}

RTBEngine::Rendering::Texture::~Texture()
{
}

bool RTBEngine::Rendering::Texture::LoadFromFile(const std::string& path)
{
	return false;
}

void RTBEngine::Rendering::Texture::Bind(unsigned int slot) const
{
}

void RTBEngine::Rendering::Texture::Unbind() const
{
}

unsigned int RTBEngine::Rendering::Texture::GetWidth() const
{
	return width;
}

unsigned int RTBEngine::Rendering::Texture::GetHeight() const
{
	return height;
}

GLuint RTBEngine::Rendering::Texture::GetID() const
{
	return textureID;
}
