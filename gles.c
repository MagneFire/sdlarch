/*
#include "GLES/gl.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"
#include "GLES2/gl2.h"*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <unistd.h>
#include <malloc.h>
#include "gles.h"

static uint32_t frame_width = 0;
static uint32_t frame_height = 0;
uint32_t screen_width = 400;
uint32_t screen_height = 400;
float m[4][4];



//IMPORTANT, make sure this function is commented out at runtime
//as it has a big performance impact!
#define	SHOW_ERROR	gles_show_error();

static const char* vertex_shader =
    "uniform mat4 u_vp_matrix;                              \n"
    "attribute vec4 a_position;                             \n"
    "attribute vec2 a_texcoord;                             \n"
    "varying mediump vec2 v_texcoord;                       \n"
    "void main()                                            \n"
    "{                                                      \n"
    "   v_texcoord = a_texcoord;                            \n"
    "   gl_Position = u_vp_matrix * a_position;             \n"
    "}                                                      \n";

static const char* fragment_shader_none =
	"varying mediump vec2 v_texcoord;												\n"
	"uniform sampler2D u_texture;													\n"
	"void main()																	\n"
	"{																				\n"
	"	gl_FragColor = texture2D(u_texture, v_texcoord);							\n" 
	"}																				\n";

// scanline-3x.shader
static const char* fragment_shader_scanline =
	"uniform sampler2D u_texture; 													\n"
	"varying mediump vec2 v_texcoord; 												\n"
	"void main()     																\n"
	"{																				\n"
	"	vec4 rgb = texture2D(u_texture, v_texcoord);  								\n"
	"	vec4 intens ;  																\n"
	"	if (fract(gl_FragCoord.y * (0.5*4.0/3.0)) > 0.5)  							\n"
	"		intens = vec4(0);  														\n"
	"	else  																		\n"
	"		intens = smoothstep(0.2,0.8,rgb) + normalize(vec4(rgb.xyz, 1.0));  		\n"
	"	float level = (4.0-0.0) * 0.19;  											\n"
	"	gl_FragColor = intens * (0.5-level) + rgb * 1.1 ;  							\n"
	"} 																				\n";

// Phospher effect shader, too slow to run at full speed on the RPI
static const char* fragment_shader_phospher =
    "uniform sampler2D u_texture; 														\n"
	"vec2 rubyTextureSize=vec2(512.0,448.0);											\n"
	"varying mediump vec2 v_texcoord; 													\n"
	"																					\n"
	"vec3 to_focus(float pixel)															\n"
	"{																					\n"
	"   pixel = mod(pixel + 3.0, 3.0);													\n"
	"   if (pixel >= 2.0) // Blue														\n"
	"      return vec3(pixel - 2.0, 0.0, 3.0 - pixel);									\n"
	"   else if (pixel >= 1.0) // Green													\n"
	"      return vec3(0.0, 2.0 - pixel, pixel - 1.0);									\n"
	"   else // Red																		\n"
	"      return vec3(1.0 - pixel, pixel, 0.0);										\n"
	"}																					\n"
	"																					\n"
	"void main()																		\n"
	"{																					\n"
	"   float y = mod(v_texcoord.y * rubyTextureSize.y, 1.0);							\n"
	"   float intensity = exp(-0.2 * y);												\n"
	"   vec2 one_x = vec2(1.0 / (3.0 * rubyTextureSize.x), 0.0);						\n"
	"   vec3 color = texture2D(u_texture, v_texcoord.xy - 0.0 * one_x).rgb;				\n"
	"   vec3 color_prev = texture2D(u_texture, v_texcoord.xy - 1.0 * one_x).rgb;		\n"
	"   vec3 color_prev_prev = texture2D(u_texture, v_texcoord.xy - 2.0 * one_x).rgb;	\n"
	"   float pixel_x = 3.0 * v_texcoord.x * rubyTextureSize.x;							\n"
	"   vec3 focus = to_focus(pixel_x - 0.0);											\n"
	"   vec3 focus_prev = to_focus(pixel_x - 1.0);										\n"
	"   vec3 focus_prev_prev = to_focus(pixel_x - 2.0);									\n"
	"   vec3 result =																	\n"
	"      0.8 * color * focus +														\n"
	"      0.6 * color_prev * focus_prev +												\n"
	"      0.3 * color_prev_prev * focus_prev_prev;										\n"
	"   result = 2.3 * pow(result, vec3(1.4));											\n"
	"   gl_FragColor = vec4(intensity * result, 1.0);									\n"
	"}																					\n";

// Bilinear smoothing, required when using palette as
// automatic smoothing won't work.
//static const char* fragment_shader_smooth =
//	"varying mediump vec2 v_texcoord;												\n"
//	"uniform sampler2D u_texture;													\n"
//	"void main()																	\n"
//	"{																				\n"
//	"	vec4 p0 = texture2D(u_texture, v_texcoord);									\n" 
//	"	vec4 p1 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 0)); 			\n"
//	"	vec4 p2 = texture2D(u_texture, v_texcoord + vec2(0, 1.0/256.0)); 			\n"
//	"	vec4 p3 = texture2D(u_texture, v_texcoord + vec2(1.0/512.0, 1.0/256.0)); 	\n"
//	"	vec2 l = vec2(fract(512.0*v_texcoord.x), fract(256.0*v_texcoord.y)); 		\n"
//	"	gl_FragColor = mix(mix(c0, c1, l.x), mix(c2, c3, l.x), l.y); 				\n"
//	"}																				\n";

static const GLfloat vertices[] =
{
	-0.5f, -0.5f, 0.0f,
	+0.5f, -0.5f, 0.0f,
	+0.5f, +0.5f, 0.0f,
	-0.5f, +0.5f, 0.0f,
};

//Values defined in gles2_create()
static GLfloat uvs[8];

static const GLushort indices[] =
{
	0, 1, 2,
	0, 2, 3,
};

static const int kVertexCount = 4;
static const int kIndexCount = 6;

void gles_show_error()
{
	GLenum error = GL_NO_ERROR;
    error = glGetError();
    if (GL_NO_ERROR != error)
        printf("GL Error %x encountered!\n", error);
}

static GLuint CreateShader(GLenum type, const char *shader_src)
{
	GLuint shader = glCreateShader(type);
	if(!shader)
		return 0;

	// Load and compile the shader source
	glShaderSource(shader, 1, &shader_src, NULL);
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(!compiled)
	{
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(sizeof(char) * info_len);
			glGetShaderInfoLog(shader, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error compiling shader:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

static GLuint CreateProgram(const char *vertex_shader_src, const char *fragment_shader_src)
{
	GLuint vertex_shader = CreateShader(GL_VERTEX_SHADER, vertex_shader_src);
	if(!vertex_shader)
		return 0;
	GLuint fragment_shader = CreateShader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if(!fragment_shader)
	{
		glDeleteShader(vertex_shader);
		return 0;
	}

	GLuint program_object = glCreateProgram();
	if(!program_object)
		return 0;
	glAttachShader(program_object, vertex_shader);
	glAttachShader(program_object, fragment_shader);

	// Link the program
	glLinkProgram(program_object);

	// Check the link status
	GLint linked = 0;
	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);
	if(!linked)
	{
		GLint info_len = 0;
		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);
		if(info_len > 1)
		{
			char* info_log = (char *)malloc(info_len);
			glGetProgramInfoLog(program_object, info_len, NULL, info_log);
			// TODO(dspringer): We could really use a logging API.
			printf("Error linking program:\n%s\n", info_log);
			free(info_log);
		}
		glDeleteProgram(program_object);
		return 0;
	}
	// Delete these here because they are attached to the program object.
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);
	return program_object;
}

typedef	struct ShaderInfo {
		GLuint program;
		GLint a_position;
		GLint a_texcoord;
		GLint u_vp_matrix;
		GLint u_texture;
} ShaderInfo;

static ShaderInfo shader;
static GLuint buffers[3];
static GLuint textures[2];

static int dis_width;
static int dis_height;
static float proj[4][4];
static float tex_width, tex_height;
static float gmw, gmh;


#define TEX_WIDTH tex_width
#define TEX_HEIGHT tex_height
/*
void video_set_filter(uint32_t filter) {
	if (filter==0) {
	    filter_min = GL_NEAREST;
	    filter_mag = GL_NEAREST;
	} else  {
	    filter_min = GL_LINEAR;
	    filter_mag = GL_LINEAR;
	}
}*/

static void gles2_destroy()
{
	if(!shader.program)
		return;
	glDeleteBuffers(3, buffers); SHOW_ERROR
	glDeleteProgram(shader.program); SHOW_ERROR
}

void SetOrtho(GLfloat m[4][4], GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat near, GLfloat far, GLfloat scale_x, GLfloat scale_y)
{
	memset(m, 0, 4*4*sizeof(GLfloat));
	m[0][0] = 2.0f/(right - left)*scale_x;
	m[1][1] = 2.0f/(top - bottom)*scale_y;
	m[2][2] = -2.0f/(far - near);
	m[3][0] = -(right + left)/(right - left);
	m[3][1] = -(top + bottom)/(top - bottom);
	m[3][2] = -(far + near)/(far - near);
	m[3][3] = 1;
}

void ortho2d(float m[4][4], float left, float right, float bottom, float top) {
    m[0][0] = 1; m[0][1] = 0; m[0][2] = 0; m[0][3] = 0;
    m[1][0] = 0; m[1][1] = 1; m[1][2] = 0; m[1][3] = 0;
    m[2][0] = 0; m[2][1] = 0; m[2][2] = 1; m[2][3] = 0;
    m[3][0] = 0; m[3][1] = 0; m[3][2] = 0; m[3][3] = 1;

    m[0][0] = 2.0f / (right - left);
    m[1][1] = 2.0f / (top - bottom);
    m[2][2] = -1.0f;
    m[3][0] = -(right + left) / (right - left);
    m[3][1] = -(top + bottom) / (top - bottom);
}
#define RGB15(r, g, b)  (((r) << (5+6)) | ((g) << 6) | (b))

void video_shader_init() {
	memset(&shader, 0, sizeof(ShaderInfo));
	shader.program = CreateProgram(vertex_shader, fragment_shader_none);
	//shader.program = CreateProgram(vertex_shader, fragment_shader);
	if(shader.program)
	{
		shader.a_position	= glGetAttribLocation(shader.program,	"a_position");
		shader.a_texcoord	= glGetAttribLocation(shader.program,	"a_texcoord");
		shader.u_vp_matrix	= glGetUniformLocation(shader.program,	"u_vp_matrix");
		shader.u_texture	= glGetUniformLocation(shader.program,	"u_texture");
	}
	if(!shader.program) return;

	glUniform1i(shader.u_texture, 0);

	/*if (g_video.hw.bottom_left_origin)
		ortho2d(m, -1, 1, 1, -1);
	else
		ortho2d(m, -1, 1, -1, 1);*/
	ortho2d(m, -1, 1, 1, -1);
    	glUniformMatrix4fv(shader.u_vp_matrix, 1, GL_FALSE, (float*)m);
}



void video_update_vertices(const struct retro_game_geometry *geom, uint32_t width, uint32_t height) {

/*
#define	TEX_WIDTH	512
#define	TEX_HEIGHT	256
#define	WIDTH		400
#define	HEIGHT		400

#define	min_u		24.0f/TEX_WIDTH
#define	max_u		(float)WIDTH/TEX_WIDTH - min_u
#define	min_v		0.0f
#define	max_v		(float)HEIGHT/TEX_HEIGHT*/
	SDL_GetWindowSize(g_win, &screen_width, &screen_height);
	printf("width: %d\theight: %d\r\n", width, height);
	printf("max_width: %d\tmax_height: %d\r\n", geom->max_width, geom->max_height);
/*
	GLfloat max_u = 1;//(float)screen_width/geom->max_width;
	GLfloat max_v = 1;//(float)screen_height/geom->max_height;

	printf("width: %d\theight: %d\r\n", width, height);
	printf("max_width: %d\tmax_height: %d\r\n", geom->max_width, geom->max_height);
	printf("max_u: %f\tmax_v: %f\r\n", max_u, max_v);

	memset(uvs,0,sizeof(GLfloat)*8);*/
	/*uvs[0]=min_u; uvs[1]=min_v;
	uvs[2]=max_u; uvs[3]=min_v;
	uvs[4]=max_u; uvs[5]=max_v;
	uvs[6]=min_u; uvs[7]=max_v;*/
/*
	uvs[3]=max_v;
	uvs[4]=max_u;
	uvs[5]=max_v;
	uvs[6]=max_u;
*/
	//Create_uvs(uvs, (float)geom->max_width/TEX_WIDTH, (float)geom->max_height/TEX_HEIGHT);

	float min_u = 0.0f;
	//float max_u=(float)bitmap_width/tex_width;
	float max_u = (float)geom->max_width/TEX_WIDTH;
	float min_v = 0.0f;
	//float max_v=(float)bitmap_height/tex_height;
	float max_v = (float)geom->max_height/TEX_HEIGHT;

	uvs[0] = min_u;
	uvs[1] = min_v;
	uvs[2] = max_u;
	uvs[3] = min_v;
	uvs[4] = max_u;
	uvs[5] = max_v;
	uvs[6] = min_u;
	uvs[7] = max_v;


	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void video_init(const struct retro_game_geometry *geom, uint32_t width, uint32_t height, uint32_t filter) {
	printf("video_init\r\n");
	tex_width = width;
	tex_height = height;
	gmw = geom->max_width;
	gmh = geom->max_height;
	float op_zoom = (float)(width)/(float)width;
	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);

	// OLD
	//glGenTextures(1, textures);
	//glBindTexture(GL_TEXTURE_2D, textures[0]);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TEX_WIDTH, TEX_HEIGHT, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, geom->max_width, geom->max_height, 0, g_video.pixtype, g_video.pixfmt, NULL);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, g_video.pixtype, g_video.pixfmt, NULL);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, geom->max_width, geom->max_height, 0, g_video.pixtype, g_video.pixfmt, NULL);


	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, g_video.pixtype, g_video.pixfmt, NULL);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, g_video.pixtype, g_video.pixfmt, NULL);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, geom->max_width, geom->max_height, 0, g_video.pixtype, g_video.pixfmt, NULL);
	//Create_uvs(uvs, (float)width/geom->max_width, (float)height/geom->max_height);


	//glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_WIDTH, TEX_HEIGHT, 0, g_video.pixtype, g_video.pixfmt, NULL);
	//Create_uvs(uvs, (float)width/TEX_WIDTH, (float)height/TEX_HEIGHT);
	//Create_uvs(uvs, ((float)width)/((float)geom->max_width), ((float)height)/((float)geom->max_height));
	//Create_uvs(uvs, ((float)width)/((float)geom->max_width), ((float)height)/((float)geom->max_height));

	glGenBuffers(3, buffers);
	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 3, vertices, GL_STATIC_DRAW);

	video_update_vertices(geom, width, height);

	/*glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glBufferData(GL_ARRAY_BUFFER, kVertexCount * sizeof(GLfloat) * 2, uvs, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);*/
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, kIndexCount * sizeof(GL_UNSIGNED_SHORT), indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_DITHER);


	SDL_GetWindowSize(g_win, &screen_width, &screen_height);

	float sx = 1.0f, sy = 1.0f;
	int h = height;
	int w = width;
	dis_width = screen_width;
	dis_height = screen_height;
	/*int rr=(screen_height*10/height);
	h = (height*rr)/10;
	w = (width*rr)/10;
	if (w>screen_width) {
	    rr = (screen_width*10/width);
	    h = (height*rr)/10;
	    w = (width*rr)/10;
	}
	glViewport((screen_width-w)/2, (screen_height-h)/2, w, h);*/
	glViewport(0, 0, w, h);

	float a = (float)screen_width/(float)screen_height;
	float a0 = (float)width/(float)height;
	if(a > a0)
		sx = a0/a;
	else
		sy = a/a0;
	SetOrtho(proj, -0.5f, +0.5f, +0.5f, -0.5f, -1.0f, 1.0f, sx*op_zoom, sy*op_zoom);
	//video_set_filter(filter);
}

void video_close()
{
	gles2_destroy();
	// Release OpenGL resources
	//eglMakeCurrent( display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );
	//eglDestroySurface( display, surface );
	//eglDestroyContext( display, context );
	//eglTerminate( display );
}

static void gles2_DrawQuad(const ShaderInfo *sh)
{
	glUniform1i(sh->u_texture, 0); SHOW_ERROR
/*
	if (Settings.DisplaySmoothStretch) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); SHOW_ERROR 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); SHOW_ERROR
	}
	else {*/
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); SHOW_ERROR 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); SHOW_ERROR
	//}

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]); SHOW_ERROR
	glVertexAttribPointer(sh->a_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_position); SHOW_ERROR

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]); SHOW_ERROR
	glVertexAttribPointer(sh->a_texcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL); SHOW_ERROR
	glEnableVertexAttribArray(sh->a_texcoord); SHOW_ERROR

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]); SHOW_ERROR

	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0); SHOW_ERROR
}

uint32_t henk = 0;

void video_draw(const void *pixels, unsigned width, unsigned height, unsigned pitch)
{
	if(!shader.program)
		return;


	if (pitch != g_video.pitch) {
		g_video.pitch = pitch;
		glPixelStorei(GL_UNPACK_ROW_LENGTH, g_video.pitch / g_video.bpp);
	}
	glClear(GL_COLOR_BUFFER_BIT); SHOW_ERROR
	glViewport(0, 0, dis_width, dis_height); SHOW_ERROR

	glDisable(GL_BLEND); SHOW_ERROR
	glUseProgram(shader.program); SHOW_ERROR
	glUniformMatrix4fv(shader.u_vp_matrix, 1, GL_FALSE, &proj[0][0]); SHOW_ERROR

	glActiveTexture(GL_TEXTURE0); SHOW_ERROR
	glBindTexture(GL_TEXTURE_2D, g_video.tex_id); SHOW_ERROR
	if (pixels && pixels != RETRO_HW_FRAME_BUFFER_VALID) {
		if (henk < 1) {
			
		struct retro_system_av_info av = {0};
		g_retro.retro_get_system_av_info(&av);
		//video_init(&av.geometry, width, height, 0);
		printf("width: %d\theight: %d\r\n", width, height);
		//gmw = 0;
		//gmh = 0;
		henk++;

		}
		//SDL_SetWindowSize(g_win, width, height);
    		/*SDL_DisplayMode mode;
		SDL_GetWindowDisplayMode(g_win, &mode);
		mode.h = height;
		mode.w = width;
		SDL_SetWindowDisplayMode(g_win, &mode);*/
		//printf("%d %d\r\n", (int)(width-gmw)/2, (int)(height-gmh)/2);
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, g_video.pixtype, g_video.pixfmt, pixels);
		glTexSubImage2D(GL_TEXTURE_2D, 0, (int)(gmw-width)/2, (int)(gmh-height)/2, width, height, g_video.pixtype, g_video.pixfmt, pixels);
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_width, frame_height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
	}
	//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, g_video.pixtype, g_video.pixfmt, pixels); SHOW_ERROR
	gles2_DrawQuad(&shader);

	glBindBuffer(GL_ARRAY_BUFFER, 0); SHOW_ERROR
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); SHOW_ERROR


	return;
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(shader.program);

	glBindTexture(GL_TEXTURE_2D, g_video.tex_id);


	if (pixels && pixels != RETRO_HW_FRAME_BUFFER_VALID) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, g_video.pixtype, g_video.pixfmt, pixels);
		//glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_width, frame_height, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, pixels);
	}
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(shader.u_texture, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_FALSE);
	glGenerateMipmap(GL_TEXTURE_2D);

	SDL_GetWindowSize(g_win, &screen_width, &screen_height);
	glViewport(0, 0, screen_width, screen_height);

/*
	SDL_GetWindowSize(g_win, &screen_width, &screen_height);

	int h = height;
	int w = width;
	int rr=(screen_height*10/height);
	h = (height*rr)/10;
	w = (width*rr)/10;
	if (w>screen_width) {
	    rr = (screen_width*10/width);
	    h = (height*rr)/10;
	    w = (width*rr)/10;
	}
	glViewport((screen_width-w)/2, (screen_height-h)/2, w, h);*/

	glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
	glVertexAttribPointer(shader.a_position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(shader.a_position);

	glBindBuffer(GL_ARRAY_BUFFER, buffers[1]);
	glVertexAttribPointer(shader.a_texcoord, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), NULL);
	glEnableVertexAttribArray(shader.a_texcoord);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[2]);
	glUniformMatrix4fv(shader.u_vp_matrix, 1, GL_FALSE, (const GLfloat * )&proj);
    	//glUniformMatrix4fv(shader.u_vp_matrix, 1, GL_FALSE, (float*)m);
	glDrawElements(GL_TRIANGLES, kIndexCount, GL_UNSIGNED_SHORT, 0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	//glFlush();
}