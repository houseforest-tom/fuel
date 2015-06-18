/*****************************************************************
 **' Main.cpp
 *****************************************************************
 **' Created on: 11.06.2015
 **' Author: HAUSWALD, Tom.
 *****************************************************************
 *****************************************************************/

#include <cstdlib>
#include "Util.h"
#include "../graphics/GLWindow.h"
#include "../graphics/GLVertexArray.h"
#include "../graphics/GLFramebuffer.h"
#include "../graphics/GLTexture.h"
#include "../graphics/shaders/GLShaderProgram.h"
#include "../input/Keyboard.h"
#include "../graphics/lighting/PointLight.h"
#include "../graphics/Camera.h"

using namespace fuel;

void prepareGeometryPass(void);

void prepareLightPass(void);

int main(int argc, char **argv)
{
	GLWindow window({1280, 720, false});
	Keyboard keyboard(window);

	// Setup camera
	Camera camera;
	camera.getTransform().setPosition({0, 0, 5});

	// Framebuffer
	GLFramebuffer deferredFramebuffer(window.getWidth(), window.getHeight());
	GLFramebuffer::bind(deferredFramebuffer);
	deferredFramebuffer.attach("depth",    GL_DEPTH_COMPONENT32F);
	deferredFramebuffer.attach("diffuse",  GL_RGB32F);
	deferredFramebuffer.attach("position", GL_RGB32F);
	deferredFramebuffer.attach("normal",   GL_RGB32F);
	deferredFramebuffer.setDrawAttachments({"diffuse", "position", "normal"});
	GLFramebuffer::unbind();

	// Cube index buffer
	GLBuffer cubeIndexBuffer(GL_ELEMENT_ARRAY_BUFFER);
	cubeIndexBuffer.write(GL_STATIC_DRAW, CUBE_INDICES);
	GLBuffer::unbind(cubeIndexBuffer);

	// Cube vertex array
	GLVertexArray cubeVertexArray(3);
	GLVertexArray::bind(cubeVertexArray);
	cubeVertexArray.getAttributeList(0).write<float, 3>(GL_STATIC_DRAW, GL_FLOAT, CUBE_VERTICES);
	cubeVertexArray.getAttributeList(1).write<float, 3>(GL_STATIC_DRAW, GL_FLOAT, CUBE_NORMALS);
	cubeVertexArray.getAttributeList(2).write<float, 2>(GL_STATIC_DRAW, GL_FLOAT, CUBE_TEXTURE_COORDS);
	GLVertexArray::unbind();

	// Deferred shader (targeting multiple output textures: gbuffer)
	GLShaderProgram deferredShader;
	deferredShader.setShader(EGLShaderType::VERTEX,   "res/glsl/deferred.vert");
	deferredShader.setShader(EGLShaderType::FRAGMENT, "res/glsl/deferred.frag");
	deferredShader.bindVertexAttribute(0, "vPosition");
	deferredShader.bindVertexAttribute(1, "vNormal");
	deferredShader.bindVertexAttribute(2, "vTexCoord");
	deferredShader.link();
	deferredShader.registerUniform("uWVP");
	deferredShader.registerUniform("uWorld");
	deferredShader.registerUniform("uDiffuseTexture");
	deferredShader.getUniform("uDiffuseTexture").set(0);

	// Pointlight shader
	GLShaderProgram pointLightShader;
	pointLightShader.setShader(EGLShaderType::VERTEX,   "res/glsl/fullscreen.vert");
	pointLightShader.setShader(EGLShaderType::FRAGMENT, "res/glsl/pointlight.frag");
	pointLightShader.bindVertexAttribute(0, "vPosition");
	pointLightShader.bindVertexAttribute(1, "vTexCoord");
	pointLightShader.link();
	pointLightShader.registerUniform("uDiffuseTexture");
	pointLightShader.registerUniform("uPositionTexture");
	pointLightShader.registerUniform("uNormalTexture");
	pointLightShader.registerUniform("uPosition");
	pointLightShader.registerUniform("uColor");
	pointLightShader.registerUniform("uRadius");
	pointLightShader.registerUniform("uLinearAttenuation");
	pointLightShader.registerUniform("uQuadraticAttenuation");
	pointLightShader.getUniform("uDiffuseTexture").set(0);
	pointLightShader.getUniform("uPositionTexture").set(1);
	pointLightShader.getUniform("uNormalTexture").set(2);

	srand(time(nullptr));
	vector<PointLight> pointLights;
	for(unsigned light=0; light<5; light++) // generate lights
	{
		pointLights.push_back(PointLight());
		pointLights[light].position = {-10 + rand() % 20, -10 + rand() % 20, -10 + rand() % 20};
		pointLights[light].color = {rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX};
		pointLights[light].setRadius(0.01f, 10 + rand() % 30);
	}

	// Fullscreen quad VAO
	GLVertexArray fullscreenQuadVertexArray(2);
	GLVertexArray::bind(fullscreenQuadVertexArray);
	fullscreenQuadVertexArray.getAttributeList(0).write<float, 2>(GL_STATIC_DRAW, GL_FLOAT,
	{
		-1.0f, -1.0f,
		 1.0f, -1.0f,
		 1.0f,  1.0f,
		-1.0f,  1.0f
	});
	fullscreenQuadVertexArray.getAttributeList(1).write<float, 2>(GL_STATIC_DRAW, GL_FLOAT,
	{
		 0.0f, 0.0f,
		 1.0f, 0.0f,
		 1.0f, 1.0f,
		 0.0f, 1.0f
	});
	GLVertexArray::unbind();

	// Calculate camera matrices
	float aspectRatio = window.getWidth() / (float)window.getHeight();
	glm::mat4 projection = glm::perspective(45.0f, aspectRatio, 0.1f, 100.0f);
	glm::mat4 viewProjection = projection * camera.calculateViewMatrix();

	// Cube transforms
	Transform cubeTransforms[50];
	cubeTransforms[0].setPosition({-2.5f, 0, 0});
	cubeTransforms[1].setPosition({ 2.5f, 0, 0});

	// Load grass texture
	GLTexture grassTexture("res/textures/grass.png");

	while(!window.closed())
	{
		// Calculate passed time in seconds
		static float lastTime = static_cast<float>(glfwGetTime());
		float time 			  = static_cast<float>(glfwGetTime());
		float dt 			  = time - lastTime;
		lastTime 			  = time;

		if(keyboard.wasKeyReleased(GLFW_KEY_ESCAPE))
			window.close();
		keyboard.update();

		float freq = 90.0f;
		cubeTransforms[0].getRotation().y =  time * freq;
		cubeTransforms[1].getRotation().y = -time * freq;

		window.prepare();

		// Render the current frame
		{
			// Bind GBuffer
			GLFramebuffer::bind(deferredFramebuffer);
			GLFramebuffer::clear();
			prepareGeometryPass();
			deferredShader.use();
			{ // Geometry pass

				GLVertexArray::bind(cubeVertexArray);
				GLBuffer::bind(cubeIndexBuffer);
				GLTexture::bind(0, grassTexture); // Bind grass texture to unit 0

				// Draw first cube
				auto worldMatrix = cubeTransforms[0].calculateWorldMatrix();
				deferredShader.getUniform("uWorld").set(worldMatrix);
				deferredShader.getUniform("uWVP").set(viewProjection * worldMatrix);
				glDrawElements(GL_TRIANGLES, cubeIndexBuffer.getElementCount<uint16_t>(), GL_UNSIGNED_SHORT, nullptr);

				// Draw second cube
				worldMatrix = cubeTransforms[1].calculateWorldMatrix();
				deferredShader.getUniform("uWorld").set(worldMatrix);
				deferredShader.getUniform("uWVP").set(viewProjection * worldMatrix);
				glDrawElements(GL_TRIANGLES, cubeIndexBuffer.getElementCount<uint16_t>(), GL_UNSIGNED_SHORT, nullptr);

				GLTexture::unbind(0);
				GLBuffer::unbind(cubeIndexBuffer);
				GLVertexArray::unbind();
			}

			// Restore & clear default framebuffer
			GLFramebuffer::unbind();
			glClear(GL_COLOR_BUFFER_BIT);
			prepareLightPass();
			pointLightShader.use();
			{ // Fullscreen passes

				GLVertexArray::bind(fullscreenQuadVertexArray);
				GLFramebuffer::bind(deferredFramebuffer, GLFramebuffer::READ);

				for(unsigned light=0; light<pointLights.size(); light++)
				{
					pointLightShader.getUniform("uPosition").set(pointLights[light].position);
					pointLightShader.getUniform("uColor").set(pointLights[light].color);
					pointLightShader.getUniform("uRadius").set(pointLights[light].getRadius());
					pointLightShader.getUniform("uLinearAttenuation").set(pointLights[light].linearAttenuation);
					pointLightShader.getUniform("uQuadraticAttenuation").set(pointLights[light].quadraticAttenuation);
					glDrawArrays(GL_QUADS, 0, 4);
				}

				GLTexture::unbind({0, 1, 2});
				GLVertexArray::unbind();
			}

			glUseProgram(GL_NONE);
			prepareGeometryPass();
			// Render downsampled gbuffer textures as overlay
			static constexpr uint16_t previewWidth = 114, previewHeight = 64;
			deferredFramebuffer.showAttachmentContent("diffuse",    		 0, previewHeight, previewWidth, previewHeight);
			deferredFramebuffer.showAttachmentContent("position",   		 0, 		    0, previewWidth, previewHeight);
			deferredFramebuffer.showAttachmentContent("normal",   previewWidth,  		    0, previewWidth, previewHeight);
		}

		window.display();

		// Busy waiting :(
		while((time = static_cast<float>(glfwGetTime())) - lastTime < 1.0f / 60);
	}

	return 0;
}

void prepareGeometryPass(void)
{
	//Disable blending, enable depth
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}

void prepareLightPass(void)
{
	//Disable depth, enable blending
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE);
}
