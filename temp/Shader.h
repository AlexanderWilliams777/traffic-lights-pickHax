#pragma once

#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>

#include <GL\glew.h>

#include "DirectionalLight.h"
#include "PointLight.h"
#include "SpotLight.h"

#include "CommonValues.h"

class Shader
{
public:
	Shader() : shaderID(0), uniformModel(0), uniformProjection(0), pointLightCount(0), spotLightCount(0) {}

	void CreateFromString(const char* vertexCode, const char* fragmnetCode);
	void CreateFromFiles(const char* vertexLocation, const char* fragmentLocation);

	GLuint GetShaderID() const { return shaderID; }
	GLuint GetProjectionLocation() const { return uniformProjection; }
	GLuint GetModelLocation() const { return uniformModel; }
	GLuint GetViewLocation() const { return uniformView; }
	GLuint GetAmbientIntensityLocation() const { return uniformDirectionalLight.uniformAmbientIntensity; }
	GLuint GetAmbientColourLocation() const { return uniformDirectionalLight.uniformColour; }
	GLuint GetDiffuseIntensityLocation() const { return uniformDirectionalLight.uniformDiffuseIntensity; }
	GLuint GetDirectionLocation() const { return uniformDirectionalLight.uniformDirection; }
	GLuint getEyePositionLocation() const { return uniformEyePosition; }
	GLuint GetSpecularIntensityLocation() const { return uniformSpecularIntensity; }
	GLuint GetShininessLocation() const { return uniformShininess; }

	void SetDirectionalLight(DirectionalLight* dLight);
	void SetPointLights(PointLight* pLight, unsigned int lightCount);
	void SetSpotLights(SpotLight* sLight, unsigned int lightCount);

	void UseShader();
	void ClearShader();

	~Shader();
private:
	int pointLightCount;
	int spotLightCount;

	GLuint shaderID, uniformProjection, uniformModel, uniformView, uniformEyePosition,
		uniformSpecularIntensity, uniformShininess;

	struct {
		GLuint uniformColour;
		GLuint uniformAmbientIntensity;
		GLuint uniformDiffuseIntensity;

		GLuint uniformDirection;
	} uniformDirectionalLight;

	GLuint uniformPointLightCount;

	struct {
		GLuint uniformColour;
		GLuint uniformAmbientIntensity;
		GLuint uniformDiffuseIntensity;

		GLuint uniformPosition;
		GLuint uniformConstant;
		GLuint uniformLinear;
		GLuint uniformExponent;
	} uniformPointLight[MAX_POINT_LIGHTS];

	GLuint uniformSpotLightCount;

	struct {
		GLuint uniformColour;
		GLuint uniformAmbientIntensity;
		GLuint uniformDiffuseIntensity;

		GLuint uniformPosition;
		GLuint uniformConstant;
		GLuint uniformLinear;
		GLuint uniformExponent;

		GLuint uniformDirection;
		GLuint uniformEdge;
	} uniformSpotLight[MAX_SPOT_LIGHTS];

	std::string ReadFile(const char* fileLocation);

	void CompileShader(const char* vertexCoe, const char* fragmnetCode);
	void AddShader(GLuint theProgram, const char* shaderCode, GLenum shaderType);
};

