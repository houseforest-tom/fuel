/*****************************************************************
 **' Main.cpp
 *****************************************************************
 **' Created on: 11.06.2015
 **' Author: HAUSWALD, Tom.
 *****************************************************************
 *****************************************************************/

#include "Util.h"
#include "../graphics/GLWindow.h"
#include "../graphics/GLVertexArray.h"
#include "../graphics/GLFramebuffer.h"
#include "../graphics/GLTexture.h"
#include "../graphics/shaders/GLShaderProgram.h"
#include "../input/Keyboard.h"
#include "../graphics/lighting/PointLight.h"

using namespace fuel;

int main(int argc, char **argv)
{
	GLWindow window({1280, 720, false, true});
	Keyboard keyboard(window);

	// Framebuffer
	GLFramebuffer fbo(window.getWidth(), window.getHeight());
	GLFramebuffer::bind(fbo);
	fbo.attach("depth",    GL_DEPTH_COMPONENT32F);
	fbo.attach("diffuse",  GL_RGB32F);
	fbo.attach("position", GL_RGB32F);
	fbo.attach("normal",   GL_RGB32F);
	fbo.attach("texcoord", GL_RGB32F);
	fbo.setDrawAttachments({"diffuse", "position", "normal", "texcoord"});
	GLFramebuffer::unbind();

	// Index buffer
	GLBuffer ibo(GL_ELEMENT_ARRAY_BUFFER);
	ibo.write(GL_STATIC_DRAW, CUBE_INDICES);
	GLBuffer::unbind(ibo);

	// Vertex array
	GLVertexArray vao(3);
	GLVertexArray::bind(vao);
	vao.getAttributeList(0).write<float, 3>(GL_STATIC_DRAW, GL_FLOAT, CUBE_VERTICES);
	vao.getAttributeList(1).write<float, 3>(GL_STATIC_DRAW, GL_FLOAT, CUBE_NORMALS);
	vao.getAttributeList(2).write<float, 2>(GL_STATIC_DRAW, GL_FLOAT, CUBE_TEXTURE_COORDS);
	GLVertexArray::unbind();

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

	// Calculate camera matrices
	float aspectRatio = window.getWidth() / (float)window.getHeight();
	glm::mat4 projection = glm::perspective(45.0f, aspectRatio, 0.1f, 100.0f);
	glm::mat4 view       = glm::lookAt(
	    glm::vec3(0,0,5),
	    glm::vec3(0,0,0),
	    glm::vec3(0,1,0)
	);
	glm::mat4 viewProjection = projection * view;

	// Test point light
	PointLight pointLight;
	pointLight.position = {0, 0, 0};
	pointLight.color = {1, 0, 0};
	pointLight.setRadius(0.1f, 20.0f);

	// Cube transforms
	Transform cubeTransforms[50];
	cubeTransforms[0].setPosition({-2.5f, 0, 0});
	cubeTransforms[1].setPosition({ 2.5f, 0, 0});

	// Cube texture
	GLTexture cubeTexture("res/textures/grass.png");
	deferredShader.getUniform("uDiffuseTexture").set(cubeTexture.getID());

	while(!window.closed())
	{
		float time = static_cast<float>(glfwGetTime());
		if(keyboard.wasKeyReleased(GLFW_KEY_ESCAPE))
			window.close();
		keyboard.update();

		float freq = 90.0f;
		cubeTransforms[0].getRotation().y =  time * freq;
		cubeTransforms[1].getRotation().y = -time * freq;

		window.prepare();

		// Render the current frame
		{
			GLFramebuffer::bind(fbo);
			GLVertexArray::bind(vao);
			GLBuffer::bind(ibo);
			fbo.clear();
			deferredShader.use();

			// Draw first cube
			auto worldMatrix = cubeTransforms[0].calculateWorldMatrix();
			deferredShader.getUniform("uWorld").set(worldMatrix);
			deferredShader.getUniform("uWVP").set(viewProjection * worldMatrix);
			glDrawElements(GL_TRIANGLES, ibo.getElementCount<uint16_t>(), GL_UNSIGNED_SHORT, nullptr);

			// Draw second cube
			worldMatrix = cubeTransforms[1].calculateWorldMatrix();
			deferredShader.getUniform("uWorld").set(worldMatrix);
			deferredShader.getUniform("uWVP").set(viewProjection * worldMatrix);
			glDrawElements(GL_TRIANGLES, ibo.getElementCount<uint16_t>(), GL_UNSIGNED_SHORT, nullptr);

			GLFramebuffer::unbind();

			// Now back at default framebuffer
			string attachmentNames[] = { "diffuse", "position", "normal", "texcoord" };
			uint16_t vpw = window.getWidth(), vph = window.getHeight();
			for(unsigned i=0; i<4; ++i)
			{
				fbo.showAttachmentContent(attachmentNames[i], (vpw / 2) * (i % 2), (vph / 2) * (1 - i / 2), vpw / 2, vph / 2);
			}
		}

		window.display();
	}

	return 0;
}
