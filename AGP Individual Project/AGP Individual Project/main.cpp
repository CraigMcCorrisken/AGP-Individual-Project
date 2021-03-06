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
#include <vector>

using namespace std;

// Globals
// Real programs don't use globals :-D
GLuint meshIndexCount;
GLuint meshObject;

GLuint phongShaderProgram;
GLuint textureBlenderProgram;
GLuint currentShader = NULL;
GLuint shaderArray[2];
GLuint currentEffectTexture = NULL;

vector<GLfloat> verts;
vector<GLfloat> norms;
vector<GLfloat> tex_coords;
vector<GLuint> indices;

GLfloat r = 0.0f;

glm::vec3 eye(0.0f, 1.0f, 15.0f);
glm::vec3 at(0.0f, 1.0f, -1.0f);
glm::vec3 up(0.0f, 1.0f, 0.0f);

stack<glm::mat4> mvStack;

// TEXTURE STUFF
GLuint textures[3];
GLuint labels[5];

rt3d::lightStruct light0 = {
	{0.3f, 0.3f, 0.3f, 1.0f}, // ambient
	{1.0f, 1.0f, 1.0f, 1.0f}, // diffuse
	{1.0f, 1.0f, 1.0f, 1.0f}, // specular
	{-10.0f, 10.0f, 10.0f, 1.0f}  // position
};
glm::vec4 lightPos(-10.0f, 10.0f, 10.0f, 1.0f); //light position

rt3d::materialStruct material1 = {
	{0.4f, 0.4f, 1.0f, 1.0f}, // ambient
	{0.8f, 0.8f, 1.0f, 1.0f}, // diffuse
	{0.8f, 0.8f, 0.8f, 1.0f}, // specular
	1.0f  // shininess
};

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
GLuint loadTexture(const char *fname) {
	GLuint texID;
	glGenTextures(1, &texID); // generate texture ID

	// load file - using extended SDL_image library
	SDL_Surface * tmpSurface = IMG_Load(fname);

	if (!tmpSurface) {
		std::cout << "Error loading texture" << std::endl;
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

void init(void) {

	//allows png files to be loaded
	IMG_Init(IMG_INIT_PNG);

	//phong
	phongShaderProgram = rt3d::initShaders("phong-tex.vert", "phong-tex.frag");
	rt3d::setLight(phongShaderProgram, light0);
	rt3d::setMaterial(phongShaderProgram, material1);

	//initialising texture blender shader
	textureBlenderProgram = rt3d::initShaders("textureBlender.vert", "textureBlender.frag");

	//skybox cube object
	rt3d::loadObj("../Resources/cube.obj", verts, norms, tex_coords, indices);
	GLuint size = indices.size();
	meshIndexCount = size;
	meshObject = rt3d::createMesh(verts.size() / 3, verts.data(), nullptr, norms.data(), tex_coords.data(), size, indices.data());

	//loading textures
	textures[4] = loadTexture("../Resources/splatter.png");
	textures[3] = loadTexture("../Resources/brick.png");
	textures[2] = loadTexture("../Resources/red2.png");
	textures[1] = loadTexture("../Resources/moss.png");
	textures[0] = loadTexture("../Resources/Cobblestone.png");

	//filling shader array
	shaderArray[0] = phongShaderProgram;
	shaderArray[1] = textureBlenderProgram;

	//starting shader is texture blender
	currentShader = shaderArray[1];
	currentEffectTexture = textures[1];

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

glm::vec3 moveForward(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::sin(glm::radians(r)), pos.y, pos.z - d * std::cos(glm::radians(r)));
}

glm::vec3 moveRight(glm::vec3 pos, GLfloat angle, GLfloat d) {
	return glm::vec3(pos.x + d * std::cos(glm::radians(r)), pos.y, pos.z + d * std::sin(glm::radians(r)));
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
		//phong
		currentShader = shaderArray[0];
	}
	if (keys[SDL_SCANCODE_2]) {
		//texture blender
		currentShader = shaderArray[1];
	}

	if (keys[SDL_SCANCODE_3]) {
		//moss
		currentEffectTexture = textures[1];
	}
	if (keys[SDL_SCANCODE_4]) {
		//red circle
		currentEffectTexture = textures[2];
	}
	if (keys[SDL_SCANCODE_5]) {
		//brick
		currentEffectTexture = textures[3];
	}
	if (keys[SDL_SCANCODE_6]) {
		//blood splatter
		currentEffectTexture = textures[4];
	}
}

void draw(SDL_Window * window) {
	// clear the screen
	glEnable(GL_CULL_FACE);
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 projection(1.0);
	projection = glm::perspective(float(glm::radians(60.0f)), 800.0f / 600.0f, 1.0f, 150.0f);
	rt3d::setUniformMatrix4fv(phongShaderProgram, "projection", glm::value_ptr(projection));

	GLfloat scale(1.0f); // just to allow easy scaling of complete scene

	glm::mat4 modelview(1.0); // set base position for scene
	mvStack.push(modelview);

	//eye
	at = moveForward(eye, r, 1.0f);
	mvStack.top() = glm::lookAt(eye, at, up);

	// back to remainder of rendering
	glDepthMask(GL_TRUE); // make sure depth test is on
	rt3d::setUniformMatrix4fv(phongShaderProgram, "projection", glm::value_ptr(projection));

	//draw a cube for texture blending
	//base texture
	glUseProgram(currentShader);
	rt3d::setUniformMatrix4fv(textureBlenderProgram, "projection", glm::value_ptr(projection));

	//enabling & binding textures which are passed to shader for blending
	glActiveTexture(GL_TEXTURE1); // < - 1
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, currentEffectTexture); 
	GLint tex1_uniform_loc = glGetUniformLocation(textureBlenderProgram, "tex1");
	glUniform1i(tex1_uniform_loc, 1); // < -- 1 - note: if you swap the numbers 1 and 2 here and the corresponding line below (labeled HERE) it will swap the textures

	//texture for blending
	glActiveTexture(GL_TEXTURE2); // <-- 2
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, textures[0]); // <-- don't forget to bind
	GLint tex2_uniform_loc = glGetUniformLocation(textureBlenderProgram, "tex2");
	glUniform1i(tex2_uniform_loc, 2); // < -- 2 (HERE)

	//drawing a basic box for proof of concept.
	mvStack.push(mvStack.top());
	mvStack.top() = glm::translate(mvStack.top(), glm::vec3(0.0f, 1.0f, 6.0f));
	mvStack.top() = glm::scale(mvStack.top(), glm::vec3(scale*2, scale*2, scale*2));
	mvStack.top() = glm::rotate(mvStack.top(), glm::radians(45.0f), glm::vec3(1.0f, 1.0f, 0.0f));
	rt3d::setUniformMatrix4fv(currentShader, "modelview", glm::value_ptr(mvStack.top()));
	rt3d::setMaterial(currentShader, material1);
	rt3d::drawIndexedMesh(meshObject, meshIndexCount, GL_TRIANGLES);
	mvStack.pop();

	// remember to use at least one pop operation per push...
	mvStack.pop(); // initial matrix
	glDepthMask(GL_TRUE);	//enable depth mask
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