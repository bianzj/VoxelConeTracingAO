#include "VoxelConeTracing.h"

#include "externals/GLM/glm/glm.hpp"
#include "externals/GLM/glm/gtc/matrix_transform.hpp"
#include "externals/GLM/glm/glm.hpp"
#include "externals/GLM/glm/gtc/matrix_transform.hpp"
#include "externals/GLM/glm/gtx/string_cast.hpp"

using namespace std;

#ifdef __unix__
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#endif


VoxelConeTracing::VoxelConeTracing()
{
	m_width = 0.0f;
	m_height = 0.0f;
	m_gbuffer = make_unique<GBuffer>();
}


VoxelConeTracing::~VoxelConeTracing()
{
}
void VoxelConeTracing::init(float width,float height) {
	// Prepare the one and only shader
	m_geomPass = make_unique<ShaderProgram>("/vertex_shaders/geom_pass.vert", "/fragment_shaders/geom_pass.frag");
	m_lightPass = make_unique<ShaderProgram>("/vertex_shaders/light_pass.vert", "/fragment_shaders/light_pass.frag");
	m_width = width;
	m_height = height;

	m_gbuffer->init(m_width, m_height);
}

void supplyFullScreenQuad(){
	//HERE FILL IN THINGS

	//*/
	const GLfloat plane_vert_data[] = {
		-1.0f, -1.0f,
		+1.0f, -1.0f,
		-1.0f, +1.0f,
		+1.0f, +1.0f,
	};
	/*/
	const GLfloat plane_vert_data[] = {
	-0.5f, -0.5f,
	+0.5f, -0.5f,
	-0.5f, +0.5f,
	+0.5f, +0.5f,
	};
	//*/
	GLuint mBufferID;
	glGenBuffers(1, &mBufferID);
	glBindBuffer(GL_ARRAY_BUFFER, mBufferID);
	glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vert_data), plane_vert_data, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, 0);

	glBindVertexArray(0);
}

void VoxelConeTracing::geometryPass(const std::unique_ptr<Scene>& scene) const{

	glDepthMask(true);
	//Bind the GBuffer before enabling (and texture stuff) else it will fail
	m_gbuffer->bindForWriting();


	glm::mat4 uniformProjection = glm::perspective(glm::radians(35.0f), m_width / m_height, 0.1f, 300.f);
	glm::mat4 uniformModel = glm::mat4(1.f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	// Use the one and only shader
	m_geomPass->use();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Create uniforms used by shader
	// Fill uniforms to shader
	m_geomPass->updateUniform("projection", uniformProjection);
	m_geomPass->updateUniform("view", scene->getCamera().getViewMatrix());
	m_geomPass->updateUniform("model", uniformModel); // all meshes have center at 0,0,0



	// Render all the buckets' content
	for (auto& bucket : scene->getRenderBuckets())
	{
		// Bind material of bucket (which binds its uniforms and textures)
		bucket.first->bind(m_geomPass.get());

		// Draw all meshes in that bucket
		for (Mesh const * pMesh : bucket.second)
		{
			pMesh->draw();
		}
	}
	m_geomPass->disable();

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
}
void VoxelConeTracing::deferredShadingPass(const std::unique_ptr<Scene>& scene,const NodePool& nodePool, const float stepSize) const{
	//Bind window framebuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_ONE, GL_ONE); //BLEND_FUNCTION BY OPENGL MAY USE (GL_SRC_ALPHA/GL_ONE_MINUS_SRC_ALPHA) for transparency

	//Bind Gbuffer so we can transfer the geometry information into the color coded main framebuffer
	m_gbuffer->bindForReading();


	glClear(GL_COLOR_BUFFER_BIT);


	glm::mat4 uniformProjection = glm::perspective(glm::radians(35.0f), m_width / m_height, 0.1f, 300.f);
	glm::mat4 uniformModel = glm::mat4(1.f);

	m_lightPass->updateUniform("projection", uniformProjection);
	m_lightPass->updateUniform("view", scene->getCamera().getViewMatrix());
	m_lightPass->updateUniform("model", uniformModel); // all meshes have center at 0,0,0

	supplyFullScreenQuad();

	//HERE FILL IN THINGS

	m_lightPass->disable();

	//Render little viewports into the main framebuffer that will be displayed onto the screen
	m_gbuffer->setReadBuffer(GBuffer::GBUFFER_TEXTURE_TYPE_POSITION);
	glBlitFramebuffer(0, 0, (GLint)m_width, (GLint)m_height, 0, 0, 150, 150, GL_COLOR_BUFFER_BIT, GL_LINEAR);


	m_gbuffer->setReadBuffer(GBuffer::GBUFFER_TEXTURE_TYPE_DIFFUSE);
	glBlitFramebuffer(0, 0, (GLint)m_width, (GLint)m_height, 150, 0, 300, 150, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	m_gbuffer->setReadBuffer(GBuffer::GBUFFER_TEXTURE_TYPE_NORMAL);
	glBlitFramebuffer(0, 0, (GLint)m_width, (GLint)m_height, 300, 0, 450, 150, GL_COLOR_BUFFER_BIT, GL_LINEAR);

	m_gbuffer->setReadBuffer(GBuffer::GBUFFER_TEXTURE_TYPE_TEXCOORD);
	glBlitFramebuffer(0, 0, (GLint)m_width, (GLint)m_height, 450, 0, 600, 150, GL_COLOR_BUFFER_BIT, GL_LINEAR);
}