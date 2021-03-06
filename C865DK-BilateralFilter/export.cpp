/*
 * export.cpp
 *
 *  Created on: April 1, 2020
 *      Author: yuan
 */
#include "./include/filter.h"
#include "./include/raster.h"

//{{ Shaders

//{{ Vertex Shaders
const GLchar * vs_common = STRINGIZE(
	uniform mat4 MVP;

	layout (location = 0) in vec4 Position;
	layout (location = 1) in vec4 Color;
	layout (location = 2) in vec2 UV;

	out vec4 FragColor;
	out vec2 TexCoord;

	void main(void)
	{
		FragColor   = Color;
		TexCoord    = UV;
		gl_Position = MVP * Position;
	}
);
//}} Vertex Shaders

//{{ Fragment Shaders
const GLchar * fs_frag_tex = STRINGIZE(
	uniform sampler2D Texture;

	in vec4 FragColor;
	in vec2 TexCoord;

	layout (location = 0) out vec4 Color;

	void main(void)
	{
		vec4 texel = texture(Texture, TexCoord);

		if (texel.a < 0.5)
		{
			discard;
		}
		else
		{
			Color  = texel * FragColor;
		}
	}
);

const GLchar * fs_tex = STRINGIZE(
	uniform sampler2D Texture;

	in vec4 FragColor;
	in vec2 TexCoord;

	layout (location = 0) out vec4 Color;

	void main(void)
	{
		vec4 texel = texture(Texture, TexCoord);
		Color = texel;
	}
);

const GLchar * fs_frag = STRINGIZE(
	uniform sampler2D Texture;

	in vec4 FragColor;
	in vec2 TexCoord;

	layout (location = 0) out vec4 Color;

	void main(void)
	{
		Color = FragColor;
	}
);

const GLchar * fs_tex_filter = STRINGIZE(

	float normpdf(in float x, in float sigma)
	{
		return 0.39894*exp(-0.5*x*x/(sigma*sigma))/sigma;
	}

	float normpdf3(in vec3 v, in float sigma)
	{
		return 0.39894*exp(-0.5*dot(v,v)/(sigma*sigma))/sigma;
	}

	uniform sampler2D Texture;
	uniform uint      Filter;

	in vec4 FragColor;
	in vec2 TexCoord;

	layout (location = 0) out vec4 Color;

void main(void) {
	vec2 iResolution = vec2(372, 558);
	vec3 c = texture(Texture, TexCoord.xy).rgb;

    int kSize = 7;

    float kernel[15];

    vec3 bfinal_colour = vec3(0.0);

    float bZ = 0.0;

    //create the 1-D kernel
    for (int j = 0; j <= kSize; ++j) {
      kernel[kSize+j] = kernel[kSize-j] = normpdf(float(j), 10.0);
    }

    vec3 cc;
    float gfactor;
    float bfactor;
    float bZnorm = 1.0/normpdf(0.0, 0.1);

    //read out the texels
    for (int i=-kSize; i <= kSize; ++i)
    {
      for (int j=-kSize; j <= kSize; ++j)
      {
        cc = texture(Texture, TexCoord.xy+vec2(float(i),float(j)) / iResolution.xy).rgb;
        // compute both the gaussian smoothed and bilateral
        gfactor = kernel[kSize+j]*kernel[kSize+i];
        bfactor = normpdf3(cc-c, 0.1)*bZnorm*gfactor;
        bZ += bfactor;

        bfinal_colour += bfactor*cc;
      }
    }
    vec4 bc = vec4(bfinal_colour/bZ, 1.0);
    
	switch(Filter)
	{
		case 1U:
			break;
		default:
			bc = texture(Texture, TexCoord);
		break;
	}
	Color = bc;
}
);
//}} Fragment Shaders

//}} Shaders

using namespace little;

static Raster    header;
static Raster    title;
static Filter    filter;

static const char * str_header = "Adreno GPU SDK";
static const char * str_title  = "Bilateral Filtering";


class InstanceRenderer : public Renderer
{
	uint32_t frame_count;

	GLuint   filter_index;
	GLuint   filter_count;

protected:
	virtual void OnInit();
	virtual void OnDraw();
};

void InstanceRenderer::OnInit()
{
	this->frame_count  = 0;
	this->filter_index = 0;
	this->filter_count = 2;

	const char * imgs[] =
	{
		"/sdcard/DCIM/res/test.png", 
		"/sdcard/DCIM/res/petal011.png",
		"/sdcard/DCIM/res/petal011.png"
	};

	stbi_set_flip_vertically_on_load(true);

	TextureLoader tex_00(imgs[0]);
	TextureLoader tex_01(imgs[1]);
	TextureLoader tex_02(imgs[2]);

	Raster::Param    param_header   = { str_header, Vector3(0.0f, 0.9f, 0.0f), 0.07f, 0.030f, 1.1f, true  };
	Raster::Param    param_title    = { str_title,  Vector3(0.0f, 0.7f, 0.0f), 0.07f, 0.026f, 0.0f, true  };
	Filter::Param    param_filter   = { "Filter" };

	Texture tex_ary[] =
	{
		{
			.data     = { .buffer = tex_00.Texture(), .width = tex_00.Width(), .height = tex_00.Height() },
			.internal = GL_RGB8,
			.format   = GL_RGB,
			.type     = GL_UNSIGNED_BYTE
		},
		{
			.data     = { .buffer = tex_01.Texture(), .width = tex_01.Width(), .height = tex_01.Height() },
			.internal = GL_RGBA8,
			.format   = GL_RGBA,
			.type     = GL_UNSIGNED_BYTE
		},
		{
			.data     = { .buffer = tex_02.Texture(), .width = tex_02.Width(), .height = tex_02.Height() },
			.internal = GL_RGBA8,
			.format   = GL_RGBA, //GL_RGB, //
			.type     = GL_UNSIGNED_BYTE
		}
	};

	this->texture_manager.Create(ARRAY_LENGTH(tex_ary), tex_ary);

	TextureArray & ta = this->texture_manager.Textrues();

	TextureArray ta_header  = ta.SubArray(1, 1);
	TextureArray ta_title   = ta.SubArray(1, 1);
	TextureArray ta_filter  = ta.SubArray(0, 1);

	float s = this->AspectRatio();
	if (s < 1.0f)
	{
		param_header.scale         *= s;
		param_header.particle_size *= s;
		param_title.scale          *= (s - 0.1);
		param_title.particle_size  *= s;
	}

	BlendFactor & title_blend_factor = title;
	title_blend_factor.dst = GL_ONE;
	//title.EnableBlending(true);

	BlendFactor & header_blend_factor = header;
	header_blend_factor.dst = GL_ONE;
	//header.EnableBlending(true);

	Node na[] =
	{
		{ true ,  &filter,   vs_common, fs_tex_filter, "MVP", &ta_filter,  &param_filter   },
		{ true ,  &header,   vs_common, fs_frag_tex,   "MVP", &ta_header,  &param_header   },
		{ true ,  &title,    vs_common, fs_frag_tex,   "MVP", &ta_title,   &param_title    }
	};

	this->nodes.Create(ARRAY_LENGTH(na), na);

#if 0
    vec3 eye = { 2.2f, 0.0f, 0.2f};

    this->xform.LookAt(eye);
    this->xform.Perspective(90.0f, this->AspectRatio(), 0.1f, 100.0f);
    this->xform.EnableRotations(false, true, true);
    this->xform.Zoom  = -0.3f;
    this->xform.Delta =  0.1f; //0.3f; //1.0f; //
#endif

    this->Renderer::OnInit();
}

void InstanceRenderer::OnDraw()
{
	this->frame_count++;

	uint32_t base      = 150;
	uint32_t remainder = this->frame_count % base;

	if (remainder == 0)
	{
		GLuint index = ++this->filter_index % this->filter_count;

		LOG_INFO("FilterIndex", index);

		filter.Select(index);
	}

    this->Renderer::OnDraw();
}

InstanceRenderer renderer;

void InitScene(uint32_t view_width, uint32_t view_height)
{
	renderer.Initialize(view_width, view_height);
}

void DrawFilter()
{
	renderer.Draw();
}
