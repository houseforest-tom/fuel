/*****************************************************************
 * GLFramebuffer.cpp
 *****************************************************************
 * Created on: 13.06.2015
 * Author: HAUSWALD, Tom.
 *****************************************************************
 *****************************************************************/

#include "GLFramebuffer.h"

namespace fuel
{
	GLFramebuffer::GLFramebuffer(uint16_t width, uint16_t height)
		:m_ID(GL_NONE), m_width(width), m_height(height), m_colorAttachmentCount(0)
	{
		glGenFramebuffers(1, &m_ID);

		if(m_ID == GL_NONE)
		{
			cerr << "Could not generate OpenGL framebuffer." << endl;
		}
		else
		{
			cout << "Generated OpenGL framebuffer: " << m_ID << endl;

			m_blitShader = make_unique<GLShaderProgram>();
			m_blitShader->setShader(EGLShaderType::VERTEX,   "res/glsl/fullscreen.vert");
			m_blitShader->setShader(EGLShaderType::FRAGMENT, "res/glsl/blit.frag");
			m_blitShader->bindVertexAttribute(0, "vPosition");
			m_blitShader->bindVertexAttribute(1, "vTexCoord");
			m_blitShader->link();
			m_blitShader->registerUniform("uTextureUnit");
		}
	}

	GLenum GLFramebuffer::getColorFormat(GLenum txrFormat)
	{
		// RGB encoded
		if(txrFormat == GL_RGB32F
				|| txrFormat == GL_RGB32UI
				|| txrFormat == GL_RGB16F
				|| txrFormat == GL_RGB16UI)
			return GL_RGB;

		// RGBA encoded
		if(txrFormat == GL_RGBA32F
				|| txrFormat == GL_RGBA32UI
				|| txrFormat == GL_RGBA16F
				|| txrFormat == GL_RGBA16UI)
			return GL_RGBA;

		// Depth
		if(txrFormat == GL_DEPTH_COMPONENT32F) return GL_DEPTH_COMPONENT;

		// Unsupported
		return GL_NONE;
	}

	GLenum GLFramebuffer::getDatatype(GLenum txrFormat)
	{
		// 16-bit floating points
		if(txrFormat == GL_RGB16F
				|| txrFormat == GL_RGBA16F)
			return GL_HALF_FLOAT;

		// 32-bit floating points
		if(txrFormat == GL_RGB32F
				|| txrFormat == GL_RGBA32F
				|| txrFormat == GL_DEPTH_COMPONENT32F)
			return GL_FLOAT;

		// 16-bit integers
		if(txrFormat == GL_RGB16UI
				|| txrFormat == GL_RGBA16UI)
			return GL_UNSIGNED_SHORT;

		// 32-bit integers
		if(txrFormat == GL_RGB32UI
				|| txrFormat == GL_RGBA32UI)
			return GL_UNSIGNED_INT;

		// Unsupported
		return GL_NONE;
	}

	GLenum GLFramebuffer::findAttachmentSlot(GLenum txrFormat)
	{
		if(txrFormat == GL_DEPTH_COMPONENT32F) return GL_DEPTH_ATTACHMENT;
		else return GL_COLOR_ATTACHMENT0 + m_colorAttachmentCount;
	}

	void GLFramebuffer::attach(const string &attachment, GLenum txrFormat)
	{
		GLenum colorFormat, datatype;
		if((colorFormat = getColorFormat(txrFormat)) == GL_NONE || (datatype = getDatatype(txrFormat)) == GL_NONE)
		{
			cerr << "Invalid texture format specified." << endl;
			cerr << "Should be one of: GL_RGB32(F/UI), GL_RGBA32(F/UI), GL_DEPTH_COMPONENT32F." << endl;
			return;
		}

		// Get FBO attachment slot for the new texture
		GLenum slot = findAttachmentSlot(txrFormat);

		GLTexture *pTexture = new GLTexture();
		GLTexture::bind(0, *pTexture);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	    glTexImage2D(GL_TEXTURE_2D, 0, txrFormat, m_width, m_height, 0, colorFormat, datatype, nullptr);
		glFramebufferTexture2D(GL_FRAMEBUFFER, slot, GL_TEXTURE_2D, pTexture->getID(), 0);
		GLTexture::unbind(0);

		// Construct new FBO attachment
		GLFramebufferAttachment fboAttachment;
		fboAttachment.name = attachment;
		fboAttachment.pTexture = pTexture;
		fboAttachment.textureFormat = txrFormat;
		fboAttachment.colorFormat = colorFormat;
		fboAttachment.datatype = datatype;
		fboAttachment.attachmentSlot = slot;

		m_attachments.insert({attachment, fboAttachment});

		// Increment color attachment count if neccessary
		if(txrFormat != GL_DEPTH_COMPONENT32F) m_colorAttachmentCount++;
	}

	void GLFramebuffer::setDrawAttachments(const vector<string> &attachments)
	{
		vector<GLenum> buffers;
		for(unsigned i=0; i<attachments.size(); ++i)
			buffers.push_back(m_attachments[attachments[i]].attachmentSlot);

		glDrawBuffers(buffers.size(), &buffers[0]);
	}

	void GLFramebuffer::bind(const GLFramebuffer &fbo, unsigned target)
	{
		if(target & READ)
		{
			// Next texture unit to use
			// Layout in the end will be: (COLOR[0], ..., COLOR[N], DEPTH)
			GLuint textureUnit = 0;

			for(; textureUnit < fbo.m_colorAttachmentCount; ++textureUnit)
			{
				// Go through all attachment textures
				for(auto iter = fbo.m_attachments.begin(); iter != fbo.m_attachments.end(); ++iter)
				{
					// Bind color textures
					if(iter->second.attachmentSlot == GL_COLOR_ATTACHMENT0 + textureUnit)
					{
						GLTexture::bind(textureUnit, *(iter->second.pTexture));
						break;
					}
				}
			}

			// Go through all attachment textures
			for(auto iter = fbo.m_attachments.begin(); iter != fbo.m_attachments.end(); ++iter)
			{
				// Bind depth texture
				if(iter->second.attachmentSlot == GL_DEPTH_ATTACHMENT)
				{
					GLTexture::bind(fbo.m_colorAttachmentCount, *(iter->second.pTexture));
					break;
				}
			}
		}

		if(target & DRAW) glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo.m_ID);
	}

	void GLFramebuffer::showAttachmentContent(GLWindow &window, const string &attachment, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
	{
		GLFramebuffer::bind(*this, READ);

		GLint textureUnit = 0;
		for(const auto &a : m_attachments)
		{
			if(a.second.name == attachment)
			{
				// Determine texture unit
				textureUnit = a.second.attachmentSlot; // FBO attachment ID
				if(textureUnit == GL_DEPTH_ATTACHMENT) textureUnit = m_colorAttachmentCount; // Depth attachment
				else textureUnit -= GL_COLOR_ATTACHMENT0; // Color attachment
				break;
			}
		}

		glPushAttrib(GL_VIEWPORT_BIT);
		glViewport(x, y, w, h);
		m_blitShader->use();
		m_blitShader->getUniform("uTextureUnit").set(textureUnit);
		window.renderFullscreenQuad();
		glPopAttrib();
	}

	GLFramebuffer::~GLFramebuffer(void)
	{
		// Delete attachment textures
		for(auto &attachment : m_attachments)
			delete attachment.second.pTexture;
		m_attachments.clear();

		// Delete FBO itself
		if(m_ID != GL_NONE)
		{
			cout << "Deleting OpenGL framebuffer: " << m_ID << endl;
			glDeleteFramebuffers(1, &m_ID);
			m_ID = GL_NONE;
		}
	}
}
