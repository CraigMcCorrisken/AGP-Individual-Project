// Windows specific: Uncomment the following line to open a console window for debug output
#if _DEBUG
#pragma comment(linker, "/subsystem:\"console\" /entry:\"WinMainCRTStartup\"")
#endif

#include "rt3d.h"
#include "rt3dObjLoader.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stack>
#include <iostream>
#include <SDL_image.h>

using namespace std;

#define DEG_TO_RADIAN 0.017453293

// Globals
// Real programs don't use globals :-D
GLuint meshIndexCount;
GLuint bunnyIndexCount;
GLuint meshObjects[3];

GLuint phongShaderProgram;
GLuint textureBlenderProgram;
GLuint skyboxProgram;

vector<GLfloat> verts;
vector<GLfloat> norms;
vector<GLfloat> tex_coords;
vector<GLuint> indices;

GLfloat r = 0.0f;

glm::vec3 movingLightPos(0.0f, 1.0f, 0.0f); //light position
glm::vec3 eye(0.0f, 1.0f, 0.0f);
glm::vec3 at(0.0f, 1.0f, -1.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);

stack<glm::mat4> mvStack;

// TEXTURE STUFF
GLuint textures[10];
GLuint skybox[5];
GLuint labels[5];

// light attenuation
float attConstant = 1.0f;
float attLinear = 0.0f;
float attQuadratic = 0.0f;




SDL_Surface* gScreenSurface = NULL;



rt3d::lightStruct light0 = {
	{0.3f, 0.3f, 0.3f, 1.0f}, // ambient
	{1.0f, 1.0f, 1.0f, 1.0f}, // diffuse
	{1.0f, 1.0f, 1.0f, 1.0f}, // specular
	{-10.0f, 10.0f, 10.0f, 1.0f}  // position
};
glm::vec4 lightPos(-10.0f, 10.0f, 10.0f, 1.0f); //light position

//lab2 moving light
rt3d::lightStruct movingLight = {
	{ 0.3f, 0.3f, 0.3f, 1.0f }, // ambient
	{ 1.0f, 1.0f, 1.0f, 1.0f }, // diffuse
	{ 1.0f, 1.0f, 1.0f, 1.0f }, // specular
	{ eye.x, eye.y, eye.z, 1.0f }  // position
};

rt3d::materialStruct material0 = {
	{0.2f, 0.4f, 0.2f, 1.0f}, // ambient
	{0.5f, 1.0f, 0.5f, 1.0f}, // diffuse
	{0.0f, 0.1f, 0.0f, 1.0f}, // specular
	2.0f  // shininess
};
rt3d::materialStruct material1 = {
	{0.4f, 0.4f, 1.0f, 1.0f}, // ambient
	{0.8f, 0.8f, 1.0f, 1.0f}, // diffuse
	{0.8f, 0.8f, 0.8f, 1.0f}, // specular
	1.0f  // shininess
};

//for rotating the cube
float theta = 0.0f;

// Set up rendering context
SDL_Window * setupRC(SDL_GLContext &context) {
	SDL_Window * window;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) // Initialize video
		rt3d::exitFatalError("Unable to initialize SDL");

	// Request an OpenGL 3.0 context.

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);  // double buffering on
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8); // 8 bit alpha buffering
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); // Turn on x4 multisampling anti-aliasing (MSAA)

	// Create 800x600 window
	window = SDL_CreateWindow("SDL/GLM/OpenGL Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		800, 600, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if (!window) // Check window was created OK
		rt3d::exitFatalError("Unable to create window");

	context = SDL_GL_CreateContext(window); // Create opengl context and attach to window
	SDL_GL_SetSwapInterval(1); // set swap buffers to sync with monitor's vertical refresh rate
	return window;
}

// A simple texture loading function
// lots of room for improvement - and better error checking!
GLuint loadBitmap(const char *fname) {
	GLuint texID;
	glGenTextures(1, &texID); // generate texture ID

	// load file - using core SDL library
	SDL_Surface * tmpSurface = SDL_LoadBMP(fname);

	if (!tmpSurface) {
		std::cout << "Error loading bitmap" << std::endl;
	}

	// bind texture and set parameters
	glBindTexture(GL_TEXTURE_2D, texID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	SDL_PixelFormat *format = tmpSurface->format;

	GLuint externalFormat, internalFormat;
	if (format->Amask) {
		internalFormat = GL_RGBA;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGBA : GL_BGRA;
	}
	else {
		internalFormat = GL_RGB;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGB : GL_BGR;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, tmpSurface->w, tmpSurface->h, 0,
		externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
	glGenerateMipmap(GL_TEXTURE_2D);

	SDL_FreeSurface(tmpSurface); // texture loaded, free the temporary buffer
	return texID;	// return value of texture ID
}

//method found
//load any image
SDL_Surface* loadSurface(const char* path)
{
	//The final optimized image
	SDL_Surface* optimizedSurface = NULL;

	//Load image at specified path
	SDL_Surface* loadedSurface = IMG_Load(path);
	if (loadedSurface == NULL)
	{
		printf("Unable to load image %s! SDL_image Error: %s\n", path, IMG_GetError());
	}
	else
	{
		//Convert surface to screen format
		optimizedSurface = SDL_ConvertSurface(loadedSurface, gScreenSurface->format, NULL);
		if (optimizedSurface == NULL)
		{
			printf("Unable to optimize image %s! SDL Error: %s\n", path, SDL_GetError());
		}

		//Get rid of old loaded surface
		SDL_FreeSurface(loadedSurface);
	}
	return optimizedSurface;
}

// A simple cubemap loading function
GLuint loadCubeMap(const char *fname[6], GLuint *texID)
{
	glGenTextures(1, texID); // generate texture ID
	GLenum sides[6] = { GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
		GL_TEXTURE_CUBE_MAP_POSITIVE_X,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
		GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
		GL_TEXTURE_CUBE_MAP_NEGATIVE_Y };
	SDL_Surface *tmpSurface;

	glBindTexture(GL_TEXTURE_CUBE_MAP, *texID); // bind texture and set parameters
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	GLuint externalFormat;
	for (int i = 0; i < 6; i++)
	{
		// load file - using core SDL library
		tmpSurface = SDL_LoadBMP(fname[i]);
		if (!tmpSurface)
		{
			std::cout << "Error loading bitmap" << std::endl;
			return *texID;
		}

		// skybox textures should not have alpha (assuming this is true!)
		SDL_PixelFormat *format = tmpSurface->format;
		externalFormat = (format->Rmask < format->Bmask) ? GL_RGB : GL_BGR;

		glTexImage2D(sides[i], 0, GL_RGB, tmpSurface->w, tmpSurface->h, 0,
			externalFormat, GL_UNSIGNED_BYTE, tmpSurface->pixels);
		// texture loaded, free the temporary buffer
		SDL_FreeSurface(tmpSurface);
	}
	return *texID;	// return value of texure ID, redundant really
}

void init(void) {
	//phong
	phongShaderProgram = rt3d::initShaders("phong-tex.vert", "phong-tex.frag");
	rt3d::setLight(phongShaderProgram, light0);
	rt3d::setMaterial(phongShaderProgram, material0);

	textureBlenderProgram = rt3d::initShaders("textureBlender.vert", "textureBlender.frag");
	//rt3d::setLight(textureBlenderProgram, light0);
	//rt3d::setMaterial(textureBlenderProgram, material0);

	// set light attenuation shader uniforms
	GLuint uniformIndex = glGetUniformLocation(phongShaderProgram, "attConst");
	glUniform1f(uniformIndex, attConstant);
	uniformIndex = glGetUniformLocation(phongShaderProgram, "attLinear");
	glUniform1f(uniformIndex, attLinear);
	uniformIndex = glGetUniformLocation(phongShaderProgram, "attQuadratic");
	glUniform1f(uniformIndex, attQuadratic);

	skyboxProgram = rt3d::initShaders("cubeMap.vert", "cubeMap.frag");

	const char *cubeTexFiles[6] = {
		"town-skybox/Town_ft.bmp", "town-skybox/Town_bk.bmp",  "town-skybox/Town_lf.bmp", "town-skybox/Town_rt.bmp", "town-skybox/Town2_up.bmp", "town-skybox/Town_dn.bmp"
	};
	loadCubeMap(cubeTexFiles, &skybox[0]);

	//skybox cube object
	rt3d::loadObj("../Resources/cube.obj", verts, norms, tex_coords, indices);
	GLuint size = indices.size();
	meshIndexCount = size;
	meshObjects[0] = rt3d::createMesh(verts.size() / 3, verts.data(), nullptr, norms.data(), tex_coords.data(), size, indices.data());

	//bitmaps
	textures[0] = loadBitmap("../Resources/fabric.bmp");
	textures[1] = loadBitmap("../Resources/hobgoblin2.bmp");
	textures[2] = loadBitmap("../Resources/studdedmetal.bmp");
	textures[3] = loadBitmap("../Resources/textBrick.bmp");
	textures[4] = loadBitmap("../Resources/textMoss.bmp");
	textures[5] = loadBitmap("../Resources/textMarble.bmp");

	//allows png files to be loaded
	IMG_Init(IMG_INIT_PNG);

	//testing .pngs
	textures[7] = (GLuint)loadSurface("../Resources/brick.png");
	textures[8] = (GLuint)loadSurface("../Resources/studdedmetal.png");


	//base texture
	glUseProgram(textureBlenderProgram);
	GLint tex1_uniform_loc = glGetUniformLocation(textureBlenderProgram, "tex1");
	glUniform1i(tex1_uniform_loc, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, textures[0]);

	//texture for blending
	GLint tex2_uniform_loc = glGetUniformLocation(textureBlenderProgram, "tex2");
	glUniform1i(tex2_uniform_loc, 1);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, textures[3]);


	//loading the stanford bunny
	verts.clear(); norms.clear(); tex_coords.clear(); indices.clear();
	rt3d::loadObj("../Resources/bunny-5000.obj", verts, norms, tex_coords, indices);
	bunnyIndexCount = indices.size();
	meshObjects[2] = rt3d::createMesh(verts.size() / 3, verts.data(), nullptr, norms.data(), nullptr, bunnyIndexCount, indices.data());

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

glm::vec3 moveForward(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::sin(r*DEG_TO_RADIAN), pos.y, pos.z - d * std::cos(r*DEG_TO_RADIAN));
}

glm::vec3 moveRight(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::cos(r*DEG_TO_RADIAN), pos.y, pos.z + d * std::sin(r*DEG_TO_RADIAN));
}

void update(void) {
	const Uint8 *keys = SDL_GetKeyboardState(NULL);
	if (keys[SDL_SCANCODE_W]) eye = moveForward(eye, r, 0.1f);
	if (keys[SDL_SCANCODE_S]) eye = moveForward(eye, r, -0.1f);
	if (keys[SDL_SCANCODE_A]) eye = moveRight(eye, r, -0.1f);
	if (keys[SDL_SCANCODE_D]) eye = moveRight(eye, r, 0.1f);
	if (keys[SDL_SCANCODE_R]) eye.y += 0.1;
	if (keys[SDL_SCANCODE_F]) eye.y -= 0.1;

	if (keys[SDL_SCANCODE_COMMA]) r -= 1.0f;
	if (keys[SDL_SCANCODE_PERIOD]) r += 1.0f;

	if (keys[SDL_SCANCODE_1]) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDisable(GL_CULL_FACE);
	}
	if (keys[SDL_SCANCODE_2]) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glEnable(GL_CULL_FACE);
	}
}

void draw(SDL_Window * window) {
	// clear the screen
	glEnable(GL_CULL_FACE);
	glClearColor(0.9f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection(1.0);
	projection = glm::perspective(float(60.0f*DEG_TO_RADIAN), 800.0f / 600.0f, 1.0f, 150.0f);
	rt3d::setUniformMatrix4fv(phongShaderProgram, "projection", glm::value_ptr(projection));

	GLfloat scale(1.0f); // just to allow easy scaling of complete scene

	glm::mat4 modelview(1.0); // set base position for scene
	mvStack.push(modelview);

	//eye
	at = moveForward(eye, r, 1.0f);
	mvStack.top() = glm::lookAt(eye, at, up);

	// draw a skybox
	glUseProgram(skyboxProgram);
	rt3d::setUniformMatrix4fv(skyboxProgram, "projection", glm::value_ptr(projection));
	glDepthMask(GL_FALSE); // make sure depth test is off
	glm::mat3 mvRotOnlyMat3 = glm::mat3(mvStack.top());
	mvStack.push(glm::mat4(mvRotOnlyMat3));

	glCullFace(GL_FRONT); // drawing inside of cube!
	glBindTexture(GL_TEXTURE_CUBE_MAP, skybox[0]);
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(1.5f, 1.5f, 1.5f));
	rt3d::setUniformMatrix4fv(skyboxProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
	mvStack.pop();
	glCullFace(GL_BACK); // drawing inside of cube!

	// back to remainder of rendering
	glDepthMask(GL_TRUE); // make sure depth test is on

	glUseProgram(phongShaderProgram);

	glm::vec4 tmp = mvStack.top()*glm::vec4(movingLightPos, 1.0);
	light0.position[0] = tmp.x;
	light0.position[1] = tmp.y;
	light0.position[2] = tmp.z;
	std::cout << tmp.x << ", " << tmp.y << ", " << tmp.z << std::endl;
	rt3d::setLightPos(phongShaderProgram, glm::value_ptr(tmp));
	rt3d::setUniformMatrix4fv(phongShaderProgram, "projection", glm::value_ptr(projection));


	//draw a cube for texture blending
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(0.0f, 1.0f, -9.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(2.0f, 2.0f, 2.0f));
	rt3d::setUniformMatrix4fv(textureBlenderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(textureBlenderProgram, material1);
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
	mvStack.pop();

	//draw a cube for ground plane
	glBindTexture(GL_TEXTURE_2D, textures[0]);
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(-10.0f, -0.1f, -10.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(20.0f, 0.1f, 20.0f));
	rt3d::setUniformMatrix4fv(phongShaderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(phongShaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[0], meshIndexCount, GL_TRIANGLES);
	mvStack.pop();


	// draw the toon shaded bunny
	glBindTexture(GL_TEXTURE_2D, 2);
	glUseProgram(phongShaderProgram);
	rt3d::setLightPos(phongShaderProgram, glm::value_ptr(tmp));
	rt3d::setUniformMatrix4fv(phongShaderProgram, "projection", glm::value_ptr(projection));
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(0.0f, 0.0f, -5.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(scale*10.0f, scale*10.0f, scale*10.0f));
	rt3d::setUniformMatrix4fv(phongShaderProgram, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(phongShaderProgram, material0);
	rt3d::drawIndexedMesh(meshObjects[2], bunnyIndexCount, GL_TRIANGLES);
	mvStack.pop();

	// remember to use at least one pop operation per push...
	mvStack.pop(); // initial matrix
	glDepthMask(GL_TRUE);

	SDL_GL_SwapWindow(window); // swap buffers
}

// Program entry point - SDL manages the actual WinMain entry point for us
int main(int argc, char *argv[]) {
	SDL_Window * hWindow; // window handle
	SDL_GLContext glContext; // OpenGL context handle
	hWindow = setupRC(glContext); // Create window and render context 

	// Required on Windows *only* init GLEW to access OpenGL beyond 1.1
	glewExperimental = GL_TRUE;
	GLenum err = glewInit();
	if (GLEW_OK != err) { // glewInit failed, something is seriously wrong
		std::cout << "glewInit failed, aborting." << endl;
		exit(1);
	}
	cout << glGetString(GL_VERSION) << endl;

	init();

	bool running = true; // set running to true
	SDL_Event sdlEvent;  // variable to detect SDL events
	while (running) {	// the event loop
		while (SDL_PollEvent(&sdlEvent)) {
			if (sdlEvent.type == SDL_QUIT)
				running = false;
		}
		update();
		draw(hWindow); // call the draw function
	}

	SDL_GL_DeleteContext(glContext);
	SDL_DestroyWindow(hWindow);
	IMG_Quit();
	SDL_Quit();
	return 0;
}